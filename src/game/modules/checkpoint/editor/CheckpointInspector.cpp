#include "game/modules/checkpoint/editor/CheckpointInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/core/EditorCommand.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

#include <cstring>

void drawCheckpointInspector(LevelCheckpointPlacement& cp,
                              EditorSceneDocument& document,
                              EditorCommandStack& commandStack,
                              EditorPendingCommand& pendingCommand,
                              const EditorSceneDocumentState& beforeState) {
    (void)beforeState;

    const auto trackItem = [&](const EditorSceneDocumentState& itemBefore,
                                const std::string& label, bool changed) {
        if (changed) {
            document.markSceneDirty();
        }
        trackLastItemCommand(itemBefore, label, pendingCommand, commandStack, document);
    };

    // Name
    renderInspectorPropertyRow("Name", [&]() {
        char buf[128];
        std::strncpy(buf, cp.name.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        const auto before = document.captureState();
        if (ImGui::InputText("##value", buf, sizeof(buf))) {
            cp.name = buf;
        }
        trackItem(before, "Checkpoint Name", ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Position
    drawPositionSection(cp.position, "Position", "Move Checkpoint",
                        document, commandStack, pendingCommand);

    // Respawn Position
    drawPositionSection(cp.respawnPosition, "Respawn Pos", "Move Checkpoint Respawn",
                        document, commandStack, pendingCommand);

    ImGui::Separator();
    ImGui::TextUnformatted("Interaction");

    // Interact Distance
    renderInspectorPropertyRow("Interact Dist", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat("##value", &cp.interactDistance, 0.05f, 0.5f, 10.0f);
        trackItem(before, "Interact Distance", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Interact Dot
    renderInspectorPropertyRow("Interact Dot", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::SliderFloat("##value", &cp.interactDotThreshold, 0.0f, 1.0f);
        trackItem(before, "Interact Dot", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    ImGui::Separator();
    ImGui::TextUnformatted("Light");

    // Light Offset
    renderInspectorPropertyRow("Light Offset", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat3("##value", &cp.lightOffset.x, 0.05f);
        trackItem(before, "Light Offset", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Light Color
    renderInspectorPropertyRow("Light Color", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::ColorEdit3("##value", &cp.lightColor.x,
                                          ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        trackItem(before, "Light Color", changed);
        return false;
    });

    // Light Radius
    renderInspectorPropertyRow("Light Radius", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat("##value", &cp.lightRadius, 0.1f, 0.1f, 50.0f);
        trackItem(before, "Light Radius", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Light Intensity
    renderInspectorPropertyRow("Intensity", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat("##value", &cp.lightIntensity, 0.05f, 0.0f, 50.0f);
        trackItem(before, "Light Intensity", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    endInspectorPropertyTable();
}
