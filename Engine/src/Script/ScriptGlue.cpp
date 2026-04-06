#include "pch.h"
#include "ScriptGlue.h"
#include "Core/UUID.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Script/ScriptEngine.h"
#include "Physics/PhysicsEngine.h"

namespace Engine {

    static std::unordered_map<std::string, std::function<bool(Entity*)>> s_HasComponentFuncs;

    static std::pair<Scene*, Entity*> GetSceneAndEntity(UUID entity_id)
    {
        Scene* scene = ScriptEngine::GetSceneContext();
        HVE_CORE_ASSERT(scene);
        Entity* entity = scene->GetEntity(entity_id);
        HVE_CORE_ASSERT(entity);
        return { scene, entity };
    }

    // ── Component check map ────────────────────────────────────────────

    static void InitComponentChecks()
    {
        s_HasComponentFuncs["TransformComponent"]           = [](Entity* e) { return e->HasComponent<TransformComponent>(); };
        s_HasComponentFuncs["CameraComponent"]              = [](Entity* e) { return e->HasComponent<CameraComponent>(); };
        s_HasComponentFuncs["MeshComponent"]                = [](Entity* e) { return e->HasComponent<MeshComponent>(); };
        s_HasComponentFuncs["PointLightComponent"]          = [](Entity* e) { return e->HasComponent<PointLightComponent>(); };
        s_HasComponentFuncs["DirectionalLightComponent"]    = [](Entity* e) { return e->HasComponent<DirectionalLightComponent>(); };
        s_HasComponentFuncs["GlobalSoundsComponent"]        = [](Entity* e) { return e->HasComponent<GlobalSoundsComponent>(); };
        s_HasComponentFuncs["LocalSoundsComponent"]         = [](Entity* e) { return e->HasComponent<LocalSoundsComponent>(); };
        s_HasComponentFuncs["ScriptComponent"]              = [](Entity* e) { return e->HasComponent<ScriptComponent>(); };
        s_HasComponentFuncs["BoxColliderComponent"]         = [](Entity* e) { return e->HasComponent<BoxColliderComponent>(); };
        s_HasComponentFuncs["SphereColliderComponent"]      = [](Entity* e) { return e->HasComponent<SphereColliderComponent>(); };
        s_HasComponentFuncs["CharacterControllerComponent"] = [](Entity* e) { return e->HasComponent<CharacterControllerComponent>(); };
        s_HasComponentFuncs["IDComponent"]                  = [](Entity* e) { return e->HasComponent<IDComponent>(); };
        s_HasComponentFuncs["TagComponent"]                 = [](Entity* e) { return e->HasComponent<TagComponent>(); };
        s_HasComponentFuncs["ParentIDComponent"]            = [](Entity* e) { return e->HasComponent<ParentIDComponent>(); };
    }

    // ── Input (3) ──────────────────────────────────────────────────────

    static bool IsKeyPressed(int key)
    {
        return Input::IsKeyPressed(key);
    }

    static bool IsMouseButtonPressed(int button)
    {
        return Input::IsMouseButtonPressed(button);
    }

    static void GetMousePosition(float* outX, float* outY)
    {
        *outX = Input::GetMouseX();
        *outY = Input::GetMouseY();
    }

    // ── Entity (2) ─────────────────────────────────────────────────────

