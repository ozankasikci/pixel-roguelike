#include "game/scenes/GenericFileScene.h"
#include "game/level/LevelBuilder.h"
#include "game/prefabs/GameplayPrefabs.h"

namespace {

static const bool kRegistered = [] {
    GenericFileScene::registerScriptedGeometry("initial_scene", [](LevelBuilder& builder) {
        // Door A -- wooden door, west wall opening
        // Wall door opening is at (-5.0, 0, 1.0) facing 90deg
        {
            SingleDoorSpawnSpec spec;
            spec.doorMeshName = "SM_DoorA";
            spec.frameMeshName = "SM_FrameA";
            spec.doorMaterialId = "qdp_door_a";
            spec.frameMaterialId = "stone_default";
            spec.rootPosition = glm::vec3(-4.85f, 0.0f, 1.0f);
            spec.doorYawDegrees = 90.0f;
            spec.openAngle = 90.0f;
            spec.openDuration = 1.2f;
            spec.interactDistance = 2.5f;
            spec.interactDotThreshold = 0.55f;
            spec.locked = false;
            spec.doorTint = glm::vec3(1.0f);
            spec.frameTint = glm::vec3(1.0f);
            spawnSingleDoor(builder, spec);
        }

        // Door B -- heavy metal door, north wall opening
        // Wall door opening is at (0.0, 0, 6.0) facing 180deg
        {
            SingleDoorSpawnSpec spec;
            spec.doorMeshName = "SM_DoorD";
            spec.frameMeshName = "SM_FrameD";
            spec.doorMaterialId = "qdp_door_d";
            spec.frameMaterialId = "stone_default";
            spec.rootPosition = glm::vec3(0.0f, 0.0f, 5.85f);
            spec.doorYawDegrees = 180.0f;
            spec.openAngle = 90.0f;
            spec.openDuration = 1.8f;
            spec.interactDistance = 2.5f;
            spec.interactDotThreshold = 0.55f;
            spec.locked = false;
            spec.doorTint = glm::vec3(0.6f, 0.58f, 0.55f);
            spec.frameTint = glm::vec3(0.65f, 0.63f, 0.6f);
            spawnSingleDoor(builder, spec);
        }

        // Door C -- white door, east wall opening (was locked, now unlocked)
        // Wall door opening is at (5.0, 0, 3.0) facing -90deg
        {
            SingleDoorSpawnSpec spec;
            spec.doorMeshName = "SM_DoorC";
            spec.frameMeshName = "SM_FrameC";
            spec.doorMaterialId = "qdp_door_c";
            spec.frameMaterialId = "stone_default";
            spec.rootPosition = glm::vec3(4.85f, 0.0f, 3.0f);
            spec.doorYawDegrees = -90.0f;
            spec.openAngle = 90.0f;
            spec.openDuration = 1.0f;
            spec.interactDistance = 2.5f;
            spec.interactDotThreshold = 0.55f;
            spec.locked = false;
            spec.doorTint = glm::vec3(0.88f, 0.85f, 0.8f);
            spec.frameTint = glm::vec3(0.85f, 0.82f, 0.78f);
            spawnSingleDoor(builder, spec);
        }
    });
    return true;
}();

} // namespace
