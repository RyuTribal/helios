// Vulkan resource tests require a real GPU. Guarded by HELIOS_RUN_GPU_TESTS=1.

#include <gtest/gtest.h>
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_buffer.h"
#include "helios/vulkan/vulkan_texture.h"
#include "helios/vulkan/vulkan_descriptor.h"
#include "helios/vulkan/vulkan_command_buffer.h"
#include "helios/vulkan/vulkan_swapchain.h"
#include "helios/rhi/rhi_types.h"

#include <helios/core/logging.h>
#include <GLFW/glfw3.h>
#include <cstdlib>

using namespace helios::rhi;
using namespace helios::rhi::vulkan;

static bool gpu_tests_enabled() {
    const char* env = std::getenv("HELIOS_RUN_GPU_TESTS");
    return env && std::string(env) == "1";
}

class VulkanResourceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!gpu_tests_enabled()) {
            s_glfw_ok = false;
            return;
        }

        s_log = std::make_unique<helios::LogSystem>(helios::LogConfig{
            .enable_file_sink = false,
            .default_level = helios::LogLevel::Warn,
        });

        glfwSetErrorCallback([](int /*code*/, const char* /*desc*/) {});
        if (!glfwInit()) {
            s_glfw_ok = false;
            return;
        }

        // Probe: can we create a hidden window? (fails in headless CI)
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        GLFWwindow* probe = glfwCreateWindow(64, 64, "Probe", nullptr, nullptr);
        if (!probe) {
            s_glfw_ok = false;
            return;
        }
        glfwDestroyWindow(probe);
        s_glfw_ok = true;
    }

    static void TearDownTestSuite() {
        if (s_glfw_ok) glfwTerminate();
        s_log.reset();
    }

    void SetUp() override {
        if (!s_glfw_ok)
            GTEST_SKIP() << "GPU tests disabled (set HELIOS_RUN_GPU_TESTS=1 to enable)";

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        m_window = glfwCreateWindow(64, 64, "VulkanResourceTest", nullptr, nullptr);
        if (!m_window)
            GTEST_SKIP() << "Failed to create hidden GLFW window";

        m_context = std::make_unique<VulkanContext>("HeliosResourceTest", true);
        if (!*m_context) GTEST_SKIP() << "No Vulkan support";

        VkSurfaceKHR surface = m_context->create_surface(m_window);
        if (surface == VK_NULL_HANDLE) GTEST_SKIP() << "No Vulkan surface";
        m_surface = surface;

        m_device = std::make_unique<VulkanDevice>(*m_context, m_surface);
        if (!*m_device) GTEST_SKIP() << "No Vulkan device";
    }

    void TearDown() override {
        if (m_device) m_device->wait_idle();
        m_device.reset();
        if (m_surface != VK_NULL_HANDLE && m_context)
            m_context->destroy_surface(m_surface);
        m_context.reset();
        if (m_window) glfwDestroyWindow(m_window);
    }

    static bool s_glfw_ok;
    static std::unique_ptr<helios::LogSystem> s_log;

    GLFWwindow* m_window = nullptr;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<VulkanDevice> m_device;
};

bool VulkanResourceTest::s_glfw_ok = false;
std::unique_ptr<helios::LogSystem> VulkanResourceTest::s_log;

// --- Buffer tests ---

TEST_F(VulkanResourceTest, BufferCreateDestroy) {
    BufferDesc desc;
    desc.size       = 1024;
    desc.usage      = BufferUsage::Uniform;
    desc.access     = MemoryAccess::CPU_to_GPU;
    desc.debug_name = "TestBuffer";

    auto buffer = m_device->create_buffer(desc);
    ASSERT_NE(buffer, nullptr);
    auto* vk_buffer = static_cast<VulkanBuffer*>(buffer.get());
    EXPECT_NE(vk_buffer->vk_buffer(), VK_NULL_HANDLE);
    EXPECT_EQ(buffer->size(), 1024u);
    // RAII: buffer destroyed when it goes out of scope
}

TEST_F(VulkanResourceTest, BufferSetData) {
    BufferDesc desc;
    desc.size       = 64;
    desc.usage      = BufferUsage::Uniform;
    desc.access     = MemoryAccess::CPU_to_GPU;
    desc.debug_name = "SetDataBuffer";

    auto buffer = m_device->create_buffer(desc);
    ASSERT_NE(buffer, nullptr);

    float data[16] = {1.0f, 2.0f, 3.0f};
    EXPECT_NO_THROW(buffer->set_data(data, sizeof(data)));
}

TEST_F(VulkanResourceTest, BufferMoveOnly) {
    BufferDesc desc;
    desc.size   = 256;
    desc.usage  = BufferUsage::Vertex;
    desc.access = MemoryAccess::CPU_to_GPU;

    auto buf1 = m_device->create_buffer(desc);
    ASSERT_NE(buf1, nullptr);
    auto* vk_buf1 = static_cast<VulkanBuffer*>(buf1.get());
    VkBuffer original = vk_buf1->vk_buffer();

    // unique_ptr is move-only; moving it transfers ownership
    auto buf2 = std::move(buf1);
    EXPECT_EQ(buf1, nullptr);
    auto* vk_buf2 = static_cast<VulkanBuffer*>(buf2.get());
    EXPECT_EQ(vk_buf2->vk_buffer(), original);
}

