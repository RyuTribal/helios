// helios-rewrite/helios-renderer/tests/graph/test_frame_packet.cpp
//
// Tests for FramePacket -- no GPU required.

#include <gtest/gtest.h>
#include "helios/graph/frame_packet.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace helios::renderer;

// ---------------------------------------------------------------------------
// Default construction
// ---------------------------------------------------------------------------

TEST(FramePacket, DefaultConstruction) {
    FramePacket fp;

    // Vectors should be empty
    EXPECT_TRUE(fp.mesh_draws.empty());
    EXPECT_TRUE(fp.point_lights.empty());
    EXPECT_TRUE(fp.dir_lights.empty());
    EXPECT_TRUE(fp.spot_lights.empty());

    // Frame metadata defaults
    EXPECT_EQ(fp.frame_number, 0u);
    EXPECT_FLOAT_EQ(fp.time_elapsed, 0.0f);
    EXPECT_FLOAT_EQ(fp.delta_time, 0.0f);

    // Camera matrices should be identity
    glm::mat4 identity(1.0f);
    EXPECT_EQ(fp.camera.view, identity);
    EXPECT_EQ(fp.camera.projection, identity);

    // Viewport dimensions
    EXPECT_GE(fp.viewport_width, 1u);
    EXPECT_GE(fp.viewport_height, 1u);
}

// ---------------------------------------------------------------------------
// clear() resets everything
// ---------------------------------------------------------------------------

TEST(FramePacket, ClearResetsState) {
    FramePacket fp;

    // Populate
    fp.frame_number   = 42;
    fp.time_elapsed   = 3.14f;
    fp.delta_time     = 0.016f;

    MeshDraw md;
    md.mesh     = 99;
    md.material = 77;
    fp.mesh_draws.push_back(md);

    PointLightData pl;
    pl.intensity = 5.0f;
    fp.point_lights.push_back(pl);

    DirLightData dl;
    dl.intensity = 2.0f;
    fp.dir_lights.push_back(dl);

    SpotLightData sl;
    sl.intensity = 3.0f;
    fp.spot_lights.push_back(sl);

    glm::mat4 custom_view = glm::lookAt(
        glm::vec3(0.0f, 5.0f, 10.0f),
        glm::vec3(0.0f, 0.0f,  0.0f),
        glm::vec3(0.0f, 1.0f,  0.0f)
    );
    fp.camera.view = custom_view;

    // Clear
    fp.clear();

    EXPECT_EQ(fp.frame_number, 0u);
    EXPECT_FLOAT_EQ(fp.time_elapsed, 0.0f);
    EXPECT_FLOAT_EQ(fp.delta_time, 0.0f);

    EXPECT_TRUE(fp.mesh_draws.empty());
    EXPECT_TRUE(fp.point_lights.empty());
    EXPECT_TRUE(fp.dir_lights.empty());
    EXPECT_TRUE(fp.spot_lights.empty());

    glm::mat4 identity(1.0f);
    EXPECT_EQ(fp.camera.view, identity);
    EXPECT_EQ(fp.camera.projection, identity);
}

// ---------------------------------------------------------------------------
// Move semantics: moving leaves the source empty and target populated
// ---------------------------------------------------------------------------

TEST(FramePacket, MoveSemantics) {
    FramePacket src;
    src.frame_number = 7;
    src.delta_time   = 0.033f;

    MeshDraw md;
    md.mesh = 1001;
    src.mesh_draws.push_back(md);
    src.mesh_draws.push_back(md);

    FramePacket dst = std::move(src);

    EXPECT_EQ(dst.frame_number, 7u);
    EXPECT_FLOAT_EQ(dst.delta_time, 0.033f);
    EXPECT_EQ(dst.mesh_draws.size(), 2u);
    EXPECT_EQ(dst.mesh_draws[0].mesh, 1001u);

    // After move, source vectors should be empty (valid but unspecified state).
    // We don't assert on src values -- that's implementation-defined for std::vector.
}

// ---------------------------------------------------------------------------
// Add mesh draws and verify contents
// ---------------------------------------------------------------------------

