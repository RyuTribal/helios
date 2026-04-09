// helios-renderer/src/helios/forward_plus/forward_plus_plugin.cpp
#include "helios/forward_plus/forward_plus_plugin.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/extract_render_data.h"
#include "helios/forward_plus/forward_plus_draw.h"
#include "helios/forward_plus/pbr_render_state.h"
#include "helios/forward_plus/skybox_state.h"
#include "helios/forward_plus/gpu_cache.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/forward_plus/ibl/equirect_to_cube.h"
#include "helios/assets/asset_server.h"
#include "helios/assets/shader_asset.h"
#include "helios/assets/importers/texture_importer.h"
#include "helios/assets/mesh_asset.h"
#include "helios/forward_plus/passes/depth_prepass.h"
#include "helios/forward_plus/passes/shadow_pass.h"
#include "helios/forward_plus/passes/light_culling_pass.h"
#include "helios/forward_plus/passes/forward_pass.h"
#include "helios/forward_plus/passes/skybox_pass.h"
#include "helios/forward_plus/passes/tonemap_pass.h"
#include "helios/forward_plus/pipeline_init.h"
#include "helios/render_plugin.h"
#include "helios/ecs/app.h"
#include "helios/graph/frame_packet.h"
#include "helios/graph/render_graph.h"

#include <glm/glm.hpp>

