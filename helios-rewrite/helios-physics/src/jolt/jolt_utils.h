// helios-physics/src/jolt/jolt_utils.h
#pragma once

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "interface/body_types.h"

namespace helios::physics::jolt {

// ---- glm <-> Jolt conversions ----

inline JPH::Vec3 to_jph(const glm::vec3& v) {
    return JPH::Vec3(v.x, v.y, v.z);
}

inline JPH::RVec3 to_jph_rvec(const glm::vec3& v) {
    return JPH::RVec3(v.x, v.y, v.z);
}

inline JPH::Quat to_jph(const glm::quat& q) {
    return JPH::Quat(q.x, q.y, q.z, q.w);
}

inline glm::vec3 to_glm(const JPH::Vec3& v) {
    return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

// RVec3 overload only needed for double-precision Jolt builds
#ifdef JPH_DOUBLE_PRECISION
inline glm::vec3 to_glm(const JPH::RVec3& v) {
    return glm::vec3(
        static_cast<float>(v.GetX()),
        static_cast<float>(v.GetY()),
        static_cast<float>(v.GetZ())
    );
}
#endif

inline glm::quat to_glm(const JPH::Quat& q) {
    return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

// ---- BodyType -> JPH::EMotionType ----

inline JPH::EMotionType to_jph_motion_type(BodyType type) {
    switch (type) {
        case BodyType::Static:    return JPH::EMotionType::Static;
        case BodyType::Dynamic:   return JPH::EMotionType::Dynamic;
        case BodyType::Kinematic: return JPH::EMotionType::Kinematic;
    }
    return JPH::EMotionType::Static;
}

// ---- Broad phase layers (adapted from existing Layers.h) ----

namespace layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING     = 1;
    static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

namespace broad_phase_layers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr uint32_t NUM_LAYERS = 2;
}

// Maps object layers to broad phase layers.
class BroadPhaseLayerMapping final : public JPH::BroadPhaseLayerInterface {
public:
    BroadPhaseLayerMapping() {
        m_object_to_broad_phase[layers::NON_MOVING] = broad_phase_layers::NON_MOVING;
        m_object_to_broad_phase[layers::MOVING]     = broad_phase_layers::MOVING;
    }

    JPH::uint GetNumBroadPhaseLayers() const override {
        return broad_phase_layers::NUM_LAYERS;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return m_object_to_broad_phase[layer];
    }

    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        switch ((JPH::BroadPhaseLayer::Type)layer) {
            case 0: return "NON_MOVING";
            case 1: return "MOVING";
            default: return "UNKNOWN";
        }
    }

private:
    JPH::BroadPhaseLayer m_object_to_broad_phase[layers::NUM_LAYERS];
};

// Determines if an object layer can collide with a broad phase layer.
class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broad) const override {
        switch (object) {
            case layers::NON_MOVING: return broad == broad_phase_layers::MOVING;
            case layers::MOVING:     return true;
            default:                 return false;
        }
    }
};

// Determines if two object layers can collide.
class ObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        switch (a) {
            case layers::NON_MOVING: return b == layers::MOVING;
            case layers::MOVING:     return true;
            default:                 return false;
        }
    }
};

// Returns the object layer for a given body type.
inline JPH::ObjectLayer object_layer_for(BodyType type) {
    return (type == BodyType::Static) ? layers::NON_MOVING : layers::MOVING;
}

} // namespace helios::physics::jolt
