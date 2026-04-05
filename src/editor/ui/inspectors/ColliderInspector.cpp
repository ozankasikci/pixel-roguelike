#include "editor/ui/inspectors/ColliderInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/components/ColliderComponent.h"
#include "game/level/LevelDef.h"

#include <glm/common.hpp>
#include <imgui.h>

void drawColliderInspector(LevelColliderPlacement& collider,
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

    // Shape dropdown
    static constexpr const char* kShapeNames[] = {"Box", "Sphere", "Cylinder", "Capsule"};
    int shapeIndex = static_cast<int>(collider.shape);
    if (renderInspectorPropertyRow("Shape", [&]() { return ImGui::Combo("##value", &shapeIndex, kShapeNames, 4); },
                                  EditorInspectorFieldKind::Enum)) {
        const EditorSceneDocumentState before = document.captureState();
        collider.shape = static_cast<ColliderShape>(shapeIndex);
        document.markSceneDirty();
        commandStack.pushDocumentStateCommand("Change Collider Shape", before, document.captureState(), document);
    }

    // Mode dropdown
    static constexpr const char* kModeNames[] = {"Solid", "Trigger", "Solid+Trigger"};
    int modeIndex = static_cast<int>(collider.mode);
    if (renderInspectorPropertyRow("Mode", [&]() { return ImGui::Combo("##value", &modeIndex, kModeNames, 3); },
                                  EditorInspectorFieldKind::Enum)) {
        const EditorSceneDocumentState before = document.captureState();
        collider.mode = static_cast<ColliderMode>(modeIndex);
        document.markSceneDirty();
        commandStack.pushDocumentStateCommand("Change Collider Mode", before, document.captureState(), document);
    }

    // Position + Rotation
    drawTransformSection(collider.position, collider.rotation,
                         "Position", "Rotation",
                         "Move Collider", "Rotate Collider",
                         document, commandStack, pendingCommand);

    // Shape-specific params
    auto itemBefore = document.captureState();
    if (collider.shape == ColliderShape::Box) {
        itemBefore = document.captureState();
        const bool extentsChanged = renderInspectorPropertyRow("Half Extents", [&]() { return editVec3("##value", collider.halfExtents, 0.02f); });
        if (extentsChanged) {
            collider.halfExtents = glm::max(collider.halfExtents, glm::vec3(0.01f));
        }
        trackSceneItem(itemBefore, "Resize Collider", extentsChanged);
    } else if (collider.shape == ColliderShape::Sphere) {
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Adjust Collider Radius", renderInspectorPropertyRow("Radius", [&]() { return ImGui::DragFloat("##value", &collider.radius, 0.02f, 0.05f, 20.0f, "%.2f"); }));
    } else {
        // Cylinder or Capsule
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Adjust Collider Radius", renderInspectorPropertyRow("Radius", [&]() { return ImGui::DragFloat("##value", &collider.radius, 0.02f, 0.05f, 20.0f, "%.2f"); }));
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Adjust Collider Height", renderInspectorPropertyRow("Half Height", [&]() { return ImGui::DragFloat("##value", &collider.halfHeight, 0.02f, 0.05f, 20.0f, "%.2f"); }));
    }
    endInspectorPropertyTable();

    // Trigger-specific options (shown when mode != Solid)
    if (collider.mode != ColliderMode::Solid) {
        ImGui::Separator();
        {
            auto fireBefore = document.captureState();
            if (ImGui::Checkbox("Fire Once", &collider.fireOnce)) {
                document.markSceneDirty();
                commandStack.pushDocumentStateCommand("Toggle Fire Once", fireBefore, document.captureState(), document);
            }
        }
        ImGui::Separator();
        drawBehaviorSections(collider.behaviors, document, commandStack);
    }
}
