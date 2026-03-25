#pragma once

#include <glm/glm.hpp>

class CathedralBuilder;

void spawnCathedralCheckpointShrineGeometry(CathedralBuilder& builder, float chamberCenterZ);
void spawnCathedralSideShrineBay(CathedralBuilder& builder, float side, float halfW, float z);
void spawnCathedralDaisStep(CathedralBuilder& builder, const glm::vec3& center, const glm::vec3& scale);