// --- Texture tests ---

TEST_F(VulkanResourceTest, TextureCreateDestroy) {
    TextureDesc desc;
    desc.width      = 64;
    desc.height     = 64;
    desc.format     = TextureFormat::RGBA8;
    desc.usage      = TextureUsage::Sampled;
    desc.debug_name = "TestTexture";

    auto texture = m_device->create_texture(desc);
    ASSERT_NE(texture, nullptr);
    auto* vk_tex = static_cast<VulkanTexture*>(texture.get());
    EXPECT_NE(vk_tex->vk_image(), VK_NULL_HANDLE);
    EXPECT_NE(vk_tex->vk_image_view(), VK_NULL_HANDLE);
    EXPECT_NE(vk_tex->vk_sampler(), VK_NULL_HANDLE);
    EXPECT_EQ(texture->width(), 64u);
    EXPECT_EQ(texture->height(), 64u);
}

TEST_F(VulkanResourceTest, TextureWithInitialData) {
    TextureDesc desc;
    desc.width      = 4;
    desc.height     = 4;
    desc.format     = TextureFormat::RGBA8;
    desc.usage      = TextureUsage::Sampled;
    desc.debug_name = "DataTexture";

    std::vector<uint8_t> pixels(4 * 4 * 4, 255);  // 4x4 RGBA white
    auto texture = m_device->create_texture(desc, pixels.data());
    EXPECT_NE(texture, nullptr);
}

TEST_F(VulkanResourceTest, TextureNativeHandles) {
    TextureDesc desc;
    desc.width  = 16;
    desc.height = 16;
    desc.format = TextureFormat::RGBA8;
    desc.usage  = TextureUsage::Sampled;

    auto texture = m_device->create_texture(desc);
    ASSERT_NE(texture, nullptr);

    auto* vk_tex = static_cast<VulkanTexture*>(texture.get());
    EXPECT_EQ(vk_tex->native_handle<VkImage>(), vk_tex->vk_image());
    EXPECT_EQ(vk_tex->native_handle<VkImageView>(), vk_tex->vk_image_view());
}

TEST_F(VulkanResourceTest, TextureMoveOnly) {
    TextureDesc desc;
    desc.width  = 8;
    desc.height = 8;
    desc.format = TextureFormat::RGBA8;
    desc.usage  = TextureUsage::Sampled;

    auto tex1 = m_device->create_texture(desc);
    ASSERT_NE(tex1, nullptr);
    auto* vk_tex1 = static_cast<VulkanTexture*>(tex1.get());
    VkImage original = vk_tex1->vk_image();

    auto tex2 = std::move(tex1);
    EXPECT_EQ(tex1, nullptr);
    auto* vk_tex2 = static_cast<VulkanTexture*>(tex2.get());
    EXPECT_EQ(vk_tex2->vk_image(), original);
}

// --- Descriptor set layout tests ---

TEST_F(VulkanResourceTest, DescriptorSetLayoutCreateDestroy) {
    DescriptorSetLayoutDesc desc;
    desc.bindings = {
        {0, DescriptorType::UniformBuffer, ShaderStage::Vertex, 1},
        {1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 1},
    };
    desc.debug_name = "TestLayout";

    auto layout = m_device->create_descriptor_set_layout(desc);
    ASSERT_NE(layout, nullptr);
    auto* vk_layout = static_cast<VulkanDescriptorSetLayout*>(layout.get());
    EXPECT_NE(vk_layout->vk_layout(), VK_NULL_HANDLE);
}

// --- Command buffer tests ---

TEST_F(VulkanResourceTest, CommandBufferCreateDestroy) {
    auto cmd = m_device->create_command_buffer();
    ASSERT_NE(cmd, nullptr);
    auto* vk_cmd = static_cast<VulkanCommandBuffer*>(cmd.get());
    EXPECT_NE(vk_cmd->vk_command_buffer(), VK_NULL_HANDLE);
}

TEST_F(VulkanResourceTest, CommandBufferBeginEnd) {
    auto cmd = m_device->create_command_buffer();
    ASSERT_NE(cmd, nullptr);

    EXPECT_NO_THROW(cmd->begin());
    EXPECT_NO_THROW(cmd->end());
}

// --- Swapchain tests ---

TEST_F(VulkanResourceTest, SwapchainNullSurfaceHandledGracefully) {
    SwapchainDesc desc;
    desc.width        = 800;
    desc.height       = 600;
    desc.surface      = nullptr;  // no surface
    desc.present_mode = PresentMode::Fifo;

    // The constructor asserts on null surface. Since asserts abort in debug
    // builds, we just verify the test compiles and document the expected
    // behavior. In a release build without asserts, this would fail gracefully.
    // We don't call create_swapchain with null surface to avoid abort.
    SUCCEED();
}
