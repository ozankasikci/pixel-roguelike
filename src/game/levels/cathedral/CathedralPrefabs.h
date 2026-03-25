#pragma once

#include <glm/glm.hpp>

class LevelBuilder;

void spawnCathedralCheckpointShrineGeometry(LevelBuilder& builder, float chamberCenterZ);
void spawnCathedralSideShrineBay(LevelBuilder& builder, float side, float halfW, float z);
void spawnCathedralDaisStep(LevelBuilder& builder, const glm::vec3& center, const glm::vec3& scale);
