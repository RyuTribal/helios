// helios-renderer/src/helios/forward_plus/passes/depth_prepass.cpp
#include "helios/forward_plus/passes/depth_prepass.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/core/assert.h"

namespace helios {

DepthPrepassOutput add_depth_prepass(
    graph::RenderGraph& graph,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config)
{
    HELIOS_ASSERT(packet.viewport_width > 0 && packet.viewport_height > 0,
                  "Viewport dimensions must be > 0 for depth prepass");

    struct PassData {
        graph::TextureHandle depth;
    };

    DepthPrepassOutput output;

    graph.add_pass<PassData>(
        "DepthPrepass",
        [&](PassData& data, graph::RenderGraphBuilder& builder) {
            // Create transient depth texture at viewport resolution
            data.depth = builder.create(rhi::TextureDesc{
                .width      = packet.viewport_width,
                .height     = packet.viewport_height,
                .format     = config.depth_format,
                .usage      = rhi::TextureUsage::DepthAttachment
                            | rhi::TextureUsage::Sampled,
                .debug_name = "DepthPrepassTexture",
            });
            data.depth = builder.write(data.depth,
                                       graph::ResourceUsage::DepthAttachment);
            output.depth = data.depth;
        },
        [](const PassData& /*data*/, graph::RenderContext& /*ctx*/) {
            // TODO: record commands (Task 16-17)
            //  - Bind depth-only pipeline
            //  - Upload camera UBO (CameraUBOData)
            //  - Begin render pass with depth-only attachment
            //  - For each mesh draw: push model transform, draw indexed
            //  - End render pass
            HELIOS_LOG_TRACE(ForwardPlus,
                             "DepthPrepass execute (commands not yet recorded)");
        });

    HELIOS_LOG_TRACE(ForwardPlus, "Added DepthPrepass node to render graph");
    return output;
}

} // namespace helios
