#include "helios/vulkan/vulkan_pipeline.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_shader.h"
#include "helios/vulkan/vulkan_render_pass.h"
#include "helios/vulkan/vulkan_descriptor.h"
#include "helios/vulkan/vulkan_utils.h"
#include "helios/vulkan/renderer_log_channels.h"

#include <helios/core/assert.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace helios::rhi::vulkan {

// --- Helper: build color blend attachment from BlendMode ---
static VkPipelineColorBlendAttachmentState to_vk_blend_attachment(BlendMode mode)
{
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                   | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    switch (mode) {
        case BlendMode::None:
            blend_attachment.blendEnable = VK_FALSE;
            break;
        case BlendMode::Alpha:
            blend_attachment.blendEnable         = VK_TRUE;
            blend_attachment.srcColorBlendFactor  = VK_BLEND_FACTOR_SRC_ALPHA;
            blend_attachment.dstColorBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend_attachment.colorBlendOp         = VK_BLEND_OP_ADD;
            blend_attachment.srcAlphaBlendFactor  = VK_BLEND_FACTOR_ONE;
            blend_attachment.dstAlphaBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            blend_attachment.alphaBlendOp         = VK_BLEND_OP_ADD;
            break;
        case BlendMode::Additive:
            blend_attachment.blendEnable         = VK_TRUE;
            blend_attachment.srcColorBlendFactor  = VK_BLEND_FACTOR_ONE;
            blend_attachment.dstColorBlendFactor  = VK_BLEND_FACTOR_ONE;
            blend_attachment.colorBlendOp         = VK_BLEND_OP_ADD;
            blend_attachment.srcAlphaBlendFactor  = VK_BLEND_FACTOR_ONE;
            blend_attachment.dstAlphaBlendFactor  = VK_BLEND_FACTOR_ONE;
            blend_attachment.alphaBlendOp         = VK_BLEND_OP_ADD;
            break;
    }

    return blend_attachment;
}

// =========================================================================
//  Graphics pipeline constructor
// =========================================================================

