#include "editor/ui/inspectors/ArchetypeInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

void drawArchetypeInspector(LevelArchetypePlacement& archetype,
                             EditorSceneDocument& document,
                             const std::vector<std::string>& archetypeIds,
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

    renderInspectorPropertyRow("Archetype Id", [&]() {
        if (ImGui::BeginCombo("##value", archetype.archetypeId.c_str())) {
            for (const auto& archetypeId : archetypeIds) {
                const bool selected = archetypeId == archetype.archetypeId;
                if (ImGui::Selectable(archetypeId.c_str(), selected)) {
                    const EditorSceneDocumentState before = document.captureState();
                    archetype.archetypeId = archetypeId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand("Change Archetype", before, document.captureState(), document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return false;
    });
    drawPositionSection(archetype.position, "Position", "Move Archetype",
                        document, commandStack, pendingCommand);
    auto itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Rotate Archetype", renderInspectorPropertyRow("Yaw", [&]() { return ImGui::DragFloat("##value", &archetype.yawDegrees, 0.5f, -360.0f, 360.0f, "%.1f"); }));
    endInspectorPropertyTable();
}
