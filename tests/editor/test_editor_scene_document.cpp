#include "editor/scene/EditorSceneDocument.h"
#include "common/TestSupport.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cassert>
#include <string>

int main() {
    EditorSceneDocument document;
    document.clear();

    LevelReflectionProbePlacement probe;
    probe.position = glm::vec3(1.0f, 2.0f, 3.0f);
    probe.extents = glm::vec3(4.0f, 2.0f, 5.0f);
    probe.blendDistance = 1.5f;
    probe.intensity = 0.75f;
    probe.boxProjection = false;
    probe.nodeId = "probe_local";

    const std::uint64_t probeId = document.addReflectionProbe(probe);
    assert(probeId != 0);
    assert(document.sceneDirty());
    assert(!document.supportsParenting(probeId));

    const EditorSceneObject* object = document.findObject(probeId);
    assert(object != nullptr);
    assert(object->kind == EditorSceneObjectKind::ReflectionProbe);
    assert(editorSceneObjectKindName(object->kind) == std::string("Reflection Probe"));
    assert(editorSceneObjectLabel(*object).find("[local]") != std::string::npos);
    assert(test_support::nearlyEqualVec3(editorSceneObjectAnchor(*object), probe.position));

    const glm::mat4 worldMatrix =
        glm::translate(glm::mat4(1.0f), glm::vec3(8.0f, 3.5f, -6.0f))
        * glm::scale(glm::mat4(1.0f), glm::vec3(12.0f, 4.0f, 10.0f));
    assert(document.applyWorldTransform(probeId, worldMatrix));

    object = document.findObject(probeId);
    assert(object != nullptr);
    const auto& transformedProbe = std::get<LevelReflectionProbePlacement>(object->payload);
    assert(test_support::nearlyEqualVec3(transformedProbe.position, glm::vec3(8.0f, 3.5f, -6.0f)));
    assert(test_support::nearlyEqualVec3(transformedProbe.extents, glm::vec3(6.0f, 2.0f, 5.0f)));
    assert(test_support::nearlyEqualVec3(editorSceneObjectAnchor(*object), transformedProbe.position));

    const LevelDef level = document.toLevelDef();
    assert(level.reflectionProbes.size() == 1);
    assert(level.reflectionProbes.front().nodeId == "probe_local");
    assert(test_support::nearlyEqualVec3(level.reflectionProbes.front().extents, glm::vec3(6.0f, 2.0f, 5.0f)));
    assert(test_support::nearlyEqual(level.reflectionProbes.front().blendDistance, 1.5f));
    assert(test_support::nearlyEqual(level.reflectionProbes.front().intensity, 0.75f));
    assert(!level.reflectionProbes.front().boxProjection);

    LevelCheckpointPlacement checkpoint;
    checkpoint.name = "Shrine";
    checkpoint.position = glm::vec3(-2.0f, 0.0f, 4.0f);
    checkpoint.respawnPosition = glm::vec3(-2.0f, 1.6f, 6.5f);
    checkpoint.nodeId = "checkpoint_shrine";

    const std::uint64_t checkpointId = document.addCheckpoint(checkpoint);
    assert(checkpointId != 0);
    assert(document.supportsParenting(checkpointId));

    object = document.findObject(checkpointId);
    assert(object != nullptr);
    assert(object->kind == EditorSceneObjectKind::Checkpoint);
    assert(editorSceneObjectKindName(object->kind) == std::string("Checkpoint"));
    assert(editorSceneObjectLabel(*object).find("[Shrine]") != std::string::npos);
    assert(test_support::nearlyEqualVec3(editorSceneObjectAnchor(*object), checkpoint.position));

    const glm::mat4 checkpointWorld =
        glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, -5.0f));
    assert(document.applyWorldTransform(checkpointId, checkpointWorld));

    object = document.findObject(checkpointId);
    assert(object != nullptr);
    const auto& transformedCheckpoint = std::get<LevelCheckpointPlacement>(object->payload);
    assert(test_support::nearlyEqualVec3(transformedCheckpoint.position, glm::vec3(3.0f, 0.0f, -5.0f)));
    assert(test_support::nearlyEqualVec3(transformedCheckpoint.respawnPosition, glm::vec3(3.0f, 1.6f, -2.5f)));

    const LevelDef checkpointLevel = document.toLevelDef();
    assert(checkpointLevel.checkpoints.size() == 1);
    assert(checkpointLevel.checkpoints.front().name == "Shrine");
    assert(checkpointLevel.checkpoints.front().nodeId == "checkpoint_shrine");
    assert(test_support::nearlyEqualVec3(checkpointLevel.checkpoints.front().respawnPosition,
                                         glm::vec3(3.0f, 1.6f, -2.5f)));

    return 0;
}