namespace helios {

// ---------------------------------------------------------------------------
// Skybox cube geometry (36 vertices, position only)
// ---------------------------------------------------------------------------

static std::vector<glm::vec3> build_skybox_cube() {
    return {
        {-1,-1, 1},{ 1,-1, 1},{ 1, 1, 1},{ 1, 1, 1},{-1, 1, 1},{-1,-1, 1},
        { 1,-1,-1},{-1,-1,-1},{-1, 1,-1},{-1, 1,-1},{ 1, 1,-1},{ 1,-1,-1},
        { 1,-1, 1},{ 1,-1,-1},{ 1, 1,-1},{ 1, 1,-1},{ 1, 1, 1},{ 1,-1, 1},
        {-1,-1,-1},{-1,-1, 1},{-1, 1, 1},{-1, 1, 1},{-1, 1,-1},{-1,-1,-1},
        {-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{ 1, 1,-1},{-1, 1,-1},{-1, 1, 1},
        {-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{ 1,-1, 1},{-1,-1, 1},{-1,-1,-1},
    };
}

// ---------------------------------------------------------------------------
// Helper: load SPIR-V bytes via AssetServer or directly from HELIOS_SHADER_DIR
// ---------------------------------------------------------------------------

static std::vector<uint8_t> load_shader_spirv(
    AssetServer* server,
    const std::string& shader_name)
{
    // Try AssetServer first (if it has a shader root configured)
    if (server) {
        auto handle = server->load_sync<ShaderAsset>(shader_name);
        if (handle) {
            const ShaderAsset* asset = server->get<ShaderAsset>(handle.untyped());
            if (asset && !asset->spirv.empty()) {
                return asset->spirv;
            }
        }
    }

    // Fallback: read directly from HELIOS_SHADER_DIR
#ifdef HELIOS_SHADER_DIR
    const std::string shader_dir = HELIOS_SHADER_DIR;
#else
    const std::string shader_dir = "shaders";
#endif

    std::filesystem::path path = std::filesystem::path(shader_dir) / shader_name;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    auto sz = file.tellg();
    std::vector<uint8_t> data(static_cast<size_t>(sz));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), sz);
    return data;
}

// ---------------------------------------------------------------------------
// Helper: load HDR equirectangular texture from CPU data to GPU
// ---------------------------------------------------------------------------

static std::unique_ptr<rhi::Texture> load_hdr_to_gpu(
    rhi::Device& device,
    const HdrTextureData& hdr_data,
    const char* debug_name)
{
    if (!hdr_data.is_valid()) return nullptr;

    // HDR data may be 3-channel; we need to expand to RGBA32F for the GPU.
    std::vector<float> rgba_pixels;
    const float* src = hdr_data.pixels.data();
    int total_pixels = hdr_data.width * hdr_data.height;

    if (hdr_data.channels == 4) {
        rgba_pixels.assign(src, src + total_pixels * 4);
    } else if (hdr_data.channels == 3) {
        rgba_pixels.resize(static_cast<size_t>(total_pixels) * 4);
        for (int i = 0; i < total_pixels; ++i) {
            rgba_pixels[i * 4 + 0] = src[i * 3 + 0];
            rgba_pixels[i * 4 + 1] = src[i * 3 + 1];
            rgba_pixels[i * 4 + 2] = src[i * 3 + 2];
            rgba_pixels[i * 4 + 3] = 1.0f;
        }
    } else {
        return nullptr;
    }

    rhi::TextureDesc desc;
    desc.width = static_cast<uint32_t>(hdr_data.width);
    desc.height = static_cast<uint32_t>(hdr_data.height);
    desc.format = rhi::TextureFormat::RGBA32F;
    desc.type = rhi::TextureType::Texture2D;
    desc.mip_levels = 1;
    desc.array_layers = 1;
    desc.usage = rhi::TextureUsage::Sampled;
    desc.sampler = rhi::SamplerMode::ClampToEdge;
    desc.debug_name = debug_name;

    auto tex = device.create_texture(desc, rgba_pixels.data());
    return tex;
}

// ---------------------------------------------------------------------------
// initialize_depth_buffer -- creates initial depth buffer for RenderContext
// ---------------------------------------------------------------------------

static void initialize_depth_buffer(RenderContext& ctx) {
    if (ctx.depth_texture) return;  // already created

    rhi::TextureDesc depth_desc;
    depth_desc.width = ctx.swapchain->width();
    depth_desc.height = ctx.swapchain->height();
    depth_desc.format = rhi::TextureFormat::Depth32F;
    depth_desc.type = rhi::TextureType::Texture2D;
    depth_desc.mip_levels = 1;
    depth_desc.array_layers = 1;
    depth_desc.usage = rhi::TextureUsage::DepthAttachment;
    depth_desc.debug_name = "DepthBuffer";
    ctx.depth_texture = ctx.device->create_texture(depth_desc);
}

// ---------------------------------------------------------------------------
// initialize_pbr_state -- creates PBR pipeline, shaders, descriptor layouts
// ---------------------------------------------------------------------------

static PBRRenderState initialize_pbr_state(
    rhi::Device& device,
    AssetServer* server,
    rhi::TextureFormat swapchain_color_fmt)
{
    PBRRenderState pbr;

    // ---- PBR Camera UBO (set 0, binding 0) ----
    {
        rhi::BufferDesc ubo_desc;
        ubo_desc.size = sizeof(PBRCameraUBO);
        ubo_desc.usage = rhi::BufferUsage::Uniform;
        ubo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
        ubo_desc.debug_name = "PBRCameraUBO";
        pbr.camera_ubo = device.create_buffer(ubo_desc);
    }

    // ---- Camera descriptor set layout (set 0) ----
    {
        rhi::DescriptorSetLayoutDesc layout_desc;
        layout_desc.bindings = {
            rhi::DescriptorBinding{
                .binding = 0,
                .type = rhi::DescriptorType::UniformBuffer,
                .stage = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment,
                .count = 1,
            },
        };
        layout_desc.debug_name = "PBRCamera_DSL";
        pbr.camera_layout = device.create_descriptor_set_layout(layout_desc);
    }

    // ---- Material descriptor set layout (set 1) ----
    {
        rhi::DescriptorSetLayoutDesc layout_desc;
        layout_desc.bindings = {
            rhi::DescriptorBinding{
                .binding = 0,
                .type = rhi::DescriptorType::CombinedImageSampler,
                .stage = rhi::ShaderStage::Fragment, .count = 1,
            },
            rhi::DescriptorBinding{
                .binding = 1,
                .type = rhi::DescriptorType::CombinedImageSampler,
                .stage = rhi::ShaderStage::Fragment, .count = 1,
            },
            rhi::DescriptorBinding{
                .binding = 2,
                .type = rhi::DescriptorType::CombinedImageSampler,
                .stage = rhi::ShaderStage::Fragment, .count = 1,
            },
            rhi::DescriptorBinding{
                .binding = 3,
                .type = rhi::DescriptorType::CombinedImageSampler,
                .stage = rhi::ShaderStage::Fragment, .count = 1,
            },
            rhi::DescriptorBinding{
                .binding = 4,
                .type = rhi::DescriptorType::CombinedImageSampler,
                .stage = rhi::ShaderStage::Fragment, .count = 1,
            },
        };
        layout_desc.debug_name = "PBRMaterial_DSL";
        pbr.material_layout = device.create_descriptor_set_layout(layout_desc);
    }

    // ---- Load PBR shaders ----
    auto vert_spirv = load_shader_spirv(server, "pbr_simple.vert.spv");
    auto frag_spirv = load_shader_spirv(server, "pbr_simple.frag.spv");

    if (vert_spirv.empty() || frag_spirv.empty()) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to load PBR shaders");
        return pbr;
    }

