#include "editor/ui/EditorOutlinerPanel.h"

#include "editor/core/EditorCommand.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"
#include "editor/viewport/EditorViewportInteraction.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
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

bool shouldHandleOutlinerKeys() {
    return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
        && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive()
        && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
}

int findOutlinerRowIndex(const std::vector<OutlinerVisibleRow>& rows, std::uint64_t objectId) {
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (rows[index].objectId == objectId) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

std::unordered_set<std::uint64_t> buildFilterVisibleSet(const EditorSceneDocument& document,
                                                        const EditorUiState& ui) {
    std::unordered_set<std::uint64_t> filterVisible;
    if (ui.outlinerFilter[0] == '\0') {
        return filterVisible;
    }

    std::string lowerFilter(ui.outlinerFilter);
    for (auto& c : lowerFilter) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    std::function<void(std::uint64_t)> markVisible = [&](std::uint64_t id) {
        while (id != 0 && !filterVisible.contains(id)) {
            filterVisible.insert(id);
            id = document.parentObjectId(id);
        }
    };

    std::function<void(std::uint64_t)> scanObject = [&](std::uint64_t objectId) {
        const EditorSceneObject* obj = document.findObject(objectId);
        if (obj == nullptr) {
            return;
        }
        std::string label = editorSceneObjectLabel(*obj);
        for (auto& c : label) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (label.find(lowerFilter) != std::string::npos) {
            markVisible(objectId);
        }
        for (const auto childId : document.childObjectIds(objectId)) {
            scanObject(childId);
        }
    };

    for (const auto rootId : document.rootObjectIds()) {
        scanObject(rootId);
    }

    return filterVisible;
}

} // namespace

std::vector<OutlinerVisibleRow> buildOutlinerVisibleRows(const EditorSceneDocument& document,
                                                         const EditorUiState& ui) {
    const bool hasFilter = ui.outlinerFilter[0] != '\0';
    const std::unordered_set<std::uint64_t> filterVisible = buildFilterVisibleSet(document, ui);

    std::vector<OutlinerVisibleRow> rows;
    std::function<void(std::uint64_t)> appendRows = [&](std::uint64_t objectId) {
        const EditorSceneObject* object = document.findObject(objectId);
        if (object == nullptr) {
            return;
        }

        if (hasFilter && !filterVisible.contains(objectId)) {
            return;
        }

        const auto children = document.childObjectIds(objectId);
        const bool expanded = hasFilter || ui.expandedOutlinerIds.contains(objectId);
        rows.push_back(OutlinerVisibleRow{
            .objectId = objectId,
            .parentObjectId = document.parentObjectId(objectId),
            .hasChildren = !children.empty(),
            .expanded = expanded,
        });

        if (!expanded) {
            return;
        }

        for (const auto childId : children) {
            appendRows(childId);
        }
    };

    for (const auto rootId : document.rootObjectIds()) {
        appendRows(rootId);
    }

    return rows;
}

bool applyOutlinerKeyboardNavigation(const EditorSceneDocument& document,
                                     EditorUiState& ui,
                                     std::vector<std::uint64_t>& selectedIds,
                                     OutlinerNavDirection direction,
                                     bool extendRange) {
    const std::vector<OutlinerVisibleRow> rows = buildOutlinerVisibleRows(document, ui);
    if (rows.empty()) {
        return false;
    }

    const std::uint64_t activeId = selectedIds.empty() ? rows.front().objectId : selectedIds.back();
    const int currentIndex = findOutlinerRowIndex(rows, activeId);
    const int resolvedIndex = currentIndex >= 0 ? currentIndex : 0;
    const OutlinerVisibleRow& currentRow = rows[static_cast<std::size_t>(resolvedIndex)];

    auto selectRow = [&](std::uint64_t objectId) {
        if (extendRange && (direction == OutlinerNavDirection::Up || direction == OutlinerNavDirection::Down)) {
            const std::uint64_t anchorId = ui.outlinerAnchorId != 0 ? ui.outlinerAnchorId : activeId;
            int anchorIndex = findOutlinerRowIndex(rows, anchorId);
            int targetIndex = findOutlinerRowIndex(rows, objectId);
            if (anchorIndex < 0) {
                anchorIndex = resolvedIndex;
            }
            if (targetIndex < 0) {
                targetIndex = resolvedIndex;
            }
            if (anchorIndex > targetIndex) {
                std::swap(anchorIndex, targetIndex);
            }
            selectedIds.clear();
            for (int index = anchorIndex; index <= targetIndex; ++index) {
                selectedIds.push_back(rows[static_cast<std::size_t>(index)].objectId);
            }
        } else {
            selectedIds = {objectId};
            ui.outlinerAnchorId = objectId;
        }
        ui.inspectorContext = EditorInspectorContext::SceneSelection;
        ui.scrollToSelection = true;
    };

    switch (direction) {
    case OutlinerNavDirection::Up: {
        const int targetIndex = std::max(resolvedIndex - 1, 0);
        if (rows[static_cast<std::size_t>(targetIndex)].objectId != activeId) {
            selectRow(rows[static_cast<std::size_t>(targetIndex)].objectId);
            return true;
        }
        return false;
    }
    case OutlinerNavDirection::Down: {
        const int targetIndex = std::min(resolvedIndex + 1, static_cast<int>(rows.size()) - 1);
        if (rows[static_cast<std::size_t>(targetIndex)].objectId != activeId) {
            selectRow(rows[static_cast<std::size_t>(targetIndex)].objectId);
            return true;
        }
        return false;
    }
    case OutlinerNavDirection::Right: {
        if (!currentRow.hasChildren) {
            return false;
        }
        if (!currentRow.expanded) {
            ui.expandedOutlinerIds.insert(currentRow.objectId);
            return true;
        }
        const int childIndex = resolvedIndex + 1;
        if (childIndex < static_cast<int>(rows.size())
            && rows[static_cast<std::size_t>(childIndex)].parentObjectId == currentRow.objectId) {
            selectRow(rows[static_cast<std::size_t>(childIndex)].objectId);
            return true;
        }
        return false;
    }
    case OutlinerNavDirection::Left: {
        if (currentRow.hasChildren && currentRow.expanded && ui.outlinerFilter[0] == '\0') {
            ui.expandedOutlinerIds.erase(currentRow.objectId);
            return true;
        }
        if (currentRow.parentObjectId != 0 && currentRow.parentObjectId != activeId) {
            selectRow(currentRow.parentObjectId);
            return true;
        }
        return false;
    }
    }

    return false;
}

