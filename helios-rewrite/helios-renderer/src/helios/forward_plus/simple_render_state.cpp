// helios-renderer/src/helios/forward_plus/simple_render_state.cpp
#include "helios/forward_plus/simple_render_state.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/rhi/rhi_device.h"
#include "helios/rhi/rhi_types.h"

#include <glm/glm.hpp>

#include <fstream>
#include <vector>
#include <cstdint>
#include <filesystem>

namespace helios {

// ---------------------------------------------------------------------------
// Unit cube geometry (position + normal per vertex)
// ---------------------------------------------------------------------------

struct FlatVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

// Cube with outward-facing normals, 24 vertices (4 per face), 36 indices.
static void build_cube_geometry(std::vector<FlatVertex>& vertices,
                                std::vector<uint32_t>& indices)
{
    vertices = {
        // +Z face (front)
        {{ -0.5f, -0.5f,  0.5f }, {  0,  0,  1 }},
        {{  0.5f, -0.5f,  0.5f }, {  0,  0,  1 }},
        {{  0.5f,  0.5f,  0.5f }, {  0,  0,  1 }},
        {{ -0.5f,  0.5f,  0.5f }, {  0,  0,  1 }},
        // -Z face (back)
        {{  0.5f, -0.5f, -0.5f }, {  0,  0, -1 }},
        {{ -0.5f, -0.5f, -0.5f }, {  0,  0, -1 }},
        {{ -0.5f,  0.5f, -0.5f }, {  0,  0, -1 }},
        {{  0.5f,  0.5f, -0.5f }, {  0,  0, -1 }},
        // +X face (right)
        {{  0.5f, -0.5f,  0.5f }, {  1,  0,  0 }},
        {{  0.5f, -0.5f, -0.5f }, {  1,  0,  0 }},
        {{  0.5f,  0.5f, -0.5f }, {  1,  0,  0 }},
        {{  0.5f,  0.5f,  0.5f }, {  1,  0,  0 }},
        // -X face (left)
        {{ -0.5f, -0.5f, -0.5f }, { -1,  0,  0 }},
        {{ -0.5f, -0.5f,  0.5f }, { -1,  0,  0 }},
        {{ -0.5f,  0.5f,  0.5f }, { -1,  0,  0 }},
        {{ -0.5f,  0.5f, -0.5f }, { -1,  0,  0 }},
        // +Y face (top)
        {{ -0.5f,  0.5f,  0.5f }, {  0,  1,  0 }},
        {{  0.5f,  0.5f,  0.5f }, {  0,  1,  0 }},
        {{  0.5f,  0.5f, -0.5f }, {  0,  1,  0 }},
        {{ -0.5f,  0.5f, -0.5f }, {  0,  1,  0 }},
        // -Y face (bottom)
        {{ -0.5f, -0.5f, -0.5f }, {  0, -1,  0 }},
        {{  0.5f, -0.5f, -0.5f }, {  0, -1,  0 }},
        {{  0.5f, -0.5f,  0.5f }, {  0, -1,  0 }},
        {{ -0.5f, -0.5f,  0.5f }, {  0, -1,  0 }},
    };

    indices = {
         0,  1,  2,   2,  3,  0,   // +Z
         4,  5,  6,   6,  7,  4,   // -Z
         8,  9, 10,  10, 11,  8,   // +X
        12, 13, 14,  14, 15, 12,   // -X
        16, 17, 18,  18, 19, 16,   // +Y
        20, 21, 22,  22, 23, 20,   // -Y
    };
}

// ---------------------------------------------------------------------------
// Read SPIR-V file from disk
// ---------------------------------------------------------------------------

static std::vector<uint8_t> read_spirv(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    auto sz = file.tellg();
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

// ---------------------------------------------------------------------------
// create_simple_render_state
// ---------------------------------------------------------------------------

SimpleRenderState create_simple_render_state(
    rhi::Device& device,
    const char* shader_dir)
{
    SimpleRenderState state;

    // --- Load shaders -------------------------------------------------------
    std::filesystem::path dir(shader_dir);

    auto vert_spirv = read_spirv(dir / "flat_color.vert.spv");
    auto frag_spirv = read_spirv(dir / "flat_color.frag.spv");

    if (vert_spirv.empty() || frag_spirv.empty()) {
        HELIOS_LOG_WARN(ForwardPlus,
            "SimpleRenderState: flat_color shaders not found in '{}', "
            "rendering will be skipped", shader_dir);
        return state;
    }

    rhi::ShaderDesc vert_desc;
    vert_desc.stage      = rhi::ShaderStage::Vertex;
    vert_desc.spirv_code = std::move(vert_spirv);
    vert_desc.entry_point = "main";
    vert_desc.debug_name = "flat_color_vert";
    state.vert_shader = device.create_shader(vert_desc);

    rhi::ShaderDesc frag_desc;
    frag_desc.stage      = rhi::ShaderStage::Fragment;
    frag_desc.spirv_code = std::move(frag_spirv);
    frag_desc.entry_point = "main";
    frag_desc.debug_name = "flat_color_frag";
    state.frag_shader = device.create_shader(frag_desc);

    if (!state.vert_shader || !state.frag_shader) {
        HELIOS_LOG_ERROR(ForwardPlus, "SimpleRenderState: shader creation failed");
        return state;
    }

    // --- Descriptor set layout for camera UBO (set 0, binding 0) -----------
    rhi::DescriptorSetLayoutDesc layout_desc;
    layout_desc.bindings = {
        rhi::DescriptorBinding{
            .binding = 0,
            .type    = rhi::DescriptorType::UniformBuffer,
            .stage   = rhi::ShaderStage::Vertex,
            .count   = 1,
        },
    };
    layout_desc.debug_name = "SimpleCamera_DSL";
    state.camera_layout = device.create_descriptor_set_layout(layout_desc);
    if (!state.camera_layout) {
        HELIOS_LOG_ERROR(ForwardPlus, "SimpleRenderState: descriptor layout creation failed");
        return state;
    }

    // --- Graphics pipeline (dynamic rendering, Vulkan 1.3) -------------------
    // No traditional render pass needed; the pipeline is created with
    // VkPipelineRenderingCreateInfo matching the swapchain format (BGRA8).
    rhi::GraphicsPipelineDesc pipe_desc;
    pipe_desc.vertex_shader   = state.vert_shader.get();
    pipe_desc.fragment_shader = state.frag_shader.get();
    pipe_desc.layout.stride   = sizeof(FlatVertex);
    pipe_desc.layout.attributes = {
        rhi::VertexAttribute{
            .location = 0,
            .binding  = 0,
            .offset   = 0,
            .format   = rhi::TextureFormat::RGB32F,
        },
        rhi::VertexAttribute{
            .location = 1,
            .binding  = 0,
            .offset   = sizeof(glm::vec3),
            .format   = rhi::TextureFormat::RGB32F,
        },
    };
    pipe_desc.state.cull        = rhi::CullMode::Back;
    pipe_desc.state.depth       = rhi::DepthCompare::Less;
    pipe_desc.state.depth_test  = false;  // no depth buffer in swapchain
    pipe_desc.state.depth_write = false;
    pipe_desc.state.blend       = rhi::BlendMode::None;
    pipe_desc.render_pass       = nullptr;  // dynamic rendering
    pipe_desc.descriptor_layouts = { state.camera_layout.get() };
    pipe_desc.push_constant_size   = sizeof(PushConstantData);
    pipe_desc.push_constant_stages = rhi::ShaderStage::Vertex;
    pipe_desc.debug_name = "SimpleFlatColor";
    pipe_desc.use_dynamic_rendering = true;
    pipe_desc.dynamic_color_formats = { rhi::TextureFormat::BGRA8 };

    state.pipeline = device.create_graphics_pipeline(pipe_desc);
    if (!state.pipeline) {
        HELIOS_LOG_ERROR(ForwardPlus, "SimpleRenderState: pipeline creation failed");
        return state;
    }

    // --- Camera UBO (CPU_to_GPU for per-frame update) ----------------------
    rhi::BufferDesc ubo_desc;
    ubo_desc.size       = sizeof(CameraUBOData);
    ubo_desc.usage      = rhi::BufferUsage::Uniform;
    ubo_desc.access     = rhi::MemoryAccess::CPU_to_GPU;
    ubo_desc.debug_name = "SimpleCameraUBO";
    state.camera_ubo = device.create_buffer(ubo_desc);
    if (!state.camera_ubo) {
        HELIOS_LOG_ERROR(ForwardPlus, "SimpleRenderState: camera UBO creation failed");
        return state;
    }

    // --- Descriptor set (allocate and write camera UBO) --------------------
    state.camera_ds = device.allocate_descriptor_set(*state.camera_layout);
    if (!state.camera_ds) {
        HELIOS_LOG_ERROR(ForwardPlus, "SimpleRenderState: descriptor set allocation failed");
        return state;
    }

    device.update_descriptor_set(*state.camera_ds, {
        rhi::DescriptorWrite{
            .binding       = 0,
            .type          = rhi::DescriptorType::UniformBuffer,
            .buffer_handle = state.camera_ubo.get(),
            .offset        = 0,
            .range         = sizeof(CameraUBOData),
        },
    });

    // --- Cube vertex and index buffers -------------------------------------
    std::vector<FlatVertex> verts;
    std::vector<uint32_t> idxs;
    build_cube_geometry(verts, idxs);

    rhi::BufferDesc vbo_desc;
    vbo_desc.size       = static_cast<uint32_t>(verts.size() * sizeof(FlatVertex));
    vbo_desc.usage      = rhi::BufferUsage::Vertex;
    vbo_desc.access     = rhi::MemoryAccess::CPU_to_GPU;
    vbo_desc.debug_name = "SimpleCubeVBO";
    state.cube_vbo = device.create_buffer(vbo_desc, verts.data());

    rhi::BufferDesc ibo_desc;
    ibo_desc.size       = static_cast<uint32_t>(idxs.size() * sizeof(uint32_t));
    ibo_desc.usage      = rhi::BufferUsage::Index;
    ibo_desc.access     = rhi::MemoryAccess::CPU_to_GPU;
    ibo_desc.debug_name = "SimpleCubeIBO";
    state.cube_ibo = device.create_buffer(ibo_desc, idxs.data());

    state.cube_index_count = static_cast<uint32_t>(idxs.size());

    if (!state.cube_vbo || !state.cube_ibo) {
        HELIOS_LOG_ERROR(ForwardPlus, "SimpleRenderState: geometry buffer creation failed");
        return state;
    }

    state.valid = true;
    HELIOS_LOG_INFO(ForwardPlus, "SimpleRenderState created successfully "
                    "({} vertices, {} indices)", verts.size(), idxs.size());
    return state;
}

} // namespace helios
