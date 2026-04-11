// Vulkan device tests require a real GPU and display server.
// These tests are guarded by a probe: if GLFW window creation fails,
// or if the Vulkan context/device creation fails, tests are SKIPPED.
//
// NOTE: Some environments (headless CI, broken GPU drivers) may crash
// inside the Vulkan driver itself. We guard with HELIOS_RUN_GPU_TESTS
// env var -- set to "1" to enable. Default: skip GPU tests.

#include <gtest/gtest.h>
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_device.h"

#include <helios/core/logging.h>

#include <GLFW/glfw3.h>
#include <cstdlib>

using namespace helios::rhi::vulkan;

static bool gpu_tests_enabled() {
    const char* env = std::getenv("HELIOS_RUN_GPU_TESTS");
    return env && std::string(env) == "1";
}

class VulkanDeviceTest : public ::testing::Test {
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
        m_window = glfwCreateWindow(64, 64, "VulkanTest", nullptr, nullptr);
        if (!m_window)
            GTEST_SKIP() << "Failed to create hidden GLFW window";

        m_context = std::make_unique<VulkanContext>("HeliosTest", true);
        if (!*m_context) {
            GTEST_SKIP() << "Vulkan instance creation failed (no Vulkan support?)";
        }

        VkSurfaceKHR surface = m_context->create_surface(m_window);
        if (surface == VK_NULL_HANDLE) {
            GTEST_SKIP() << "Failed to create Vulkan surface";
        }
        m_surface = surface;

        m_device = std::make_unique<VulkanDevice>(*m_context, m_surface);
        if (!*m_device) {
            GTEST_SKIP() << "Vulkan device creation failed";
        }
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

bool VulkanDeviceTest::s_glfw_ok = false;
std::unique_ptr<helios::LogSystem> VulkanDeviceTest::s_log;

TEST_F(VulkanDeviceTest, DeviceCreation) {
    EXPECT_NE(m_device->device(), VK_NULL_HANDLE);
    EXPECT_NE(m_device->physical_device(), VK_NULL_HANDLE);
    EXPECT_NE(m_device->allocator(), VK_NULL_HANDLE);
    EXPECT_NE(m_device->graphics_queue(), VK_NULL_HANDLE);
}

TEST_F(VulkanDeviceTest, NativeHandleDevice) {
    auto handle = m_device->native_handle<VkDevice>();
    EXPECT_EQ(handle, m_device->device());
    EXPECT_NE(handle, VK_NULL_HANDLE);
}

TEST_F(VulkanDeviceTest, NativeHandlePhysicalDevice) {
    auto handle = m_device->native_handle<VkPhysicalDevice>();
    EXPECT_EQ(handle, m_device->physical_device());
}

TEST_F(VulkanDeviceTest, NativeHandleAllocator) {
    auto handle = m_device->native_handle<VmaAllocator>();
    EXPECT_EQ(handle, m_device->allocator());
}

TEST_F(VulkanDeviceTest, WaitIdleDoesNotCrash) {
    EXPECT_NO_THROW(m_device->wait_idle());
}

TEST_F(VulkanDeviceTest, MoveConstruct) {
    VkDevice original_handle = m_device->device();
    VulkanDevice moved(std::move(*m_device));

    EXPECT_EQ(moved.device(), original_handle);
    EXPECT_EQ(m_device->device(), VK_NULL_HANDLE);  // source nulled

    // Clean up moved device properly
    moved.wait_idle();
    m_device = std::make_unique<VulkanDevice>(std::move(moved));
}
