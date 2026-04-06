#pragma once
#include <cstdint>

namespace Engine {

    // C++ function pointers passed to C# at initialization.
    // C# stores these and calls them via unmanaged function pointer delegates.
    // All parameters must be blittable (no managed types, no C++ objects).
    struct NativeEngineAPI
    {
        // Input (3)
        bool (*IsKeyPressed)(int keycode);
        bool (*IsMouseButtonPressed)(int button);
        void (*GetMousePosition)(float* outX, float* outY);

        // Entity (2)
        bool (*EntityHasComponent)(uint64_t entityId, const char* componentName);
        void (*EntityDestroy)(uint64_t entityId);

        // Transform (6)
        void (*TransformGetTranslation)(uint64_t entityId, float* outXYZ);
        void (*TransformSetTranslation)(uint64_t entityId, float* inXYZ);
        void (*TransformGetRotation)(uint64_t entityId, float* outXYZ);
        void (*TransformSetRotation)(uint64_t entityId, float* inXYZ);
        void (*TransformGetScale)(uint64_t entityId, float* outXYZ);
        void (*TransformSetScale)(uint64_t entityId, float* inXYZ);

        // Camera (8)
        void (*CameraRotateAroundEntity)(uint64_t entityId, float* rotation2, float speed, bool inverse);
        void (*CameraRotate)(uint64_t entityId, float* rotation2, float speed, bool inverse);
        void (*CameraGetForwardDirection)(uint64_t entityId, float* outXYZ);
        void (*CameraGetRightDirection)(uint64_t entityId, float* outXYZ);
        void (*CameraGetPosition)(uint64_t entityId, float* outXYZ);
        void (*CameraGetRotation)(uint64_t entityId, float* outXYZ);
        void (*CameraSetPosition)(uint64_t entityId, float* inXYZ);
        void (*CameraSetRotation)(uint64_t entityId, float* inXYZ);

        // Sounds (2)
        void (*SoundsPlayGlobal)(uint64_t entityId, int index);
        void (*SoundsPlayLocal)(uint64_t entityId, int index);

        // Box Collider (7)
        void (*BoxColliderGetLinearVelocity)(uint64_t entityId, float* outXYZ);
        void (*BoxColliderSetLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddAngularVelocity)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddImpulse)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddAngularImpulse)(uint64_t entityId, float* inXYZ);
        void (*BoxColliderAddLinearAngularImpulse)(uint64_t entityId, float* linear, float* angular);

        // Sphere Collider (7)
        void (*SphereColliderGetLinearVelocity)(uint64_t entityId, float* outXYZ);
        void (*SphereColliderSetLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddAngularVelocity)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddImpulse)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddAngularImpulse)(uint64_t entityId, float* inXYZ);
        void (*SphereColliderAddLinearAngularImpulse)(uint64_t entityId, float* linear, float* angular);

        // Character Controller (11)
        void (*CharControllerGetLinearVelocity)(uint64_t entityId, float* outXYZ);
        void (*CharControllerSetLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddLinearVelocity)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddAngularVelocity)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddImpulse)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddAngularImpulse)(uint64_t entityId, float* inXYZ);
        void (*CharControllerAddLinearAngularImpulse)(uint64_t entityId, float* linear, float* angular);
        bool (*CharControllerIsGrounded)(uint64_t entityId);
        void (*CharControllerGetRotation)(uint64_t entityId, float* outXYZ);
        void (*CharControllerSetRotation)(uint64_t entityId, float* inXYZ);
        void (*CharControllerRotate)(uint64_t entityId, float* inXYZ);
    };
}