std::vector<std::uint64_t> renderOutliner(EditorSceneDocument& document,
                                          EditorUiState& ui,
                                          std::vector<std::uint64_t>& selectedIds,
                                          bool* open,
                                          EditorCommandStack& commandStack) {
    std::vector<std::uint64_t> deleteRequests;
    if (open != nullptr && !*open) {
        return deleteRequests;
    }
    if (!beginCompactEditorPanelWindow("Outliner", open)) {
        ImGui::End();
        return deleteRequests;
    }
    const bool shiftHeld = ImGui::GetIO().KeyShift;
    const bool ctrlHeld = ImGui::GetIO().KeyMods & ImGuiMod_Super
                        || ImGui::GetIO().KeyMods & ImGuiMod_Ctrl;

    const std::vector<OutlinerVisibleRow> visibleRows = buildOutlinerVisibleRows(document, ui);
    std::vector<std::uint64_t> flatOrder;
    flatOrder.reserve(visibleRows.size());
    for (const auto& row : visibleRows) {
        flatOrder.push_back(row.objectId);
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

    // Search/filter bar
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##outliner_filter", "Search...", ui.outlinerFilter, sizeof(ui.outlinerFilter));

    const bool hasFilter = ui.outlinerFilter[0] != '\0';
    const std::unordered_set<std::uint64_t> filterVisible = buildFilterVisibleSet(document, ui);

    ImGui::Separator();

    if (shouldHandleOutlinerKeys()) {
        const auto selectionBefore = selectedIds;
        bool handled = false;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            handled = applyOutlinerKeyboardNavigation(document, ui, selectedIds, OutlinerNavDirection::Up, shiftHeld);
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            handled = applyOutlinerKeyboardNavigation(document, ui, selectedIds, OutlinerNavDirection::Down, shiftHeld);
        } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            handled = applyOutlinerKeyboardNavigation(document, ui, selectedIds, OutlinerNavDirection::Right);
        } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            handled = applyOutlinerKeyboardNavigation(document, ui, selectedIds, OutlinerNavDirection::Left);
        }
        if (handled && selectionBefore != selectedIds) {
            commandStack.pushSelectionCommand("Select", &selectedIds, selectionBefore, selectedIds);
        }
    }

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

        // Skip objects that don't match the filter
        if (hasFilter && filterVisible.find(objectId) == filterVisible.end()) {
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

        const bool shouldOpen = hasFilter
            || expandForScroll.count(objectId) > 0
            || ui.expandedOutlinerIds.contains(objectId);
        ImGui::SetNextItemOpen(shouldOpen, ImGuiCond_Always);

        const bool openNode = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(objectId)),
                                                flags,
                                                "%s",
                                                editorSceneObjectLabel(*object).c_str());
        if (ImGui::IsItemToggledOpen()) {
            if (openNode) {
                ui.expandedOutlinerIds.insert(objectId);
            } else {
                ui.expandedOutlinerIds.erase(objectId);
            }
        }

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
