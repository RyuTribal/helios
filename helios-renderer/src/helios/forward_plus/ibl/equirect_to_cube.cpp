#include "helios/forward_plus/ibl/equirect_to_cube.h"
#include "helios/forward_plus/forward_plus_log_channel.h"

#include "helios/rhi/rhi.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace helios {

void convert_equirect_to_cube(
    rhi::Device& device,
    rhi::PipelineCache& /*cache*/,
    const rhi::Texture& equirect_input,
    rhi::Texture& cubemap_output,
    uint32_t cube_size)
{
    // 1. Load compute shader
#ifdef HELIOS_SHADER_DIR
    const char* shader_dir = HELIOS_SHADER_DIR;
#else
    const char* shader_dir = "shaders";
#endif

    std::filesystem::path shader_path = std::filesystem::path(shader_dir) / "equirect_to_cube.comp.spv";
    std::ifstream file(shader_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to load equirect_to_cube.comp.spv from '{}'", shader_path.string());
        return;
    }
    auto sz = file.tellg();
    std::vector<uint8_t> comp_spirv(static_cast<size_t>(sz));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(comp_spirv.data()), sz);

    rhi::ShaderDesc shader_desc;
    shader_desc.stage = rhi::ShaderStage::Compute;
    shader_desc.spirv_code = std::move(comp_spirv);
    shader_desc.entry_point = "main";
    shader_desc.debug_name = "equirect_to_cube_comp";
    auto comp_shader = device.create_shader(shader_desc);

    // 2. Create descriptor layout
    rhi::DescriptorSetLayoutDesc layout_desc;
    layout_desc.bindings = {
        rhi::DescriptorBinding{
            .binding = 0,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .stage = rhi::ShaderStage::Compute,
            .count = 1,
        },
        rhi::DescriptorBinding{
            .binding = 1,
            .type = rhi::DescriptorType::StorageImage,
            .stage = rhi::ShaderStage::Compute,
            .count = 1,
        },
    };
    layout_desc.debug_name = "EquirectToCube_DSL";
    auto layout = device.create_descriptor_set_layout(layout_desc);

    // 3. Create compute pipeline
    rhi::ComputePipelineDesc pipe_desc;
    pipe_desc.compute_shader = comp_shader.get();
    pipe_desc.descriptor_layouts = { layout.get() };
    pipe_desc.debug_name = "EquirectToCube";
    auto pipeline = device.create_compute_pipeline(pipe_desc);

    // 4. Allocate and write descriptor set
    auto ds = device.allocate_descriptor_set(*layout);
    device.update_descriptor_set(*ds, {
        rhi::DescriptorWrite{
            .binding = 0,
            .type = rhi::DescriptorType::CombinedImageSampler,
            .texture_handle = const_cast<rhi::Texture*>(&equirect_input),
        },
        rhi::DescriptorWrite{
            .binding = 1,
            .type = rhi::DescriptorType::StorageImage,
            .texture_handle = &cubemap_output,
        },
    });

    // 5. Record and submit
    auto cmd = device.create_command_buffer();
    cmd->begin();

    // Transition cubemap to General for storage image write
    cmd->transition_image(cubemap_output,
        rhi::TextureLayout::Undefined, rhi::TextureLayout::General);

    cmd->bind_pipeline(*pipeline);
    cmd->bind_descriptor_set(0, *ds);

    uint32_t groups_x = (cube_size + 15) / 16;
    uint32_t groups_y = (cube_size + 15) / 16;
    cmd->dispatch(groups_x, groups_y, 6);

    // Transition cubemap: General -> ShaderReadOnly for sampling
    cmd->transition_image(cubemap_output,
        rhi::TextureLayout::General, rhi::TextureLayout::ShaderReadOnly);

    cmd->end();
    device.submit(*cmd);
    device.wait_idle();

    HELIOS_LOG_INFO(ForwardPlus, "Converted equirectangular to {}x{} cubemap", cube_size, cube_size);
}

} // namespace helios
