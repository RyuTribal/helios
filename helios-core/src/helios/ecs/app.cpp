#include "helios/ecs/app.h"
#include "helios/ecs/time.h"
#include "helios/ecs/thread_pool.h"
#include "helios/ecs/transform_propagation.h"
#include "helios/window/windows.h"
#include "helios/core/engine_log_channels.h"

namespace helios {

App::App() {
    m_world.insert_resource<Time>(Time{});
    m_world.insert_resource<FixedTimeAccumulator>(FixedTimeAccumulator{});

    // Shared thread pool — used by scheduler, asset server, editor tasks
    auto pool = std::make_shared<ThreadPool>();
    m_scheduler.set_pool(pool);
    m_world.insert_resource(std::move(pool));
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

    if (m_has_windows) {
        if (m_world.resource<Windows>().quit_requested()) {
            HELIOS_LOG(Core, Info, "Window close requested — shutting down");
            m_running = false;
            return;
        }
    }

    m_scheduler.run(m_world, Schedule::Update);

    {
        float frame_delta = m_world.resource<Time>().delta();
        auto& acc = m_world.resource<FixedTimeAccumulator>();
        acc.remaining += frame_delta;

        uint32_t ticks = 0;
        while (acc.remaining >= acc.timestep && ticks < acc.max_ticks_per_frame) {
            m_scheduler.run(m_world, Schedule::FixedUpdate);
            acc.remaining -= acc.timestep;
            ticks++;
        }
        if (ticks == acc.max_ticks_per_frame && acc.remaining >= acc.timestep) {
            acc.remaining = 0.0f;
        }

        acc.alpha = (acc.timestep > 0.0f)
                        ? (acc.remaining / acc.timestep)
                        : 0.0f;
    }

    m_scheduler.run(m_world, Schedule::PostUpdate);

    propagate_transforms(m_world);

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

    // Run shutdown systems (e.g. GPU flush) before resource destructors fire.
    m_scheduler.run(m_world, Schedule::Shutdown);

    HELIOS_LOG(Core, Info, "App shutting down");
}

} // namespace helios