    {
        rhi::ShaderDesc vert_desc;
        vert_desc.stage = rhi::ShaderStage::Vertex;
        vert_desc.spirv_code = std::move(vert_spirv);
        vert_desc.entry_point = "main";
        vert_desc.debug_name = "pbr_simple_vert";
        pbr.vert_shader = device.create_shader(vert_desc);

        rhi::ShaderDesc frag_desc;
        frag_desc.stage = rhi::ShaderStage::Fragment;
        frag_desc.spirv_code = std::move(frag_spirv);
        frag_desc.entry_point = "main";
        frag_desc.debug_name = "pbr_simple_frag";
        pbr.frag_shader = device.create_shader(frag_desc);
    }

    // ---- PBR graphics pipeline ----
    {
        rhi::GraphicsPipelineDesc pipe_desc;
        pipe_desc.vertex_shader = pbr.vert_shader.get();
        pipe_desc.fragment_shader = pbr.frag_shader.get();
        pipe_desc.layout.stride = sizeof(PBRVertex);
        pipe_desc.layout.attributes = {
            rhi::VertexAttribute{
                .location = 0, .binding = 0, .offset = 0,
                .format = rhi::TextureFormat::RGB32F,
            },
            rhi::VertexAttribute{
                .location = 1, .binding = 0,
                .offset = sizeof(glm::vec3),
                .format = rhi::TextureFormat::RGB32F,
            },
            rhi::VertexAttribute{
                .location = 2, .binding = 0,
                .offset = sizeof(glm::vec3) * 2,
                .format = rhi::TextureFormat::RG32F,
            },
            rhi::VertexAttribute{
                .location = 3, .binding = 0,
                .offset = sizeof(glm::vec3) * 2 + sizeof(glm::vec2),
                .format = rhi::TextureFormat::RGBA32F,
            },
        };
        pipe_desc.state.cull = rhi::CullMode::Back;
        pipe_desc.state.depth = rhi::DepthCompare::Less;
        pipe_desc.state.depth_test = true;
        pipe_desc.state.depth_write = true;
        pipe_desc.state.blend = rhi::BlendMode::None;
        pipe_desc.render_pass = nullptr;
        pipe_desc.descriptor_layouts = {
            pbr.camera_layout.get(),
            pbr.material_layout.get()
        };
        pipe_desc.push_constant_size = sizeof(PushConstantData);
        pipe_desc.push_constant_stages = rhi::ShaderStage::Vertex;
        pipe_desc.debug_name = "PBRSimple";
        pipe_desc.use_dynamic_rendering = true;
        pipe_desc.dynamic_color_formats = { swapchain_color_fmt };
        pipe_desc.dynamic_depth_format = rhi::TextureFormat::Depth32F;

        pbr.pipeline = device.create_graphics_pipeline(pipe_desc);
        if (!pbr.pipeline) {
            HELIOS_LOG_ERROR(ForwardPlus, "Failed to create PBR pipeline");
            return pbr;
        }
    }

    // ---- Camera descriptor set ----
    {
        pbr.camera_ds = device.allocate_descriptor_set(*pbr.camera_layout);
        device.update_descriptor_set(*pbr.camera_ds, {
            rhi::DescriptorWrite{
                .binding = 0,
                .type = rhi::DescriptorType::UniformBuffer,
                .buffer_handle = pbr.camera_ubo.get(),
                .range = sizeof(PBRCameraUBO),
            },
        });
    }

    pbr.valid = true;
    HELIOS_LOG_INFO(ForwardPlus, "PBR pipeline created");
    return pbr;
}

