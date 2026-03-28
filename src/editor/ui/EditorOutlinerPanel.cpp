#include "editor/ui/EditorOutlinerPanel.h"

#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"
#include "editor/viewport/EditorViewportInteraction.h"

#include <imgui.h>

#include <algorithm>
#include <functional>

namespace {

enum class OutlinerDropMode {
    Before,
    MakeChild,
    After,
};

OutlinerDropMode outlinerDropModeForItem() {
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float height = std::max(max.y - min.y, 1.0f);
    const float localY = ImGui::GetIO().MousePos.y - min.y;
    if (localY < height * 0.28f) {
        return OutlinerDropMode::Before;
    }
    if (localY > height * 0.72f) {
        return OutlinerDropMode::After;
    }
    return OutlinerDropMode::MakeChild;
}

} // namespace

std::vector<std::uint64_t> renderOutliner(EditorSceneDocument& document,
                                          EditorUiState& ui,
                                          std::vector<std::uint64_t>& selectedIds,
                                          bool* open,
                                          EditorCommandStack& commandStack) {
    std::vector<std::uint64_t> deleteRequests;
    if (open != nullptr && !*open) {
        return deleteRequests;
    }
    if (!ImGui::Begin("Outliner", open)) {
        ImGui::End();
        return deleteRequests;
    }
    const bool additive = ImGui::GetIO().KeyShift;

    ImGui::BeginDisabled(selectedIds.empty());
    if (ImGui::Button("Delete Selected")) {
        deleteRequests = selectedIds;
        ui.inspectorContext = EditorInspectorContext::SceneSelection;
    }
    ImGui::EndDisabled();
    ImGui::Separator();

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive()
        && ImGui::IsKeyPressed(ImGuiKey_Delete)
        && !selectedIds.empty()) {
        deleteRequests = selectedIds;
        ui.inspectorContext = EditorInspectorContext::SceneSelection;
    }

    std::function<void(std::uint64_t)> renderNode = [&](std::uint64_t objectId) {
        const EditorSceneObject* object = document.findObject(objectId);
        if (object == nullptr) {
            return;
        }

        const auto children = document.childObjectIds(objectId);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
        if (children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (isSelected(selectedIds, objectId)) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool openNode = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(objectId)),
                                                flags,
                                                "%s",
                                                editorSceneObjectLabel(*object).c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            toggleSelection(selectedIds, objectId, additive);
            ui.inspectorContext = EditorInspectorContext::SceneSelection;
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !isSelected(selectedIds, objectId)) {
            selectedIds = {objectId};
            ui.inspectorContext = EditorInspectorContext::SceneSelection;
        }

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("EDITOR_OUTLINER_OBJECT", &objectId, sizeof(objectId));
            ImGui::TextUnformatted(editorSceneObjectLabel(*object).c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_OUTLINER_OBJECT", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
                if (payload->DataSize == sizeof(std::uint64_t)) {
                    const std::uint64_t draggedId = *static_cast<const std::uint64_t*>(payload->Data);
                    const OutlinerDropMode dropMode = outlinerDropModeForItem();

                    if (payload->IsPreview()) {
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        const ImVec2 min = ImGui::GetItemRectMin();
                        const ImVec2 max = ImGui::GetItemRectMax();
                        if (dropMode == OutlinerDropMode::MakeChild) {
                            drawList->AddRect(min, max, IM_COL32(255, 220, 120, 220), 3.0f, 0, 2.0f);
                        } else {
                            const float y = dropMode == OutlinerDropMode::Before ? min.y : max.y;
                            drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), IM_COL32(255, 220, 120, 255), 3.0f);
                        }
                    }

                    if (payload->Delivery && draggedId != objectId) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        bool changed = false;
                        const char* label = nullptr;
                        switch (dropMode) {
                        case OutlinerDropMode::Before:
                            changed = document.moveObjectBefore(draggedId, objectId);
                            label = "Move Object Before";
                            break;
                        case OutlinerDropMode::MakeChild:
                            if (document.canSetParent(draggedId, objectId)) {
                                changed = document.setParent(draggedId, objectId);
                                label = "Parent Object";
                            }
                            break;
                        case OutlinerDropMode::After:
                            changed = document.moveObjectAfter(draggedId, objectId);
                            label = "Move Object After";
                            break;
                        }

                        if (changed && label != nullptr) {
                            commandStack.pushDocumentStateCommand(label, beforeState, document.captureState(), document);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("OutlinerContext")) {
            if (ImGui::MenuItem("Delete")) {
                if (isSelected(selectedIds, objectId)) {
                    deleteRequests = selectedIds;
                } else {
                    deleteRequests = {objectId};
                }
                ui.inspectorContext = EditorInspectorContext::SceneSelection;
            }
            if (document.parentObjectId(objectId) != 0 && ImGui::MenuItem("Clear Parent")) {
                const EditorSceneDocumentState beforeState = document.captureState();
                if (document.clearParent(objectId)) {
                    commandStack.pushDocumentStateCommand("Clear Parent", beforeState, document.captureState(), document);
                }
            }
            ImGui::EndPopup();
        }

        if (openNode) {
            for (const auto childId : children) {
                renderNode(childId);
            }
            ImGui::TreePop();
        }
    };

    for (const auto rootId : document.rootObjectIds()) {
        renderNode(rootId);
    }

    ImGui::Spacing();
    const float dropWidth = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton("##outliner_root_drop", ImVec2(dropWidth, 22.0f));
    ImDrawList* rootDropDrawList = ImGui::GetWindowDrawList();
    const ImVec2 rootDropMin = ImGui::GetItemRectMin();
    const ImVec2 rootDropMax = ImGui::GetItemRectMax();
    rootDropDrawList->AddRect(rootDropMin, rootDropMax, IM_COL32(96, 140, 110, 120), 4.0f, 0, 1.5f);
    rootDropDrawList->AddText(ImVec2(rootDropMin.x + 8.0f, rootDropMin.y + 4.0f),
                              IM_COL32(160, 210, 170, 220),
                              "Drop here to move to root");
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_OUTLINER_OBJECT", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
            if (payload->DataSize == sizeof(std::uint64_t)) {
                const std::uint64_t draggedId = *static_cast<const std::uint64_t*>(payload->Data);
                if (payload->IsPreview()) {
                    rootDropDrawList->AddRect(rootDropMin, rootDropMax, IM_COL32(255, 220, 120, 220), 4.0f, 0, 2.5f);
                }
                if (payload->Delivery) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    if (document.moveObjectToRootEnd(draggedId)) {
                        commandStack.pushDocumentStateCommand("Move Object To Root", beforeState, document.captureState(), document);
                    }
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
    return deleteRequests;
}