TEST(FramePacket, AddMeshDraws) {
    FramePacket fp;

    MeshDraw d1;
    d1.mesh     = 10;
    d1.material = 20;
    d1.transform = glm::mat4(2.0f);

    MeshDraw d2;
    d2.mesh     = 30;
    d2.material = 40;
    d2.transform = glm::mat4(3.0f);

    fp.mesh_draws.push_back(d1);
    fp.mesh_draws.push_back(d2);

    ASSERT_EQ(fp.mesh_draws.size(), 2u);
    EXPECT_EQ(fp.mesh_draws[0].mesh,     10u);
    EXPECT_EQ(fp.mesh_draws[0].material, 20u);
    EXPECT_EQ(fp.mesh_draws[0].transform, glm::mat4(2.0f));
    EXPECT_EQ(fp.mesh_draws[1].mesh,     30u);
    EXPECT_EQ(fp.mesh_draws[1].material, 40u);
}

// ---------------------------------------------------------------------------
// Camera data roundtrip
// ---------------------------------------------------------------------------

TEST(FramePacket, CameraDataRoundtrip) {
    FramePacket fp;

    glm::mat4 view = glm::lookAt(
        glm::vec3(1.0f, 2.0f, 3.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 proj = glm::perspective(
        glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 500.0f
    );

    fp.camera.view       = view;
    fp.camera.projection = proj;
    fp.camera.position   = glm::vec3(1.0f, 2.0f, 3.0f);
    fp.camera.near_plane = 0.1f;
    fp.camera.far_plane  = 500.0f;

    EXPECT_EQ(fp.camera.view, view);
    EXPECT_EQ(fp.camera.projection, proj);
    EXPECT_EQ(fp.camera.position, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_FLOAT_EQ(fp.camera.near_plane, 0.1f);
    EXPECT_FLOAT_EQ(fp.camera.far_plane,  500.0f);
}

// ---------------------------------------------------------------------------
// Light data
// ---------------------------------------------------------------------------

TEST(FramePacket, PointLightData) {
    FramePacket fp;

    PointLightData pl;
    pl.position  = glm::vec3(1.0f, 2.0f, 3.0f);
    pl.color     = glm::vec3(1.0f, 0.5f, 0.0f);
    pl.intensity = 4.0f;
    pl.radius    = 20.0f;
    fp.point_lights.push_back(pl);

    ASSERT_EQ(fp.point_lights.size(), 1u);
    EXPECT_EQ(fp.point_lights[0].position, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_FLOAT_EQ(fp.point_lights[0].intensity, 4.0f);
    EXPECT_FLOAT_EQ(fp.point_lights[0].radius, 20.0f);
}

TEST(FramePacket, DirLightData) {
    FramePacket fp;

    DirLightData dl;
    dl.direction     = glm::vec3(0.0f, -1.0f, 0.0f);
    dl.intensity     = 1.5f;
    dl.cast_shadows  = true;
    fp.dir_lights.push_back(dl);

    ASSERT_EQ(fp.dir_lights.size(), 1u);
    EXPECT_EQ(fp.dir_lights[0].direction, glm::vec3(0.0f, -1.0f, 0.0f));
    EXPECT_FLOAT_EQ(fp.dir_lights[0].intensity, 1.5f);
    EXPECT_TRUE(fp.dir_lights[0].cast_shadows);
}

// ---------------------------------------------------------------------------
// Skybox data defaults
// ---------------------------------------------------------------------------

TEST(FramePacket, SkyboxDefaults) {
    FramePacket fp;
    EXPECT_EQ(fp.skybox.cubemap_texture, 0u);
    EXPECT_EQ(fp.skybox.irradiance_map,  0u);
    EXPECT_FLOAT_EQ(fp.skybox.intensity, 1.0f);
    EXPECT_FLOAT_EQ(fp.skybox.rotation,  0.0f);
}

// ---------------------------------------------------------------------------
// Frame metadata
// ---------------------------------------------------------------------------

TEST(FramePacket, FrameMetadata) {
    FramePacket fp;
    fp.frame_number    = 100;
    fp.time_elapsed    = 5.0f;
    fp.delta_time      = 0.016f;
    fp.viewport_width  = 1920;
    fp.viewport_height = 1080;

    EXPECT_EQ(fp.frame_number, 100u);
    EXPECT_FLOAT_EQ(fp.time_elapsed, 5.0f);
    EXPECT_FLOAT_EQ(fp.delta_time, 0.016f);
    EXPECT_EQ(fp.viewport_width, 1920u);
    EXPECT_EQ(fp.viewport_height, 1080u);
}
