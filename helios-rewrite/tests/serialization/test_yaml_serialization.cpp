#include <gtest/gtest.h>

#include "helios/serialization/yaml_serializer.h"
#include "helios/serialization/scene_serializer.h"
#include "helios/components/components.h"
#include "helios/ecs/world.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <sstream>
#include <string>

namespace helios::test {

// Helper: serialize a value to a YAML string then parse it back.
static std::string emit_to_string(std::function<void(YAML::Emitter&)> fn) {
    YAML::Emitter out;
    fn(out);
    return std::string(out.c_str());
}

// ============================================================================
// Scalar round-trips
// ============================================================================

TEST(YamlSerializerTest, FloatRoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, 3.14f);
    });
    YAML::Node node = YAML::Load(yaml);
    EXPECT_FLOAT_EQ(node.as<float>(), 3.14f);
}

TEST(YamlSerializerTest, IntRoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, -42);
    });
    YAML::Node node = YAML::Load(yaml);
    EXPECT_EQ(node.as<int>(), -42);
}

TEST(YamlSerializerTest, Uint32RoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, uint32_t{99});
    });
    YAML::Node node = YAML::Load(yaml);
    EXPECT_EQ(node.as<uint32_t>(), 99u);
}

TEST(YamlSerializerTest, BoolRoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, true);
    });
    YAML::Node node = YAML::Load(yaml);
    EXPECT_TRUE(node.as<bool>());
}

TEST(YamlSerializerTest, StringRoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, std::string("hello world"));
    });
    YAML::Node node = YAML::Load(yaml);
    EXPECT_EQ(node.as<std::string>(), "hello world");
}

// ============================================================================
// Enum round-trips
// ============================================================================

TEST(YamlSerializerTest, AssetStatusRoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, AssetStatus::Loaded);
    });
    YAML::Node node = YAML::Load(yaml);
    EXPECT_EQ(deserialize_yaml_asset_status(node), AssetStatus::Loaded);
}

TEST(YamlSerializerTest, ProjectionTypeRoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, ProjectionType::Orthographic);
    });
    YAML::Node node = YAML::Load(yaml);
    EXPECT_EQ(deserialize_yaml_projection_type(node), ProjectionType::Orthographic);
}

TEST(YamlSerializerTest, BodyTypeRoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, BodyType::Kinematic);
    });
    YAML::Node node = YAML::Load(yaml);
    EXPECT_EQ(deserialize_yaml_body_type(node), BodyType::Kinematic);
}

// ============================================================================
// AssetHandle round-trip
// ============================================================================

TEST(YamlSerializerTest, AssetHandleRoundTrip) {
    AssetHandle original{42, 7};
    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_asset_handle(node);
    EXPECT_EQ(result.index, 42u);
    EXPECT_EQ(result.generation, 7u);
}

TEST(YamlSerializerTest, NullAssetHandleRoundTrip) {
    AssetHandle original{};
    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_asset_handle(node);
    EXPECT_EQ(result.index, 0u);
    EXPECT_EQ(result.generation, 0u);
}

// ============================================================================
// glm type round-trips
// ============================================================================

TEST(YamlSerializerTest, Vec2RoundTrip) {
    glm::vec2 original{1.5f, -2.3f};
    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_vec2(node);
    EXPECT_FLOAT_EQ(result.x, 1.5f);
    EXPECT_FLOAT_EQ(result.y, -2.3f);
}

TEST(YamlSerializerTest, Vec3RoundTrip) {
    glm::vec3 original{1.0f, 2.0f, 3.0f};
    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_vec3(node);
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    EXPECT_FLOAT_EQ(result.y, 2.0f);
    EXPECT_FLOAT_EQ(result.z, 3.0f);
}

TEST(YamlSerializerTest, Vec4RoundTrip) {
    glm::vec4 original{1.0f, 2.0f, 3.0f, 4.0f};
    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_vec4(node);
    EXPECT_FLOAT_EQ(result.x, 1.0f);
    EXPECT_FLOAT_EQ(result.y, 2.0f);
    EXPECT_FLOAT_EQ(result.z, 3.0f);
    EXPECT_FLOAT_EQ(result.w, 4.0f);
}

