#include "helios/camera_render_schedule.h"

#include <algorithm>

namespace helios {

void CameraRenderSchedules::add_step(RenderScheduleLabel label, CameraDrawStep step) {
    m_steps[label.value].push_back(std::move(step));
    m_sorted = false;
}

void CameraRenderSchedules::run(RenderScheduleLabel label, World& world,
                                const renderer::CameraView& view,
                                uint32_t camera_index) {
    auto it = m_steps.find(label.value);
    if (it == m_steps.end()) return;

    // Sort by order on first run after any add_step call
    if (!m_sorted) {
        for (auto& [_, steps] : m_steps) {
            std::sort(steps.begin(), steps.end(),
                      [](const CameraDrawStep& a, const CameraDrawStep& b) {
                          return a.order < b.order;
                      });
        }
        m_sorted = true;
    }

    for (auto& step : it->second) {
        step.run(world, view, camera_index);
    }
}

bool CameraRenderSchedules::has_steps(RenderScheduleLabel label) const {
    auto it = m_steps.find(label.value);
    return it != m_steps.end() && !it->second.empty();
}

} // namespace helios