// ---------------------------------------------------------------------------
// initialize_skybox_state -- creates skybox pipeline, shaders, cubemap
// ---------------------------------------------------------------------------

static SkyboxState initialize_skybox_state(
    rhi::Device& device,
    AssetServer* server,
    rhi::TextureFormat swapchain_color_fmt,
    const std::string& skybox_hdr_path)
{
    SkyboxState skybox;

    if (skybox_hdr_path.empty()) {
        HELIOS_LOG_INFO(ForwardPlus, "No skybox HDR path configured, skipping skybox setup");
        return skybox;
    }

    // ---- Load HDR equirectangular and convert to cubemap ----
    std::unique_ptr<rhi::Texture> env_cubemap;
    {
        // Load HDR data via AssetServer
        HdrTextureData hdr_data;
        bool have_hdr = false;

        if (server) {
            auto hdr_handle = server->load_sync<HdrTextureData>(skybox_hdr_path);
            if (hdr_handle) {
                const HdrTextureData* loaded = server->get<HdrTextureData>(hdr_handle.untyped());
                if (loaded && loaded->is_valid()) {
                    hdr_data = *loaded;
                    have_hdr = true;
                }
            }
        }

        if (!have_hdr) {
            HELIOS_LOG_ERROR(ForwardPlus, "Failed to load skybox HDR: {}", skybox_hdr_path);
            return skybox;
        }

        // Upload HDR to GPU as equirectangular texture
        auto equirect = load_hdr_to_gpu(device, hdr_data, "SkyboxEquirect");
        if (!equirect) {
            HELIOS_LOG_ERROR(ForwardPlus, "Failed to upload HDR texture to GPU");
            return skybox;
        }

        // Create output cubemap
        rhi::TextureDesc cube_desc;
        cube_desc.width = 1024;
        cube_desc.height = 1024;
        cube_desc.format = rhi::TextureFormat::RGBA16F;
        cube_desc.type = rhi::TextureType::TextureCube;
        cube_desc.mip_levels = 1;
        cube_desc.array_layers = 6;
        cube_desc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
        cube_desc.sampler = rhi::SamplerMode::ClampToEdge;
        cube_desc.debug_name = "EnvCubemap";
        env_cubemap = device.create_texture(cube_desc);

        if (!env_cubemap) {
            HELIOS_LOG_ERROR(ForwardPlus, "Failed to create cubemap texture");
            return skybox;
        }

        // Use a PipelineCache for the equirect-to-cube conversion
        rhi::PipelineCache pipe_cache(device);
        convert_equirect_to_cube(device, pipe_cache, *equirect, *env_cubemap, 1024);
    }

    if (!env_cubemap) return skybox;

    // ---- Load skybox shaders ----
    auto sky_vert_spirv = load_shader_spirv(server, "skybox.vert.spv");
    auto sky_frag_spirv = load_shader_spirv(server, "skybox.frag.spv");

    if (sky_vert_spirv.empty() || sky_frag_spirv.empty()) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to load skybox shaders");
        return skybox;
    }

    {
        rhi::ShaderDesc sv_desc;
        sv_desc.stage = rhi::ShaderStage::Vertex;
        sv_desc.spirv_code = std::move(sky_vert_spirv);
        sv_desc.entry_point = "main";
        sv_desc.debug_name = "skybox_vert";
        skybox.vert_shader = device.create_shader(sv_desc);

        rhi::ShaderDesc sf_desc;
        sf_desc.stage = rhi::ShaderStage::Fragment;
        sf_desc.spirv_code = std::move(sky_frag_spirv);
        sf_desc.entry_point = "main";
        sf_desc.debug_name = "skybox_frag";
        skybox.frag_shader = device.create_shader(sf_desc);
    }

    // ---- Descriptor set layout ----
    {
        rhi::DescriptorSetLayoutDesc sky_layout_desc;
        sky_layout_desc.bindings = {
            rhi::DescriptorBinding{
                .binding = 0,
                .type = rhi::DescriptorType::UniformBuffer,
                .stage = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment,
                .count = 1,
            },
            rhi::DescriptorBinding{
                .binding = 1,
                .type = rhi::DescriptorType::CombinedImageSampler,
                .stage = rhi::ShaderStage::Fragment,
                .count = 1,
            },
        };
        sky_layout_desc.debug_name = "Skybox_DSL";
        skybox.layout = device.create_descriptor_set_layout(sky_layout_desc);
    }

    // ---- UBO ----
    {
        rhi::BufferDesc sky_ubo_desc;
        sky_ubo_desc.size = sizeof(SkyboxUBOData);
        sky_ubo_desc.usage = rhi::BufferUsage::Uniform;
        sky_ubo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
        sky_ubo_desc.debug_name = "SkyboxUBO";
        skybox.ubo = device.create_buffer(sky_ubo_desc);
    }

    // ---- Graphics pipeline ----
    {
        rhi::GraphicsPipelineDesc sky_pipe;
        sky_pipe.vertex_shader = skybox.vert_shader.get();
        sky_pipe.fragment_shader = skybox.frag_shader.get();
        sky_pipe.layout.stride = sizeof(glm::vec3);
        sky_pipe.layout.attributes = {
            rhi::VertexAttribute{
                .location = 0, .binding = 0, .offset = 0,
                .format = rhi::TextureFormat::RGB32F,
            },
        };
        sky_pipe.state.cull = rhi::CullMode::None;
        sky_pipe.state.depth = rhi::DepthCompare::LessEqual;
        sky_pipe.state.depth_test = true;
        sky_pipe.state.depth_write = false;
        sky_pipe.state.blend = rhi::BlendMode::None;
        sky_pipe.render_pass = nullptr;
        sky_pipe.descriptor_layouts = { skybox.layout.get() };
        sky_pipe.push_constant_size = 0;
        sky_pipe.debug_name = "SkyboxPipeline";
        sky_pipe.use_dynamic_rendering = true;
        sky_pipe.dynamic_color_formats = { swapchain_color_fmt };
        sky_pipe.dynamic_depth_format = rhi::TextureFormat::Depth32F;

        skybox.pipeline = device.create_graphics_pipeline(sky_pipe);
    }

    // ---- Cube VBO ----
    {
        auto sky_verts = build_skybox_cube();
        rhi::BufferDesc sky_vbo_desc;
        sky_vbo_desc.size = static_cast<uint32_t>(sky_verts.size() * sizeof(glm::vec3));
        sky_vbo_desc.usage = rhi::BufferUsage::Vertex;
        sky_vbo_desc.access = rhi::MemoryAccess::CPU_to_GPU;
        sky_vbo_desc.debug_name = "SkyboxCubeVBO";
        skybox.cube_vbo = device.create_buffer(sky_vbo_desc, sky_verts.data());
    }

    skybox.env_cubemap = std::move(env_cubemap);

    // ---- Descriptor set ----
    {
        skybox.ds = device.allocate_descriptor_set(*skybox.layout);
        device.update_descriptor_set(*skybox.ds, {
            rhi::DescriptorWrite{
                .binding = 0,
                .type = rhi::DescriptorType::UniformBuffer,
                .buffer_handle = skybox.ubo.get(),
                .range = sizeof(SkyboxUBOData),
            },
            rhi::DescriptorWrite{
                .binding = 1,
                .type = rhi::DescriptorType::CombinedImageSampler,
                .texture_handle = skybox.env_cubemap.get(),
            },
        });
    }

    skybox.valid = true;
    HELIOS_LOG_INFO(ForwardPlus, "Skybox pipeline created");
    return skybox;
}

