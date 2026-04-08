// helios-rewrite/helios-renderer/src/helios/graph/render_thread.h
#pragma once

#include "helios/graph/frame_packet.h"
#include "helios/graph/render_graph.h"
#include "helios/graph/resource_pool.h"
#include "helios/rhi/rhi.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace helios::renderer {

// Callback type for building the render graph from a FramePacket.
// The ForwardPlus plugin (or any custom pipeline) provides this.
using GraphBuildFn = std::function<void(const FramePacket&, graph::RenderGraph&)>;

class RenderThread {
public:
    // Starts the render thread immediately. The thread runs until destruction.
    //
    // device / swapchain -- GPU objects owned externally (e.g. by VulkanRenderPlugin).
    //                       Must outlive the RenderThread.
    // graph_builder      -- callback that populates the RenderGraph from a FramePacket.
    //                       Called on the render thread each frame.
    RenderThread(rhi::Device& device, rhi::Swapchain& swapchain, GraphBuildFn graph_builder);

    // Signals shutdown, wakes the thread, and joins.
    ~RenderThread();

    // Non-copyable, non-movable (owns a running thread).
    RenderThread(const RenderThread&) = delete;
    RenderThread& operator=(const RenderThread&) = delete;
    RenderThread(RenderThread&&) = delete;
    RenderThread& operator=(RenderThread&&) = delete;

    // Called by the main thread at the end of PreRender schedule.
    // Moves the packet into the pending slot and wakes the render thread.
    // If the render thread has not yet consumed the previous packet, the
    // previous packet is dropped (latest-wins policy -- no queuing).
    void submit(FramePacket packet);

    // Returns true if the render thread is still running.
    bool is_running() const { return m_running.load(std::memory_order_relaxed); }

    // Block until the render thread has consumed the current pending packet
    // and become idle. Useful for shutdown and resize synchronization.
    void wait_idle();

private:
    void thread_main();

    rhi::Device& m_device;
    rhi::Swapchain& m_swapchain;
    GraphBuildFn m_graph_builder;

    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::condition_variable m_idle_cv;     // signaled when render thread goes idle
    std::optional<FramePacket> m_pending_packet;
    std::atomic<bool> m_running{true};
    bool m_idle = true;                    // protected by m_mutex
};

} // namespace helios::renderer
