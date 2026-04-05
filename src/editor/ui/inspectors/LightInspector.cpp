#include "editor/ui/inspectors/LightInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

void drawLightInspector(LevelLightPlacement& light,
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

    int typeIndex = static_cast<int>(light.type);
    const char* lightTypes[] = {"Point", "Spot", "Directional"};
    if (renderInspectorPropertyRow("Light Type", [&]() { return ImGui::Combo("##value", &typeIndex, lightTypes, 3); },
                                  EditorInspectorFieldKind::Enum)) {
        const EditorSceneDocumentState before = document.captureState();
        light.type = static_cast<LightType>(typeIndex);
        document.markSceneDirty();
        commandStack.pushDocumentStateCommand("Change Light Type", before, document.captureState(), document);
    }
    if (light.type != LightType::Directional) {
        const auto itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Move Light", renderInspectorPropertyRow("Position", [&]() { return editVec3("##value", light.position); }));
    }
    auto itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Adjust Light Direction", renderInspectorPropertyRow("Direction", [&]() { return editVec3("##value", light.direction, 0.01f); }));
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Change Light Color", renderInspectorPropertyRow("Color", [&]() { return editColor("##value", light.color); }));
    itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Adjust Light Intensity", renderInspectorPropertyRow("Intensity", [&]() { return ImGui::DragFloat("##value", &light.intensity, 0.01f, 0.0f, 10.0f, "%.2f"); }));
    if (light.type != LightType::Directional) {
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Adjust Light Radius", renderInspectorPropertyRow("Radius", [&]() { return ImGui::DragFloat("##value", &light.radius, 0.05f, 0.1f, 40.0f, "%.2f"); }));
    }
    if (light.type == LightType::Spot) {
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Adjust Spot Inner Cone", renderInspectorPropertyRow("Inner Cone", [&]() { return ImGui::DragFloat("##value", &light.innerConeDegrees, 0.5f, 1.0f, 85.0f, "%.1f"); }));
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Adjust Spot Outer Cone", renderInspectorPropertyRow("Outer Cone", [&]() { return ImGui::DragFloat("##value", &light.outerConeDegrees, 0.5f, 1.0f, 89.0f, "%.1f"); }));
        itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Toggle Spot Shadows", renderInspectorPropertyRow("Casts Shadows", [&]() { return ImGui::Checkbox("##value", &light.castsShadows); }));
    }
    endInspectorPropertyTable();
    ImGui::Separator();
    drawBehaviorSections(light.behaviors, document, commandStack);
}
