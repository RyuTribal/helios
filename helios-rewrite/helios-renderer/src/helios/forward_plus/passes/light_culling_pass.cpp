// helios-renderer/src/helios/forward_plus/passes/light_culling_pass.cpp
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
        [](const PassData& /*data*/, graph::RenderContext& /*ctx*/) {
            // TODO: record commands (Task 16-17)
            //  - Upload point lights to light_ssbo
            //  - Upload directional lights to dir_light_ssbo
            //  - Initialize visible_indices with -1 sentinel
            //  - Upload LightCullingParams to params_ubo
            //  - Bind compute pipeline "light_culling"
            //  - Bind descriptor set (params UBO, light SSBO, visible indices, depth)
            //  - Dispatch(tiles_x, tiles_y, 1)
            HELIOS_LOG_TRACE(ForwardPlus,
                             "LightCulling execute (commands not yet recorded)");
        });

    HELIOS_LOG_TRACE(ForwardPlus,
                     "Added LightCulling node ({} tiles: {}x{})",
                     num_tiles, tiles_x, tiles_y);
    return output;
}

} // namespace helios
