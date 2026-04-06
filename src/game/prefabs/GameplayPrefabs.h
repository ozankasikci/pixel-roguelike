#pragma once

#include "game/prefabs/GameplayPrefabData.h"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

class LevelBuilder;
class Mesh;

// Shared pivot math helpers for door leaf spawning.
// groupWorldPos: world position of the door group root.
// groupYawDeg: Y rotation of the door assembly in degrees.
// pivot: local-space hinge offset on the leaf mesh.
// scale: world scale of the leaf mesh.
glm::mat4 makePivotLeafModel(const glm::vec3& groupWorldPos,
                              float groupYawDeg,
                              const glm::vec3& pivot,
                              const glm::vec3& scale);

glm::vec3 computeHingeWorldPos(const glm::vec3& groupWorldPos,
                                float groupYawDeg,
                                const glm::vec3& pivot,
                                const glm::vec3& scale = glm::vec3(1.0f));

entt::entity spawnCheckpoint(LevelBuilder& builder, const CheckpointSpawnSpec& spec);
entt::entity spawnDoubleDoor(LevelBuilder& builder,
                             Mesh* leftDoorMesh,
                             Mesh* rightDoorMesh,
                             const DoubleDoorSpawnSpec& spec);
entt::entity spawnDoubleDoor(LevelBuilder& builder, const DoubleDoorSpawnSpec& spec);
entt::entity spawnGameplayPrefab(LevelBuilder& builder, const GameplayPrefabInstance& instance);
