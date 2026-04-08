// helios-renderer/src/helios/forward_plus/passes/tonemap_pass.cpp
#include "helios/forward_plus/passes/tonemap_pass.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/core/assert.h"

namespace helios {

graph::TextureHandle add_tonemap_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle hdr_input,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config)
{
    HELIOS_ASSERT(hdr_input.is_valid(),
                  "Tonemap pass requires a valid HDR input texture");
    HELIOS_ASSERT(packet.viewport_width > 0 && packet.viewport_height > 0,
                  "Viewport dimensions must be > 0 for tonemap pass");

    struct PassData {
        graph::TextureHandle hdr;
        graph::TextureHandle ldr;
    };

    graph::TextureHandle result;
    const uint32_t w = packet.viewport_width;
    const uint32_t h = packet.viewport_height;

    graph.add_pass<PassData>(
        "TonemapPass",
        [&](PassData& data, graph::RenderGraphBuilder& builder) {
            data.hdr = builder.read(hdr_input);
            data.ldr = builder.create(rhi::TextureDesc{
                .width      = w,
                .height     = h,
                .format     = rhi::TextureFormat::RGBA8,
                .usage      = rhi::TextureUsage::Sampled
                            | rhi::TextureUsage::Storage,
                .debug_name = "TonemapLDR",
            });
            data.ldr = builder.write(data.ldr,
                                     graph::ResourceUsage::ShaderWrite);
            result = data.ldr;
        },
        [exposure = config.exposure](
            const PassData& /*data*/, graph::RenderContext& /*ctx*/) {
            // TODO: record commands (Task 16-17)
            //  - Bind compute pipeline "tonemap_compute"
            //  - Bind descriptor set:
            //      binding 0: HDR input (CombinedImageSampler)
            //      binding 1: LDR output (StorageImage)
            //  - Push TonemapPushConstants{.exposure = exposure}
            //  - Dispatch((w + 15) / 16, (h + 15) / 16, 1)
            //    (workgroup size 16x16 matches tonemap.comp)
            (void)exposure;
            HELIOS_LOG_TRACE(ForwardPlus,
                             "TonemapPass execute (commands not yet recorded)");
        });

    HELIOS_LOG_TRACE(ForwardPlus,
                     "Added TonemapPass node ({}x{}, exposure={})",
                     w, h, config.exposure);
    return result;
}

} // namespace helios
