// helios-renderer/src/helios/forward_plus/passes/forward_pass.cpp
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
        [](const PassData& /*data*/, graph::RenderContext& /*ctx*/) {
            // TODO: record commands (Task 16-17)
            //  - Upload GlobalUBOData (camera, light counts, tiles_x, etc.)
            //  - Begin render pass with HDR color + depth attachments
            //  - Bind forward pipeline
            //  - Set viewport and scissor
            //  - Bind global descriptor set (set 0):
            //      binding 0: GlobalUBO
            //      binding 1: LightSSBO
            //      binding 2: DirLightSSBO
            //      binding 3: VisibleIndicesSSBO
            //      binding 4: LightMatricesUBO
            //  - For each mesh draw:
            //      - Push model transform
            //      - Bind material descriptor set (set 1)
            //      - Draw indexed
            //  - End render pass
            HELIOS_LOG_TRACE(ForwardPlus,
                             "ForwardPass execute (commands not yet recorded)");
        });

    HELIOS_LOG_TRACE(ForwardPlus,
                     "Added ForwardPass node ({}x{}, format=RGBA16F)", w, h);
    return output;
}

} // namespace helios
