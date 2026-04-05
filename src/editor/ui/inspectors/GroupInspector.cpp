#include "editor/ui/inspectors/GroupInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

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
    drawTransformSectionWithScale(group.position, group.rotation, group.scale,
                                  "Position", "Rotation", "Scale",
                                  "Move Group", "Rotate Group", "Scale Group",
                                  document, commandStack, pendingCommand);
    endInspectorPropertyTable();
}
