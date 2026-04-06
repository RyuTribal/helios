#include "pch.h"
#include "Renderer/Vulkan/VulkanPipeline.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanShader.h"
#include "Renderer/Vulkan/VulkanRenderPass.h"
#include "Renderer/Vulkan/VulkanUtils.h"

#include "Renderer/Vulkan/VulkanDescriptor.h"

namespace Engine {

    // --- Helper: map CullMode to VkCullModeFlags ---
    static VkCullModeFlags ToVkCullMode(CullMode mode)
    {
        switch (mode) {
            case CullMode::None:  return VK_CULL_MODE_NONE;
            case CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
            case CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
        }
        return VK_CULL_MODE_NONE;
    }

    // --- Helper: map DepthCompare to VkCompareOp ---
    static VkCompareOp ToVkCompareOp(DepthCompare compare)
    {
        switch (compare) {
            case DepthCompare::Less:         return VK_COMPARE_OP_LESS;
            case DepthCompare::LessEqual:    return VK_COMPARE_OP_LESS_OR_EQUAL;
            case DepthCompare::Greater:      return VK_COMPARE_OP_GREATER;
            case DepthCompare::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
            case DepthCompare::Never:        return VK_COMPARE_OP_NEVER;
            case DepthCompare::Always:       return VK_COMPARE_OP_ALWAYS;
        }
        return VK_COMPARE_OP_LESS;
    }

    // --- Helper: build color blend attachment from BlendMode ---
    static VkPipelineColorBlendAttachmentState ToVkBlendAttachment(BlendMode mode)
    {
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                       | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        switch (mode) {
            case BlendMode::None:
                blendAttachment.blendEnable = VK_FALSE;
                break;
            case BlendMode::Alpha:
                blendAttachment.blendEnable         = VK_TRUE;
                blendAttachment.srcColorBlendFactor  = VK_BLEND_FACTOR_SRC_ALPHA;
                blendAttachment.dstColorBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blendAttachment.colorBlendOp         = VK_BLEND_OP_ADD;
                blendAttachment.srcAlphaBlendFactor  = VK_BLEND_FACTOR_ONE;
                blendAttachment.dstAlphaBlendFactor  = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                blendAttachment.alphaBlendOp         = VK_BLEND_OP_ADD;
                break;
            case BlendMode::Additive:
                blendAttachment.blendEnable         = VK_TRUE;
                blendAttachment.srcColorBlendFactor  = VK_BLEND_FACTOR_ONE;
                blendAttachment.dstColorBlendFactor  = VK_BLEND_FACTOR_ONE;
                blendAttachment.colorBlendOp         = VK_BLEND_OP_ADD;
                blendAttachment.srcAlphaBlendFactor  = VK_BLEND_FACTOR_ONE;
                blendAttachment.dstAlphaBlendFactor  = VK_BLEND_FACTOR_ONE;
                blendAttachment.alphaBlendOp         = VK_BLEND_OP_ADD;
                break;
        }

        return blendAttachment;
    }

    // =========================================================================
    //  Graphics pipeline constructor
    // =========================================================================
    VulkanPipeline::VulkanPipeline(VulkanDevice* device, const GraphicsPipelineDesc& desc)
        : m_Device(device), m_BindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
        // -- 1. Shader stages --
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        auto addStage = [&](RHIShader* shader) {
            if (!shader) return;
            auto* vkShader = static_cast<VulkanShader*>(shader);
            VkPipelineShaderStageCreateInfo stageInfo{};
            stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stageInfo.stage  = vkShader->GetVkStage();
            stageInfo.module = vkShader->GetModule();
            stageInfo.pName  = vkShader->GetEntryPoint();
            shaderStages.push_back(stageInfo);
        };

        addStage(desc.VertexShader);
        addStage(desc.FragmentShader);
        addStage(desc.GeometryShader);

