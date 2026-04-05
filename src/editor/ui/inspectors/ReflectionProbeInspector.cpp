#include "editor/ui/inspectors/ReflectionProbeInspector.h"

#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

#include <glm/common.hpp>
#include <imgui.h>

void drawReflectionProbeInspector(LevelReflectionProbePlacement& probe,
                                   EditorSceneDocument& document,
                                   EditorCommandStack& commandStack,
                                   EditorPendingCommand& pendingCommand,
                                   const EditorSceneDocumentState& beforeState) {
    (void)beforeState;

    const auto trackSceneItem = [&](const EditorSceneDocumentState& itemBefore, const std::string& label, bool changed) {
        if (changed) {
            document.markSceneDirty();
        }
        trackLastItemCommand(itemBefore, label, pendingCommand, commandStack, document);
    };

    auto itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Move Reflection Probe", renderInspectorPropertyRow("Position", [&]() { return editVec3("##value", probe.position); }));
    itemBefore = document.captureState();
    const bool extentsChanged = renderInspectorPropertyRow("Extents", [&]() { return editVec3("##value", probe.extents, 0.02f); });
    if (extentsChanged) {
        probe.extents = glm::max(probe.extents, glm::vec3(0.05f));
    }
    trackSceneItem(itemBefore, "Resize Reflection Probe", extentsChanged);
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Adjust Probe Blend Distance", renderInspectorPropertyRow("Blend Distance", [&]() { return ImGui::DragFloat("##value", &probe.blendDistance, 0.02f, 0.0f, 20.0f, "%.2f"); }));
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Adjust Probe Intensity", renderInspectorPropertyRow("Intensity", [&]() { return ImGui::DragFloat("##value", &probe.intensity, 0.02f, 0.0f, 4.0f, "%.2f"); }));
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Toggle Box Projection", renderInspectorPropertyRow("Box Projection", [&]() { return ImGui::Checkbox("##value", &probe.boxProjection); }));
    endInspectorPropertyTable();
}
