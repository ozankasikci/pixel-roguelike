#include "editor/ui/inspectors/SceneSelectionInspector.h"

#include "editor/ui/inspectors/ArchetypeInspector.h"
#include "editor/ui/inspectors/ColliderInspector.h"
#include "editor/ui/inspectors/GroupInspector.h"
#include "editor/ui/inspectors/LightInspector.h"
#include "editor/ui/inspectors/MeshInspector.h"
#include "editor/ui/inspectors/PlayerSpawnInspector.h"
#include "editor/ui/inspectors/PrefabInspector.h"
#include "editor/ui/inspectors/ReflectionProbeInspector.h"
#include "editor/ui/inspectors/SingleDoorInspector.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

void renderSceneSelectionInspector(EditorSceneDocument& document,
                                   const std::vector<std::uint64_t>& selectedIds,
                                   const ContentRegistry& content,
                                   const std::vector<std::string>& meshIds,
                                   const std::vector<std::string>& materialIds,
                                   const std::vector<std::string>& archetypeIds,
                                   EditorPendingCommand& pendingCommand,
                                   EditorCommandStack& commandStack) {
    if (selectedIds.empty()) {
        ImGui::TextUnformatted("No scene selection");
        return;
    }

    if (selectedIds.size() > 1) {
        ImGui::Text("%zu objects selected", selectedIds.size());
        const auto meshObjects = selectedMeshObjects(document, selectedIds);
        if (!meshObjects.empty()) {
            ImGui::Separator();
            ImGui::Text("Mesh Materials");
            const std::string currentMaterialLabel = materialSelectionLabel(meshObjects);
            if (ImGui::BeginCombo("Material Id", currentMaterialLabel.c_str())) {
                for (const auto& materialId : materialIds) {
                    const bool selected = materialId == currentMaterialLabel;
                    if (ImGui::Selectable(materialId.c_str(), selected)) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        if (applyMaterialToMeshes(meshObjects, materialId, content, document)) {
                            commandStack.pushDocumentStateCommand(
                                meshObjects.size() == 1 ? "Assign Mesh Material" : "Assign Mesh Materials",
                                beforeState,
                                document.captureState(),
                                document);
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("Applies to %zu selected mesh%s", meshObjects.size(), meshObjects.size() == 1 ? "" : "es");
        }
        return;
    }

    EditorSceneObject* object = document.findObject(selectedIds.front());
    if (object == nullptr) {
        ImGui::TextUnformatted("Selection missing");
        return;
    }

    ImGui::TextUnformatted(editorSceneObjectKindName(object->kind));
    ImGui::TextDisabled("Id: %llu", static_cast<unsigned long long>(object->id));
    ImGui::Separator();

    if (!beginInspectorPropertyTable("SceneSelectionProperties")) {
        return;
    }

    // Parent picker — rendered into the open property table
    const auto renderParentPicker = [&]() {
        const std::uint64_t currentParentId = document.parentObjectId(object->id);
        if (!(document.supportsParenting(object->id) || currentParentId != 0)) {
            return;
        }

        std::string parentLabel = "<None>";
        if (currentParentId != 0) {
            if (const EditorSceneObject* parentObject = document.findObject(currentParentId)) {
                parentLabel = editorSceneObjectLabel(*parentObject);
            }
        }

        renderInspectorPropertyRow("Parent", [&]() {
            if (ImGui::BeginCombo("##value", parentLabel.c_str())) {
                const bool noParentSelected = (currentParentId == 0);
                if (ImGui::Selectable("<None>", noParentSelected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    if (document.clearParent(object->id)) {
                        commandStack.pushDocumentStateCommand("Clear Parent", beforeState, document.captureState(), document);
                    }
                }
                if (noParentSelected) {
                    ImGui::SetItemDefaultFocus();
                }
                for (const auto& candidate : document.objects()) {
                    if (candidate.id == object->id) {
                        continue;
                    }
                    if (!document.canSetParent(object->id, candidate.id) && candidate.id != currentParentId) {
                        continue;
                    }
                    const bool selected = (candidate.id == currentParentId);
                    if (ImGui::Selectable(editorSceneObjectLabel(candidate).c_str(), selected)) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        if (document.setParent(object->id, candidate.id)) {
                            commandStack.pushDocumentStateCommand("Set Parent", beforeState, document.captureState(), document);
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
    };

    renderParentPicker();

    // Each per-type inspector receives the open property table and closes it.
    const EditorSceneDocumentState beforeState = document.captureState();

    switch (object->kind) {
    case EditorSceneObjectKind::Mesh:
        drawMeshInspector(std::get<LevelMeshPlacement>(object->payload), document,
                          meshIds, materialIds, content, commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::Light:
        drawLightInspector(std::get<LevelLightPlacement>(object->payload), document,
                           commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::Collider:
        drawColliderInspector(std::get<LevelColliderPlacement>(object->payload), document,
                              commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::ReflectionProbe:
        drawReflectionProbeInspector(std::get<LevelReflectionProbePlacement>(object->payload), document,
                                     commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::PlayerSpawn:
        drawPlayerSpawnInspector(std::get<LevelPlayerSpawn>(object->payload), document,
                                 commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::Archetype:
        drawArchetypeInspector(std::get<LevelArchetypePlacement>(object->payload), document,
                               archetypeIds, commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::Group:
        drawGroupInspector(std::get<LevelGroupNode>(object->payload), document,
                           commandStack, pendingCommand, beforeState);
        return;
    case EditorSceneObjectKind::SingleDoor:
        drawSingleDoorInspector(std::get<LevelSingleDoorPlacement>(object->payload), document,
                                meshIds, materialIds, commandStack, pendingCommand, beforeState);
        return;
    }
}
