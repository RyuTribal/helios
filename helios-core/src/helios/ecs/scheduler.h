#pragma once

#include "helios/ecs/dag_builder.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/system_descriptor.h"
#include "helios/ecs/system_set.h"
#include "helios/ecs/commands.h"
#include "helios/ecs/system_param_traits.h"
#include "helios/ecs/thread_pool.h"
#include "helios/core/assert.h"

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace helios {

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    /// Register a system into a specific schedule. Returns a builder for
    /// attaching ordering constraints.
    ///
    /// Usage:
    ///   scheduler.add_system(Schedule::Update, my_system);
    ///   scheduler.add_system(Schedule::Update, my_other_system).after(id);
    template <typename F>
    SystemDescriptorBuilder add_system(Schedule schedule, F&& system,
                                       std::string name = "");

    /// Overload for SystemSet<F> -- applies embedded ordering constraints.
    template <typename F>
    SystemDescriptorBuilder add_system(Schedule schedule, SystemSet<F> set,
                                       std::string name = "");

    /// Add a pre-built SystemDescriptor. Used by state management to register
    /// member-function systems with a type-erased invoker and an owner_tag.
    void add_system(Schedule schedule, SystemDescriptor descriptor);

    /// Remove all systems tagged with a given owner_tag.
    /// Returns the number of systems removed.
    size_t remove_systems_by_tag(std::type_index tag);

    /// Run all systems in the given schedule.
    void run(World& world, Schedule schedule);

    /// Force rebuild of the execution plan for a schedule.
    /// Called automatically on first run or after adding systems.
    void rebuild_plan(Schedule schedule);

    /// Enable or disable parallel execution. When disabled, all systems run
    /// sequentially in topological order (useful for debugging data races).
    void set_parallel(bool enabled) { m_parallel = enabled; }
    bool is_parallel() const { return m_parallel; }

    /// Enable parallel execution with a given thread count.
    void enable_parallel(uint32_t thread_count = 0);

    /// Set the shared thread pool (called by App at startup).
    void set_pool(std::shared_ptr<ThreadPool> pool) { m_pool = std::move(pool); }

    /// Get the SystemId assigned to a named system. Useful for cross-plugin
    /// ordering: .after(scheduler.id_of("frame_begin")).
    /// Returns a zero SystemId if not found.
    SystemId id_of(const std::string& name) const;

private:
    static constexpr size_t SCHEDULE_COUNT = static_cast<size_t>(Schedule::COUNT);

    struct ScheduleData {
        std::vector<SystemDescriptor> systems;
        ExecutionPlan                 plan;
        bool                          dirty = true; // needs rebuild
    };

    // Execution strategy: run_sequential or run_parallel, selected once at
    // enable_parallel() time.  Avoids a branch on every schedule run.
    using RunStrategy = void (Scheduler::*)(ScheduleData&, World&);
    RunStrategy m_run_strategy = &Scheduler::run_sequential;

    void run_sequential(ScheduleData& data, World& world);
    void run_parallel(ScheduleData& data, World& world);

    std::array<ScheduleData, SCHEDULE_COUNT> m_schedules;
    std::atomic<uint64_t>                    m_next_id{1};
    bool                                     m_parallel = false;
    std::shared_ptr<ThreadPool>              m_pool;

    // Name → SystemId mapping for id_of()
    std::unordered_map<std::string, SystemId> m_name_ids;

    SystemId next_id();
};

// ============================================================================
// Template implementations
// ============================================================================

template <typename F>
SystemDescriptorBuilder Scheduler::add_system(Schedule schedule, F&& system,
                                              std::string name) {
    auto& data = m_schedules[static_cast<size_t>(schedule)];
    data.dirty = true;

    SystemId id = next_id();

    // Build descriptor
    SystemDescriptor desc;
    desc.id       = id;
    desc.name     = name.empty()
                        ? ("system_" + std::to_string(id.value))
                        : std::move(name);
    desc.accesses = SystemParamExtractor<std::decay_t<F>>::accesses();
    desc.run      = SystemParamExtractor<std::decay_t<F>>::wrap(
                        std::forward<F>(system));

    // Store name → id mapping for cross-plugin ordering via id_of()
    auto [it, inserted] = m_name_ids.emplace(desc.name, id);
    HELIOS_ASSERT(inserted,
        ("Duplicate system name: '" + desc.name + "' -- system names must be unique").c_str());
    (void)it;

    data.systems.push_back(std::move(desc));
    return SystemDescriptorBuilder(data.systems.back());
}

template <typename F>
SystemDescriptorBuilder Scheduler::add_system(Schedule schedule, SystemSet<F> set,
                                              std::string name) {
    auto builder = add_system(schedule, set.function(), std::move(name));
    for (const auto& id : set.after_ids()) {
        builder.after(id);
    }
    for (const auto& id : set.before_ids()) {
        builder.before(id);
    }
    return builder;
}

} // namespace helios
