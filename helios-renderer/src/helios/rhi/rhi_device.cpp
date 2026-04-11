#include "helios/rhi/rhi_device.h"

namespace helios::rhi {

void Device::flush_deferred_deletions() {
    // Advance frame. The queue at the new index is MAX_FRAMES_IN_FLIGHT old —
    // the GPU is guaranteed to be done with those resources.
    m_deletion_frame++;
    auto& queue = m_deletion_queues[m_deletion_frame % MAX_FRAMES_IN_FLIGHT];
    // Run all destructors
    for (auto& fn : queue) fn();
    queue.clear();
}

} // namespace helios::rhi
