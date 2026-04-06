#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/ModelLoader.h"
#include "common/TestSupport.h"

#include <cassert>
#include <cstdio>

namespace {

glm::vec3 computeMin(const RawMeshData& mesh) {
    assert(!mesh.positions.empty());
    glm::vec3 result = mesh.positions.front();
    for (const auto& position : mesh.positions) {
        result = glm::min(result, position);
    }
    return result;
}

glm::vec3 computeMax(const RawMeshData& mesh) {
    assert(!mesh.positions.empty());
    glm::vec3 result = mesh.positions.front();
    for (const auto& position : mesh.positions) {
        result = glm::max(result, position);
    }
    return result;
}

void measureDoor(const char* name, const char* relPath) {
    const RawMeshData mesh = ModelLoader::loadRaw(resolveProjectPath(relPath));
    assert(!mesh.positions.empty() && "Door model failed to load");

    const glm::vec3 minV = computeMin(mesh);
    const glm::vec3 maxV = computeMax(mesh);
    const glm::vec3 size = maxV - minV;

    printf("%s: min=(%.4f, %.4f, %.4f) max=(%.4f, %.4f, %.4f) size=(%.4f, %.4f, %.4f)\n",
           name,
           minV.x, minV.y, minV.z,
           maxV.x, maxV.y, maxV.z,
           size.x, size.y, size.z);
    printf("  width=%.4f  height=%.4f  depth=%.4f\n", size.x, size.y, size.z);

    // Compute scale factors for a 2.0m tall x 0.85m wide target fit
    const float targetHeight = 2.0f;
    const float targetWidth  = 0.85f;
    const float scaleByHeight = targetHeight / size.y;
    const float scaleByWidth  = targetWidth  / size.x;
    const float uniformScale  = (scaleByHeight < scaleByWidth) ? scaleByHeight : scaleByWidth;
    printf("  scale_by_height=%.4f  scale_by_width=%.4f  recommended_uniform=%.4f\n\n",
           scaleByHeight, scaleByWidth, uniformScale);
}

} // namespace

int main() {
    printf("=== QuestDoorsPack AABB Diagnostic ===\n");
    printf("Target opening: 0.9m wide x 2.1m tall (prison_wall_door)\n");
    printf("Target fit:     0.85m wide x 2.0m tall (door leaf with jamb gap)\n\n");

    measureDoor("SM_DoorA", "assets/packs/QuestDoorsPack/Models/SM_DoorA.fbx");
    measureDoor("SM_DoorC", "assets/packs/QuestDoorsPack/Models/SM_DoorC.fbx");
    measureDoor("SM_DoorD", "assets/packs/QuestDoorsPack/Models/SM_DoorD.fbx");

    printf("\n=== Frame Meshes ===\n\n");
    measureDoor("SM_FrameA", "assets/packs/QuestDoorsPack/Models/SM_FrameA.fbx");
    measureDoor("SM_FrameC", "assets/packs/QuestDoorsPack/Models/SM_FrameC.fbx");
    measureDoor("SM_FrameD", "assets/packs/QuestDoorsPack/Models/SM_FrameD.fbx");

    printf("\n=== Scale Verification ===\n");
    printf("At scale 0.22:\n");
    printf("  FrameA: %.3fm wide x %.3fm tall\n", 4.7672f * 0.22f, 9.5702f * 0.22f);
    printf("  DoorA:  %.3fm wide x %.3fm tall\n", 4.0556f * 0.22f, 9.2150f * 0.22f);
    printf("  Wall opening (local 1.4m @ Y=1.5): %.3fm wide x %.3fm tall\n", 0.9f, 1.4f * 1.5f);

    printf("\n=== Diagnostic complete ===\n");
    return 0;
}