TEST(YamlSerializerTest, QuatRoundTrip) {
    glm::quat original{0.707f, 0.0f, 0.707f, 0.0f};
    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_quat(node);
    EXPECT_FLOAT_EQ(result.w, 0.707f);
    EXPECT_FLOAT_EQ(result.x, 0.0f);
    EXPECT_FLOAT_EQ(result.y, 0.707f);
    EXPECT_FLOAT_EQ(result.z, 0.0f);
}

TEST(YamlSerializerTest, Mat4RoundTrip) {
    glm::mat4 original(1.0f);
    original[0][0] = 2.0f;
    original[1][2] = 3.5f;
    original[3][3] = 7.0f;

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_mat4(node);

    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            EXPECT_FLOAT_EQ(result[col][row], original[col][row])
                << "Mismatch at [" << col << "][" << row << "]";
        }
    }
}

// ============================================================================
// Component struct round-trips
// ============================================================================

TEST(YamlSerializerTest, TransformRoundTrip) {
    Transform original;
    original.position = glm::vec3(1.0f, 2.0f, 3.0f);
    original.rotation = glm::quat(0.707f, 0.0f, 0.707f, 0.0f);
    original.scale    = glm::vec3(2.0f, 2.0f, 2.0f);

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_transform(node);

    EXPECT_FLOAT_EQ(result.position.x, 1.0f);
    EXPECT_FLOAT_EQ(result.position.y, 2.0f);
    EXPECT_FLOAT_EQ(result.position.z, 3.0f);
    EXPECT_FLOAT_EQ(result.rotation.w, 0.707f);
    EXPECT_FLOAT_EQ(result.scale.x, 2.0f);
}

TEST(YamlSerializerTest, CameraRoundTrip) {
    Camera original;
    original.projection = ProjectionType::Perspective;
    original.fov_degrees = 90.0f;
    original.near_plane  = 0.01f;
    original.far_plane   = 500.0f;
    original.ortho_size  = 20.0f;

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_camera(node);

    EXPECT_EQ(result.projection, ProjectionType::Perspective);
    EXPECT_FLOAT_EQ(result.fov_degrees, 90.0f);
    EXPECT_FLOAT_EQ(result.near_plane, 0.01f);
    EXPECT_FLOAT_EQ(result.far_plane, 500.0f);
    EXPECT_FLOAT_EQ(result.ortho_size, 20.0f);
}

TEST(YamlSerializerTest, MeshRendererRoundTrip) {
    MeshRenderer original;
    original.mesh     = Handle<MeshAsset>{1, 2};
    original.material = Handle<MaterialAsset>{3, 4};
    original.flags    = MeshFlags::CastShadows;

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_mesh_renderer(node);

    EXPECT_EQ(result.mesh.index, 1u);
    EXPECT_EQ(result.mesh.generation, 2u);
    EXPECT_EQ(result.material.index, 3u);
    EXPECT_EQ(result.material.generation, 4u);
    EXPECT_EQ(result.flags, MeshFlags::CastShadows);
}

TEST(YamlSerializerTest, PointLightRoundTrip) {
    PointLight original;
    original.color     = glm::vec3(1.0f, 0.5f, 0.0f);
    original.intensity = 2.5f;
    original.radius    = 15.0f;

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_point_light(node);

    EXPECT_FLOAT_EQ(result.color.r, 1.0f);
    EXPECT_FLOAT_EQ(result.color.g, 0.5f);
    EXPECT_FLOAT_EQ(result.intensity, 2.5f);
    EXPECT_FLOAT_EQ(result.radius, 15.0f);
}

