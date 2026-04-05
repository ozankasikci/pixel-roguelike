#include "editor/ui/inspectors/PlayerSpawnInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

void drawPlayerSpawnInspector(LevelPlayerSpawn& spawn,
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

    drawPositionSection(spawn.position, "Position", "Move Player Spawn",
                        document, commandStack, pendingCommand);
    auto itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Adjust Fall Respawn Height", renderInspectorPropertyRow("Fall Respawn Y", [&]() { return ImGui::DragFloat("##value", &spawn.fallRespawnY, 0.1f, -100.0f, 100.0f, "%.2f"); }));
    endInspectorPropertyTable();
}