VulkanPipeline::VulkanPipeline(VulkanDevice& device, const GraphicsPipelineDesc& desc)
    : m_device(&device)
    , m_bind_point(VK_PIPELINE_BIND_POINT_GRAPHICS)
{
    HELIOS_ASSERT(device, "VulkanDevice must be initialized");

    // -- 1. Shader stages --
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

    auto add_stage = [&](const Shader* shader_ptr) {
        if (!shader_ptr) return;
        const auto* shader = static_cast<const VulkanShader*>(shader_ptr);
        VkPipelineShaderStageCreateInfo stage_info{};
        stage_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage_info.stage  = shader->vk_stage();
        stage_info.module = shader->module();
        stage_info.pName  = shader->entry_point();
        shader_stages.push_back(stage_info);
    };

    add_stage(desc.vertex_shader);
    add_stage(desc.fragment_shader);
    add_stage(desc.geometry_shader);

    // -- 2. Vertex input --
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding   = 0;
    binding_desc.stride    = desc.layout.stride;
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attribute_descs;
    for (const auto& attr : desc.layout.attributes) {
        VkVertexInputAttributeDescription vk_attr{};
        vk_attr.location = attr.location;
        vk_attr.binding  = attr.binding;
        vk_attr.format   = to_vk_format(attr.format);
        vk_attr.offset   = attr.offset;
        attribute_descs.push_back(vk_attr);
    }

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (desc.layout.stride > 0) {
        vertex_input.vertexBindingDescriptionCount   = 1;
        vertex_input.pVertexBindingDescriptions      = &binding_desc;
    }
    vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descs.size());
    vertex_input.pVertexAttributeDescriptions    = attribute_descs.data();

    // -- 3. Input assembly --
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // -- 4. Viewport & scissor: dynamic state --
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount  = 1;

    std::array<VkDynamicState, 2> dynamic_states = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_states.size());
    dynamic_state.pDynamicStates    = dynamic_states.data();

    // -- 5. Rasterization --
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = to_vk_cull_mode(desc.state.cull);
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE; // Y-flip in projection reverses winding
    rasterizer.depthBiasEnable         = VK_FALSE;

    // -- 6. Multisampling --
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // -- 7. Depth stencil --
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable       = desc.state.depth_test ? VK_TRUE : VK_FALSE;
    depth_stencil.depthWriteEnable      = desc.state.depth_write ? VK_TRUE : VK_FALSE;
    depth_stencil.depthCompareOp        = to_vk_compare_op(desc.state.depth);
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable     = VK_FALSE;

    // -- 8. Color blending --
    // One blend attachment per color attachment in the render pass
    uint32_t color_attachment_count = 1; // default
    if (desc.render_pass) {
        const auto* rp = static_cast<const VulkanRenderPass*>(desc.render_pass);
        color_attachment_count = static_cast<uint32_t>(rp->desc().color_attachments.size());
        if (color_attachment_count == 0) color_attachment_count = 1;
    }

    VkPipelineColorBlendAttachmentState blend_template = to_vk_blend_attachment(desc.state.blend);
    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(color_attachment_count, blend_template);

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable   = VK_FALSE;
    color_blending.attachmentCount = static_cast<uint32_t>(blend_attachments.size());
    color_blending.pAttachments    = blend_attachments.data();

    // -- 9. Pipeline layout --
    std::vector<VkDescriptorSetLayout> set_layouts;
    for (const auto* layout_ptr : desc.descriptor_layouts) {
        if (layout_ptr) {
            const auto* vk_layout = static_cast<const VulkanDescriptorSetLayout*>(layout_ptr);
            set_layouts.push_back(vk_layout->vk_layout());
        }
    }

    VkPushConstantRange push_constant_range{};
    bool has_push_constants = desc.push_constant_size > 0;
    if (has_push_constants) {
        push_constant_range.stageFlags = to_vk_shader_stage_flags(desc.push_constant_stages);
        push_constant_range.offset     = 0;
        push_constant_range.size       = desc.push_constant_size;
    }

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount         = static_cast<uint32_t>(set_layouts.size());
    layout_info.pSetLayouts            = set_layouts.data();
    layout_info.pushConstantRangeCount = has_push_constants ? 1 : 0;
    layout_info.pPushConstantRanges    = has_push_constants ? &push_constant_range : nullptr;

    VkResult result = vkCreatePipelineLayout(device.device(), &layout_info, nullptr, &m_pipeline_layout);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create pipeline layout for '{}'", desc.debug_name);
        return;
    }

    // -- 10. Create graphics pipeline --
    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount          = static_cast<uint32_t>(shader_stages.size());
    pipeline_info.pStages             = shader_stages.data();
    pipeline_info.pVertexInputState   = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState      = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState   = &multisampling;
    pipeline_info.pDepthStencilState  = &depth_stencil;
    pipeline_info.pColorBlendState    = &color_blending;
    pipeline_info.pDynamicState       = &dynamic_state;
    pipeline_info.layout              = m_pipeline_layout;

    // Dynamic rendering (Vulkan 1.3): when use_dynamic_rendering is set, the
    // pipeline is created with VkPipelineRenderingCreateInfo instead of a
    // traditional VkRenderPass.
    VkPipelineRenderingCreateInfo rendering_create_info{};
    std::vector<VkFormat> vk_color_formats;

    if (desc.use_dynamic_rendering) {
        for (auto fmt : desc.dynamic_color_formats) {
            vk_color_formats.push_back(to_vk_format(fmt));
        }
        rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering_create_info.colorAttachmentCount = static_cast<uint32_t>(vk_color_formats.size());
        rendering_create_info.pColorAttachmentFormats = vk_color_formats.data();
        if (desc.state.depth_test || desc.state.depth_write) {
            rendering_create_info.depthAttachmentFormat = to_vk_format(desc.dynamic_depth_format);
        }
        pipeline_info.pNext = &rendering_create_info;
        pipeline_info.renderPass = VK_NULL_HANDLE;
    } else if (desc.render_pass) {
        const auto* rp = static_cast<const VulkanRenderPass*>(desc.render_pass);
        pipeline_info.renderPass = rp->vk_render_pass();
    }
    pipeline_info.subpass = 0;

    result = vkCreateGraphicsPipelines(device.device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create graphics pipeline '{}'", desc.debug_name);
        return;
    }

    // -- 11. Debug names --
    if (!desc.debug_name.empty()) {
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_PIPELINE,
                                        reinterpret_cast<uint64_t>(m_pipeline),
                                        desc.debug_name.c_str());
        std::string layout_name = desc.debug_name + "_Layout";
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                        reinterpret_cast<uint64_t>(m_pipeline_layout),
                                        layout_name.c_str());
    }

    HELIOS_LOG_INFO(Renderer, "Created graphics pipeline '{}'", desc.debug_name);
}

