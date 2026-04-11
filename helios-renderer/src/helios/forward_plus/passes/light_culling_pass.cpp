#include "helios/forward_plus/passes/light_culling_pass.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/core/assert.h"

namespace helios {

LightCullingOutput add_light_culling_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle depth_texture,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config)
{
    HELIOS_ASSERT(depth_texture.is_valid(),
                  "Light culling requires a valid depth texture from the depth prepass");
    HELIOS_ASSERT(packet.viewport_width > 0 && packet.viewport_height > 0,
                  "Viewport dimensions must be > 0 for light culling");

    LightCullingOutput output;

    struct PassData {
        graph::TextureHandle depth_input;
        graph::BufferHandle  light_ssbo;
        graph::BufferHandle  dir_light_ssbo;
        graph::BufferHandle  visible_indices;
        graph::BufferHandle  params_ubo;
    };

    const uint32_t tile_size = config.tile_size;
    const uint32_t tiles_x   = (packet.viewport_width + tile_size - 1) / tile_size;
    const uint32_t tiles_y   = (packet.viewport_height + tile_size - 1) / tile_size;
    const uint32_t num_tiles  = tiles_x * tiles_y;

    graph.add_pass<PassData>(
        "LightCulling",
        [&](PassData& data, graph::RenderGraphBuilder& builder) {
            // Read depth texture from prepass
            data.depth_input = builder.read(depth_texture);

            // Create light SSBO (CPU-uploaded per frame)
            data.light_ssbo = builder.create(rhi::BufferDesc{
                .size       = static_cast<uint32_t>(
                    sizeof(PointLightGPU) * config.max_point_lights),
                .usage      = rhi::BufferUsage::Storage | rhi::BufferUsage::Transfer,
                .debug_name = "LightSSBO",
            });
            data.light_ssbo = builder.write(data.light_ssbo);
            output.light_ssbo = data.light_ssbo;

            // Directional light SSBO
            data.dir_light_ssbo = builder.create(rhi::BufferDesc{
                .size       = static_cast<uint32_t>(
                    sizeof(DirLightGPU) * config.max_dir_lights),
                .usage      = rhi::BufferUsage::Storage | rhi::BufferUsage::Transfer,
                .debug_name = "DirLightSSBO",
            });
            data.dir_light_ssbo = builder.write(data.dir_light_ssbo);
            output.dir_light_ssbo = data.dir_light_ssbo;

            // Visible indices SSBO -- one VisibleIndex per tile per max visible light
            const uint32_t vis_size = num_tiles
                * static_cast<uint32_t>(sizeof(VisibleIndex))
                * MAX_VISIBLE_LIGHTS_PER_TILE;
            data.visible_indices = builder.create(rhi::BufferDesc{
                .size       = vis_size,
                .usage      = rhi::BufferUsage::Storage | rhi::BufferUsage::Transfer,
                .debug_name = "VisibleIndicesSSBO",
            });
            data.visible_indices = builder.write(data.visible_indices);
            output.visible_indices_ssbo = data.visible_indices;

            // Params UBO (camera matrices, screen size, light count)
            data.params_ubo = builder.create(rhi::BufferDesc{
                .size       = static_cast<uint32_t>(sizeof(LightCullingParams)),
                .usage      = rhi::BufferUsage::Uniform,
                .debug_name = "LightCullingParamsUBO",
            });
            data.params_ubo = builder.write(data.params_ubo);
        },
        [point_lights = packet.point_lights,
         dir_lights   = packet.dir_lights,
         camera       = packet.camera,
         vp_w         = packet.viewport_width,
         vp_h         = packet.viewport_height,
         tile_size,
         tiles_x,
         tiles_y,
         num_tiles](
            const PassData& data, graph::RenderContext& ctx)
        {
            // Record light culling compute commands.
            //
            // Shader: light_culling.comp (16x16 workgroup)
            //   set 0, binding 0: LightCullingParams UBO
            //   set 0, binding 1: PointLightInfo SSBO (readonly)
            //   set 0, binding 2: VisibleIndex SSBO (writeonly)
            //   set 0, binding 3: sampler2D depthMap
            //
            // Steps:
            //   1. Upload point light data to light_ssbo
            //   2. Upload directional light data to dir_light_ssbo
            //   3. Upload LightCullingParams to params_ubo
            //   4. Bind compute pipeline + descriptor set
            //   5. Dispatch(tiles_x, tiles_y, 1)

            auto& cmd = ctx.cmd();

            // Upload point light data into the SSBO.
            auto& light_buf = ctx.resolve(data.light_ssbo);
            if (!point_lights.empty()) {
                // Convert from FramePacket PointLightData -> GPU PointLightGPU
                std::vector<PointLightGPU> gpu_lights;
                gpu_lights.reserve(point_lights.size());
                for (const auto& pl : point_lights) {
                    PointLightGPU g;
                    // Use radius-based attenuation approximation
                    g.constant_attenuation  = 1.0f;
                    g.linear_attenuation    = 2.0f / pl.radius;
                    g.quadratic_attenuation = 1.0f / (pl.radius * pl.radius);
                    g.intensity             = pl.intensity;
                    g.color                 = glm::vec4(pl.color, 1.0f);
                    g.position              = glm::vec4(pl.position, 1.0f);
                    gpu_lights.push_back(g);
                }
                light_buf.set_data(gpu_lights.data(),
                    static_cast<uint32_t>(gpu_lights.size() * sizeof(PointLightGPU)));
            }

            // Upload directional light data.
            auto& dir_buf = ctx.resolve(data.dir_light_ssbo);
            if (!dir_lights.empty()) {
                std::vector<DirLightGPU> gpu_dirs;
                gpu_dirs.reserve(dir_lights.size());
                for (const auto& dl : dir_lights) {
                    DirLightGPU g;
                    g._padding  = glm::vec3(0.0f);
                    g.intensity = dl.intensity;
                    g.color     = glm::vec4(dl.color, 1.0f);
                    g.direction = glm::vec4(dl.direction, 0.0f);
                    gpu_dirs.push_back(g);
                }
                dir_buf.set_data(gpu_dirs.data(),
                    static_cast<uint32_t>(gpu_dirs.size() * sizeof(DirLightGPU)));
            }

            // Upload culling parameters.
            auto& params_buf = ctx.resolve(data.params_ubo);
            LightCullingParams params;
            params.view        = camera.view;
            params.projection  = camera.projection;
            params.screen_size = glm::ivec2(
                static_cast<int>(vp_w), static_cast<int>(vp_h));
            params.light_count = static_cast<int>(point_lights.size());
            params._pad        = 0;
            params_buf.set_data(&params, sizeof(LightCullingParams));

            // Dispatch the compute shader: one workgroup per tile.
            cmd.dispatch(tiles_x, tiles_y, 1);

            HELIOS_LOG_TRACE(ForwardPlus,
                             "LightCulling: dispatched {}x{} tiles, {} point lights",
                             tiles_x, tiles_y, point_lights.size());
        });

    HELIOS_LOG_TRACE(ForwardPlus,
                     "Added LightCulling node ({} tiles: {}x{})",
                     num_tiles, tiles_x, tiles_y);
    return output;
}

} // namespace helios
