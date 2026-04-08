// helios-renderer/src/helios/forward_plus/passes/skybox_pass.cpp
#include "helios/forward_plus/passes/skybox_pass.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/core/assert.h"

namespace helios {

graph::TextureHandle add_skybox_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle hdr_color,
    const renderer::FramePacket& /*packet*/)
{
    HELIOS_ASSERT(hdr_color.is_valid(),
                  "Skybox pass requires a valid HDR color texture");

    struct PassData {
        graph::TextureHandle color;
    };

    graph::TextureHandle result;

    graph.add_pass<PassData>(
        "SkyboxPass",
        [&](PassData& data, graph::RenderGraphBuilder& builder) {
            // Read-modify-write the HDR color texture
            data.color = builder.write(hdr_color,
                                       graph::ResourceUsage::ColorAttachment);
            result = data.color;
        },
        [](const PassData& /*data*/, graph::RenderContext& /*ctx*/) {
            // TODO: record commands (Task 16-17)
            //  - Check if skybox cubemap is available (packet.skybox.cubemap_texture)
            //  - Upload SkyboxUBOData (camera view/projection, brightness)
            //  - Bind skybox pipeline (depth test <=, depth write disabled)
            //  - Bind descriptor set: SkyboxUBO (binding 0), cubemap (binding 1)
            //  - Draw unit cube (36 vertices, no index buffer)
            //  Note: vertex shader outputs clipPos.xyww so skybox renders
            //  only where nothing else has drawn.
            HELIOS_LOG_TRACE(ForwardPlus,
                             "SkyboxPass execute (commands not yet recorded)");
        });

    HELIOS_LOG_TRACE(ForwardPlus, "Added SkyboxPass node to render graph");
    return result;
}

} // namespace helios
