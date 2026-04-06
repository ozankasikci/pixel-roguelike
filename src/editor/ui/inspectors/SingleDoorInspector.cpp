#include "editor/ui/inspectors/SingleDoorInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

void drawSingleDoorInspector(LevelSingleDoorPlacement& door,
                              EditorSceneDocument& document,
                              const std::vector<std::string>& meshIds,
                              const std::vector<std::string>& materialIds,
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

    // Door mesh
    renderInspectorPropertyRow("Door Mesh", [&]() {
        if (ImGui::BeginCombo("##value", door.doorMeshName.c_str())) {
            for (const auto& meshId : meshIds) {
                const bool selected = meshId == door.doorMeshName;
                if (ImGui::Selectable(meshId.c_str(), selected)) {
                    const EditorSceneDocumentState before = document.captureState();
                    door.doorMeshName = meshId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand("Change Door Mesh", before, document.captureState(), document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return false;
    });

    // Frame mesh
    renderInspectorPropertyRow("Frame Mesh", [&]() {
        if (ImGui::BeginCombo("##value", door.frameMeshName.c_str())) {
            for (const auto& meshId : meshIds) {
                const bool selected = meshId == door.frameMeshName;
                if (ImGui::Selectable(meshId.c_str(), selected)) {
                    const EditorSceneDocumentState before = document.captureState();
                    door.frameMeshName = meshId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand("Change Frame Mesh", before, document.captureState(), document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return false;
    });

    // Door material
    renderInspectorPropertyRow("Door Material", [&]() {
        if (ImGui::BeginCombo("##value", door.doorMaterialId.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == door.doorMaterialId;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    const EditorSceneDocumentState before = document.captureState();
                    door.doorMaterialId = materialId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand("Change Door Material", before, document.captureState(), document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return false;
    });

    // Frame material
    renderInspectorPropertyRow("Frame Material", [&]() {
        if (ImGui::BeginCombo("##value", door.frameMaterialId.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == door.frameMaterialId;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    const EditorSceneDocumentState before = document.captureState();
                    door.frameMaterialId = materialId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand("Change Frame Material", before, document.captureState(), document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return false;
    });

    // Position
    drawPositionSection(door.rootPosition, "Position", "Move Door",
                        document, commandStack, pendingCommand);

    // Yaw
    auto itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Rotate Door",
        renderInspectorPropertyRow("Yaw", [&]() {
            return ImGui::DragFloat("##value", &door.doorYawDegrees, 0.5f, -360.0f, 360.0f, "%.1f");
        }));

    // Open angle
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Change Open Angle",
        renderInspectorPropertyRow("Open Angle", [&]() {
            return ImGui::DragFloat("##value", &door.openAngle, 0.5f, 0.0f, 180.0f, "%.1f");
        }));

    // Open duration
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Change Open Duration",
        renderInspectorPropertyRow("Open Duration", [&]() {
            return ImGui::DragFloat("##value", &door.openDuration, 0.01f, 0.1f, 10.0f, "%.2f");
        }));

    // Interact distance
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Change Interact Distance",
        renderInspectorPropertyRow("Interact Dist", [&]() {
            return ImGui::DragFloat("##value", &door.interactDistance, 0.05f, 0.5f, 10.0f, "%.2f");
        }));

    // Locked
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Toggle Locked",
        renderInspectorPropertyRow("Locked", [&]() {
            return ImGui::Checkbox("##value", &door.locked);
        }, EditorInspectorFieldKind::Toggle));

    // Locked prompt (only visible when locked)
    if (door.locked) {
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Change Locked Prompt",
            renderInspectorPropertyRow("Locked Prompt", [&]() {
                return editString("##value", door.lockedPrompt);
            }, EditorInspectorFieldKind::Text));
    }

    // Door tint
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Change Door Tint",
        renderInspectorPropertyRow("Door Tint", [&]() {
            return editColor("##value", door.doorTint);
        }, EditorInspectorFieldKind::Color));

    // Frame tint
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Change Frame Tint",
        renderInspectorPropertyRow("Frame Tint", [&]() {
            return editColor("##value", door.frameTint);
        }, EditorInspectorFieldKind::Color));

    endInspectorPropertyTable();
}