        // -- 2. Vertex input --
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding   = 0;
        bindingDesc.stride    = desc.Layout.Stride;
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescs;
        for (const auto& attr : desc.Layout.Attributes) {
            VkVertexInputAttributeDescription vkAttr{};
            vkAttr.location = attr.Location;
            vkAttr.binding  = attr.Binding;
            vkAttr.format   = ToVkFormat(attr.Format);
            vkAttr.offset   = attr.Offset;
            attributeDescs.push_back(vkAttr);
        }

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        if (desc.Layout.Stride > 0) {
            vertexInput.vertexBindingDescriptionCount   = 1;
            vertexInput.pVertexBindingDescriptions      = &bindingDesc;
        }
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescs.size());
        vertexInput.pVertexAttributeDescriptions    = attributeDescs.data();

        // -- 3. Input assembly --
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // -- 4. Viewport & scissor: dynamic state --
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        // -- 5. Rasterization --
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth               = 1.0f;
        rasterizer.cullMode                = ToVkCullMode(desc.State.Cull);
        rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable         = VK_FALSE;

        // -- 6. Multisampling --
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // -- 7. Depth stencil --
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable       = desc.State.DepthTest ? VK_TRUE : VK_FALSE;
        depthStencil.depthWriteEnable      = desc.State.DepthWrite ? VK_TRUE : VK_FALSE;
        depthStencil.depthCompareOp        = ToVkCompareOp(desc.State.Depth);
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable     = VK_FALSE;

        // -- 8. Color blending --
        // One blend attachment per color attachment in the render pass
        uint32_t colorAttachmentCount = 1; // default
        if (desc.RenderPass) {
            auto* rp = static_cast<VulkanRenderPass*>(desc.RenderPass);
            colorAttachmentCount = static_cast<uint32_t>(rp->GetDesc().ColorAttachments.size());
            if (colorAttachmentCount == 0) colorAttachmentCount = 1;
        }

        VkPipelineColorBlendAttachmentState blendTemplate = ToVkBlendAttachment(desc.State.Blend);
        std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(colorAttachmentCount, blendTemplate);

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable   = VK_FALSE;
        colorBlending.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
        colorBlending.pAttachments    = blendAttachments.data();

        // -- 9. Pipeline layout --
        std::vector<VkDescriptorSetLayout> setLayouts;
        for (auto* layout : desc.DescriptorLayouts) {
            if (layout) {
                auto* vkLayout = static_cast<VulkanDescriptorSetLayout*>(layout);
                setLayouts.push_back(vkLayout->GetVkLayout());
            }
        }

        VkPushConstantRange pushConstantRange{};
        bool hasPushConstants = desc.PushConstantSize > 0;
        if (hasPushConstants) {
            pushConstantRange.stageFlags = ToVkShaderStageFlags(desc.PushConstantStages);
            pushConstantRange.offset     = 0;
            pushConstantRange.size       = desc.PushConstantSize;
        }

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
        layoutInfo.pSetLayouts            = setLayouts.data();
        layoutInfo.pushConstantRangeCount = hasPushConstants ? 1 : 0;
        layoutInfo.pPushConstantRanges    = hasPushConstants ? &pushConstantRange : nullptr;

        VkResult result = vkCreatePipelineLayout(device->GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create pipeline layout for '{}'", desc.DebugName);
            return;
        }

        // -- 10. Create graphics pipeline --
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages             = shaderStages.data();
        pipelineInfo.pVertexInputState   = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = m_PipelineLayout;

        if (desc.RenderPass) {
            auto* rp = static_cast<VulkanRenderPass*>(desc.RenderPass);
            pipelineInfo.renderPass = rp->GetVkRenderPass();
        }
        pipelineInfo.subpass = 0;

        result = vkCreateGraphicsPipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create graphics pipeline '{}'", desc.DebugName);
            return;
        }

        // -- 11. Debug names --
        if (!desc.DebugName.empty()) {
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_PIPELINE,
                                       reinterpret_cast<uint64_t>(m_Pipeline), desc.DebugName.c_str());
            std::string layoutName = desc.DebugName + "_Layout";
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                       reinterpret_cast<uint64_t>(m_PipelineLayout), layoutName.c_str());
        }

        HVE_CORE_INFO_TAG("Vulkan", "Created graphics pipeline '{}'", desc.DebugName);
    }

    // =========================================================================
    //  Compute pipeline constructor
    // =========================================================================
    VulkanPipeline::VulkanPipeline(VulkanDevice* device, const ComputePipelineDesc& desc)
        : m_Device(device), m_BindPoint(VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        // -- 1. Shader stage --
        if (!desc.ComputeShader) {
            HVE_CORE_ERROR_TAG("Vulkan", "Compute pipeline '{}': no compute shader provided", desc.DebugName);
            return;
        }

        auto* vkShader = static_cast<VulkanShader*>(desc.ComputeShader);
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = vkShader->GetModule();
        stageInfo.pName  = vkShader->GetEntryPoint();

        // -- 2. Pipeline layout --
        std::vector<VkDescriptorSetLayout> setLayouts;
        for (auto* layout : desc.DescriptorLayouts) {
            if (layout) {
                auto* vkLayout = static_cast<VulkanDescriptorSetLayout*>(layout);
                setLayouts.push_back(vkLayout->GetVkLayout());
            }
        }

        VkPushConstantRange pushConstantRange{};
        bool hasPushConstants = desc.PushConstantSize > 0;
        if (hasPushConstants) {
            pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            pushConstantRange.offset     = 0;
            pushConstantRange.size       = desc.PushConstantSize;
        }

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
        layoutInfo.pSetLayouts            = setLayouts.data();
        layoutInfo.pushConstantRangeCount = hasPushConstants ? 1 : 0;
        layoutInfo.pPushConstantRanges    = hasPushConstants ? &pushConstantRange : nullptr;

        VkResult result = vkCreatePipelineLayout(device->GetDevice(), &layoutInfo, nullptr, &m_PipelineLayout);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create pipeline layout for compute pipeline '{}'", desc.DebugName);
            return;
        }

        // -- 3. Create compute pipeline --
        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage  = stageInfo;
        pipelineInfo.layout = m_PipelineLayout;

        result = vkCreateComputePipelines(device->GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
        if (result != VK_SUCCESS) {
            HVE_CORE_ERROR_TAG("Vulkan", "Failed to create compute pipeline '{}'", desc.DebugName);
            return;
        }

        // -- 4. Debug names --
        if (!desc.DebugName.empty()) {
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_PIPELINE,
                                       reinterpret_cast<uint64_t>(m_Pipeline), desc.DebugName.c_str());
            std::string layoutName = desc.DebugName + "_Layout";
            VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                                       reinterpret_cast<uint64_t>(m_PipelineLayout), layoutName.c_str());
        }

        HVE_CORE_INFO_TAG("Vulkan", "Created compute pipeline '{}'", desc.DebugName);
    }

    // =========================================================================
    //  Destructor
    // =========================================================================
    VulkanPipeline::~VulkanPipeline()
    {
        if (m_Pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(m_Device->GetDevice(), m_Pipeline, nullptr);
            m_Pipeline = VK_NULL_HANDLE;
        }
        if (m_PipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(m_Device->GetDevice(), m_PipelineLayout, nullptr);
            m_PipelineLayout = VK_NULL_HANDLE;
        }
    }

} // namespace Engine
