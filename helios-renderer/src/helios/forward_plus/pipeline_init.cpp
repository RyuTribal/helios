#include "helios/forward_plus/pipeline_init.h"
#include "helios/forward_plus/forward_plus_log_channel.h"
#include "helios/forward_plus/gpu_data.h"
#include "helios/rhi/rhi.h"

#include <filesystem>

namespace helios {

// ---------------------------------------------------------------------------
// Helper: resolve the shader directory path.
// ---------------------------------------------------------------------------
static std::filesystem::path shader_dir() {
#ifdef HELIOS_SHADER_DIR
    return std::filesystem::path(HELIOS_SHADER_DIR);
#else
    return std::filesystem::path("shaders");
#endif
}

void initialize_forward_plus_pipelines(
    rhi::Device& device,
    rhi::PipelineCache& cache)
{
    const auto dir = shader_dir();

    // =====================================================================
    // 1. Depth prepass pipeline (graphics)
    //    Shaders: depth_prepass.vert.spv, depth_prepass.frag.spv
    //    Descriptor set 0: CameraUBO { mat4 view; mat4 projection; }
    //    Push constants: PushConstantData { mat4 transform; }
    //    State: depth_write=true, depth_test=true, cull=Back
    // =====================================================================
    {
        auto* vert = cache.load_shader("depth_prepass_vert",
                                       dir / "depth_prepass.vert.spv",
                                       rhi::ShaderStage::Vertex);
        auto* frag = cache.load_shader("depth_prepass_frag",
                                       dir / "depth_prepass.frag.spv",
                                       rhi::ShaderStage::Fragment);
        if (vert && frag) {
            // Descriptor set 0: CameraUBO
            rhi::DescriptorSetLayoutDesc camera_layout_desc;
            camera_layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type    = rhi::DescriptorType::UniformBuffer,
                    .stage   = rhi::ShaderStage::Vertex,
                    .count   = 1,
                },
            };
            camera_layout_desc.debug_name = "DepthPrepass_CameraUBO_DSL";
            auto camera_layout = device.create_descriptor_set_layout(camera_layout_desc);

            // Vertex layout: position only (vec3)
            rhi::GraphicsPipelineDesc desc;
            desc.vertex_shader   = vert;
            desc.fragment_shader = frag;
            desc.layout.stride   = sizeof(float) * 3;  // vec3 position only
            desc.layout.attributes = {
                rhi::VertexAttribute{
                    .location = 0, .binding = 0, .offset = 0,
                    .format   = rhi::TextureFormat::RGB32F,
                },
            };
            desc.state.cull        = rhi::CullMode::Back;
            desc.state.depth       = rhi::DepthCompare::Less;
            desc.state.depth_test  = true;
            desc.state.depth_write = true;
            desc.state.blend       = rhi::BlendMode::None;
            desc.descriptor_layouts       = { camera_layout.get() };
            desc.push_constant_size       = sizeof(PushConstantData);
            desc.push_constant_stages     = rhi::ShaderStage::Vertex;
            desc.debug_name               = "DepthPrepass";
            desc.use_dynamic_rendering    = true;
            desc.dynamic_color_formats    = {};  // depth-only, no color
            desc.dynamic_depth_format     = rhi::TextureFormat::Depth32F;

            if (cache.get_or_create_graphics_pipeline("depth_prepass", desc)) {
                HELIOS_LOG_INFO(ForwardPlus, "Created depth_prepass pipeline");
            } else {
                HELIOS_LOG_WARN(ForwardPlus, "Failed to create depth_prepass pipeline");
            }
        } else {
            HELIOS_LOG_WARN(ForwardPlus,
                "depth_prepass shaders not found -- pipeline skipped");
        }
    }

    // =====================================================================
    // 2. Shadow pass pipeline (graphics, with geometry shader)
    //    Shaders: dir_light_shadows.vert/geom/frag
    //    Descriptor set 0: LightSpaceMatrices UBO { mat4[16] }
    //    Push constants: ShadowPushConstant { mat4 transform; }
    //    State: depth_write=true, depth_test=true, cull=Front (shadow bias)
    // =====================================================================
    {
        auto* vert = cache.load_shader("shadow_vert",
                                       dir / "dir_light_shadows.vert.spv",
                                       rhi::ShaderStage::Vertex);
        auto* geom = cache.load_shader("shadow_geom",
                                       dir / "dir_light_shadows.geom.spv",
                                       rhi::ShaderStage::Geometry);
        auto* frag = cache.load_shader("shadow_frag",
                                       dir / "dir_light_shadows.frag.spv",
                                       rhi::ShaderStage::Fragment);
        if (vert && frag) {
            // Descriptor set 0: LightSpaceMatrices UBO
            rhi::DescriptorSetLayoutDesc lsm_layout_desc;
            lsm_layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type    = rhi::DescriptorType::UniformBuffer,
                    .stage   = rhi::ShaderStage::Geometry,
                    .count   = 1,
                },
            };
            lsm_layout_desc.debug_name = "ShadowPass_LSM_DSL";
            auto lsm_layout = device.create_descriptor_set_layout(lsm_layout_desc);

            rhi::GraphicsPipelineDesc desc;
            desc.vertex_shader   = vert;
            desc.fragment_shader = frag;
            desc.geometry_shader = geom;  // may be nullptr if .geom.spv missing
            desc.layout.stride   = sizeof(float) * 3;  // position only
            desc.layout.attributes = {
                rhi::VertexAttribute{
                    .location = 0, .binding = 0, .offset = 0,
                    .format   = rhi::TextureFormat::RGB32F,
                },
            };
            desc.state.cull        = rhi::CullMode::Front;  // shadow bias
            desc.state.depth       = rhi::DepthCompare::Less;
            desc.state.depth_test  = true;
            desc.state.depth_write = true;
            desc.state.blend       = rhi::BlendMode::None;
            desc.descriptor_layouts       = { lsm_layout.get() };
            desc.push_constant_size       = sizeof(ShadowPushConstant);
            desc.push_constant_stages     = rhi::ShaderStage::Vertex;
            desc.debug_name               = "ShadowPass";
            desc.use_dynamic_rendering    = true;
            desc.dynamic_color_formats    = {};  // depth-only
            desc.dynamic_depth_format     = rhi::TextureFormat::Depth32F;

            if (cache.get_or_create_graphics_pipeline("shadow_pass", desc)) {
                HELIOS_LOG_INFO(ForwardPlus, "Created shadow_pass pipeline");
            } else {
                HELIOS_LOG_WARN(ForwardPlus, "Failed to create shadow_pass pipeline");
            }
        } else {
            HELIOS_LOG_WARN(ForwardPlus,
                "shadow_pass shaders not found -- pipeline skipped");
        }
    }

    // =====================================================================
    // 3. Light culling pipeline (compute)
    //    Shader: light_culling.comp.spv
    //    Descriptor set 0:
    //      binding 0: LightCullingParams UBO (std140)
    //      binding 1: PointLightInfo SSBO (readonly)
    //      binding 2: VisibleIndex SSBO (writeonly)
    //      binding 3: sampler2D depthMap
    // =====================================================================
    {
        auto* comp = cache.load_shader("light_culling_comp",
                                       dir / "light_culling.comp.spv",
                                       rhi::ShaderStage::Compute);
        if (comp) {
            rhi::DescriptorSetLayoutDesc lc_layout_desc;
            lc_layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type    = rhi::DescriptorType::UniformBuffer,
                    .stage   = rhi::ShaderStage::Compute,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 1,
                    .type    = rhi::DescriptorType::StorageBuffer,
                    .stage   = rhi::ShaderStage::Compute,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 2,
                    .type    = rhi::DescriptorType::StorageBuffer,
                    .stage   = rhi::ShaderStage::Compute,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 3,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Compute,
                    .count   = 1,
                },
            };
            lc_layout_desc.debug_name = "LightCulling_DSL";
            auto lc_layout = device.create_descriptor_set_layout(lc_layout_desc);

            rhi::ComputePipelineDesc desc;
            desc.compute_shader    = comp;
            desc.descriptor_layouts = { lc_layout.get() };
            desc.debug_name        = "LightCulling";

            if (cache.get_or_create_compute_pipeline("light_culling", desc)) {
                HELIOS_LOG_INFO(ForwardPlus, "Created light_culling pipeline");
            } else {
                HELIOS_LOG_WARN(ForwardPlus, "Failed to create light_culling pipeline");
            }
        } else {
            HELIOS_LOG_WARN(ForwardPlus,
                "light_culling shader not found -- pipeline skipped");
        }
    }

    // =====================================================================
    // 4. Forward pass pipeline (graphics)
    //    Shaders: default_static.vert.spv, default_static.frag.spv
    //    Descriptor set 0 (global):
    //      binding 0: GlobalUBO
    //      binding 1: LightSSBO (PointLightInfo[])
    //      binding 2: DirLightSSBO (DirectionalLightInfo[])
    //      binding 3: VisibleIndicesSSBO (VisibleIndex[])
    //      binding 4: LightSpaceMatrices UBO (mat4[16])
    //    Descriptor set 1 (material):
    //      binding 0:  MaterialUBO
    //      binding 1:  sampler2D u_NormalTexture
    //      binding 2:  sampler2D u_RoughnessTexture
    //      binding 3:  sampler2D u_MetalnessTexture
    //      binding 5:  sampler2D u_AlbedoTexture
    //      binding 6:  sampler2D u_AOTexture
    //      binding 7:  sampler2D u_EmissionTexture
    //      binding 8:  sampler2D u_SpecularTexture
    //      binding 10: samplerCube u_IrradianceMap
    //      binding 11: samplerCube u_PrefilterMap
    //      binding 12: sampler2D u_BrdfLUT
    //      binding 13: sampler2DArray u_ShadowMap
    //    Push constants: PushConstantData { mat4 transform; }
    //    State: depth_write=true, depth_test=true, cull=Back
    // =====================================================================
    {
        auto* vert = cache.load_shader("forward_vert",
                                       dir / "default_static.vert.spv",
                                       rhi::ShaderStage::Vertex);
        auto* frag = cache.load_shader("forward_frag",
                                       dir / "default_static.frag.spv",
                                       rhi::ShaderStage::Fragment);
        if (vert && frag) {
            // Set 0: global
            rhi::DescriptorSetLayoutDesc global_layout_desc;
            global_layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type    = rhi::DescriptorType::UniformBuffer,
                    .stage   = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 1,
                    .type    = rhi::DescriptorType::StorageBuffer,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 2,
                    .type    = rhi::DescriptorType::StorageBuffer,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 3,
                    .type    = rhi::DescriptorType::StorageBuffer,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 4,
                    .type    = rhi::DescriptorType::UniformBuffer,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
            };
            global_layout_desc.debug_name = "ForwardPass_Global_DSL";
            auto global_layout = device.create_descriptor_set_layout(global_layout_desc);

            // Set 1: material
            rhi::DescriptorSetLayoutDesc mat_layout_desc;
            mat_layout_desc.bindings = {
                // binding 0: MaterialUBO
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type    = rhi::DescriptorType::UniformBuffer,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 1: normal map
                rhi::DescriptorBinding{
                    .binding = 1,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 2: roughness
                rhi::DescriptorBinding{
                    .binding = 2,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 3: metalness
                rhi::DescriptorBinding{
                    .binding = 3,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 4: reserved
                rhi::DescriptorBinding{
                    .binding = 4,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 5: albedo
                rhi::DescriptorBinding{
                    .binding = 5,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 6: AO
                rhi::DescriptorBinding{
                    .binding = 6,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 7: emission
                rhi::DescriptorBinding{
                    .binding = 7,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 8: specular
                rhi::DescriptorBinding{
                    .binding = 8,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 9: reserved
                rhi::DescriptorBinding{
                    .binding = 9,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 10: irradiance map (cubemap)
                rhi::DescriptorBinding{
                    .binding = 10,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 11: prefilter map (cubemap)
                rhi::DescriptorBinding{
                    .binding = 11,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 12: BRDF LUT
                rhi::DescriptorBinding{
                    .binding = 12,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                // binding 13: shadow map (2D array)
                rhi::DescriptorBinding{
                    .binding = 13,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
            };
            mat_layout_desc.debug_name = "ForwardPass_Material_DSL";
            auto mat_layout = device.create_descriptor_set_layout(mat_layout_desc);

            // Full vertex layout matching default_static.vert:
            //   location 0: vec3 a_coords
            //   location 1: vec4 a_colors
            //   location 2: vec2 a_texture_coords
            //   location 3: vec3 a_normals
            //   location 4: vec3 a_tangent
            //   location 5: vec3 a_bitangent
            constexpr uint32_t stride =
                sizeof(glm::vec3) +   // position
                sizeof(glm::vec4) +   // color
                sizeof(glm::vec2) +   // texcoord
                sizeof(glm::vec3) +   // normal
                sizeof(glm::vec3) +   // tangent
                sizeof(glm::vec3);    // bitangent

            rhi::GraphicsPipelineDesc desc;
            desc.vertex_shader   = vert;
            desc.fragment_shader = frag;
            desc.layout.stride   = stride;
            desc.layout.attributes = {
                rhi::VertexAttribute{
                    .location = 0, .binding = 0, .offset = 0,
                    .format   = rhi::TextureFormat::RGB32F,    // vec3 position
                },
                rhi::VertexAttribute{
                    .location = 1, .binding = 0,
                    .offset   = sizeof(glm::vec3),
                    .format   = rhi::TextureFormat::RGBA32F,   // vec4 color
                },
                rhi::VertexAttribute{
                    .location = 2, .binding = 0,
                    .offset   = sizeof(glm::vec3) + sizeof(glm::vec4),
                    .format   = rhi::TextureFormat::RG32F,     // vec2 texcoord
                },
                rhi::VertexAttribute{
                    .location = 3, .binding = 0,
                    .offset   = sizeof(glm::vec3) + sizeof(glm::vec4) + sizeof(glm::vec2),
                    .format   = rhi::TextureFormat::RGB32F,    // vec3 normal
                },
                rhi::VertexAttribute{
                    .location = 4, .binding = 0,
                    .offset   = sizeof(glm::vec3) * 2 + sizeof(glm::vec4) + sizeof(glm::vec2),
                    .format   = rhi::TextureFormat::RGB32F,    // vec3 tangent
                },
                rhi::VertexAttribute{
                    .location = 5, .binding = 0,
                    .offset   = sizeof(glm::vec3) * 3 + sizeof(glm::vec4) + sizeof(glm::vec2),
                    .format   = rhi::TextureFormat::RGB32F,    // vec3 bitangent
                },
            };
            desc.state.cull        = rhi::CullMode::Back;
            desc.state.depth       = rhi::DepthCompare::Less;
            desc.state.depth_test  = true;
            desc.state.depth_write = true;
            desc.state.blend       = rhi::BlendMode::None;
            desc.descriptor_layouts       = { global_layout.get(), mat_layout.get() };
            desc.push_constant_size       = sizeof(PushConstantData);
            desc.push_constant_stages     = rhi::ShaderStage::Vertex;
            desc.debug_name               = "ForwardPass";
            desc.use_dynamic_rendering    = true;
            desc.dynamic_color_formats    = { rhi::TextureFormat::RGBA16F };
            desc.dynamic_depth_format     = rhi::TextureFormat::Depth32F;

            if (cache.get_or_create_graphics_pipeline("forward_pass", desc)) {
                HELIOS_LOG_INFO(ForwardPlus, "Created forward_pass pipeline");
            } else {
                HELIOS_LOG_WARN(ForwardPlus, "Failed to create forward_pass pipeline");
            }
        } else {
            HELIOS_LOG_WARN(ForwardPlus,
                "forward_pass shaders not found -- pipeline skipped");
        }
    }

    // =====================================================================
    // 5. Skybox pipeline (graphics)
    //    Shaders: skybox.vert.spv, skybox.frag.spv
    //    Descriptor set 0:
    //      binding 0: SkyboxUBO
    //      binding 1: samplerCube u_EnvironmentMap
    //    State: depth_write=false, depth_test=true, depth_compare=LessEqual,
    //           cull=None
    //    Vertex layout: position only (vec3)
    // =====================================================================
    {
        auto* vert = cache.load_shader("skybox_vert",
                                       dir / "skybox.vert.spv",
                                       rhi::ShaderStage::Vertex);
        auto* frag = cache.load_shader("skybox_frag",
                                       dir / "skybox.frag.spv",
                                       rhi::ShaderStage::Fragment);
        if (vert && frag) {
            rhi::DescriptorSetLayoutDesc sky_layout_desc;
            sky_layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type    = rhi::DescriptorType::UniformBuffer,
                    .stage   = rhi::ShaderStage::Vertex | rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 1,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Fragment,
                    .count   = 1,
                },
            };
            sky_layout_desc.debug_name = "Skybox_DSL";
            auto sky_layout = device.create_descriptor_set_layout(sky_layout_desc);

            rhi::GraphicsPipelineDesc desc;
            desc.vertex_shader   = vert;
            desc.fragment_shader = frag;
            desc.layout.stride   = sizeof(glm::vec3);
            desc.layout.attributes = {
                rhi::VertexAttribute{
                    .location = 0, .binding = 0, .offset = 0,
                    .format   = rhi::TextureFormat::RGB32F,
                },
            };
            desc.state.cull        = rhi::CullMode::None;
            desc.state.depth       = rhi::DepthCompare::LessEqual;
            desc.state.depth_test  = true;
            desc.state.depth_write = false;
            desc.state.blend       = rhi::BlendMode::None;
            desc.descriptor_layouts       = { sky_layout.get() };
            desc.push_constant_size       = 0;
            desc.debug_name               = "SkyboxPipeline";
            desc.use_dynamic_rendering    = true;
            desc.dynamic_color_formats    = { rhi::TextureFormat::RGBA16F };
            desc.dynamic_depth_format     = rhi::TextureFormat::Depth32F;

            if (cache.get_or_create_graphics_pipeline("skybox", desc)) {
                HELIOS_LOG_INFO(ForwardPlus, "Created skybox pipeline");
            } else {
                HELIOS_LOG_WARN(ForwardPlus, "Failed to create skybox pipeline");
            }
        } else {
            HELIOS_LOG_WARN(ForwardPlus,
                "skybox shaders not found -- pipeline skipped");
        }
    }

    // =====================================================================
    // 6. Tonemap pipeline (compute)
    //    Shader: tonemap.comp.spv
    //    Descriptor set 0:
    //      binding 0: sampler2D u_HDRInput
    //      binding 1: image2D u_LDROutput (rgba8, writeonly)
    //    Push constants: TonemapPushConstants { float exposure; }
    // =====================================================================
    {
        auto* comp = cache.load_shader("tonemap_comp",
                                       dir / "tonemap.comp.spv",
                                       rhi::ShaderStage::Compute);
        if (comp) {
            rhi::DescriptorSetLayoutDesc tm_layout_desc;
            tm_layout_desc.bindings = {
                rhi::DescriptorBinding{
                    .binding = 0,
                    .type    = rhi::DescriptorType::CombinedImageSampler,
                    .stage   = rhi::ShaderStage::Compute,
                    .count   = 1,
                },
                rhi::DescriptorBinding{
                    .binding = 1,
                    .type    = rhi::DescriptorType::StorageImage,
                    .stage   = rhi::ShaderStage::Compute,
                    .count   = 1,
                },
            };
            tm_layout_desc.debug_name = "Tonemap_DSL";
            auto tm_layout = device.create_descriptor_set_layout(tm_layout_desc);

            rhi::ComputePipelineDesc desc;
            desc.compute_shader      = comp;
            desc.descriptor_layouts  = { tm_layout.get() };
            desc.push_constant_size  = sizeof(TonemapPushConstants);
            desc.debug_name          = "TonemapCompute";

            if (cache.get_or_create_compute_pipeline("tonemap_compute", desc)) {
                HELIOS_LOG_INFO(ForwardPlus, "Created tonemap_compute pipeline");
            } else {
                HELIOS_LOG_WARN(ForwardPlus, "Failed to create tonemap_compute pipeline");
            }
        } else {
            HELIOS_LOG_WARN(ForwardPlus,
                "tonemap shader not found -- pipeline skipped");
        }
    }

    HELIOS_LOG_INFO(ForwardPlus, "Forward+ pipeline initialization complete");
}

} // namespace helios
