#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

class CathedralBuilder;
class Mesh;

struct CathedralCheckpointPlacement;
struct CathedralDoubleDoorPlacement;

void spawnCathedralCheckpointShrineGeometry(CathedralBuilder& builder, float chamberCenterZ);
void spawnCathedralSideShrineBay(CathedralBuilder& builder, float side, float halfW, float z);
void spawnCathedralDaisStep(CathedralBuilder& builder, const glm::vec3& center, const glm::vec3& scale);
entt::entity spawnCathedralCheckpoint(CathedralBuilder& builder, const CathedralCheckpointPlacement& placement);
entt::entity spawnCathedralDoubleDoor(CathedralBuilder& builder,
                                      Mesh* leftDoorMesh,
                                      Mesh* rightDoorMesh,
                                      const CathedralDoubleDoorPlacement& placement);
