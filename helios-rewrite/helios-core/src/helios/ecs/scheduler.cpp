// helios-core/src/helios/ecs/scheduler.cpp
#include "helios/ecs/scheduler.h"

namespace helios {

Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

SystemId Scheduler::next_id() {
    return SystemId{ m_next_id.fetch_add(1, std::memory_order_relaxed) };
}

void Scheduler::rebuild_plan(Schedule schedule) {
    auto& data = m_schedules[static_cast<size_t>(schedule)];
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

    const auto& plan = data.plan;

    if (!m_parallel || !m_pool) {
        // --- Sequential execution ---
        // Execute stages in order. Within each stage, execute systems in order
        // of their index (which is registration order for conflict-resolved edges).
        for (const auto& stage : plan.stages) {
            for (size_t idx : stage.system_indices) {
                data.systems[idx].run(world);
            }
        }
    } else {
        // --- Parallel execution ---
        for (const auto& stage : plan.stages) {
            if (stage.system_indices.size() == 1) {
                // Single system in stage -- run inline, no pool overhead
                data.systems[stage.system_indices[0]].run(world);
            } else {
                // Multiple systems -- dispatch to pool
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
}

void Scheduler::enable_parallel(uint32_t thread_count) {
    if (m_pool) return; // already created
    m_pool = std::make_unique<ThreadPool>(thread_count);
    m_parallel = true;
}

} // namespace helios
