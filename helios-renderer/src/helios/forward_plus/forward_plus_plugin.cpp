#include "helios/forward_plus/forward_plus_plugin.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/extract_render_data.h"
#include "helios/forward_plus/forward_plus_draw.h"
#include "helios/render_schedule.h"
#include "helios/camera_render_schedule.h"
#include "helios/forward_plus/pbr_render_state.h"
#include "helios/forward_plus/skybox_state.h"
#include "helios/forward_plus/gpu_cache.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/assets/asset_server.h"
#include "helios/assets/shader_asset.h"
#include "helios/assets/cubemap_asset.h"
#include "helios/assets/mesh_asset.h"
#include "helios/forward_plus/passes/depth_prepass.h"
#include "helios/forward_plus/passes/shadow_pass.h"
#include "helios/forward_plus/passes/light_culling_pass.h"
#include "helios/forward_plus/passes/forward_pass.h"
#include "helios/forward_plus/passes/skybox_pass.h"
#include "helios/forward_plus/passes/tonemap_pass.h"
#include "helios/forward_plus/pipeline_init.h"
#include "helios/render_plugin.h"
#include "helios/render_settings.h"
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
// ---------------------------------------------------------------------------


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
    GPUResourceCache& cache,
    rhi::TextureFormat swapchain_color_fmt,
    const std::string& skybox_hdr_path)
{
    SkyboxState skybox;

    if (skybox_hdr_path.empty() || !server) {
        HELIOS_LOG_INFO(ForwardPlus, "No skybox HDR path configured, skipping skybox setup");
        return skybox;
    }

    // ---- Load cubemap through asset pipeline + GPU cache ----
    auto cubemap_handle = server->load_sync<CubeMapAsset>(skybox_hdr_path);
    if (!cubemap_handle) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to load skybox HDR: {}", skybox_hdr_path);
        return skybox;
    }
    const CubeMapAsset* cm_asset = server->get<CubeMapAsset>(cubemap_handle.untyped());
    if (!cm_asset || !cm_asset->is_valid()) {
        HELIOS_LOG_ERROR(ForwardPlus, "Invalid cubemap asset: {}", skybox_hdr_path);
        return skybox;
    }
    rhi::Texture* env_cubemap = cache.get_or_upload_cubemap(
        cubemap_handle.untyped(), *cm_asset, device);
    if (!env_cubemap) {
        HELIOS_LOG_ERROR(ForwardPlus, "Failed to upload cubemap to GPU");
        return skybox;
    }

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

    skybox.env_cubemap = env_cubemap;
    skybox.env_cubemap_handle = cubemap_handle; // Keep refcount alive

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
                .texture_handle = skybox.env_cubemap,
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


    // Get AssetServer (may be null)
    AssetServer* asset_server = nullptr;
    if (app.world().has_resource<std::shared_ptr<AssetServer>>()) {
        auto& server_ptr = app.world().resource<std::shared_ptr<AssetServer>>();
        asset_server = server_ptr.get();
    }

    // Swapchain may not exist yet (lazy creation in frame_begin).
    // Use the known default format — pipelines are format-compatible.
    const rhi::TextureFormat swapchain_color_fmt = ctx.swapchain
        ? ctx.swapchain->color_format()
        : rhi::TextureFormat::BGRA8;

    // Create PBR pipeline state
    auto pbr = initialize_pbr_state(device, asset_server, swapchain_color_fmt);

    // Create default material descriptor set (white albedo, flat normal, no metallic)
    // This is the fallback for meshes with no material assigned.
    if (pbr.material_layout) {
        auto& cache = app.world().resource<GPUResourceCache>();
        auto* white = cache.get_or_create_default_white(device);
        auto* blue  = cache.get_or_create_default_blue(device);
        auto* black = cache.get_or_create_default_black(device);
        if (white && blue && black) {
            pbr.material_ds = device.allocate_descriptor_set(*pbr.material_layout);
            device.update_descriptor_set(*pbr.material_ds, {
                rhi::DescriptorWrite{.binding = 0, .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = white},   // albedo
                rhi::DescriptorWrite{.binding = 1, .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = blue},    // normal (flat = 0.5,0.5,1.0 encoded as blue-ish)
                rhi::DescriptorWrite{.binding = 2, .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = black},   // metallic-roughness
                rhi::DescriptorWrite{.binding = 3, .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = black},   // emissive
                rhi::DescriptorWrite{.binding = 4, .type = rhi::DescriptorType::CombinedImageSampler,
                    .texture_handle = white},   // env cubemap fallback
            });
            HELIOS_LOG_INFO(ForwardPlus, "Default material created");
        }
    }

    app.insert_resource(std::move(pbr));

    // Create skybox state (only if an HDR path is configured)
    auto& cache = app.world().resource<GPUResourceCache>();
    auto skybox = initialize_skybox_state(
        device, asset_server, cache, swapchain_color_fmt, config.skybox_hdr_path);
    app.insert_resource(std::move(skybox));

    HELIOS_LOG_INFO(ForwardPlus, "GPU pipeline infrastructure initialized");

    // Sync ForwardPlusConfig defaults into RenderSettings
    if (app.world().has_resource<RenderSettings>()) {
        auto& settings = app.world().resource<RenderSettings>();
        settings.exposure = config.exposure;
        settings.shadow_resolution = config.shadow_resolution;
        settings.shadow_cascades = config.shadow_cascades;
        settings.max_point_lights = config.max_point_lights;
        settings.max_dir_lights = config.max_dir_lights;
    }

    // Look up the frame_begin and frame_end SystemIds registered by RenderPlugin
    // so we can insert systems between them.
    auto begin_id = app.id_of("frame_begin");
    auto end_id = app.id_of("frame_end");

    auto driver_id = app.id_of("camera_driver");

    // Register the extraction system (main thread, runs during PreRender).
    // Must run after frame_begin, before camera_driver AND frame_end.
    // The .before(end_id) is needed to prevent the DAG builder from adding
    // an implicit reverse edge due to RenderContext read/write conflict.
    auto extract_builder = app.add_system(Schedule::PreRender, extract_render_data,
                                          "extract_render_data");
    extract_builder.after(begin_id);
    extract_builder.before(driver_id);
    extract_builder.before(end_id);
    auto extract_id = extract_builder.id();

    // Register the graph builder after extraction completes, before camera_driver.
    auto graph_builder = app.add_system(Schedule::PreRender, build_forward_plus_graph,
                                        "build_forward_plus_graph");
    graph_builder.after(extract_id);
    graph_builder.before(driver_id);
    graph_builder.before(end_id);

    // Register the Forward+ draw step via the camera render schedule system.
    // The camera_driver (registered by RenderPlugin) dispatches this for each
    // camera whose render_schedule is "forward_plus" (or empty/default).
    {
        auto& registry = app.world().resource<RenderScheduleRegistry>();
        registry.label("forward_plus"); // ensure label exists

        auto& schedules = app.world().resource<CameraRenderSchedules>();
        schedules.add_step(registry.find("forward_plus"), CameraDrawStep{
            .name = "forward_plus_draw",
            .order = 0,
            .run = forward_plus_draw_view,
        });
    }

    // Skybox hot-reload: watches ForwardPlusConfig::skybox_hdr_path for changes.
    // Loads/reloads the skybox whenever the path changes (including initial load).
    app.add_system(Schedule::PreUpdate, [](
        ResMut<ForwardPlusConfig> config,
        ResMut<SkyboxState> skybox,
        ResMut<RenderContext> ctx,
        ResMut<GPUResourceCache> cache,
        Res<std::shared_ptr<AssetServer>> server_ptr)
    {
        static std::string s_loaded_path;

        if (config->skybox_hdr_path == s_loaded_path) return;
        s_loaded_path = config->skybox_hdr_path;

        if (s_loaded_path.empty()) {
            // Clear the skybox
            *skybox = SkyboxState{};
            HELIOS_LOG_INFO(ForwardPlus, "Skybox cleared");
            return;
        }

        HELIOS_LOG_INFO(ForwardPlus, "Reloading skybox: {}", s_loaded_path);
        ctx->device->wait_idle();

        if (!ctx->swapchain) return;  // swapchain not ready yet
        auto new_skybox = initialize_skybox_state(
            *ctx->device, server_ptr->get(), *cache,
            ctx->swapchain->color_format(), s_loaded_path);
        *skybox = std::move(new_skybox);
    }, "skybox_hot_reload");

    HELIOS_LOG_INFO(ForwardPlus, "ForwardPlus plugin registered");
}

} // namespace helios