TEST(YamlSerializerTest, DirectionalLightRoundTrip) {
    DirectionalLight original;
    original.color     = glm::vec3(0.9f, 0.9f, 1.0f);
    original.intensity = 1.5f;

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_directional_light(node);

    EXPECT_FLOAT_EQ(result.color.r, 0.9f);
    EXPECT_FLOAT_EQ(result.intensity, 1.5f);
}

TEST(YamlSerializerTest, ActiveCameraRoundTrip) {
    auto yaml = emit_to_string([](YAML::Emitter& out) {
        serialize_yaml(out, ActiveCamera{});
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_active_camera(node);
    (void)result; // just verify no crash
}

TEST(YamlSerializerTest, RigidBodyRoundTrip) {
    RigidBody original;
    original.body_type = BodyType::Kinematic;
    original.mass      = 5.0f;
    original.flags     = RigidBodyFlags::UseGravity;

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_rigid_body(node);

    EXPECT_EQ(result.body_type, BodyType::Kinematic);
    EXPECT_FLOAT_EQ(result.mass, 5.0f);
    EXPECT_EQ(result.flags, RigidBodyFlags::UseGravity);
}

TEST(YamlSerializerTest, BoxColliderRoundTrip) {
    BoxCollider original;
    original.half_extents = glm::vec3(1.0f, 2.0f, 3.0f);
    original.offset       = glm::vec3(0.5f, 0.0f, -0.5f);
    original.flags        = ColliderFlags::IsTrigger;

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_box_collider(node);

    EXPECT_FLOAT_EQ(result.half_extents.x, 1.0f);
    EXPECT_FLOAT_EQ(result.offset.x, 0.5f);
    EXPECT_EQ(result.flags, ColliderFlags::IsTrigger);
}

TEST(YamlSerializerTest, SphereColliderRoundTrip) {
    SphereCollider original;
    original.radius = 2.5f;
    original.offset = glm::vec3(0.0f, 1.0f, 0.0f);

    auto yaml = emit_to_string([&](YAML::Emitter& out) {
        serialize_yaml(out, original);
    });
    YAML::Node node = YAML::Load(yaml);
    auto result = deserialize_yaml_sphere_collider(node);

    EXPECT_FLOAT_EQ(result.radius, 2.5f);
    EXPECT_FLOAT_EQ(result.offset.y, 1.0f);
}

// ============================================================================
// Scene save/load round-trip
// ============================================================================

// Helper: register all standard components with a SceneSerializer.
static void register_all_components(SceneSerializer& serializer) {
    serializer.register_component<Transform>(
        "Transform",
        [](YAML::Emitter& out, const Transform& t) { serialize_yaml(out, t); },
        [](const YAML::Node& n) { return deserialize_yaml_transform(n); }
    );
    serializer.register_component<Camera>(
        "Camera",
        [](YAML::Emitter& out, const Camera& c) { serialize_yaml(out, c); },
        [](const YAML::Node& n) { return deserialize_yaml_camera(n); }
    );
    serializer.register_component<ActiveCamera>(
        "ActiveCamera",
        [](YAML::Emitter& out, const ActiveCamera& c) { serialize_yaml(out, c); },
        [](const YAML::Node& n) { return deserialize_yaml_active_camera(n); }
    );
    serializer.register_component<MeshRenderer>(
        "MeshRenderer",
        [](YAML::Emitter& out, const MeshRenderer& m) { serialize_yaml(out, m); },
        [](const YAML::Node& n) { return deserialize_yaml_mesh_renderer(n); }
    );
    serializer.register_component<PointLight>(
        "PointLight",
        [](YAML::Emitter& out, const PointLight& l) { serialize_yaml(out, l); },
        [](const YAML::Node& n) { return deserialize_yaml_point_light(n); }
    );
    serializer.register_component<DirectionalLight>(
        "DirectionalLight",
        [](YAML::Emitter& out, const DirectionalLight& l) { serialize_yaml(out, l); },
        [](const YAML::Node& n) { return deserialize_yaml_directional_light(n); }
    );
    serializer.register_component<Tag>(
        "Tag",
        [](YAML::Emitter& out, const Tag& t) { serialize_yaml(out, t); },
        [](const YAML::Node& n) { return deserialize_yaml_tag(n); }
    );
}

TEST(SceneSerializerTest, SaveAndLoadRoundTrip) {
    auto tmp_path = std::filesystem::temp_directory_path() / "helios_test_scene.yaml";

    // --- Save ---
    {
        World world;

        world.spawn(
            Tag{"main_camera"},
            Transform{glm::vec3(0, 5, 10),
                      glm::quat(1, 0, 0, 0),
                      glm::vec3(1, 1, 1)},
            Camera{ProjectionType::Perspective, 60.0f, 0.1f, 100.0f, 10.0f},
            ActiveCamera{}
        );

        world.spawn(
            Tag{"sun"},
            Transform{glm::vec3(0, 100, 0),
                      glm::quat(1, 0, 0, 0),
                      glm::vec3(1, 1, 1)},
            DirectionalLight{glm::vec3(1, 1, 1), 1.0f}
        );

        world.spawn(
            Tag{"player"},
            Transform{glm::vec3(0, 0, 0),
                      glm::quat(1, 0, 0, 0),
                      glm::vec3(1, 1, 1)},
            MeshRenderer{Handle<MeshAsset>{1, 1}, Handle<MaterialAsset>{2, 1},
                         MeshFlags::CastShadows | MeshFlags::ReceiveShadows}
        );

        SceneSerializer serializer;
        register_all_components(serializer);
        serializer.save(world, tmp_path);
    }

    // Verify file exists
    ASSERT_TRUE(std::filesystem::exists(tmp_path));

    // --- Load ---
    {
        World world;

        SceneSerializer serializer;
        register_all_components(serializer);
        serializer.load(world, tmp_path);

        // Check that entities were loaded by querying for tags.
        int entity_count = 0;
        bool found_camera = false;
        bool found_sun = false;
        bool found_player = false;

        world.archetypes().for_each_archetype([&](Archetype& arch) {
            if (!arch.has_component(component_id<Tag>())) return;
            auto& tag_col = arch.get_column<Tag>();
            for (size_t row = 0; row < arch.size(); ++row) {
                entity_count++;
                auto& tag = tag_col.get<Tag>(row);
                if (tag.name == "main_camera") {
                    found_camera = true;
                    // Verify the camera's transform
                    ASSERT_TRUE(arch.has_component(component_id<Transform>()));
                    auto& t = arch.get<Transform>(row);
                    EXPECT_FLOAT_EQ(t.position.y, 5.0f);
                    // Verify camera component
                    ASSERT_TRUE(arch.has_component(component_id<Camera>()));
                    auto& cam = arch.get<Camera>(row);
                    EXPECT_FLOAT_EQ(cam.fov_degrees, 60.0f);
                }
                if (tag.name == "sun") {
                    found_sun = true;
                    ASSERT_TRUE(arch.has_component(component_id<DirectionalLight>()));
                    auto& dl = arch.get<DirectionalLight>(row);
                    EXPECT_FLOAT_EQ(dl.intensity, 1.0f);
                }
                if (tag.name == "player") {
                    found_player = true;
                    ASSERT_TRUE(arch.has_component(component_id<MeshRenderer>()));
                    auto& mr = arch.get<MeshRenderer>(row);
                    EXPECT_EQ(mr.mesh.index, 1u);
                    EXPECT_EQ(mr.material.index, 2u);
                }
            }
        });

        EXPECT_EQ(entity_count, 3);
        EXPECT_TRUE(found_camera);
        EXPECT_TRUE(found_sun);
        EXPECT_TRUE(found_player);
    }

    std::filesystem::remove(tmp_path);
}

TEST(SceneSerializerTest, LoadMissingFileThrows) {
    World world;
    SceneSerializer serializer;
    EXPECT_THROW(serializer.load(world, "/nonexistent/scene.yaml"),
                 std::exception);
}

} // namespace helios::test
