// helios-rewrite/helios-renderer/src/helios/graph/render_thread.cpp
#include "helios/graph/render_thread.h"
#include "helios/graph/graph_log_channel.h"
#include "helios/core/assert.h"

namespace helios::renderer {

RenderThread::RenderThread(rhi::Device& device, rhi::Swapchain& swapchain, GraphBuildFn graph_builder)
    : m_device(device)
    , m_swapchain(swapchain)
    , m_graph_builder(std::move(graph_builder))
    , m_thread(&RenderThread::thread_main, this)
{
    HELIOS_LOG(Graph, Info, "RenderThread: started");
}

RenderThread::~RenderThread() {
    HELIOS_LOG(Graph, Info, "RenderThread: shutting down");

    // Signal the thread to stop.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_running.store(false, std::memory_order_release);
    }
    m_cv.notify_one();

    // Wait for the thread to finish.
    if (m_thread.joinable()) {
        m_thread.join();
    }

    HELIOS_LOG(Graph, Info, "RenderThread: joined");
}

void RenderThread::submit(FramePacket packet) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // Latest-wins: overwrite any unconsumed packet.
        m_pending_packet = std::move(packet);
    }
    m_cv.notify_one();
}

void RenderThread::wait_idle() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idle_cv.wait(lock, [this] { return m_idle || !m_running.load(std::memory_order_relaxed); });
}

void RenderThread::thread_main() {
    // Each frame owns a ResourcePool for transient allocations.
    graph::ResourcePool pool(m_device);
    graph::RenderGraph graph;

    while (m_running.load(std::memory_order_acquire)) {
        FramePacket packet;

        // Wait for a packet.
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_idle = true;
            m_idle_cv.notify_all();

            m_cv.wait(lock, [this] {
                return m_pending_packet.has_value() || !m_running.load(std::memory_order_relaxed);
            });

            if (!m_running.load(std::memory_order_relaxed)) {
                break;
            }

            // Consume the packet.
            packet = std::move(*m_pending_packet);
            m_pending_packet.reset();
            m_idle = false;
        }

        // --- Render one frame ---

        HELIOS_LOG(Graph, Trace, "RenderThread: processing frame {}", packet.frame_number);

        // 1. Acquire swapchain image.
        if (!m_swapchain.acquire_next_image()) {
            // Swapchain out of date (e.g. window resized). Skip this frame.
            // The main thread will handle resize and resubmit.
            HELIOS_LOG(Graph, Warn, "RenderThread: swapchain acquire failed, skipping frame");
            continue;
        }

        // 2. Build the render graph from the packet.
        graph.clear();
        m_graph_builder(packet, graph);

        // 3. Compile and execute the graph.
        graph.compile_and_execute(m_device, pool);

        // 4. Present.
        m_swapchain.present();

        // 5. Tick the resource pool (evict stale resources).
        pool.tick();
    }

    // Drain GPU before exiting.
    m_device.wait_idle();
    pool.clear();

    HELIOS_LOG(Graph, Debug, "RenderThread: thread_main exiting");
}

} // namespace helios::renderer
