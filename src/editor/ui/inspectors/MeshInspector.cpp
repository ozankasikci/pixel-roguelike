#include "editor/ui/inspectors/MeshInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

void drawMeshInspector(LevelMeshPlacement& mesh,
                       EditorSceneDocument& document,
                       const std::vector<std::string>& meshIds,
                       const std::vector<std::string>& materialIds,
                       const ContentRegistry& content,
                       EditorCommandStack& commandStack,
                       EditorPendingCommand& pendingCommand,
                       const EditorSceneDocumentState& beforeState) {
    (void)content;
    (void)beforeState;

    const auto trackSceneItem = [&](const EditorSceneDocumentState& itemBefore, const std::string& label, bool changed) {
        if (changed) {
            document.markSceneDirty();
        }
        trackLastItemCommand(itemBefore, label, pendingCommand, commandStack, document);
    };

    const std::string currentMaterialLabel = mesh.materialId.empty() ? "stone_default" : mesh.materialId;
    renderInspectorPropertyRow("Mesh Id", [&]() {
        if (ImGui::BeginCombo("##value", mesh.meshId.c_str())) {
            for (const auto& meshId : meshIds) {
                const bool selected = meshId == mesh.meshId;
                if (ImGui::Selectable(meshId.c_str(), selected)) {
                    const EditorSceneDocumentState before = document.captureState();
                    mesh.meshId = meshId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand("Change Mesh Asset", before, document.captureState(), document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return false;
    });
    renderInspectorPropertyRow("Material Id", [&]() {
        if (ImGui::BeginCombo("##value", currentMaterialLabel.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == currentMaterialLabel;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    if (mesh.materialId != materialId) {
                        const EditorSceneDocumentState before = document.captureState();
                        mesh.materialId = materialId;
                        document.markSceneDirty();
                        commandStack.pushDocumentStateCommand("Change Mesh Material", before, document.captureState(), document);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        return false;
    });

    drawTransformSectionWithScale(mesh.position, mesh.rotation, mesh.scale,
                                  "Position", "Rotation", "Scale",
                                  "Move Mesh", "Rotate Mesh", "Scale Mesh",
                                  document, commandStack, pendingCommand);
    bool hasTint = mesh.tint.has_value();
    auto itemBefore = document.captureState();
    const bool tintToggleChanged = renderInspectorPropertyRow("Use Tint", [&]() { return ImGui::Checkbox("##value", &hasTint); });
    if (tintToggleChanged) {
        mesh.tint = hasTint ? std::optional<glm::vec3>{mesh.tint.value_or(glm::vec3(1.0f))} : std::nullopt;
    }
    trackSceneItem(itemBefore, "Toggle Mesh Tint", tintToggleChanged);
    if (mesh.tint.has_value()) {
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Change Mesh Tint", renderInspectorPropertyRow("Tint", [&]() { return editColor("##value", *mesh.tint); }));
    }

    endInspectorPropertyTable();

    // Make Interactable section
    ImGui::Separator();
    bool isInteractable = mesh.interactable.has_value();
    {
        auto interactBefore = document.captureState();
        if (ImGui::Checkbox("Make Interactable", &isInteractable)) {
            if (isInteractable) {
                mesh.interactable = InteractableDeclaration{
                    "Press E to interact",
                    2.0f,
                    0.55f,
                };
            } else {
                mesh.interactable = std::nullopt;
            }
            document.markSceneDirty();
            commandStack.pushDocumentStateCommand("Toggle Interactable", interactBefore, document.captureState(), document);
        }
    }

    if (mesh.interactable.has_value()) {
        if (beginInspectorPropertyTable("InteractableProperties")) {
            auto interactBefore = document.captureState();
            char promptBuf[64];
            std::snprintf(promptBuf, sizeof(promptBuf), "%s", mesh.interactable->promptText.c_str());
            if (renderInspectorPropertyRow("Prompt", [&]() { return ImGui::InputText("##value", promptBuf, sizeof(promptBuf)); })) {
                mesh.interactable->promptText = promptBuf;
                document.markSceneDirty();
                commandStack.pushDocumentStateCommand("Edit Interaction Prompt", interactBefore, document.captureState(), document);
                interactBefore = document.captureState();
            }

            const bool distChanged = renderInspectorPropertyRow("Distance", [&]() {
                bool c = ImGui::DragFloat("##value", &mesh.interactable->distance, 0.1f, 0.1f, 10.0f, "%.1f m");
                mesh.interactable->distance = std::max(mesh.interactable->distance, 0.1f);
                return c;
            });
            if (distChanged) {
                document.markSceneDirty();
                commandStack.pushDocumentStateCommand("Edit Interaction Distance", interactBefore, document.captureState(), document);
                interactBefore = document.captureState();
            }

            const bool dotChanged = renderInspectorPropertyRow("Dot Threshold", [&]() {
                return ImGui::DragFloat("##value", &mesh.interactable->dotThreshold, 0.05f, 0.0f, 1.0f, "%.2f");
            });
            if (dotChanged) {
                document.markSceneDirty();
                commandStack.pushDocumentStateCommand("Edit Dot Threshold", interactBefore, document.captureState(), document);
            }

            endInspectorPropertyTable();
        }
    }

    // Behavior sections
    ImGui::Separator();
    drawBehaviorSections(mesh.behaviors, document, commandStack);
}