// =========================================================================
//  Compute pipeline constructor
// =========================================================================

VulkanPipeline::VulkanPipeline(VulkanDevice& device, const ComputePipelineDesc& desc)
    : m_device(&device)
    , m_bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
{
    HELIOS_ASSERT(device, "VulkanDevice must be initialized");

    // -- 1. Shader stage --
    if (!desc.compute_shader) {
        HELIOS_LOG_ERROR(Renderer, "Compute pipeline '{}': no compute shader provided", desc.debug_name);
        return;
    }

    const auto* shader = static_cast<const VulkanShader*>(desc.compute_shader);
    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = shader->module();
    stage_info.pName  = shader->entry_point();

    // -- 2. Pipeline layout --
    std::vector<VkDescriptorSetLayout> set_layouts;
    for (const auto* layout_ptr : desc.descriptor_layouts) {
        if (layout_ptr) {
            const auto* vk_layout = static_cast<const VulkanDescriptorSetLayout*>(layout_ptr);
            set_layouts.push_back(vk_layout->vk_layout());
        }
    }

    VkPushConstantRange push_constant_range{};
    bool has_push_constants = desc.push_constant_size > 0;
    if (has_push_constants) {
        push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        push_constant_range.offset     = 0;
        push_constant_range.size       = desc.push_constant_size;
    }

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount         = static_cast<uint32_t>(set_layouts.size());
    layout_info.pSetLayouts            = set_layouts.data();
    layout_info.pushConstantRangeCount = has_push_constants ? 1 : 0;
    layout_info.pPushConstantRanges    = has_push_constants ? &push_constant_range : nullptr;

    VkResult result = vkCreatePipelineLayout(device.device(), &layout_info, nullptr, &m_pipeline_layout);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create pipeline layout for compute pipeline '{}'", desc.debug_name);
        return;
    }

    // -- 3. Create compute pipeline --
    VkComputePipelineCreateInfo pipeline_info{};
    pipeline_info.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage  = stage_info;
    pipeline_info.layout = m_pipeline_layout;

    result = vkCreateComputePipelines(device.device(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_pipeline);
    if (result != VK_SUCCESS) {
        HELIOS_LOG_ERROR(Renderer, "Failed to create compute pipeline '{}'", desc.debug_name);
        return;
    }

    // -- 4. Debug names --
    if (!desc.debug_name.empty()) {
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_PIPELINE,
                                        reinterpret_cast<uint64_t>(m_pipeline),
                                        desc.debug_name.c_str());
        std::string layout_name = desc.debug_name + "_Layout";
        device.context().set_debug_name(device.device(), VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                        reinterpret_cast<uint64_t>(m_pipeline_layout),
                                        layout_name.c_str());
    }

    HELIOS_LOG_INFO(Renderer, "Created compute pipeline '{}'", desc.debug_name);
}

// =========================================================================
//  Destructor
// =========================================================================

VulkanPipeline::~VulkanPipeline()
{
    destroy();
}

// =========================================================================
//  Move semantics
// =========================================================================

VulkanPipeline::VulkanPipeline(VulkanPipeline&& other) noexcept
    : m_pipeline(std::exchange(other.m_pipeline, VK_NULL_HANDLE))
    , m_pipeline_layout(std::exchange(other.m_pipeline_layout, VK_NULL_HANDLE))
    , m_device(std::exchange(other.m_device, nullptr))
    , m_bind_point(std::exchange(other.m_bind_point, VK_PIPELINE_BIND_POINT_GRAPHICS))
{
}

VulkanPipeline& VulkanPipeline::operator=(VulkanPipeline&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_pipeline        = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
        m_pipeline_layout = std::exchange(other.m_pipeline_layout, VK_NULL_HANDLE);
        m_device          = std::exchange(other.m_device, nullptr);
        m_bind_point      = std::exchange(other.m_bind_point, VK_PIPELINE_BIND_POINT_GRAPHICS);
    }
    return *this;
}

// =========================================================================
//  Private
// =========================================================================

void VulkanPipeline::destroy()
{
    if (m_device == nullptr) return;

    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(m_device->device(), m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device->device(), m_pipeline_layout, nullptr);
        m_pipeline_layout = VK_NULL_HANDLE;
    }
}

} // namespace helios::rhi::vulkan
