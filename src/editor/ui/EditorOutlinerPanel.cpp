#include "editor/ui/EditorOutlinerPanel.h"

#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"
#include "editor/viewport/EditorViewportInteraction.h"

#include <imgui.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_set>

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
    const bool shiftHeld = ImGui::GetIO().KeyShift;
    const bool ctrlHeld = ImGui::GetIO().KeyMods & ImGuiMod_Super
                        || ImGui::GetIO().KeyMods & ImGuiMod_Ctrl;

    // Build flat visible-order list for shift-range selection.
    std::vector<std::uint64_t> flatOrder;
    std::function<void(std::uint64_t)> collectOrder = [&](std::uint64_t objectId) {
        const EditorSceneObject* object = document.findObject(objectId);
        if (object == nullptr) {
            return;
        }
        flatOrder.push_back(objectId);
        for (const auto childId : document.childObjectIds(objectId)) {
            collectOrder(childId);
        }
    };
    for (const auto rootId : document.rootObjectIds()) {
        collectOrder(rootId);
    }

    ImGui::BeginDisabled(selectedIds.empty());
    if (ImGui::Button("Delete Selected")) {
        deleteRequests = selectedIds;
        ui.inspectorContext = EditorInspectorContext::SceneSelection;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Add Group")) {
        const EditorSceneDocumentState beforeState = document.captureState();
        LevelGroupNode group;
        group.name = "Group";
        const std::uint64_t groupId = document.addGroup(group);
        commandStack.pushDocumentStateCommand("Add Group", beforeState, document.captureState(), document);
        selectedIds = {groupId};
        ui.inspectorContext = EditorInspectorContext::SceneSelection;
    }
    ImGui::Separator();

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive()
        && ImGui::IsKeyPressed(ImGuiKey_Delete)
        && !selectedIds.empty()) {
        deleteRequests = selectedIds;
        ui.inspectorContext = EditorInspectorContext::SceneSelection;
    }

    // Build ancestor expand set: when scrollToSelection is set, force-open all
    // ancestor nodes that contain the selected item so the selected row is visible.
    std::unordered_set<std::uint64_t> expandForScroll;
    if (ui.scrollToSelection) {
        for (const std::uint64_t selId : selectedIds) {
            std::uint64_t ancestorId = document.parentObjectId(selId);
            while (ancestorId != 0) {
                expandForScroll.insert(ancestorId);
                ancestorId = document.parentObjectId(ancestorId);
            }
        }
    }
    bool scrolledThisFrame = false;

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

        // Force-expand ancestors of the selected item so the selected row is visible.
        if (expandForScroll.count(objectId) > 0) {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        }

        const bool openNode = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(objectId)),
                                                flags,
                                                "%s",
                                                editorSceneObjectLabel(*object).c_str());

        // Scroll to the first selected item after a viewport-originated selection change.
        if (ui.scrollToSelection && !scrolledThisFrame && isSelected(selectedIds, objectId)) {
            ImGui::SetScrollHereY(0.5f);
            scrolledThisFrame = true;
        }

        const auto selectionBefore = selectedIds;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            if (shiftHeld && ui.outlinerAnchorId != 0) {
                // Range selection: select all items between anchor and clicked item.
                auto anchorIt = std::find(flatOrder.begin(), flatOrder.end(), ui.outlinerAnchorId);
                auto clickIt = std::find(flatOrder.begin(), flatOrder.end(), objectId);
                if (anchorIt != flatOrder.end() && clickIt != flatOrder.end()) {
                    if (anchorIt > clickIt) {
                        std::swap(anchorIt, clickIt);
                    }
                    selectedIds.clear();
                    for (auto it = anchorIt; it <= clickIt; ++it) {
                        selectedIds.push_back(*it);
                    }
                } else {
                    selectedIds = {objectId};
                    ui.outlinerAnchorId = objectId;
                }
            } else {
                toggleSelection(selectedIds, objectId, ctrlHeld);
                ui.outlinerAnchorId = objectId;
            }
            ui.inspectorContext = EditorInspectorContext::SceneSelection;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                ui.frameSelectionRequested = true;
            }
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !isSelected(selectedIds, objectId)) {
            selectedIds = {objectId};
            ui.inspectorContext = EditorInspectorContext::SceneSelection;
        }
        commandStack.pushSelectionCommand("Select", &selectedIds, selectionBefore, selectedIds);

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
            ImGui::Separator();
            if (!selectedIds.empty() && ImGui::MenuItem("Create Group")) {
                const EditorSceneDocumentState beforeState = document.captureState();
                glm::vec3 centroid(0.0f);
                for (const auto selId : selectedIds) {
                    centroid += glm::vec3(document.worldTransformMatrix(selId)[3]);
                }
                centroid /= static_cast<float>(selectedIds.size());
                LevelGroupNode group;
                group.name = "Group";
                group.position = centroid;
                const std::uint64_t groupId = document.addGroup(group);
                for (const auto selId : selectedIds) {
                    document.setParent(selId, groupId);
                }
                commandStack.pushDocumentStateCommand("Create Group", beforeState, document.captureState(), document);
                selectedIds = {groupId};
                ui.inspectorContext = EditorInspectorContext::SceneSelection;
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

    // Consume the scroll flag — set by viewport selection, reset here after rendering.
    ui.scrollToSelection = false;

    ImGui::End();
    return deleteRequests;
}