// ---------------------------------------------------------------------------
// build_forward_plus_graph -- wires all 6 passes into the render graph.
//
// Pass ordering:
//   DepthPrepass --> LightCulling --> ForwardPass --> SkyboxPass --> TonemapPass
//   ShadowPass  --^                  ^
//                                    |
//   (shadow map read by forward) ----+
// ---------------------------------------------------------------------------

void build_forward_plus_graph(
    Res<renderer::FramePacket> packet,
    Res<ForwardPlusConfig> config,
    ResMut<graph::RenderGraph> graph)
{
    // Clear the graph from the previous frame.
    graph->clear();

    // 1. Depth prepass -- writes a depth texture at viewport resolution.
    auto depth = add_depth_prepass(*graph, *packet, *config);

    // 2. Shadow pass -- cascade shadow maps from the first shadow-casting dir light.
    auto shadows = add_shadow_pass(*graph, *packet, *config);

    // 3. Light culling -- tile-based compute that reads the depth buffer.
    auto light_cull = add_light_culling_pass(
        *graph, depth.depth, *packet, *config);

    // 4. Forward PBR pass -- reads depth, shadow map, light buffers.
    auto hdr = add_forward_pass(
        *graph, depth.depth, shadows, light_cull, *packet, *config);

    // 5. Skybox pass -- renders behind all geometry into the HDR target.
    auto skybox = add_skybox_pass(*graph, hdr.color, *packet);

    // 6. Tonemap pass -- HDR -> LDR compute dispatch.
    auto ldr = add_tonemap_pass(*graph, skybox, *packet, *config);

    // Set final output: the render graph will cull any passes
    // that don't contribute to this output.
    graph->set_output(ldr);
}

