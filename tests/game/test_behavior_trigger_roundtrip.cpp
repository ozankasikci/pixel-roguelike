#include "game/behavior/ActionTypes.h"
#include "game/components/ColliderComponent.h"
#include "game/level/LevelDef.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>
#include <string>

int main() {
    namespace fs = std::filesystem;

    // Build a LevelDef with colliders (trigger mode), behaviors, and interactables
    LevelDef level;
    level.environmentId = "neutral";

    // Test 1: Box trigger with on_activate behavior (ToggleDoor targeting "door_01")
    {
        LevelColliderPlacement trigger;
        trigger.shape = ColliderShape::Box;
        trigger.mode = ColliderMode::Trigger;
        trigger.position = glm::vec3(5.0f, 1.0f, -3.0f);
        trigger.halfExtents = glm::vec3(2.0f, 1.5f, 1.0f);
        trigger.nodeId = "collider_trig_01";
        trigger.fireOnce = false;
        BehaviorDeclaration behavior;
        behavior.eventType = "on_activate";
        behavior.action.type = ActionType::ToggleDoor;
        behavior.action.targetNodeId = "door_01";
        behavior.action.params = DoorActionParams{.duration = 1.2f};
        trigger.behaviors.push_back(behavior);
        level.colliders.push_back(trigger);
    }

    // Test 2: Sphere trigger with on_enter + on_exit behaviors
    {
        LevelColliderPlacement trigger;
        trigger.shape = ColliderShape::Sphere;
        trigger.mode = ColliderMode::Trigger;
        trigger.position = glm::vec3(-2.0f, 0.0f, 4.0f);
        trigger.radius = 3.5f;
        trigger.nodeId = "collider_trig_02";
        trigger.fireOnce = true;
        trigger.parentNodeId = "some_parent";
        BehaviorDeclaration enterBehavior;
        enterBehavior.eventType = "on_enter";
        enterBehavior.action.type = ActionType::EnableEntity;
        enterBehavior.action.targetNodeId = "light_01";
        enterBehavior.action.params = EntityToggleParams{};
        trigger.behaviors.push_back(enterBehavior);
        BehaviorDeclaration exitBehavior;
        exitBehavior.eventType = "on_exit";
        exitBehavior.action.type = ActionType::DisableEntity;
        exitBehavior.action.targetNodeId = "light_01";
        exitBehavior.action.params = EntityToggleParams{};
        trigger.behaviors.push_back(exitBehavior);
        level.colliders.push_back(trigger);
    }

    // Test 3: Mesh with interactable + behavior
    {
        LevelMeshPlacement mesh;
        mesh.meshId = "door_frame";
        mesh.position = glm::vec3(3.0f, 0.0f, 0.0f);
        mesh.scale = glm::vec3(1.0f);
        mesh.rotation = glm::vec3(0.0f);
        mesh.nodeId = "door_01";
        mesh.materialId = "wood_default";
        mesh.interactable = InteractableDeclaration{
            .promptText = "Press E to open",
            .distance = 2.5f,
            .dotThreshold = 0.6f,
        };
        BehaviorDeclaration meshBehavior;
        meshBehavior.eventType = "on_activate";
        meshBehavior.action.type = ActionType::ToggleDoor;
        meshBehavior.action.targetNodeId = "self";
        meshBehavior.action.params = DoorActionParams{.duration = 1.5f};
        mesh.behaviors.push_back(meshBehavior);
        level.meshes.push_back(mesh);
    }

    // Test 4: Light with on_activate behavior (SetLight)
    {
        LevelLightPlacement light;
        light.type = LightType::Point;
        light.position = glm::vec3(0.0f, 3.0f, 0.0f);
        light.nodeId = "light_01";
        light.color = glm::vec3(1.0f, 0.8f, 0.6f);
        light.radius = 10.0f;
        light.intensity = 1.0f;
        BehaviorDeclaration lightBehavior;
        lightBehavior.eventType = "on_activate";
        lightBehavior.action.type = ActionType::SetLight;
        lightBehavior.action.targetNodeId = "self";
        lightBehavior.action.params = LightActionParams{
            .intensity = 0.5f,
            .color = glm::vec3(1.0f, 0.8f, 0.6f),
            .radius = 0.0f,
        };
        light.behaviors.push_back(lightBehavior);
        level.lights.push_back(light);
    }

    // Step 1: Verify serialization contains expected tokens (new unified format)
    const std::string serialized = serializeLevelDef(level);
    assert(serialized.find("collider box trigger") != std::string::npos);
    assert(serialized.find("collider sphere trigger") != std::string::npos);
    assert(serialized.find("on_activate") != std::string::npos);
    assert(serialized.find("on_enter") != std::string::npos);
    assert(serialized.find("on_exit") != std::string::npos);
    assert(serialized.find("interactable") != std::string::npos);
    assert(serialized.find("toggle_door") != std::string::npos);
    assert(serialized.find("set_light") != std::string::npos);
    assert(serialized.find("enable_entity") != std::string::npos);
    assert(serialized.find("disable_entity") != std::string::npos);
    // Old keywords must NOT appear
    assert(serialized.find("trigger_box") == std::string::npos);
    assert(serialized.find("trigger_sphere") == std::string::npos);

    // Step 2: Save to temp file, reload, compare
    const fs::path tempPath = test_support::tempPath("test_behavior_trigger_roundtrip.scene");
    saveLevelDef(tempPath.string(), level);

    const LevelDef loaded = loadLevelDef(tempPath.string());
    fs::remove(tempPath);

    // Verify box trigger collider
    assert(loaded.colliders.size() == 2);
    const auto& loadedBox = loaded.colliders[0];
    assert(loadedBox.shape == ColliderShape::Box);
    assert(loadedBox.mode == ColliderMode::Trigger);
    assert(test_support::nearlyEqualVec3(loadedBox.position, glm::vec3(5.0f, 1.0f, -3.0f)));
    assert(test_support::nearlyEqualVec3(loadedBox.halfExtents, glm::vec3(2.0f, 1.5f, 1.0f)));
    assert(loadedBox.nodeId == "collider_trig_01");
    assert(!loadedBox.fireOnce);
    assert(loadedBox.behaviors.size() == 1);
    assert(loadedBox.behaviors[0].eventType == "on_activate");
    assert(loadedBox.behaviors[0].action.type == ActionType::ToggleDoor);
    assert(loadedBox.behaviors[0].action.targetNodeId == "door_01");

    // Test 5: Sphere trigger with fireOnce=true and parentNodeId
    const auto& loadedSphere = loaded.colliders[1];
    assert(loadedSphere.shape == ColliderShape::Sphere);
    assert(loadedSphere.mode == ColliderMode::Trigger);
    assert(test_support::nearlyEqualVec3(loadedSphere.position, glm::vec3(-2.0f, 0.0f, 4.0f)));
    assert(test_support::nearlyEqual(loadedSphere.radius, 3.5f));
    assert(loadedSphere.nodeId == "collider_trig_02");
    assert(loadedSphere.fireOnce);
    assert(loadedSphere.parentNodeId == "some_parent");
    assert(loadedSphere.behaviors.size() == 2);
    assert(loadedSphere.behaviors[0].eventType == "on_enter");
    assert(loadedSphere.behaviors[0].action.type == ActionType::EnableEntity);
    assert(loadedSphere.behaviors[1].eventType == "on_exit");
    assert(loadedSphere.behaviors[1].action.type == ActionType::DisableEntity);

    // Verify mesh with interactable
    assert(loaded.meshes.size() == 1);
    const auto& loadedMesh = loaded.meshes[0];
    assert(loadedMesh.nodeId == "door_01");
    assert(loadedMesh.interactable.has_value());
    assert(loadedMesh.interactable->promptText == "Press E to open");
    assert(test_support::nearlyEqual(loadedMesh.interactable->distance, 2.5f));
    assert(test_support::nearlyEqual(loadedMesh.interactable->dotThreshold, 0.6f));
    assert(loadedMesh.behaviors.size() == 1);
    assert(loadedMesh.behaviors[0].action.type == ActionType::ToggleDoor);

    // Verify light with behavior
    assert(loaded.lights.size() == 1);
    const auto& loadedLight = loaded.lights[0];
    assert(loadedLight.nodeId == "light_01");
    assert(loadedLight.behaviors.size() == 1);
    assert(loadedLight.behaviors[0].action.type == ActionType::SetLight);
    const auto& lightParams = std::get<LightActionParams>(loadedLight.behaviors[0].action.params);
    assert(test_support::nearlyEqual(lightParams.intensity, 0.5f));
    assert(test_support::nearlyEqualVec3(lightParams.color, glm::vec3(1.0f, 0.8f, 0.6f)));
    assert(test_support::nearlyEqual(lightParams.radius, 0.0f));

    return 0;
}
