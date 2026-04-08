#pragma once

#include "helios/rhi/rhi_types.h"
#include <cstdint>
#include <memory>

namespace helios::rhi {

// Forward declarations for abstract types used by CommandBuffer.
class Pipeline;
class Buffer;
class DescriptorSet;
class RenderPass;
class Framebuffer;

// Abstract command buffer interface.
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;

    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;

    // Recording
    virtual void begin() = 0;
    virtual void end() = 0;

    // Render pass (dynamic rendering)
    virtual void begin_render_pass(const RenderPass& render_pass,
                                   const Framebuffer& framebuffer,
                                   const ClearValues& clear) = 0;
    virtual void end_render_pass() = 0;

    // Pipeline & state
    virtual void bind_pipeline(const Pipeline& pipeline) = 0;
    virtual void set_viewport(float x, float y, float width, float height) = 0;
    virtual void set_scissor(int32_t x, int32_t y, uint32_t width, uint32_t height) = 0;

    // Resources
    virtual void bind_vertex_buffer(const Buffer& buffer, uint32_t binding = 0) = 0;
    virtual void bind_index_buffer(const Buffer& buffer, IndexType type = IndexType::Uint32) = 0;
    virtual void bind_descriptor_set(uint32_t set, const DescriptorSet& ds) = 0;
    virtual void push_constants(ShaderStage stage, uint32_t offset,
                                uint32_t size, const void* data) = 0;

    // Draw
    virtual void draw(uint32_t vertex_count, uint32_t instance_count = 1,
                      uint32_t first_vertex = 0) = 0;
    virtual void draw_indexed(uint32_t index_count, uint32_t instance_count = 1,
                              uint32_t first_index = 0) = 0;

    // Compute
    virtual void dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;

    // Synchronization
    virtual void pipeline_barrier(const BarrierDesc& barrier) = 0;

    // Transfer
    virtual void copy_buffer(const Buffer& src, const Buffer& dst, uint32_t size) = 0;

protected:
    CommandBuffer() = default;
    CommandBuffer(CommandBuffer&&) noexcept = default;
    CommandBuffer& operator=(CommandBuffer&&) noexcept = default;
};

} // namespace helios::rhi
