// VulkanTest -- proof-of-life: render a colored triangle using the Helios RHI
// This standalone test bypasses the engine's Application/Window/Renderer layers
// and drives the Vulkan backend directly through the RHI interfaces.

#include "Core/Base.h"
#include "Core/Log.h"
#include "RHI/RHI.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanSwapchain.h"
#include "Renderer/Vulkan/VulkanBuffer.h"
#include "Renderer/Vulkan/VulkanShader.h"
#include "Renderer/Vulkan/VulkanPipeline.h"
#include "Renderer/Vulkan/VulkanRenderPass.h"
#include "Renderer/Vulkan/VulkanFramebuffer.h"
#include "Renderer/Vulkan/VulkanTexture.h"
#include "Renderer/Vulkan/VulkanCommandBuffer.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>

// ---------------------------------------------------------------------------
// Helper: read a SPIR-V file into a byte vector
// ---------------------------------------------------------------------------
static std::vector<uint8_t> ReadFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        HVE_CORE_ERROR_TAG("VulkanTest", "Failed to open shader file: {}", path);
        return {};
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<uint8_t> buffer(fileSize);
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    return buffer;
}

// ---------------------------------------------------------------------------
// Vertex layout -- interleaved position + color
// ---------------------------------------------------------------------------
struct Vertex {
    float pos[3];
    float color[3];
};

static Vertex s_TriangleVertices[] = {
    {{ 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},  // top    -- red
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},  // right  -- green
    {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},  // left   -- blue
};

// ---------------------------------------------------------------------------
// Helper: create swapchain-image VulkanTexture wrappers and framebuffers
// ---------------------------------------------------------------------------
struct SwapchainResources {
    std::vector<Engine::Ref<Engine::VulkanTexture>> Textures;
    std::vector<Engine::Ref<Engine::RHIFramebuffer>> Framebuffers;
};

static SwapchainResources CreateSwapchainResources(
    Engine::VulkanDevice* device,
    Engine::VulkanSwapchain* swapchain,
    Engine::RHIRenderPass* renderPass)
{
    SwapchainResources res;
    for (uint32_t i = 0; i < swapchain->GetImageCount(); i++) {
        auto tex = Engine::CreateRef<Engine::VulkanTexture>(
            device,
            swapchain->GetImages()[i],
            swapchain->GetVkFormat(),
            swapchain->GetWidth(),
            swapchain->GetHeight(),
            ("SwapchainTex_" + std::to_string(i)).c_str());
        res.Textures.push_back(tex);

        Engine::FramebufferDesc fbDesc{};
        fbDesc.RenderPass  = renderPass;
        fbDesc.Attachments = { tex.get() };
        fbDesc.Width       = swapchain->GetWidth();
        fbDesc.Height      = swapchain->GetHeight();
        fbDesc.DebugName   = "SwapchainFB_" + std::to_string(i);
        res.Framebuffers.push_back(device->CreateFramebuffer(fbDesc));
    }
    return res;
}

