#include "game/levels/cathedral/CathedralPrefabs.h"

#include "game/level/LevelBuilder.h"

void spawnCathedralCheckpointShrineGeometry(LevelBuilder& builder, float chamberCenterZ) {
    const glm::vec3 shrineBase(0.0f, -0.02f, chamberCenterZ - 1.7f);
    builder.addMesh("cube", shrineBase, glm::vec3(2.7f, 0.32f, 2.2f));
    builder.addMesh("cube", shrineBase + glm::vec3(0.0f, 0.32f, 0.0f), glm::vec3(2.1f, 0.12f, 1.7f));
    builder.addMesh("cube", glm::vec3(0.0f, 0.72f, chamberCenterZ - 1.7f), glm::vec3(0.45f, 1.1f, 0.45f));
    builder.addMesh("cube", glm::vec3(0.0f, 1.55f, chamberCenterZ - 1.7f), glm::vec3(0.8f, 0.12f, 0.8f));
    builder.addMesh("cylinder", glm::vec3(0.0f, 1.25f, chamberCenterZ - 1.7f), glm::vec3(0.26f, 0.5f, 0.26f));
}

void spawnCathedralSideShrineBay(LevelBuilder& builder, float side, float halfW, float z) {
    const float wallX = side * (halfW - 0.25f);
    const float frameX = side * (halfW - 1.1f);

    builder.addMesh("cube", glm::vec3(wallX, 2.45f, z), glm::vec3(0.12f, 2.6f, 1.8f));
    builder.addMesh("cube", glm::vec3(frameX, 4.3f, z), glm::vec3(0.35f, 0.35f, 2.2f));
    builder.addMesh("cube", glm::vec3(frameX, 0.95f, z), glm::vec3(0.35f, 0.2f, 2.2f));
    builder.addMesh("cube", glm::vec3(frameX, 2.55f, z - 1.0f), glm::vec3(0.35f, 2.2f, 0.18f));
    builder.addMesh("cube", glm::vec3(frameX, 2.55f, z + 1.0f), glm::vec3(0.35f, 2.2f, 0.18f));

    for (float barOffset : {-0.7f, 0.0f, 0.7f}) {
        builder.addMesh("cube", glm::vec3(side * (halfW - 1.45f), 2.55f, z + barOffset), glm::vec3(0.1f, 1.9f, 0.08f));
    }

    builder.addMesh(
        "cube",
        glm::vec3(side * (halfW - 1.85f), 0.35f, z),
        glm::vec3(0.8f, 0.45f, 1.1f),
        glm::vec3(0.0f, 0.0f, side * 6.0f)
    );
}

void spawnCathedralDaisStep(LevelBuilder& builder, const glm::vec3& center, const glm::vec3& scale) {
    builder.addMesh("cube", center, scale);
    builder.addBoxCollider(center, scale * 0.5f);
}
