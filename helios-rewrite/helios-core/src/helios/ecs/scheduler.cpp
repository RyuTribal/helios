// helios-core/src/helios/ecs/scheduler.cpp
#include "helios/ecs/scheduler.h"
#include "helios/core/engine_log_channels.h"

namespace helios {

Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

SystemId Scheduler::next_id() {
    return SystemId{ m_next_id.fetch_add(1, std::memory_order_relaxed) };
}

void Scheduler::rebuild_plan(Schedule schedule) {
    auto& data = m_schedules[static_cast<size_t>(schedule)];
    HELIOS_LOG(Scheduler, Debug, "Rebuilding execution plan for schedule {} ({} systems)",
        static_cast<int>(schedule), data.systems.size());
    data.plan  = build_execution_plan(data.systems);
    data.dirty = false;
}

void Scheduler::run(World& world, Schedule schedule) {
    auto& data = m_schedules[static_cast<size_t>(schedule)];

    if (data.systems.empty()) return;

    // Rebuild plan if dirty
    if (data.dirty) {
        rebuild_plan(schedule);
    }

    HELIOS_LOG(Scheduler, Trace, "Running schedule {} ({} systems)",
        static_cast<int>(schedule), data.systems.size());

    // Dispatch via the strategy pointer -- sequential or parallel, decided
    // once at enable_parallel() time rather than branching every frame.
    (this->*m_run_strategy)(data, world);
}

void Scheduler::run_sequential(ScheduleData& data, World& world) {
    for (const auto& stage : data.plan.stages) {
        for (size_t idx : stage.system_indices) {
            data.systems[idx].run(world);
        }
    }
}

void Scheduler::run_parallel(ScheduleData& data, World& world) {
    for (const auto& stage : data.plan.stages) {
        if (stage.system_indices.size() == 1) {
            // Single system in stage -- run inline, no pool overhead
            data.systems[stage.system_indices[0]].run(world);
        } else {
            // Multiple systems -- dispatch to pool
            HELIOS_LOG(Scheduler, Trace, "Dispatching {} systems in parallel for stage",
                stage.system_indices.size());
            std::vector<std::future<void>> futures;
            futures.reserve(stage.system_indices.size());

            for (size_t idx : stage.system_indices) {
                auto& sys = data.systems[idx];
                futures.push_back(m_pool->submit([&sys, &world] {
                    sys.run(world);
                }));
            }

            // Wait for all systems in this stage to complete before
            // moving to the next stage.
            for (auto& f : futures) {
                f.get();
            }
        }
    }
}

void Scheduler::enable_parallel(uint32_t thread_count) {
    if (m_pool) return; // already created
    m_pool = std::make_unique<ThreadPool>(thread_count);
    m_parallel = true;
    m_run_strategy = &Scheduler::run_parallel;
}

} // namespace helios
