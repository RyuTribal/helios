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
        [exposure = config.exposure,
         vp_w     = w,
         vp_h     = h](
            const PassData& /*data*/, graph::RenderContext& ctx)
        {
            // Record tonemap compute commands.
            //
            // Shader: tonemap.comp (16x16 workgroup)
            //   set 0, binding 0: sampler2D u_HDRInput
            //   set 0, binding 1: image2D u_LDROutput (rgba8, writeonly)
            //   push_constant:    TonemapPushConstants { float exposure; }
            //
            // Applies exposure-based tone mapping:
            //   color = 1.0 - exp(-color * exposure)

            auto& cmd = ctx.cmd();

            // Push exposure value.
            TonemapPushConstants pc;
            pc.exposure = exposure;
            cmd.push_constants(rhi::ShaderStage::Compute, 0,
                               sizeof(TonemapPushConstants), &pc);

            // Dispatch: one workgroup per 16x16 tile covering the viewport.
            uint32_t groups_x = (vp_w + 15) / 16;
            uint32_t groups_y = (vp_h + 15) / 16;
            cmd.dispatch(groups_x, groups_y, 1);

            HELIOS_LOG_TRACE(ForwardPlus,
                             "TonemapPass: dispatched {}x{} (exposure={})",
                             groups_x, groups_y, exposure);
        });

    HELIOS_LOG_TRACE(ForwardPlus,
                     "Added TonemapPass node ({}x{}, exposure={})",
                     w, h, config.exposure);
    return result;
}

} // namespace helios
