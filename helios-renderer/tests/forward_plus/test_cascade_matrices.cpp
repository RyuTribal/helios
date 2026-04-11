// helios-renderer/tests/forward_plus/test_cascade_matrices.cpp
//
// Tests for cascade shadow map split computation and matrix generation.
// No GPU required -- purely CPU math.

#include <gtest/gtest.h>

#include "helios/forward_plus/passes/shadow_pass.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace helios;
using namespace helios::renderer;

// ===========================================================================
// Cascade splits: basic counts
// ===========================================================================

TEST(CascadeMatrices, SplitsFor4Cascades) {
    auto splits = compute_cascade_splits(0.1f, 500.0f, 4);
    // 4 cascades = 3 split points (between cascades)
    EXPECT_EQ(splits.size(), 3u);
}

TEST(CascadeMatrices, SplitsFor2Cascades) {
    auto splits = compute_cascade_splits(0.1f, 500.0f, 2);
    // 2 cascades = 1 split point
    EXPECT_EQ(splits.size(), 1u);
}

TEST(CascadeMatrices, SplitsFor1Cascade) {
    auto splits = compute_cascade_splits(0.1f, 500.0f, 1);
    // 1 cascade = 0 split points
    EXPECT_EQ(splits.size(), 0u);
}

// ===========================================================================
// Cascade splits: values are increasing
// ===========================================================================

TEST(CascadeMatrices, SplitsAreIncreasing) {
    auto splits = compute_cascade_splits(0.1f, 500.0f, 4);
    for (size_t i = 1; i < splits.size(); ++i) {
        EXPECT_GT(splits[i], splits[i - 1])
            << "Split " << i << " should be greater than split " << (i - 1);
    }
}

// ===========================================================================
// Cascade splits: all values between near and far
// ===========================================================================

TEST(CascadeMatrices, SplitsWithinRange) {
    float near_plane = 0.1f;
    float far_plane  = 500.0f;
    auto splits = compute_cascade_splits(near_plane, far_plane, 4);
    for (size_t i = 0; i < splits.size(); ++i) {
        EXPECT_GT(splits[i], 0.0f)
            << "Split " << i << " should be positive";
        EXPECT_LT(splits[i], far_plane)
            << "Split " << i << " should be less than far plane";
    }
}

// ===========================================================================
// Cascade matrices: correct count
// ===========================================================================

TEST(CascadeMatrices, MatricesCorrectCount) {
    CameraData camera{
        .view       = glm::lookAt(glm::vec3(0, 0, 5),
                                  glm::vec3(0, 0, 0),
                                  glm::vec3(0, 1, 0)),
        .projection = glm::perspective(
                          glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 500.0f),
        .position     = {0.0f, 0.0f, 5.0f},
        .near_plane   = 0.1f,
        .far_plane    = 500.0f,
        .fov_y        = 45.0f,
        .aspect_ratio = 16.0f / 9.0f,
    };

    auto splits = compute_cascade_splits(0.1f, 500.0f, 4);
    auto matrices = compute_cascade_matrices(camera, glm::vec3(0, -1, 0), splits);

    // 4 cascades = 4 matrices (one per cascade)
    EXPECT_EQ(matrices.size(), 4u);
}

// ===========================================================================
// Cascade matrices: each matrix has non-zero determinant (valid projection)
// ===========================================================================

TEST(CascadeMatrices, MatricesAreValid) {
    CameraData camera{
        .view       = glm::lookAt(glm::vec3(0, 0, 5),
                                  glm::vec3(0, 0, 0),
                                  glm::vec3(0, 1, 0)),
        .projection = glm::perspective(
                          glm::radians(45.0f), 16.0f / 9.0f, 0.1f, 500.0f),
        .position     = {0.0f, 0.0f, 5.0f},
        .near_plane   = 0.1f,
        .far_plane    = 500.0f,
        .fov_y        = 45.0f,
        .aspect_ratio = 16.0f / 9.0f,
    };

    auto splits = compute_cascade_splits(0.1f, 500.0f, 4);
    auto matrices = compute_cascade_matrices(camera, glm::vec3(0, -1, 0), splits);

    for (size_t i = 0; i < matrices.size(); ++i) {
        EXPECT_NE(glm::determinant(matrices[i]), 0.0f)
            << "Cascade matrix " << i << " should have non-zero determinant";
    }
}

// ===========================================================================
// Single light-space matrix: non-degenerate
// ===========================================================================

TEST(CascadeMatrices, SingleLightSpaceMatrix) {
    CameraData camera{
        .view       = glm::lookAt(glm::vec3(0, 0, 5),
                                  glm::vec3(0, 0, 0),
                                  glm::vec3(0, 1, 0)),
        .projection = glm::perspective(
                          glm::radians(60.0f), 1.0f, 0.1f, 100.0f),
        .position     = {0.0f, 0.0f, 5.0f},
        .near_plane   = 0.1f,
        .far_plane    = 100.0f,
        .fov_y        = 60.0f,
        .aspect_ratio = 1.0f,
    };

    glm::mat4 m = compute_light_space_matrix(camera, 0.1f, 50.0f,
                                              glm::vec3(0, -1, 0));
    EXPECT_NE(glm::determinant(m), 0.0f)
        << "Light-space matrix should be non-degenerate";
}
