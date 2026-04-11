#include "helios/forward_plus/passes/forward_pass.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/core/assert.h"

namespace helios {

ForwardPassOutput add_forward_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle depth_texture,
    const ShadowPassOutput& shadows,
    const LightCullingOutput& light_cull,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config)
{
    HELIOS_ASSERT(packet.viewport_width > 0 && packet.viewport_height > 0,
                  "Viewport dimensions must be > 0 for forward pass");
    HELIOS_ASSERT(light_cull.light_ssbo.is_valid(),
                  "Forward pass requires valid light culling output");

    struct PassData {
        graph::TextureHandle hdr_color;
        graph::TextureHandle depth;
        graph::TextureHandle shadow_map;
        graph::BufferHandle  light_ssbo;
        graph::BufferHandle  dir_light_ssbo;
        graph::BufferHandle  visible_indices;
    };

    ForwardPassOutput output;
    const uint32_t w = packet.viewport_width;
    const uint32_t h = packet.viewport_height;

    graph.add_pass<PassData>(
        "ForwardPass",
        [&](PassData& data, graph::RenderGraphBuilder& builder) {
            // Create HDR color output
            data.hdr_color = builder.create(rhi::TextureDesc{
                .width      = w,
                .height     = h,
                .format     = config.hdr_format,
                .usage      = rhi::TextureUsage::ColorAttachment
                            | rhi::TextureUsage::Sampled,
                .debug_name = "ForwardHDRColor",
            });
            data.hdr_color = builder.write(data.hdr_color,
                                           graph::ResourceUsage::ColorAttachment);
            output.color = data.hdr_color;

            // Read depth from prepass (for depth testing, no new depth created)
            if (depth_texture.is_valid()) {
                data.depth = builder.read(depth_texture);
            }

            // Read light culling outputs
            data.light_ssbo      = builder.read(light_cull.light_ssbo);
            data.dir_light_ssbo  = builder.read(light_cull.dir_light_ssbo);
            data.visible_indices = builder.read(light_cull.visible_indices_ssbo);

            // Read shadow map (if available)
            if (shadows.shadow_map.is_valid()) {
                data.shadow_map = builder.read(shadows.shadow_map);
            }
        },
        [mesh_draws   = packet.mesh_draws,
         camera       = packet.camera,
         dir_lights   = packet.dir_lights,
         skybox_data  = packet.skybox,
         vp_w         = w,
         vp_h         = h,
         tile_size    = config.tile_size,
         cascade_mats = shadows.cascade_matrices,
         env_bright   = 1.0f](
            const PassData& /*data*/, graph::RenderContext& ctx)
        {
            // Record forward PBR pass commands.
            //
            // Shader requirements (default_static.vert):
            //   set 0, binding 0: GlobalUBO
            //   push_constant:    PushConstantData { mat4 transform; }
            //
            // Shader requirements (default_static.frag):
            //   set 0, binding 0: GlobalUBO
            //   set 0, binding 1: LightSSBO (PointLightInfo[])
            //   set 0, binding 2: DirLightSSBO (DirectionalLightInfo[])
            //   set 0, binding 3: VisibleLightIndicesSSBO (VisibleIndex[])
            //   set 0, binding 4: LightSpaceMatrices UBO (mat4[16])
            //   set 1, binding 0: MaterialUBO
            //   set 1, binding 1..13: Material textures + IBL maps + shadow map

            auto& cmd = ctx.cmd();

            // Set viewport and scissor.
            cmd.set_viewport(0.0f, 0.0f,
                             static_cast<float>(vp_w),
                             static_cast<float>(vp_h));
            cmd.set_scissor(0, 0, vp_w, vp_h);

            // Draw each mesh with its model transform pushed.
            // The pipeline, global descriptor set (set 0), and per-material
            // descriptor set (set 1) are expected to be bound externally when
            // the render graph is fully connected to pipeline_init and the
            // GPU resource cache.
            for (const auto& draw : mesh_draws) {
                PushConstantData pc;
                pc.transform = draw.transform;
                cmd.push_constants(rhi::ShaderStage::Vertex, 0,
                                   sizeof(PushConstantData), &pc);
                // NOTE: Vertex/index buffer binding, material descriptor set
                // binding, and draw_indexed call are deferred until the render
                // graph is connected to GPUResourceCache + pipeline_init.
            }

            HELIOS_LOG_TRACE(ForwardPlus,
                             "ForwardPass: recorded {} mesh draw commands ({}x{})",
                             mesh_draws.size(), vp_w, vp_h);
        });

    HELIOS_LOG_TRACE(ForwardPlus,
                     "Added ForwardPass node ({}x{}, format=RGBA16F)", w, h);
    return output;
}

} // namespace helios
