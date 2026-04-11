#pragma once

#include <chrono>
#include <cstdint>

namespace helios {

// Forward declaration -- App will be defined in a later plan.
class App;

/// Per-frame timing resource. Updated by the App main loop at the end of each
/// frame. Systems read this via Res<Time>.
class Time {
public:
    /// Seconds elapsed since last frame.
    float delta() const { return m_delta; }

    /// Seconds elapsed since App::run() was called.
    float elapsed() const { return m_elapsed; }

    /// Frames rendered since startup.
    uint64_t frame_count() const { return m_frame_count; }

private:
    friend class App; // only App may mutate

    float    m_delta       = 0.0f;
    float    m_elapsed     = 0.0f;
    uint64_t m_frame_count = 0;
};

/// Fixed-timestep accumulator. Drives Schedule::FixedUpdate.
/// The App main loop accumulates frame delta into `remaining`, then ticks
/// FixedUpdate once per `timestep` until the accumulator is drained.
struct FixedTimeAccumulator {
    float    timestep            = 1.0f / 60.0f;  // 60 Hz default
    float    remaining           = 0.0f;
    float    alpha               = 0.0f;           // interpolation fraction for rendering
    uint32_t max_ticks_per_frame = 10;             // prevent spiral of death
};

} // namespace helios