    static bool EntityHasComponent(uint64_t entityId, const char* componentName)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto it = s_HasComponentFuncs.find(componentName);
        if (it != s_HasComponentFuncs.end())
            return it->second(entity);
        HVE_CORE_WARN_TAG("ScriptGlue", "Unknown component type: {}", componentName);
        return false;
    }

    static void EntityDestroy(uint64_t entityId)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        scene->DestroyEntity(entityId);
    }

    // ── Transform (6) ──────────────────────────────────────────────────

    static void TransformGetTranslation(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto& t = entity->GetComponent<TransformComponent>()->world_transform.translation;
        outXYZ[0] = t.x;
        outXYZ[1] = t.y;
        outXYZ[2] = t.z;
    }

    static void TransformSetTranslation(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        entity->GetComponent<TransformComponent>()->world_transform.translation = glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
    }

    static void TransformGetRotation(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto& r = entity->GetComponent<TransformComponent>()->world_transform.rotation;
        outXYZ[0] = r.x;
        outXYZ[1] = r.y;
        outXYZ[2] = r.z;
    }

    static void TransformSetRotation(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        entity->GetComponent<TransformComponent>()->world_transform.rotation = glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
    }

    static void TransformGetScale(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto& s = entity->GetComponent<TransformComponent>()->world_transform.scale;
        outXYZ[0] = s.x;
        outXYZ[1] = s.y;
        outXYZ[2] = s.z;
    }

    static void TransformSetScale(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        entity->GetComponent<TransformComponent>()->world_transform.scale = glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
    }

    // ── Camera (8) ─────────────────────────────────────────────────────

    static void CameraRotateAroundEntity(uint64_t entityId, float* rotation2, float speed, bool inverse)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto camera_comp = entity->GetComponent<CameraComponent>();
        if (camera_comp)
        {
            glm::vec2 rot(rotation2[0], rotation2[1]);
            camera_comp->camera.RotateAroundFocalPoint(rot, speed, inverse);
        }
    }

    static void CameraRotate(uint64_t entityId, float* rotation2, float speed, bool inverse)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto camera_comp = entity->GetComponent<CameraComponent>();
        if (camera_comp)
        {
            glm::vec2 rot(rotation2[0], rotation2[1]);
            camera_comp->camera.Rotate(rot, speed, inverse);
        }
    }

    static void CameraGetForwardDirection(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto camera_comp = entity->GetComponent<CameraComponent>();
        if (camera_comp)
        {
            glm::vec3 fwd = camera_comp->camera.GetForwardDirection();
            outXYZ[0] = fwd.x;
            outXYZ[1] = fwd.y;
            outXYZ[2] = fwd.z;
        }
    }

    static void CameraGetRightDirection(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto camera_comp = entity->GetComponent<CameraComponent>();
        if (camera_comp)
        {
            glm::vec3 right = camera_comp->camera.GetRightDirection();
            outXYZ[0] = right.x;
            outXYZ[1] = right.y;
            outXYZ[2] = right.z;
        }
    }

    static void CameraGetPosition(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto camera_comp = entity->GetComponent<CameraComponent>();
        if (camera_comp)
        {
            glm::vec3 pos = camera_comp->camera.GetPosition();
            outXYZ[0] = pos.x;
            outXYZ[1] = pos.y;
            outXYZ[2] = pos.z;
        }
    }

    static void CameraGetRotation(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto camera_comp = entity->GetComponent<CameraComponent>();
        if (camera_comp)
        {
            glm::vec3 rot = glm::eulerAngles(camera_comp->camera.GetOrientation());
            outXYZ[0] = rot.x;
            outXYZ[1] = rot.y;
            outXYZ[2] = rot.z;
        }
    }

    static void CameraSetPosition(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto camera_comp = entity->GetComponent<CameraComponent>();
        if (camera_comp)
        {
            camera_comp->camera.SetPosition(glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void CameraSetRotation(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto camera_comp = entity->GetComponent<CameraComponent>();
        if (camera_comp)
        {
            camera_comp->camera.SetRotation(glm::vec2(inXYZ[0], inXYZ[1]));
        }
    }

    // ── Sounds (2) ─────────────────────────────────────────────────────

    static void SoundsPlayGlobal(uint64_t entityId, int index)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sounds_library = entity->GetComponent<GlobalSoundsComponent>();
        if (sounds_library && index <= sounds_library->Sounds.size() - 1)
        {
            sounds_library->Sounds.at(index)->PlaySound(false);
        }
    }

    static void SoundsPlayLocal(uint64_t entityId, int index)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sounds_library = entity->GetComponent<LocalSoundsComponent>();
        if (sounds_library && index <= sounds_library->Sounds.size() - 1)
        {
            sounds_library->Sounds.at(index)->PlaySound(scene->GetCurrentCamera()->CalculatePosition(), false);
        }
    }

    // ── Box Collider (7) ───────────────────────────────────────────────

    static void BoxColliderGetLinearVelocity(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto box_collider = entity->GetComponent<BoxColliderComponent>();
        if (box_collider)
        {
            glm::vec3 vel = PhysicsEngine::Get()->GetCurrentScene()->GetLinearVelocity(entityId);
            outXYZ[0] = vel.x;
            outXYZ[1] = vel.y;
            outXYZ[2] = vel.z;
        }
    }

    static void BoxColliderSetLinearVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto box_collider = entity->GetComponent<BoxColliderComponent>();
        if (box_collider)
        {
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void BoxColliderAddLinearVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto box_collider = entity->GetComponent<BoxColliderComponent>();
        if (box_collider)
        {
            glm::vec3 curr = PhysicsEngine::Get()->GetCurrentScene()->GetLinearVelocity(entityId);
            curr += glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, curr);
        }
    }

    static void BoxColliderAddAngularVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto box_collider = entity->GetComponent<BoxColliderComponent>();
        if (box_collider)
        {
            glm::vec3 curr = PhysicsEngine::Get()->GetCurrentScene()->GetAngularVelocity(entityId);
            curr += glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, curr);
        }
    }

    static void BoxColliderAddImpulse(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto box_collider = entity->GetComponent<BoxColliderComponent>();
        if (box_collider)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddLinearImpulse(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void BoxColliderAddAngularImpulse(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto box_collider = entity->GetComponent<BoxColliderComponent>();
        if (box_collider)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddAngularImpulse(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void BoxColliderAddLinearAngularImpulse(uint64_t entityId, float* linear, float* angular)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto box_collider = entity->GetComponent<BoxColliderComponent>();
        if (box_collider)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddLinearAndAngularImpulse(
                entityId,
                glm::vec3(linear[0], linear[1], linear[2]),
                glm::vec3(angular[0], angular[1], angular[2]));
        }
    }

    // ── Sphere Collider (7) ────────────────────────────────────────────

    static void SphereColliderGetLinearVelocity(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sphere_collider = entity->GetComponent<SphereColliderComponent>();
        if (sphere_collider)
        {
            glm::vec3 vel = PhysicsEngine::Get()->GetCurrentScene()->GetLinearVelocity(entityId);
            outXYZ[0] = vel.x;
            outXYZ[1] = vel.y;
            outXYZ[2] = vel.z;
        }
    }

    static void SphereColliderSetLinearVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sphere_collider = entity->GetComponent<SphereColliderComponent>();
        if (sphere_collider)
        {
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void SphereColliderAddLinearVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sphere_collider = entity->GetComponent<SphereColliderComponent>();
        if (sphere_collider)
        {
            glm::vec3 curr = PhysicsEngine::Get()->GetCurrentScene()->GetLinearVelocity(entityId);
            curr += glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, curr);
        }
    }

    static void SphereColliderAddAngularVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sphere_collider = entity->GetComponent<SphereColliderComponent>();
        if (sphere_collider)
        {
            glm::vec3 curr = PhysicsEngine::Get()->GetCurrentScene()->GetAngularVelocity(entityId);
            curr += glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, curr);
        }
    }

    static void SphereColliderAddImpulse(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sphere_collider = entity->GetComponent<SphereColliderComponent>();
        if (sphere_collider)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddLinearImpulse(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void SphereColliderAddAngularImpulse(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sphere_collider = entity->GetComponent<SphereColliderComponent>();
        if (sphere_collider)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddAngularImpulse(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void SphereColliderAddLinearAngularImpulse(uint64_t entityId, float* linear, float* angular)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto sphere_collider = entity->GetComponent<SphereColliderComponent>();
        if (sphere_collider)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddLinearAndAngularImpulse(
                entityId,
                glm::vec3(linear[0], linear[1], linear[2]),
                glm::vec3(angular[0], angular[1], angular[2]));
        }
    }

    // ── Character Controller (11) ──────────────────────────────────────

    static void CharControllerGetLinearVelocity(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            glm::vec3 vel = PhysicsEngine::Get()->GetCurrentScene()->GetLinearVelocity(entityId);
            outXYZ[0] = vel.x;
            outXYZ[1] = vel.y;
            outXYZ[2] = vel.z;
        }
    }

    static void CharControllerSetLinearVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void CharControllerAddLinearVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            glm::vec3 curr = PhysicsEngine::Get()->GetCurrentScene()->GetLinearVelocity(entityId);
            curr += glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, curr);
        }
    }

    static void CharControllerAddAngularVelocity(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            glm::vec3 curr = PhysicsEngine::Get()->GetCurrentScene()->GetAngularVelocity(entityId);
            curr += glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]);
            PhysicsEngine::Get()->GetCurrentScene()->SetLinearVelocity(entityId, curr);
        }
    }

    static void CharControllerAddImpulse(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddLinearImpulse(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void CharControllerAddAngularImpulse(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddAngularImpulse(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void CharControllerAddLinearAngularImpulse(uint64_t entityId, float* linear, float* angular)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            PhysicsEngine::Get()->GetCurrentScene()->AddLinearAndAngularImpulse(
                entityId,
                glm::vec3(linear[0], linear[1], linear[2]),
                glm::vec3(angular[0], angular[1], angular[2]));
        }
    }

    static bool CharControllerIsGrounded(uint64_t entityId)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            return PhysicsEngine::Get()->GetCurrentScene()->IsCharacterGrounded(entityId);
        }
        return false;
    }

    static void CharControllerGetRotation(uint64_t entityId, float* outXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            glm::vec3 rot = PhysicsEngine::Get()->GetCurrentScene()->GetRotation(entityId);
            outXYZ[0] = rot.x;
            outXYZ[1] = rot.y;
            outXYZ[2] = rot.z;
        }
    }

    static void CharControllerSetRotation(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            PhysicsEngine::Get()->GetCurrentScene()->SetRotation(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    static void CharControllerRotate(uint64_t entityId, float* inXYZ)
    {
        auto [scene, entity] = GetSceneAndEntity(entityId);
        auto cc = entity->GetComponent<CharacterControllerComponent>();
        if (cc)
        {
            PhysicsEngine::Get()->GetCurrentScene()->Rotate(entityId, glm::vec3(inXYZ[0], inXYZ[1], inXYZ[2]));
        }
    }

    // ── Fill the API struct ────────────────────────────────────────────

    void ScriptGlue::FillNativeAPI(NativeEngineAPI& api)
    {
        InitComponentChecks();

        // Input
        api.IsKeyPressed        = IsKeyPressed;
        api.IsMouseButtonPressed = IsMouseButtonPressed;
        api.GetMousePosition    = GetMousePosition;

        // Entity
        api.EntityHasComponent  = EntityHasComponent;
        api.EntityDestroy       = EntityDestroy;

        // Transform
        api.TransformGetTranslation = TransformGetTranslation;
        api.TransformSetTranslation = TransformSetTranslation;
        api.TransformGetRotation    = TransformGetRotation;
        api.TransformSetRotation    = TransformSetRotation;
        api.TransformGetScale       = TransformGetScale;
        api.TransformSetScale       = TransformSetScale;

        // Camera
        api.CameraRotateAroundEntity = CameraRotateAroundEntity;
        api.CameraRotate             = CameraRotate;
        api.CameraGetForwardDirection = CameraGetForwardDirection;
        api.CameraGetRightDirection  = CameraGetRightDirection;
        api.CameraGetPosition        = CameraGetPosition;
        api.CameraGetRotation        = CameraGetRotation;
        api.CameraSetPosition        = CameraSetPosition;
        api.CameraSetRotation        = CameraSetRotation;

        // Sounds
        api.SoundsPlayGlobal = SoundsPlayGlobal;
        api.SoundsPlayLocal  = SoundsPlayLocal;

        // Box Collider
        api.BoxColliderGetLinearVelocity      = BoxColliderGetLinearVelocity;
        api.BoxColliderSetLinearVelocity      = BoxColliderSetLinearVelocity;
        api.BoxColliderAddLinearVelocity      = BoxColliderAddLinearVelocity;
        api.BoxColliderAddAngularVelocity     = BoxColliderAddAngularVelocity;
        api.BoxColliderAddImpulse             = BoxColliderAddImpulse;
        api.BoxColliderAddAngularImpulse      = BoxColliderAddAngularImpulse;
        api.BoxColliderAddLinearAngularImpulse = BoxColliderAddLinearAngularImpulse;

        // Sphere Collider
        api.SphereColliderGetLinearVelocity      = SphereColliderGetLinearVelocity;
        api.SphereColliderSetLinearVelocity      = SphereColliderSetLinearVelocity;
        api.SphereColliderAddLinearVelocity      = SphereColliderAddLinearVelocity;
        api.SphereColliderAddAngularVelocity     = SphereColliderAddAngularVelocity;
        api.SphereColliderAddImpulse             = SphereColliderAddImpulse;
        api.SphereColliderAddAngularImpulse      = SphereColliderAddAngularImpulse;
        api.SphereColliderAddLinearAngularImpulse = SphereColliderAddLinearAngularImpulse;

        // Character Controller
        api.CharControllerGetLinearVelocity      = CharControllerGetLinearVelocity;
        api.CharControllerSetLinearVelocity      = CharControllerSetLinearVelocity;
        api.CharControllerAddLinearVelocity      = CharControllerAddLinearVelocity;
        api.CharControllerAddAngularVelocity     = CharControllerAddAngularVelocity;
        api.CharControllerAddImpulse             = CharControllerAddImpulse;
        api.CharControllerAddAngularImpulse      = CharControllerAddAngularImpulse;
        api.CharControllerAddLinearAngularImpulse = CharControllerAddLinearAngularImpulse;
        api.CharControllerIsGrounded             = CharControllerIsGrounded;
        api.CharControllerGetRotation            = CharControllerGetRotation;
        api.CharControllerSetRotation            = CharControllerSetRotation;
        api.CharControllerRotate                 = CharControllerRotate;
    }
}
