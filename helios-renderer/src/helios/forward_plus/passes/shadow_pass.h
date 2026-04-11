#pragma once

#include "helios/graph/render_graph.h"
#include "helios/graph/frame_packet.h"
#include "helios/forward_plus/forward_plus_config.h"

#include <glm/glm.hpp>
#include <vector>

namespace helios {

struct ShadowPassOutput {
    graph::TextureHandle shadow_map;
    std::vector<glm::mat4> cascade_matrices;
};

/// Compute cascade split distances (geometric splits from camera planes).
std::vector<float> compute_cascade_splits(
    float near_plane, float far_plane, uint32_t cascade_count);

/// Compute light-space matrix for a single cascade frustum slice.
glm::mat4 compute_light_space_matrix(
    const renderer::CameraData& camera,
    float split_near, float split_far,
    const glm::vec3& light_dir);

/// Compute all cascade matrices given the split distances.
std::vector<glm::mat4> compute_cascade_matrices(
    const renderer::CameraData& camera,
    const glm::vec3& light_dir,
    const std::vector<float>& splits);

/// Adds the shadow pass to the render graph.
ShadowPassOutput add_shadow_pass(
    graph::RenderGraph& graph,
    const renderer::FramePacket& packet,
    const ForwardPlusConfig& config);

} // namespace helios
