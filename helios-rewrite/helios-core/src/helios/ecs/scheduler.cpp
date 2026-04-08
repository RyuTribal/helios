// helios-core/src/helios/ecs/scheduler.cpp
#include "helios/ecs/scheduler.h"
#include "helios/core/engine_log_channels.h"

#include <algorithm>
#include <exception>

namespace helios {

Scheduler::Scheduler() = default;
Scheduler::~Scheduler() = default;

SystemId Scheduler::next_id() {
    return SystemId{ m_next_id.fetch_add(1, std::memory_order_relaxed) };
}

SystemId Scheduler::id_of(const std::string& name) const {
    auto it = m_name_ids.find(name);
    if (it != m_name_ids.end()) return it->second;
    return SystemId{0};
}

void Scheduler::add_system(Schedule schedule, SystemDescriptor descriptor) {
    auto& data = m_schedules[static_cast<size_t>(schedule)];
    data.dirty = true;

    // Assign an id if the descriptor doesn't already have one.
    if (descriptor.id.value == 0) {
        descriptor.id = next_id();
    }

    // Ensure name is set.
    if (descriptor.name.empty()) {
        descriptor.name = "state_system_" + std::to_string(descriptor.id.value);
    }

    // Store name -> id mapping (skip duplicate names from state systems).
    m_name_ids.emplace(descriptor.name, descriptor.id);

    data.systems.push_back(std::move(descriptor));
}

size_t Scheduler::remove_systems_by_tag(std::type_index tag) {
    size_t removed = 0;
    for (auto& data : m_schedules) {
        auto it = std::remove_if(data.systems.begin(), data.systems.end(),
            [&](const SystemDescriptor& desc) {
                if (desc.owner_tag.has_value() && *desc.owner_tag == tag) {
                    // Remove name mapping.
                    m_name_ids.erase(desc.name);
                    ++removed;
                    return true;
                }
                return false;
            });
        if (it != data.systems.end()) {
            data.systems.erase(it, data.systems.end());
            data.dirty = true;
        }
    }
    return removed;
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
            try {
                data.systems[idx].run(world);
            } catch (const std::exception& e) {
                HELIOS_LOG(Scheduler, Error, "System '{}' threw: {}",
                    data.systems[idx].name, e.what());
            } catch (...) {
                HELIOS_LOG(Scheduler, Error, "System '{}' threw an unknown exception",
                    data.systems[idx].name);
            }
        }
        // Apply deferred commands accumulated during this stage.
        world.apply_and_clear_pending_commands();
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
            // moving to the next stage. Collect exceptions without
            // aborting so all futures are joined.
            std::exception_ptr first_error;
            for (auto& f : futures) {
                try {
                    f.get();
                } catch (const std::exception& e) {
                    if (!first_error) first_error = std::current_exception();
                    HELIOS_LOG(Scheduler, Error, "System threw in parallel stage: {}", e.what());
                } catch (...) {
                    if (!first_error) first_error = std::current_exception();
                    HELIOS_LOG(Scheduler, Error, "System threw unknown exception in parallel stage");
                }
            }
            if (first_error) {
                HELIOS_LOG(Scheduler, Error,
                    "One or more parallel systems failed; continuing to next stage");
            }
        }
        // Apply deferred commands accumulated during this stage.
        world.apply_and_clear_pending_commands();
    }
}

void Scheduler::enable_parallel(uint32_t thread_count) {
    if (m_pool) return; // already created
    m_pool = std::make_unique<ThreadPool>(thread_count);
    m_parallel = true;
    m_run_strategy = &Scheduler::run_parallel;
}

} // namespace helios
