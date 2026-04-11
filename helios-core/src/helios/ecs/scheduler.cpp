#include "helios/ecs/scheduler.h"
#include "helios/ecs/access_descriptor.h"
#include "helios/core/engine_log_channels.h"

#include <algorithm>

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

// Thread-safety note: Systems that take Commands as a parameter report
// AccessMode::Write on typeid(Commands).  The DAG builder detects the
// write-write conflict and adds serialization edges between all such
// systems, guaranteeing they never execute in the same parallel stage.
// After each stage completes, pending commands are applied and cleared
// on the main thread (see apply_and_clear_pending_commands below).

void Scheduler::run_sequential(ScheduleData& data, World& world) {
    for (const auto& stage : data.plan.stages) {
        for (size_t idx : stage.system_indices) {
            world.advance_tick();
            auto& desc = data.systems[idx];
            desc.run(world, desc.last_run_tick);
            desc.last_run_tick = world.current_tick();
        }
        // Apply deferred commands accumulated during this stage.
        world.apply_and_clear_pending_commands();
    }
}

void Scheduler::run_parallel(ScheduleData& data, World& world) {
    for (const auto& stage : data.plan.stages) {
        if (stage.system_indices.size() == 1) {
            world.advance_tick();
            auto& desc = data.systems[stage.system_indices[0]];
            desc.run(world, desc.last_run_tick);
            desc.last_run_tick = world.current_tick();
        } else {
            // Runtime access gating: partition the stage into sub-batches
            // of non-conflicting systems, then dispatch each batch to the
            // pool. Systems within a batch run in parallel; batches run
            // sequentially. No spin loops, no condvars — just submit + get.
            world.advance_tick();

            std::vector<size_t> remaining(stage.system_indices.begin(),
                                           stage.system_indices.end());

            while (!remaining.empty()) {
                // Build a batch of non-conflicting systems
                std::vector<AccessDescriptor> batch_access;
                std::vector<size_t> batch;

                auto it = remaining.begin();
                while (it != remaining.end()) {
                    auto& sys = data.systems[*it];
                    if (!has_conflict(sys.accesses, batch_access)) {
                        batch_access.insert(batch_access.end(),
                            sys.accesses.begin(), sys.accesses.end());
                        batch.push_back(*it);
                        it = remaining.erase(it);
                    } else {
                        ++it;
                    }
                }

                if (batch.size() == 1) {
                    // Single system — run inline
                    auto& sys = data.systems[batch[0]];
                    sys.run(world, sys.last_run_tick);
                    sys.last_run_tick = world.current_tick();
                } else {
                    // Multiple non-conflicting systems — dispatch to pool
                    std::vector<std::future<void>> futures;
                    futures.reserve(batch.size());

                    for (size_t idx : batch) {
                        auto& sys = data.systems[idx];
                        uint32_t last_tick = sys.last_run_tick;
                        futures.push_back(m_pool->submit([&sys, &world, last_tick] {
                            sys.run(world, last_tick);
                        }));
                    }

                    for (auto& f : futures) f.get();

                    uint32_t tick = world.current_tick();
                    for (size_t idx : batch) {
                        data.systems[idx].last_run_tick = tick;
                    }
                }
            }
        }
        world.apply_and_clear_pending_commands();
    }
}

void Scheduler::enable_parallel(uint32_t /*thread_count*/) {
    if (m_parallel) return;
    // Pool is expected to be set externally via set_pool().
    // If not set, create a default one.
    if (!m_pool)
        m_pool = std::make_shared<ThreadPool>();
    m_parallel = true;
    m_run_strategy = &Scheduler::run_parallel;
}

} // namespace helios
