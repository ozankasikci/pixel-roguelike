#include "editor/ui/inspectors/GroupInspector.h"

#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

#include <glm/common.hpp>
#include <imgui.h>

void drawGroupInspector(LevelGroupNode& group,
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

    char nameBuf[256];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", group.name.c_str());
    auto itemBefore = document.captureState();
    const bool nameChanged = renderInspectorPropertyRow("Name", [&]() { return ImGui::InputText("##value", nameBuf, sizeof(nameBuf)); });
    if (nameChanged) {
        group.name = nameBuf;
        document.markSceneDirty();
        commandStack.pushDocumentStateCommand("Rename Group", itemBefore, document.captureState(), document);
    }
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Move Group", renderInspectorPropertyRow("Position", [&]() { return editVec3("##value", group.position); }));
    itemBefore = document.captureState();
    const bool scaleChanged = renderInspectorPropertyRow("Scale", [&]() { return editVec3("##value", group.scale, 0.02f); });
    if (scaleChanged) {
        group.scale = glm::max(group.scale, glm::vec3(0.01f));
    }
    trackSceneItem(itemBefore, "Scale Group", scaleChanged);
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Rotate Group", renderInspectorPropertyRow("Rotation", [&]() { return editVec3("##value", group.rotation, 0.5f); }));
    endInspectorPropertyTable();
}