// ---------------------------------------------------------------------------
// ForwardPlusPlugin::build
// ---------------------------------------------------------------------------

void ForwardPlusPlugin::build(App& app) {
    HELIOS_LOG_INFO(ForwardPlus, "Initializing ForwardPlus plugin");

    // Insert the pipeline configuration as a world resource.
    app.insert_resource(config);

    // Insert an empty FramePacket so extract_render_data can write into it.
    app.insert_resource(renderer::FramePacket{});

    // Insert an empty RenderGraph resource.
    app.insert_resource(graph::RenderGraph{});

    // Insert GPUResourceCache for automatic CPU->GPU upload
    app.insert_resource(GPUResourceCache{});

    // Insert a null AssetServer resource if one wasn't already registered
    // (e.g., if AssetPlugin wasn't used). The draw system checks for null.
    if (!app.world().has_resource<std::shared_ptr<AssetServer>>()) {
        app.insert_resource(std::shared_ptr<AssetServer>{});
    }

    // --- Initialize rendering infrastructure ---
    auto& ctx = app.world().resource<RenderContext>();
    auto& device = *ctx.device;

    // Create depth buffer if not already present
    initialize_depth_buffer(ctx);

    // Get AssetServer (may be null)
    AssetServer* asset_server = nullptr;
    if (app.world().has_resource<std::shared_ptr<AssetServer>>()) {
        auto& server_ptr = app.world().resource<std::shared_ptr<AssetServer>>();
        asset_server = server_ptr.get();
    }

    const rhi::TextureFormat swapchain_color_fmt = ctx.swapchain->color_format();

    // Create PBR pipeline state
    auto pbr = initialize_pbr_state(device, asset_server, swapchain_color_fmt);
    app.insert_resource(std::move(pbr));

    // Create skybox state (only if an HDR path is configured)
    auto skybox = initialize_skybox_state(
        device, asset_server, swapchain_color_fmt, config.skybox_hdr_path);
    app.insert_resource(std::move(skybox));

    HELIOS_LOG_INFO(ForwardPlus, "GPU pipeline infrastructure initialized");

    // Look up the frame_begin and frame_end SystemIds registered by RenderPlugin
    // so we can insert systems between them.
    auto begin_id = app.id_of("frame_begin");
    auto end_id = app.id_of("frame_end");

    // Register the extraction system (main thread, runs during PreRender).
    // Must run after frame_begin (needs RenderContext) and before frame_end.
    auto extract_builder = app.add_system(Schedule::PreRender, extract_render_data,
                                          "extract_render_data");
    extract_builder.after(begin_id);
    extract_builder.before(end_id);
    auto extract_id = extract_builder.id();

    // Register the graph builder after extraction completes, before frame_end.
    auto graph_builder = app.add_system(Schedule::PreRender, build_forward_plus_graph,
                                        "build_forward_plus_graph");
    graph_builder.after(extract_id);
    graph_builder.before(end_id);

    // Register the draw system between frame_begin and frame_end.
    // Runs after extraction + graph build so FramePacket is populated.
    auto draw_builder = app.add_system(Schedule::PreRender, forward_plus_draw,
                                       "forward_plus_draw");
    draw_builder.after(extract_id);
    draw_builder.before(end_id);

    HELIOS_LOG_INFO(ForwardPlus, "ForwardPlus plugin registered");
}

} // namespace helios
