// helios-renderer/src/helios/forward_plus/pipeline_init.cpp
#include "helios/forward_plus/pipeline_init.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"

namespace helios {

void initialize_forward_plus_pipelines(
    rhi::Device& /*device*/,
    rhi::PipelineCache& /*cache*/)
{
    // TODO: Implement when shader compilation (Task 16) produces .spv files
    // and we have a running render loop.
    //
    // Implementation plan -- each block loads shaders and creates a pipeline:
    //
    // 1. Depth prepass pipeline (graphics):
    //    - Shaders: depth_prepass.vert.spv, depth_prepass.frag.spv
    //    - State: depth_write=true, depth_test=true, cull=Back
    //    - Push constants: PushConstantData (model transform)
    //
    // 2. Shadow pass pipeline (graphics):
    //    - Shaders: dir_light_shadows.vert.spv, dir_light_shadows.frag.spv,
    //               dir_light_shadows.geom.spv (optional)
    //    - State: depth_write=true, depth_test=true, cull=Front (shadow bias)
    //    - Push constants: ShadowPushConstant (model transform)
    //
    // 3. Light culling pipeline (compute):
    //    - Shader: light_culling.comp.spv
    //    - Descriptor bindings: params UBO, light SSBO, visible indices SSBO, depth
    //
    // 4. Forward pass pipeline (graphics):
    //    - Shaders: default_static.vert.spv, default_static.frag.spv
    //    - State: depth_write=true, depth_test=true, cull=Back
    //    - Push constants: PushConstantData (model transform)
    //    - Descriptor sets: global UBO, material UBO + textures
    //
    // 5. Skybox pipeline (graphics):
    //    - Shaders: skybox.vert.spv, skybox.frag.spv
    //    - State: depth_write=false, depth_test=true, depth_compare=LessEqual, cull=None
    //    - Vertex layout: position only (3 floats)
    //
    // 6. Tonemap pipeline (compute):
    //    - Shader: tonemap.comp.spv
    //    - Push constants: TonemapPushConstants (exposure)
    //    - Descriptor bindings: HDR input (sampler), LDR output (storage image)
    //
    HELIOS_LOG_WARN(ForwardPlus,
        "initialize_forward_plus_pipelines: stub -- requires compiled shaders");
}

} // namespace helios
