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
    // device        -- GPU device owned externally; must outlive the RenderThread.
    // swapchain_ptr -- pointer to the swapchain unique_ptr owned externally.
    //                 Storing a pointer-to-unique_ptr means the render thread
    //                 always dereferences the *current* swapchain, even after the
    //                 main thread replaces it during a resize via
    //                 `ctx->swapchain = std::move(new_swapchain)`.
    //                 The pointed-to unique_ptr must outlive the RenderThread.
    // graph_builder -- callback that populates the RenderGraph from a FramePacket.
    //                 Called on the render thread each frame.
    RenderThread(rhi::Device& device,
                 std::unique_ptr<rhi::Swapchain>* swapchain_ptr,
                 GraphBuildFn graph_builder);

    // Update the swapchain pointer (called from the main thread after resize,
    // while the render thread is idle -- use wait_idle() before calling this).
    void set_swapchain(std::unique_ptr<rhi::Swapchain>* swapchain_ptr);

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
    // Pointer to the owner's unique_ptr<Swapchain>. Indirecting through the
    // unique_ptr rather than caching the raw Swapchain* means we always reach
    // the *current* swapchain even if the main thread replaces it on resize.
    std::unique_ptr<rhi::Swapchain>* m_swapchain_ptr;
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
