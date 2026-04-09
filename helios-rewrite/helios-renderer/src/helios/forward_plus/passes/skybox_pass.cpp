// helios-renderer/src/helios/forward_plus/passes/skybox_pass.cpp
#include "helios/forward_plus/passes/skybox_pass.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/core/assert.h"

namespace helios {

graph::TextureHandle add_skybox_pass(
    graph::RenderGraph& graph,
    graph::TextureHandle hdr_color,
    const renderer::FramePacket& packet)
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
            // Read-modify-write the HDR color texture.
            // The read establishes the dependency on the forward pass that
            // created/wrote this texture; without it the graph compiler has
            // no edge to enforce ordering.
            builder.read(hdr_color);
            data.color = builder.write(hdr_color,
                                       graph::ResourceUsage::ColorAttachment);
            result = data.color;
        },
        [camera     = packet.camera,
         skybox_cfg = packet.skybox](
            const PassData& /*data*/, graph::RenderContext& ctx)
        {
            // Record skybox pass commands.
            //
            // Shader requirements:
            //   skybox.vert:
            //     layout(location = 0) in vec3 a_position
            //     set 0, binding 0: SkyboxUBO { mat4 view; mat4 proj; float brightness; }
            //   skybox.frag:
            //     set 0, binding 0: SkyboxUBO (same)
            //     set 0, binding 1: samplerCube u_EnvironmentMap
            //
            // Pipeline state:
            //     depth_test = true, depth_compare = LessEqual, depth_write = false
            //     cull = None
            //
            // The vertex shader outputs clipPos.xyww so the skybox fragment
            // only renders where depth == 1.0 (behind all geometry).
            //
            // The pipeline, descriptor set, and cube VBO are expected to be
            // pre-created by pipeline_init (or the existing SkyboxState) and
            // bound externally when the render graph is fully connected.

            auto& cmd = ctx.cmd();

            // When the skybox cubemap is not available, skip drawing.
            if (skybox_cfg.cubemap_texture == 0) {
                HELIOS_LOG_TRACE(ForwardPlus,
                                 "SkyboxPass: no cubemap configured, skipping");
                return;
            }

            // Draw unit cube (36 vertices, position-only).
            // Pipeline + descriptor set + cube VBO binding is deferred until
            // the render graph is connected to SkyboxState / pipeline_init.
            cmd.draw(36);

            HELIOS_LOG_TRACE(ForwardPlus, "SkyboxPass: recorded skybox draw");
        });

    HELIOS_LOG_TRACE(ForwardPlus, "Added SkyboxPass node to render graph");
    return result;
}

} // namespace helios
