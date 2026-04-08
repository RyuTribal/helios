// helios-core/src/helios/ecs/app.h
#pragma once

#include "helios/ecs/plugin.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/scheduler.h"
#include "helios/ecs/system_set.h"
#include "helios/ecs/time.h"
#include "helios/ecs/world.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_set>

namespace helios {

// Forward declaration so insert_resource<Windows>() can cache m_has_windows
// without pulling in the full window header.
class Windows;

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // ---- Plugin registration ----

    /// Add a plugin. If the same plugin type was already added, this is a no-op
    /// (prevents double-registration when plugins depend on each other).
    template <Plugin P>
    App& add_plugin(P plugin = {});

    // ---- Convenience methods (delegate to World / Scheduler) ----

    /// Insert a resource into the World.
    template <typename T>
    App& insert_resource(T resource);

    /// Register an event type.
    template <typename T>
    App& add_event();

    /// Register a system into a schedule. Returns a builder for .after()/.before().
    template <typename F>
    SystemDescriptorBuilder add_system(Schedule schedule, F&& system,
                                       std::string name = "");

    /// Overload for SystemSet<F> with embedded ordering constraints.
    template <typename F>
    SystemDescriptorBuilder add_system(Schedule schedule, SystemSet<F> set,
                                       std::string name = "");

    /// Get the SystemId for a previously registered function.
    template <typename F>
    SystemId id_of(F&& fn) const;

    // ---- Execution ----

    /// Run the main loop. Blocks until the app is stopped.
    void run();

    /// Request shutdown. The current frame will complete, then run() returns.
    void quit();

    /// Run a single frame. Useful for testing and headless mode.
    void tick();

    // ---- Accessors ----

    World&           world()     { return m_world; }
    const World&     world()     const { return m_world; }
    Scheduler&       scheduler() { return m_scheduler; }
    const Scheduler& scheduler() const { return m_scheduler; }

    /// Enable parallel system execution.
    void enable_parallel(uint32_t thread_count = 0);

    // ---- Post-PreRender hook ----

    /// Type-erased callback invoked after PreRender schedule each frame.
    /// Used by the render plugin to submit FramePacket to RenderThread
    /// without creating a circular dependency (helios-core -> helios-renderer).
    using PostPreRenderFn = std::function<void(World&)>;

    /// Install a callback that runs after the PreRender schedule.
    /// Only one callback is supported; later calls overwrite the previous one.
    void set_post_pre_render(PostPreRenderFn fn) {
        m_post_pre_render = std::move(fn);
        m_has_post_pre_render = static_cast<bool>(m_post_pre_render);
    }

private:
    World     m_world;
    Scheduler m_scheduler;
    bool      m_running = false;
    bool      m_has_windows = false;   // cached at build time, avoids hash-map lookup per frame
    bool      m_has_post_pre_render = false; // cached when hook is set

    std::unordered_set<std::type_index> m_registered_plugins;

    // Frame timing state
    std::chrono::high_resolution_clock::time_point m_frame_start;

    // Optional callback run after PreRender schedule (e.g. FramePacket submission).
    PostPreRenderFn m_post_pre_render;
};

// ============================================================================
// Template implementations
// ============================================================================

template <Plugin P>
App& App::add_plugin(P plugin) {
    auto tid = std::type_index(typeid(P));
    if (m_registered_plugins.contains(tid)) return *this;
    m_registered_plugins.insert(tid);
    plugin.build(*this);
    return *this;
}

template <typename T>
App& App::insert_resource(T resource) {
    m_world.insert_resource<T>(std::move(resource));
    // Cache whether the Windows resource exists so tick() avoids a hash-map
    // lookup every frame.
    if constexpr (std::is_same_v<std::decay_t<T>, Windows>) {
        m_has_windows = true;
    }
    return *this;
}

template <typename T>
App& App::add_event() {
    m_world.register_event<T>();
    return *this;
}

template <typename F>
SystemDescriptorBuilder App::add_system(Schedule schedule, F&& system,
                                        std::string name) {
    return m_scheduler.add_system(schedule, std::forward<F>(system),
                                  std::move(name));
}

template <typename F>
SystemDescriptorBuilder App::add_system(Schedule schedule, SystemSet<F> set,
                                        std::string name) {
    return m_scheduler.add_system(schedule, std::move(set), std::move(name));
}

template <typename F>
SystemId App::id_of(F&& fn) const {
    return m_scheduler.id_of(std::forward<F>(fn));
}

} // namespace helios
