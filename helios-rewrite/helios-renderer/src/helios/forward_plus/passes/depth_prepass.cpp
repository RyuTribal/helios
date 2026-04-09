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

    // Capture mesh draws and camera for the execute lambda.
    // These are copied by value so the lambda is self-contained.
    const auto mesh_draws = packet.mesh_draws;
    const auto camera     = packet.camera;
    const uint32_t vp_w   = packet.viewport_width;
    const uint32_t vp_h   = packet.viewport_height;

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
        [mesh_draws, camera, vp_w, vp_h](
            const PassData& /*data*/, graph::RenderContext& ctx)
        {
            // Record depth prepass commands.
            //
            // This pass renders all meshes to a depth-only buffer using the
            // depth_prepass.vert shader.  The pipeline is expected to be
            // pre-created by pipeline_init and bound externally when the
            // render graph is fully connected.  For now the commands are
            // recorded structurally; the pipeline/descriptor binding assumes
            // that the "depth_prepass" pipeline is available.
            //
            // Shader requirements (depth_prepass.vert):
            //   set 0, binding 0: CameraUBO { mat4 view; mat4 projection; }
            //   push_constant:    PushConstantData { mat4 transform; }
            // Fragment shader is empty (depth writes only).

            auto& cmd = ctx.cmd();

            // Set viewport and scissor to match the depth texture dimensions.
            cmd.set_viewport(0.0f, 0.0f,
                             static_cast<float>(vp_w),
                             static_cast<float>(vp_h));
            cmd.set_scissor(0, 0, vp_w, vp_h);

            // Draw each mesh with its model transform pushed.
            for (const auto& draw : mesh_draws) {
                PushConstantData pc;
                pc.transform = draw.transform;
                cmd.push_constants(rhi::ShaderStage::Vertex, 0,
                                   sizeof(PushConstantData), &pc);
                // NOTE: Vertex/index buffer binding and draw_indexed are
                // deferred until the render graph is connected to the GPU
                // resource cache that resolves AssetHandle -> GPUMesh.
            }

            HELIOS_LOG_TRACE(ForwardPlus,
                             "DepthPrepass: recorded {} mesh draw commands",
                             mesh_draws.size());
        });

    HELIOS_LOG_TRACE(ForwardPlus, "Added DepthPrepass node to render graph");
    return output;
}

} // namespace helios
