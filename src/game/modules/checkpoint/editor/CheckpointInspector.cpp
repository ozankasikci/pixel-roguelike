#include "game/modules/checkpoint/editor/CheckpointInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

void drawCheckpointInspector(LevelCheckpointPlacement& checkpoint,
                             EditorSceneDocument& document,
                             EditorCommandStack& commandStack,
                             EditorPendingCommand& pendingCommand,
                             const EditorSceneDocumentState& beforeState) {
    (void)beforeState;

    const auto trackSceneItem = [&](const EditorSceneDocumentState& itemBefore,
                                    const std::string& label,
                                    bool changed) {
        if (changed) {
            document.markSceneDirty();
        }
        trackLastItemCommand(itemBefore, label, pendingCommand, commandStack, document);
    };

    auto itemBefore = document.captureState();
    trackSceneItem(itemBefore,
                   "Rename Checkpoint",
                   renderInspectorPropertyRow("Name", [&]() {
                       return editString("##value", checkpoint.name, "Checkpoint");
                   }));

    drawPositionSection(checkpoint.position,
                        "Position",
                        "Move Checkpoint",
                        document,
                        commandStack,
                        pendingCommand);
    drawPositionSection(checkpoint.respawnPosition,
                        "Respawn",
                        "Move Checkpoint Respawn",
                        document,
                        commandStack,
                        pendingCommand);

    itemBefore = document.captureState();
    trackSceneItem(itemBefore,
                   "Adjust Checkpoint Interact Distance",
                   renderInspectorPropertyRow("Interact Dist", [&]() {
                       return ImGui::DragFloat("##value",
                                               &checkpoint.interactDistance,
                                               0.05f,
                                               0.1f,
                                               20.0f,
                                               "%.2f");
                   }));

    itemBefore = document.captureState();
    trackSceneItem(itemBefore,
                   "Adjust Checkpoint Interact Dot",
                   renderInspectorPropertyRow("Interact Dot", [&]() {
                       return ImGui::SliderFloat("##value",
                                                 &checkpoint.interactDotThreshold,
                                                 0.0f,
                                                 1.0f);
                   }));

    itemBefore = document.captureState();
    trackSceneItem(itemBefore,
                   "Adjust Checkpoint Light Offset",
                   renderInspectorPropertyRow("Light Offset", [&]() {
                       return editVec3("##value", checkpoint.lightOffset, 0.01f);
                   }));

    itemBefore = document.captureState();
    trackSceneItem(itemBefore,
                   "Adjust Checkpoint Light Color",
                   renderInspectorPropertyRow("Light Color", [&]() {
                       return editColor("##value", checkpoint.lightColor);
                   }));

    itemBefore = document.captureState();
    trackSceneItem(itemBefore,
                   "Adjust Checkpoint Light Radius",
                   renderInspectorPropertyRow("Light Radius", [&]() {
                       return ImGui::DragFloat("##value",
                                               &checkpoint.lightRadius,
                                               0.05f,
                                               0.1f,
                                               30.0f,
                                               "%.2f");
                   }));

    itemBefore = document.captureState();
    trackSceneItem(itemBefore,
                   "Adjust Checkpoint Light Intensity",
                   renderInspectorPropertyRow("Light Intensity", [&]() {
                       return ImGui::DragFloat("##value",
                                               &checkpoint.lightIntensity,
                                               0.05f,
                                               0.0f,
                                               20.0f,
                                               "%.2f");
                   }));

    endInspectorPropertyTable();
}
