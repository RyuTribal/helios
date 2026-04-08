// helios-core/src/helios/ecs/app.cpp
#include "helios/ecs/app.h"
#include "helios/ecs/time.h"
#include "helios/window/windows.h"
#include "helios/core/engine_log_channels.h"

namespace helios {

App::App() {
    // Insert core resources that every App needs
    m_world.insert_resource<Time>(Time{});
    m_world.insert_resource<FixedTimeAccumulator>(FixedTimeAccumulator{});
}

App::~App() = default;

void App::quit() {
    m_running = false;
}

void App::enable_parallel(uint32_t thread_count) {
    m_scheduler.enable_parallel(thread_count);
}

void App::tick() {
    HELIOS_LOG(Core, Trace, "App::tick frame={}", m_world.resource<Time>().frame_count());
    auto frame_start = std::chrono::high_resolution_clock::now();

    m_scheduler.run(m_world, Schedule::PreUpdate);

    // Check if window system requested quit (close button pressed).
    // m_has_windows is cached at build time so we avoid a hash-map lookup
    // every frame.  The resource itself is guaranteed to exist if the flag
    // is true (WindowPlugin inserts it during build).
    if (m_has_windows) {
        if (m_world.resource<Windows>().quit_requested()) {
            HELIOS_LOG(Core, Info, "Window close requested — shutting down");
            m_running = false;
            return;
        }
    }

    m_scheduler.run(m_world, Schedule::Update);

    // ---- Fixed update loop ----
    {
        float frame_delta = m_world.resource<Time>().delta();
        auto& acc = m_world.resource<FixedTimeAccumulator>();
        acc.remaining += frame_delta;

        while (acc.remaining >= acc.timestep) {
            m_scheduler.run(m_world, Schedule::FixedUpdate);
            acc.remaining -= acc.timestep;
        }

        acc.alpha = (acc.timestep > 0.0f)
                        ? (acc.remaining / acc.timestep)
                        : 0.0f;
    }

    m_scheduler.run(m_world, Schedule::PostUpdate);
    m_scheduler.run(m_world, Schedule::PreRender);

    // Submit render data to the render thread (if a render plugin installed the hook).
    // m_has_post_pre_render is cached when set_post_pre_render() is called,
    // avoiding a std::function truthiness test every frame.
    if (m_has_post_pre_render) {
        m_post_pre_render(m_world);
    }

    // Swap event buffers so next frame's readers see this frame's writes.
    m_world.swap_event_buffers();

    // ---- Update Time resource ----
    auto frame_end = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration<float>(frame_end - frame_start).count();

    auto& time = m_world.resource<Time>();
    time.m_delta    = dt;
    time.m_elapsed += dt;
    time.m_frame_count++;
}

void App::run() {
    HELIOS_LOG(Core, Info, "App starting");
    // Run startup systems exactly once
    m_scheduler.run(m_world, Schedule::Startup);
    HELIOS_LOG(Core, Debug, "Startup systems complete");

    m_running = true;

    while (m_running) {
        tick();
    }

    // Run shutdown hook before resource destructors fire (e.g. GPU wait_idle).
    if (m_shutdown_hook) {
        m_shutdown_hook(m_world);
    }

    HELIOS_LOG(Core, Info, "App shutting down");
}

} // namespace helios
