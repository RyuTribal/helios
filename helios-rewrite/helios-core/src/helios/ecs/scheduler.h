// helios-core/src/helios/ecs/scheduler.h
#pragma once

#include "helios/ecs/dag_builder.h"
#include "helios/ecs/schedule.h"
#include "helios/ecs/system_descriptor.h"
#include "helios/ecs/system_set.h"
#include "helios/ecs/commands.h"
#include "helios/ecs/system_param_traits.h"
#include "helios/ecs/thread_pool.h"

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
    /// If thread_count is 0, uses hardware_concurrency - 1.
    /// Can only be called once; subsequent calls are no-ops.
    void enable_parallel(uint32_t thread_count = 0);

    /// Get the SystemId assigned to a function pointer. Useful for chaining
    /// .after(scheduler.id_of(some_system)).
    /// Returns a zero SystemId if not found.
    template <typename F>
    SystemId id_of(F&& fn) const;

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
    bool                                     m_parallel = false; // start sequential
    std::unique_ptr<ThreadPool>              m_pool;

    // Function-pointer to SystemId mapping for id_of()
    std::unordered_map<std::type_index, SystemId> m_function_ids;

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

    // Store function type -> id mapping
    m_function_ids[std::type_index(typeid(std::decay_t<F>))] = id;

    // Build descriptor
    SystemDescriptor desc;
    desc.id       = id;
    desc.name     = name.empty()
                        ? ("system_" + std::to_string(id.value))
                        : std::move(name);
    desc.accesses = SystemParamExtractor<std::decay_t<F>>::accesses();
    desc.run      = SystemParamExtractor<std::decay_t<F>>::wrap(
                        std::forward<F>(system));

    data.systems.push_back(std::move(desc));
    return SystemDescriptorBuilder(data.systems.back());
}

template <typename F>
SystemId Scheduler::id_of(F&& /*fn*/) const {
    auto it = m_function_ids.find(std::type_index(typeid(std::decay_t<F>)));
    if (it != m_function_ids.end()) return it->second;
    return SystemId{0};
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
