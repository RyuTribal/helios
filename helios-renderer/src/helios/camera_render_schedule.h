#pragma once

#include "helios/render_schedule.h"
#include "helios/graph/frame_packet.h"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace helios {

class World;

/// A single draw step within a camera render schedule.
/// Not an ECS system — a plain callback that records GPU commands.
struct CameraDrawStep {
    std::string name;
    int32_t order = 0;
    std::function<void(World&, const renderer::CameraView&, uint32_t camera_index)> run;
};

/// Storage for all camera render schedules, keyed by RenderScheduleLabel.
/// Stored as a World resource.
class CameraRenderSchedules {
public:
    /// Register a draw step under a label.
    void add_step(RenderScheduleLabel label, CameraDrawStep step);

    /// Run all steps for a given label, in order.
    void run(RenderScheduleLabel label, World& world,
             const renderer::CameraView& view, uint32_t camera_index);

    /// Check if any steps are registered for a label.
    bool has_steps(RenderScheduleLabel label) const;

private:
    std::unordered_map<uint32_t, std::vector<CameraDrawStep>> m_steps;
    bool m_sorted = false;
};

} // namespace helios
