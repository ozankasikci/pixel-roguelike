// Verifies edit/play parity for door group position math.
//
// The core invariant:
//   makePivotLeafModel(group.position, closedYaw, closedYaw, pivot, leafScale)
// must produce the same model matrix whether called from:
//   - LevelBuilder::addDoorGroup  (play mode)
//   - EditorPreviewWorld::rebuild  (editor initial load)
//   - EditorPreviewWorld::syncTransforms  (editor after gizmo drag)
//
// Key property: when closed (currentYaw == closedYaw), the door model equals
// T(basePos) * R(closedYaw) * S(scale) — exactly the frame transform. No gap.
// When opening, the door rotates around the pivot (hinge) in mesh-local space.

#include "common/TestSupport.h"
#include "game/level/LevelDef.h"
#include "game/prefabs/GameplayPrefabs.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <cmath>

static bool mat4NearlyEqual(const glm::mat4& a, const glm::mat4& b, float eps = 0.001f) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            if (std::fabs(a[col][row] - b[col][row]) > eps) {
                return false;
            }
        }
    }
    return true;
}

int main() {
    const glm::vec3 groupPosition(3.0f, 0.0f, -2.0f);
    const float     closedYaw = 90.0f;
    const float     openYaw = 0.0f;  // 90 degrees open
    const glm::vec3 leafScale(0.22f, 0.22f, 0.22f);
    const glm::vec3 pivot(-0.45f, 0.0f, 0.04f);

    // ---------------------------------------------------------------------------
    // 1. When closed, door model must equal frame model (no gap).
    // ---------------------------------------------------------------------------
    const glm::mat4 closedModel = makePivotLeafModel(groupPosition, closedYaw, closedYaw, pivot, leafScale);

    // Frame model: T(basePos) * R(closedYaw) * S(scale)
    glm::mat4 frameModel = glm::translate(glm::mat4(1.0f), groupPosition);
    frameModel = glm::rotate(frameModel, glm::radians(closedYaw), glm::vec3(0.0f, 1.0f, 0.0f));
    frameModel = glm::scale(frameModel, leafScale);

    assert(mat4NearlyEqual(closedModel, frameModel));

    // Mesh center at (0,0,0) maps to groupPosition (same as frame center).
    const glm::vec3 closedCenter = glm::vec3(closedModel * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    assert(test_support::nearlyEqualVec3(closedCenter, groupPosition, 0.001f));

    // ---------------------------------------------------------------------------
    // 2. When open, door model differs from closed model.
    // ---------------------------------------------------------------------------
    const glm::mat4 openModel = makePivotLeafModel(groupPosition, closedYaw, openYaw, pivot, leafScale);
    assert(!mat4NearlyEqual(closedModel, openModel));

    // The hinge point should be the same in both closed and open states.
    const glm::vec3 closedHinge = glm::vec3(closedModel * glm::vec4(pivot, 1.0f));
    const glm::vec3 openHinge = glm::vec3(openModel * glm::vec4(pivot, 1.0f));
    assert(test_support::nearlyEqualVec3(closedHinge, openHinge, 0.001f));

    // ---------------------------------------------------------------------------
    // 3. Zero yaw degenerate case.
    // ---------------------------------------------------------------------------
    {
        const glm::vec3 origin(0.0f);
        const glm::mat4 m = makePivotLeafModel(origin, 0.0f, 0.0f, pivot, leafScale);

        // When closed, mesh center is at origin.
        const glm::vec3 center = glm::vec3(m * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        assert(test_support::nearlyEqualVec3(center, origin, 0.001f));
    }

    // ---------------------------------------------------------------------------
    // 4. resolveLevelHierarchy preserves group position.
    // ---------------------------------------------------------------------------
    LevelDef rawLevel;
    LevelDoorGroupPlacement dg;
    dg.name = "TestDoor";
    dg.position = groupPosition;
    dg.yawDegrees = closedYaw;
    dg.openAngle = 90.0f;
    dg.openDuration = 1.2f;
    dg.interactDistance = 2.5f;
    dg.interactDotThreshold = 0.55f;
    dg.nodeId = "n_test_door_group";

    LevelMeshPlacement leafMesh;
    leafMesh.meshId = "door_leaf";
    leafMesh.position = glm::vec3(0.0f);
    leafMesh.scale = leafScale;
    leafMesh.rotation = glm::vec3(0.0f);
    leafMesh.pivot = pivot;
    leafMesh.nodeId = "n_test_door_leaf";
    leafMesh.parentNodeId = "n_test_door_group";

    rawLevel.doors.push_back(dg);
    rawLevel.meshes.push_back(leafMesh);

    const LevelDef level = resolveLevelHierarchy(rawLevel);
    assert(level.doors.size() == 1);
    assert(test_support::nearlyEqualVec3(level.doors.front().position, groupPosition));
    assert(test_support::nearlyEqual(level.doors.front().yawDegrees, closedYaw));

    return 0;
}
