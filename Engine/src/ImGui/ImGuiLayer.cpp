#include "pch.h"
#include "ImGui/ImGuiLayer.h"

#include "Core/Application.h"
#include "Renderer/Renderer.h"
#include "Renderer/Vulkan/VulkanContext.h"
#include "Renderer/Vulkan/VulkanDevice.h"
#include "Renderer/Vulkan/VulkanSwapchain.h"

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "ImGui/ImGuizmo.h"

static VkRenderPass s_GlobalImGuiRenderPass = VK_NULL_HANDLE;
VkRenderPass GetImGuiRenderPass() { return s_GlobalImGuiRenderPass; }

namespace Engine
{

	ImGuiLayer::ImGuiLayer()
	{
	}

	void ImGuiLayer::OnAttach()
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		// Disable viewports for Vulkan (multi-viewport with Vulkan is complex, can be re-added later)

		std::string boldFontPath = "Resources/Fonts/opensans/static/OpenSans-Bold.ttf";
		std::string regularFontPath = "Resources/Fonts/opensans/static/OpenSans-Regular.ttf";

		if (std::filesystem::exists(boldFontPath))
		{
			io.Fonts->AddFontFromFileTTF(boldFontPath.c_str(), 18.0f);
		}
		if (std::filesystem::exists(regularFontPath))
		{
			io.FontDefault = io.Fonts->AddFontFromFileTTF(regularFontPath.c_str(), 18.0f);
		}

		ImGui::StyleColorsDark();
		SetDarkThemeColors();

		Application& app = Application::Get();
		GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

		// Init GLFW backend for Vulkan (not OpenGL)
		ImGui_ImplGlfw_InitForVulkan(window, true);

		// Init Vulkan backend
		auto* device = static_cast<VulkanDevice*>(Renderer::GetDevice());
		auto* swapchain = static_cast<VulkanSwapchain*>(Renderer::GetSwapchain());

		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.Instance = VulkanContext::GetInstance();
		initInfo.PhysicalDevice = device->GetPhysicalDevice();
		initInfo.Device = device->GetDevice();
		initInfo.QueueFamily = device->GetGraphicsQueueFamily();
		initInfo.Queue = device->GetGraphicsQueue();
		initInfo.DescriptorPool = device->GetDescriptorPool();
		initInfo.MinImageCount = 2;
		initInfo.ImageCount = swapchain->GetImageCount();
		initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

		// Use a compatible render pass for ImGui's pipeline creation
		// Dynamic rendering caused GPU hangs — use legacy render pass for ImGui
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapchain->GetVkFormat();
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorRef{};
		colorRef.attachment = 0;
		colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef;

		VkSubpassDependency dep{};
		dep.srcSubpass = VK_SUBPASS_EXTERNAL;
		dep.dstSubpass = 0;
		dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dep.srcAccessMask = 0;
		dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo rpInfo{};
		rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rpInfo.attachmentCount = 1;
		rpInfo.pAttachments = &colorAttachment;
		rpInfo.subpassCount = 1;
		rpInfo.pSubpasses = &subpass;
		rpInfo.dependencyCount = 1;
		rpInfo.pDependencies = &dep;

		vkCreateRenderPass(device->GetDevice(), &rpInfo, nullptr, &s_GlobalImGuiRenderPass);

		initInfo.RenderPass = s_GlobalImGuiRenderPass;

		HVE_CORE_TRACE_TAG("ImGui", "Created ImGui render pass: format={}", (int)swapchain->GetVkFormat());

		ImGui_ImplVulkan_Init(&initInfo);

		// Upload fonts and wait for GPU to finish the upload
		ImGui_ImplVulkan_CreateFontsTexture();
		device->WaitIdle();

		HVE_CORE_TRACE_TAG("ImGui", "ImGui Vulkan backend initialized");
	}

	void ImGuiLayer::OnDetach()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (m_BlockEvents)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
		}
	}

	void ImGuiLayer::Begin()
	{
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::End()
	{
		ImGuiIO& io = ImGui::GetIO();
		Application& app = Application::Get();

		io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

		ImGui::Render();
		// ImGui draw data will be recorded into the Vulkan command buffer by the Renderer
		// (via ImGui_ImplVulkan_RenderDrawData in Renderer::BeginDrawing after the tonemap pass)
	}

	void ImGuiLayer::SetDarkThemeColors()
	{
		auto& colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_WindowBg] = ImVec4{ 0.1f, 0.105f, 0.11f, 1.0f };

		// Headers
		colors[ImGuiCol_Header] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_HeaderActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Buttons
		colors[ImGuiCol_Button] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_ButtonActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Frame BG
		colors[ImGuiCol_FrameBg] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };
		colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.3f, 0.305f, 0.31f, 1.0f };
		colors[ImGuiCol_FrameBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };

		// Tabs
		colors[ImGuiCol_Tab] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabHovered] = ImVec4{ 0.38f, 0.3805f, 0.381f, 1.0f };
		colors[ImGuiCol_TabActive] = ImVec4{ 0.28f, 0.2805f, 0.281f, 1.0f };
		colors[ImGuiCol_TabUnfocused] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.2f, 0.205f, 0.21f, 1.0f };

		// Title
		colors[ImGuiCol_TitleBg] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgActive] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.15f, 0.1505f, 0.151f, 1.0f };
	}

}
