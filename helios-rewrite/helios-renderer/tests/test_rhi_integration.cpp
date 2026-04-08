// Integration test: verify all abstract RHI interfaces compile and have
// correct type properties (non-copyable, polymorphic via unique_ptr).
#include "helios/rhi/rhi.h"
#include "helios/vulkan/vulkan_context.h"
#include "helios/vulkan/vulkan_device.h"
#include "helios/vulkan/vulkan_texture.h"
#include "helios/vulkan/vulkan_buffer.h"
#include "helios/vulkan/vulkan_pipeline.h"
#include "helios/vulkan/vulkan_command_buffer.h"
#include "helios/vulkan/vulkan_swapchain.h"
#include "helios/vulkan/vulkan_descriptor.h"
#include "helios/vulkan/vulkan_render_pass.h"
#include "helios/vulkan/vulkan_framebuffer.h"
#include "helios/vulkan/vulkan_shader.h"
#include <gtest/gtest.h>
#include <type_traits>
#include <memory>

using namespace helios::rhi;
using namespace helios::rhi::vulkan;

// Static assertions: all abstract base types are polymorphic (have virtual methods)
static_assert(std::has_virtual_destructor_v<Device>);
static_assert(std::has_virtual_destructor_v<Texture>);
static_assert(std::has_virtual_destructor_v<Buffer>);
static_assert(std::has_virtual_destructor_v<Pipeline>);
static_assert(std::has_virtual_destructor_v<CommandBuffer>);
static_assert(std::has_virtual_destructor_v<Swapchain>);
static_assert(std::has_virtual_destructor_v<DescriptorSet>);
static_assert(std::has_virtual_destructor_v<DescriptorSetLayout>);
static_assert(std::has_virtual_destructor_v<RenderPass>);
static_assert(std::has_virtual_destructor_v<Framebuffer>);
static_assert(std::has_virtual_destructor_v<Shader>);

// Abstract base types are not directly constructible from outside
static_assert(!std::is_copy_constructible_v<Device>);
static_assert(!std::is_copy_constructible_v<Texture>);
static_assert(!std::is_copy_constructible_v<Buffer>);
static_assert(!std::is_copy_constructible_v<Pipeline>);
static_assert(!std::is_copy_constructible_v<CommandBuffer>);
static_assert(!std::is_copy_constructible_v<Swapchain>);
static_assert(!std::is_copy_constructible_v<DescriptorSet>);
static_assert(!std::is_copy_constructible_v<DescriptorSetLayout>);
static_assert(!std::is_copy_constructible_v<RenderPass>);
static_assert(!std::is_copy_constructible_v<Framebuffer>);
static_assert(!std::is_copy_constructible_v<Shader>);

// Concrete Vulkan types are move-constructible and not copy-constructible
static_assert(std::is_move_constructible_v<VulkanDevice>);
static_assert(std::is_move_constructible_v<VulkanTexture>);
static_assert(std::is_move_constructible_v<VulkanBuffer>);
static_assert(std::is_move_constructible_v<VulkanPipeline>);
static_assert(std::is_move_constructible_v<VulkanCommandBuffer>);
static_assert(std::is_move_constructible_v<VulkanSwapchain>);
static_assert(std::is_move_constructible_v<VulkanDescriptorSet>);
static_assert(std::is_move_constructible_v<VulkanDescriptorSetLayout>);
static_assert(std::is_move_constructible_v<VulkanRenderPass>);
static_assert(std::is_move_constructible_v<VulkanFramebuffer>);
static_assert(std::is_move_constructible_v<VulkanShader>);

