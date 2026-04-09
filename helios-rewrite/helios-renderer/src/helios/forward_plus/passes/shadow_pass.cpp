// helios-renderer/src/helios/forward_plus/passes/shadow_pass.cpp
#include "helios/forward_plus/passes/shadow_pass.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/core/assert.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <limits>

namespace helios {

// ---------------------------------------------------------------------------
// Cascade matrix computation
// ---------------------------------------------------------------------------

std::vector<float> compute_cascade_splits(
    float /*near_plane*/, float far_plane, uint32_t cascade_count)
{
    // Geometric split scheme matching the original engine:
    //   far/50, far/25, far/10, far/2
    constexpr float ratios[] = {50.0f, 25.0f, 10.0f, 2.0f};

    std::vector<float> splits;
    splits.reserve(cascade_count > 0 ? cascade_count - 1 : 0);

    for (uint32_t i = 0; i + 1 < cascade_count && i < 4; ++i) {
        splits.push_back(far_plane / ratios[i]);
    }
    return splits;
}

glm::mat4 compute_light_space_matrix(
    const renderer::CameraData& camera,
    float split_near, float split_far,
    const glm::vec3& light_dir)
{
    const glm::mat4 proj = glm::perspective(
        glm::radians(camera.fov_y), camera.aspect_ratio,
        split_near, split_far);
    const glm::mat4 inv = glm::inverse(proj * camera.view);

    // Frustum corners in world space (NDC cube -> world)
    std::array<glm::vec4, 8> corners{};
    int idx = 0;
    for (int x = 0; x < 2; ++x)
        for (int y = 0; y < 2; ++y)
            for (int z = 0; z < 2; ++z)
                corners[static_cast<size_t>(idx++)] = inv * glm::vec4(
                    2.0f * static_cast<float>(x) - 1.0f,
                    2.0f * static_cast<float>(y) - 1.0f,
                    2.0f * static_cast<float>(z) - 1.0f, 1.0f);

    // Perspective divide
    for (auto& c : corners)
        c /= c.w;

    // Frustum center
    glm::vec3 center(0.0f);
    for (const auto& c : corners)
        center += glm::vec3(c);
    center /= 8.0f;

    const glm::mat4 light_view = glm::lookAt(
        center - glm::normalize(light_dir), center,
        glm::vec3(0.0f, 1.0f, 0.0f));

    // AABB in light space
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_y = std::numeric_limits<float>::max();
    float max_y = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();

    for (const auto& c : corners) {
        const glm::vec4 lc = light_view * c;
        min_x = std::min(min_x, lc.x);
        max_x = std::max(max_x, lc.x);
        min_y = std::min(min_y, lc.y);
        max_y = std::max(max_y, lc.y);
        min_z = std::min(min_z, lc.z);
        max_z = std::max(max_z, lc.z);
    }

    // Extend Z range to capture shadow casters behind the frustum
    constexpr float z_mult = 10.0f;
    min_z = (min_z < 0) ? min_z * z_mult : min_z / z_mult;
    max_z = (max_z < 0) ? max_z / z_mult : max_z * z_mult;

    return glm::ortho(min_x, max_x, min_y, max_y, min_z, max_z) * light_view;
}

std::vector<glm::mat4> compute_cascade_matrices(
    const renderer::CameraData& camera,
    const glm::vec3& light_dir,
    const std::vector<float>& splits)
{
    std::vector<glm::mat4> matrices;
    matrices.reserve(splits.size() + 1);

    float last_split = camera.near_plane;
    for (size_t i = 0; i <= splits.size(); ++i) {
        const float split_end = (i < splits.size()) ? splits[i] : camera.far_plane;
        matrices.push_back(
            compute_light_space_matrix(camera, last_split, split_end, light_dir));
        last_split = split_end;
    }
    return matrices;
}

// ---------------------------------------------------------------------------
// Shadow pass render graph node
// ---------------------------------------------------------------------------

ShadowPassOutput add_shadow_pass(
    graph::RenderGraph& graph,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config)
{
    ShadowPassOutput output;

    // Find the first shadow-casting directional light
    bool has_shadow_caster = false;
    glm::vec3 light_dir{0.0f, -1.0f, 0.0f};
    for (const auto& dl : packet.dir_lights) {
        if (dl.cast_shadows) {
            has_shadow_caster = true;
            light_dir = dl.direction;
            break;
        }
    }

    if (!has_shadow_caster) {
        HELIOS_LOG_TRACE(ForwardPlus,
                         "No shadow-casting directional light; shadow pass skipped");
        output.shadow_map = graph::TextureHandle{}; // invalid handle
        return output;
    }

    // Compute cascade matrices on the CPU
    const auto splits = config.cascade_splits.empty()
        ? compute_cascade_splits(packet.camera.near_plane,
                                 packet.camera.far_plane,
                                 config.shadow_cascades)
        : config.cascade_splits;
    output.cascade_matrices = compute_cascade_matrices(
        packet.camera, light_dir, splits);

    HELIOS_ASSERT(!output.cascade_matrices.empty(),
                  "Cascade matrix computation produced no matrices");

    struct PassData {
        graph::TextureHandle shadow_map;
    };

    const uint32_t res    = config.shadow_resolution;
    const uint32_t layers = static_cast<uint32_t>(output.cascade_matrices.size());

    graph.add_pass<PassData>(
        "ShadowPass",
        [&](PassData& data, graph::RenderGraphBuilder& builder) {
            data.shadow_map = builder.create(rhi::TextureDesc{
                .width        = res,
                .height       = res,
                .format       = rhi::TextureFormat::Depth32F,
                .type         = rhi::TextureType::Texture2DArray,
                .array_layers = layers,
                .usage        = rhi::TextureUsage::DepthAttachment
                              | rhi::TextureUsage::Sampled,
                .debug_name   = "ShadowMap",
            });
            data.shadow_map = builder.write(data.shadow_map,
                                            graph::ResourceUsage::DepthAttachment);
            output.shadow_map = data.shadow_map;
        },
        [mesh_draws = packet.mesh_draws,
         cascade_mats = output.cascade_matrices,
         shadow_res = res](
            const PassData& /*data*/, graph::RenderContext& ctx)
        {
            // Record shadow pass commands.
            //
            // This pass renders all meshes from the directional light's
            // perspective into a cascade shadow map array.
            //
            // Shader requirements:
            //   dir_light_shadows.vert:
            //     push_constant: ShadowPushConstant { mat4 transform; }
            //     (positions only -- just vertex coords)
            //   dir_light_shadows.geom:
            //     set 0, binding 0: LightSpaceMatrices UBO { mat4[16] }
            //     invocations = cascade_count, gl_Layer = gl_InvocationID
            //   dir_light_shadows.frag:
            //     empty (depth writes only)
            //
            // The geometry shader replicates each triangle into each cascade
            // layer, transforming by the corresponding light-space matrix.
            // This lets us render all cascades in a single draw call per mesh.

            auto& cmd = ctx.cmd();

            // Set viewport/scissor to shadow map resolution.
            cmd.set_viewport(0.0f, 0.0f,
                             static_cast<float>(shadow_res),
                             static_cast<float>(shadow_res));
            cmd.set_scissor(0, 0, shadow_res, shadow_res);

            // Draw each mesh.  The geometry shader handles per-cascade
            // replication via gl_InvocationID and LightSpaceMatrices UBO.
            for (const auto& draw : mesh_draws) {
                ShadowPushConstant pc;
                pc.transform = draw.transform;
                cmd.push_constants(rhi::ShaderStage::Vertex, 0,
                                   sizeof(ShadowPushConstant), &pc);
                // NOTE: Vertex/index buffer binding and draw_indexed are
                // deferred until the render graph is connected to the GPU
                // resource cache that resolves AssetHandle -> GPUMesh.
            }

            HELIOS_LOG_TRACE(ForwardPlus,
                             "ShadowPass: recorded {} mesh draws across {} cascades",
                             mesh_draws.size(), cascade_mats.size());
        });

    HELIOS_LOG_TRACE(ForwardPlus,
                     "Added ShadowPass node ({} cascades, {}x{} resolution)",
                     layers, res, res);
    return output;
}

} // namespace helios