// ---------------------------------------------------------------------------
//  main
// ---------------------------------------------------------------------------
int main()
{
    Engine::Log::Init();

    // ---- GLFW ----
    if (!glfwInit()) {
        HVE_CORE_FATAL_TAG("VulkanTest", "Failed to init GLFW");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // NO OpenGL context
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Triangle Test", nullptr, nullptr);
    if (!window) {
        HVE_CORE_FATAL_TAG("VulkanTest", "Failed to create GLFW window");
        glfwTerminate();
        return 1;
    }

    // ---- Vulkan context + device + swapchain ----
    Engine::VulkanContext::Init("VulkanTest", true);
    VkSurfaceKHR surface = Engine::VulkanContext::CreateSurface(window);

    auto device    = Engine::CreateRef<Engine::VulkanDevice>(surface);
    auto swapchain = Engine::CreateRef<Engine::VulkanSwapchain>(device.get(), surface, 800, 600);

    // ---- Vertex buffer ----
    Engine::BufferDesc vbDesc{};
    vbDesc.Size      = sizeof(s_TriangleVertices);
    vbDesc.Usage     = Engine::BufferUsage::Vertex;
    vbDesc.Access    = Engine::MemoryAccess::CPU_to_GPU;
    vbDesc.DebugName = "TriangleVBO";
    auto vbo = device->CreateBuffer(vbDesc, s_TriangleVertices);

    // ---- Shaders ----
    auto vertSpv = ReadFile("Resources/triangle.vert.spv");
    auto fragSpv = ReadFile("Resources/triangle.frag.spv");
    if (vertSpv.empty() || fragSpv.empty()) {
        HVE_CORE_FATAL_TAG("VulkanTest", "Failed to load shaders -- check working directory");
        return 1;
    }

    Engine::ShaderDesc vertDesc{};
    vertDesc.Stage     = Engine::ShaderStage::Vertex;
    vertDesc.SpirVCode = vertSpv;
    vertDesc.DebugName = "TriangleVert";
    auto vertShader = device->CreateShader(vertDesc);

    Engine::ShaderDesc fragDesc{};
    fragDesc.Stage     = Engine::ShaderStage::Fragment;
    fragDesc.SpirVCode = fragSpv;
    fragDesc.DebugName = "TriangleFrag";
    auto fragShader = device->CreateShader(fragDesc);

    // ---- Render pass ----
    // The swapchain format (e.g. B8G8R8A8_SRGB) may not map cleanly to the
    // engine's ImageFormat enum. We create the render pass using the engine's
    // GetFormat() (which returns RGBA8), and the render pass object creates a
    // VkRenderPass with VK_FORMAT_R8G8B8A8_UNORM.
    //
    // For correct format matching we create a custom VkRenderPass directly
    // using the actual swapchain VkFormat.
    VkFormat swapchainVkFormat = swapchain->GetVkFormat();

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = swapchainVkFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments    = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments    = &colorAttachment;
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    VkRenderPass vkRenderPass = VK_NULL_HANDLE;
    VkResult vkResult = vkCreateRenderPass(device->GetDevice(), &rpInfo, nullptr, &vkRenderPass);
    if (vkResult != VK_SUCCESS) {
        HVE_CORE_FATAL_TAG("VulkanTest", "Failed to create VkRenderPass");
        return 1;
    }
    Engine::VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_RENDER_PASS,
        reinterpret_cast<uint64_t>(vkRenderPass), "TriangleRenderPass");

    // ---- Pipeline ----
    // We create the pipeline layout and graphics pipeline manually so we can
    // pass the custom VkRenderPass (with the correct swapchain format).

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    vkCreatePipelineLayout(device->GetDevice(), &layoutInfo, nullptr, &pipelineLayout);

    // Shader stages
    auto* vkVert = static_cast<Engine::VulkanShader*>(vertShader.get());
    auto* vkFrag = static_cast<Engine::VulkanShader*>(fragShader.get());

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vkVert->GetModule();
    shaderStages[0].pName  = "main";
    shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = vkFrag->GetModule();
    shaderStages[1].pName  = "main";

    // Vertex input
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding   = 0;
    bindingDesc.stride    = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribDescs[2]{};
    attribDescs[0].location = 0;
    attribDescs[0].binding  = 0;
    attribDescs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attribDescs[0].offset   = offsetof(Vertex, pos);
    attribDescs[1].location = 1;
    attribDescs[1].binding  = 0;
    attribDescs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attribDescs[1].offset   = offsetof(Vertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount   = 1;
    vertexInput.pVertexBindingDescriptions      = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions    = attribDescs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth   = 1.0f;
    rasterizer.cullMode    = VK_CULL_MODE_NONE;
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                   | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &blendAttachment;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = pipelineLayout;
    pipelineInfo.renderPass          = vkRenderPass;
    pipelineInfo.subpass             = 0;

    VkPipeline vkPipeline = VK_NULL_HANDLE;
    vkResult = vkCreateGraphicsPipelines(device->GetDevice(), VK_NULL_HANDLE, 1,
                                          &pipelineInfo, nullptr, &vkPipeline);
    if (vkResult != VK_SUCCESS) {
        HVE_CORE_FATAL_TAG("VulkanTest", "Failed to create graphics pipeline (VkResult={})",
                           static_cast<int>(vkResult));
        return 1;
    }
    Engine::VulkanContext::SetDebugName(device->GetDevice(), VK_OBJECT_TYPE_PIPELINE,
        reinterpret_cast<uint64_t>(vkPipeline), "TrianglePipeline");

    // ---- Framebuffers (one per swapchain image) ----
    auto createFramebuffers = [&]() {
        std::vector<VkFramebuffer> fbs;
        for (uint32_t i = 0; i < swapchain->GetImageCount(); i++) {
            VkImageView attachments[] = { swapchain->GetImageViews()[i] };

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass      = vkRenderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments    = attachments;
            fbInfo.width           = swapchain->GetWidth();
            fbInfo.height          = swapchain->GetHeight();
            fbInfo.layers          = 1;

            VkFramebuffer fb = VK_NULL_HANDLE;
            vkCreateFramebuffer(device->GetDevice(), &fbInfo, nullptr, &fb);
            fbs.push_back(fb);
        }
        return fbs;
    };

    auto destroyFramebuffers = [&](std::vector<VkFramebuffer>& fbs) {
        for (auto fb : fbs) {
            if (fb != VK_NULL_HANDLE)
                vkDestroyFramebuffer(device->GetDevice(), fb, nullptr);
        }
        fbs.clear();
    };

    std::vector<VkFramebuffer> framebuffers = createFramebuffers();

    // ---- Command buffers (one per frame in flight) ----
    std::vector<Engine::Ref<Engine::RHICommandBuffer>> cmdBuffers;
    for (uint32_t i = 0; i < Engine::VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; i++) {
        cmdBuffers.push_back(device->CreateCommandBuffer());
    }

    HVE_CORE_INFO_TAG("VulkanTest", "Initialization complete. Entering render loop.");

    // ---- Render loop ----
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Handle minimized window
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        if (w == 0 || h == 0) continue;

        // Acquire next image
        if (!swapchain->AcquireNextImage()) {
            // Swapchain out of date -- resize
            swapchain->Resize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            destroyFramebuffers(framebuffers);
            framebuffers = createFramebuffers();
            continue;
        }

        uint32_t imageIndex = swapchain->GetCurrentImageIndex();
        uint32_t frameIndex = swapchain->GetCurrentFrame();
        auto* cmd   = cmdBuffers[frameIndex].get();
        auto* vkCmd = static_cast<Engine::VulkanCommandBuffer*>(cmd);
        VkCommandBuffer cmdBuf = vkCmd->GetVkCommandBuffer();

        // Begin recording
        cmd->Begin();

        // Transition swapchain image: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
        Engine::VulkanTexture::TransitionLayout(
            cmdBuf,
            swapchain->GetCurrentVkImage(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        // Begin render pass (traditional, matching the pipeline's VkRenderPass)
        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass        = vkRenderPass;
        rpBegin.framebuffer       = framebuffers[imageIndex];
        rpBegin.renderArea.offset = {0, 0};
        rpBegin.renderArea.extent = {swapchain->GetWidth(), swapchain->GetHeight()};

        VkClearValue clearValue{};
        clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues    = &clearValue;

        vkCmdBeginRenderPass(cmdBuf, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        // Bind pipeline
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline);

        // Set viewport (Y-flip for Vulkan)
        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = static_cast<float>(h);
        viewport.width    = static_cast<float>(w);
        viewport.height   = -static_cast<float>(h);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
        vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

        // Bind vertex buffer (using RHI layer for this)
        auto* vkVbo = static_cast<Engine::VulkanBuffer*>(vbo.get());
        VkBuffer vertexBuffers[] = { vkVbo->GetVkBuffer() };
        VkDeviceSize offsets[]   = { 0 };
        vkCmdBindVertexBuffers(cmdBuf, 0, 1, vertexBuffers, offsets);

        // Draw the triangle
        vkCmdDraw(cmdBuf, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmdBuf);

        // Transition swapchain image: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
        Engine::VulkanTexture::TransitionLayout(
            cmdBuf,
            swapchain->GetCurrentVkImage(),
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        cmd->End();

        // Submit
        VkSemaphore waitSem       = swapchain->GetImageAvailableSemaphore();
        VkSemaphore signalSem     = swapchain->GetRenderFinishedSemaphore();
        VkFence     fence         = swapchain->GetInFlightFence();
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo{};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &waitSem;
        submitInfo.pWaitDstStageMask    = &waitStage;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &cmdBuf;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &signalSem;

        vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, fence);

        swapchain->Present();
    }

    // ---- Cleanup ----
    device->WaitIdle();

    cmdBuffers.clear();
    destroyFramebuffers(framebuffers);
    vbo.reset();
    vertShader.reset();
    fragShader.reset();

    vkDestroyPipeline(device->GetDevice(), vkPipeline, nullptr);
    vkDestroyPipelineLayout(device->GetDevice(), pipelineLayout, nullptr);
    vkDestroyRenderPass(device->GetDevice(), vkRenderPass, nullptr);

    swapchain.reset();
    device.reset();

    Engine::VulkanContext::DestroySurface(surface);
    Engine::VulkanContext::Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();

    HVE_CORE_INFO_TAG("VulkanTest", "Clean shutdown.");
    return 0;
}