static_assert(!std::is_copy_constructible_v<VulkanDevice>);
static_assert(!std::is_copy_constructible_v<VulkanTexture>);
static_assert(!std::is_copy_constructible_v<VulkanBuffer>);
static_assert(!std::is_copy_constructible_v<VulkanPipeline>);
static_assert(!std::is_copy_constructible_v<VulkanCommandBuffer>);
static_assert(!std::is_copy_constructible_v<VulkanSwapchain>);
static_assert(!std::is_copy_constructible_v<VulkanDescriptorSet>);
static_assert(!std::is_copy_constructible_v<VulkanDescriptorSetLayout>);
static_assert(!std::is_copy_constructible_v<VulkanRenderPass>);
static_assert(!std::is_copy_constructible_v<VulkanFramebuffer>);
static_assert(!std::is_copy_constructible_v<VulkanShader>);

// Concrete Vulkan types inherit from abstract interfaces
static_assert(std::is_base_of_v<Device, VulkanDevice>);
static_assert(std::is_base_of_v<Texture, VulkanTexture>);
static_assert(std::is_base_of_v<Buffer, VulkanBuffer>);
static_assert(std::is_base_of_v<Pipeline, VulkanPipeline>);
static_assert(std::is_base_of_v<CommandBuffer, VulkanCommandBuffer>);
static_assert(std::is_base_of_v<Swapchain, VulkanSwapchain>);
static_assert(std::is_base_of_v<DescriptorSet, VulkanDescriptorSet>);
static_assert(std::is_base_of_v<DescriptorSetLayout, VulkanDescriptorSetLayout>);
static_assert(std::is_base_of_v<RenderPass, VulkanRenderPass>);
static_assert(std::is_base_of_v<Framebuffer, VulkanFramebuffer>);
static_assert(std::is_base_of_v<Shader, VulkanShader>);

TEST(RHIIntegration, TypeAliasesCompile) {
    // This test passes if the file compiles -- static_asserts above
    // verify the type properties at compile time.
    SUCCEED();
}

TEST(RHIIntegration, AllConcreteTypesAreMoveAssignable) {
    static_assert(std::is_move_assignable_v<VulkanDevice>);
    static_assert(std::is_move_assignable_v<VulkanTexture>);
    static_assert(std::is_move_assignable_v<VulkanBuffer>);
    static_assert(std::is_move_assignable_v<VulkanPipeline>);
    static_assert(std::is_move_assignable_v<VulkanCommandBuffer>);
    static_assert(std::is_move_assignable_v<VulkanSwapchain>);
    static_assert(std::is_move_assignable_v<VulkanDescriptorSet>);
    static_assert(std::is_move_assignable_v<VulkanDescriptorSetLayout>);
    static_assert(std::is_move_assignable_v<VulkanRenderPass>);
    static_assert(std::is_move_assignable_v<VulkanFramebuffer>);
    static_assert(std::is_move_assignable_v<VulkanShader>);
    SUCCEED();
}

TEST(RHIIntegration, AllConcreteTypesAreNotCopyAssignable) {
    static_assert(!std::is_copy_assignable_v<VulkanDevice>);
    static_assert(!std::is_copy_assignable_v<VulkanTexture>);
    static_assert(!std::is_copy_assignable_v<VulkanBuffer>);
    static_assert(!std::is_copy_assignable_v<VulkanPipeline>);
    static_assert(!std::is_copy_assignable_v<VulkanCommandBuffer>);
    static_assert(!std::is_copy_assignable_v<VulkanSwapchain>);
    static_assert(!std::is_copy_assignable_v<VulkanDescriptorSet>);
    static_assert(!std::is_copy_assignable_v<VulkanDescriptorSetLayout>);
    static_assert(!std::is_copy_assignable_v<VulkanRenderPass>);
    static_assert(!std::is_copy_assignable_v<VulkanFramebuffer>);
    static_assert(!std::is_copy_assignable_v<VulkanShader>);
    SUCCEED();
}

TEST(RHIIntegration, BackendEnumExists) {
    EXPECT_NE(static_cast<int>(Backend::Vulkan), -1);
    EXPECT_EQ(detect_best_backend(), Backend::Vulkan);
}
