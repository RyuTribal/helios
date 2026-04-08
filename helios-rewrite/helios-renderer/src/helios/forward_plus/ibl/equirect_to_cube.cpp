// helios-renderer/src/helios/forward_plus/ibl/equirect_to_cube.cpp
#include "helios/forward_plus/ibl/equirect_to_cube.h"
#include "helios/forward_plus/forward_plus_log_channel.h"

namespace helios {

void convert_equirect_to_cube(
    rhi::Device& /*device*/,
    rhi::PipelineCache& /*cache*/,
    const rhi::Texture& /*equirect_input*/,
    rhi::Texture& /*cubemap_output*/,
    uint32_t /*cube_size*/)
{
    // TODO: Implement when running render loop is available.
    //
    // Implementation plan:
    //   1. Load compute shader: "shaders/equirect_to_cube.comp.spv"
    //   2. Create compute pipeline via cache
    //   3. Allocate descriptor set:
    //        binding 0: sampler2D (equirect_input)
    //        binding 1: imageCube (cubemap_output, storage write)
    //   4. device.immediate_submit([&](CommandBuffer& cmd) {
    //        - Transition cubemap_output to General layout
    //        - Bind pipeline + descriptors
    //        - Dispatch: ceil(cube_size/16), ceil(cube_size/16), 6 (one per face)
    //        - Transition cubemap_output to ShaderReadOnly
    //      });
    HELIOS_LOG_WARN(ForwardPlus,
        "convert_equirect_to_cube: stub -- requires GPU pipeline setup");
}

} // namespace helios
