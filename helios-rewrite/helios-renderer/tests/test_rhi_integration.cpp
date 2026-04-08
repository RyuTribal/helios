// Integration test: verify all type aliases resolve and basic operations compile.
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

using namespace helios::rhi;

// Static assertions: all RHI types are move-constructible and not copy-constructible
static_assert(std::is_move_constructible_v<Device>);
static_assert(std::is_move_constructible_v<Texture>);
static_assert(std::is_move_constructible_v<Buffer>);
static_assert(std::is_move_constructible_v<Pipeline>);
static_assert(std::is_move_constructible_v<CommandBuffer>);
static_assert(std::is_move_constructible_v<Swapchain>);
static_assert(std::is_move_constructible_v<DescriptorSet>);
static_assert(std::is_move_constructible_v<DescriptorSetLayout>);
static_assert(std::is_move_constructible_v<RenderPass>);
static_assert(std::is_move_constructible_v<Framebuffer>);
static_assert(std::is_move_constructible_v<Shader>);

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

TEST(RHIIntegration, TypeAliasesCompile) {
    // This test passes if the file compiles -- static_asserts above
    // verify the type properties at compile time.
    SUCCEED();
}

TEST(RHIIntegration, AllTypesAreMoveAssignable) {
    static_assert(std::is_move_assignable_v<Device>);
    static_assert(std::is_move_assignable_v<Texture>);
    static_assert(std::is_move_assignable_v<Buffer>);
    static_assert(std::is_move_assignable_v<Pipeline>);
    static_assert(std::is_move_assignable_v<CommandBuffer>);
    static_assert(std::is_move_assignable_v<Swapchain>);
    static_assert(std::is_move_assignable_v<DescriptorSet>);
    static_assert(std::is_move_assignable_v<DescriptorSetLayout>);
    static_assert(std::is_move_assignable_v<RenderPass>);
    static_assert(std::is_move_assignable_v<Framebuffer>);
    static_assert(std::is_move_assignable_v<Shader>);
    SUCCEED();
}

TEST(RHIIntegration, AllTypesAreNotCopyAssignable) {
    static_assert(!std::is_copy_assignable_v<Device>);
    static_assert(!std::is_copy_assignable_v<Texture>);
    static_assert(!std::is_copy_assignable_v<Buffer>);
    static_assert(!std::is_copy_assignable_v<Pipeline>);
    static_assert(!std::is_copy_assignable_v<CommandBuffer>);
    static_assert(!std::is_copy_assignable_v<Swapchain>);
    static_assert(!std::is_copy_assignable_v<DescriptorSet>);
    static_assert(!std::is_copy_assignable_v<DescriptorSetLayout>);
    static_assert(!std::is_copy_assignable_v<RenderPass>);
    static_assert(!std::is_copy_assignable_v<Framebuffer>);
    static_assert(!std::is_copy_assignable_v<Shader>);
    SUCCEED();
}
