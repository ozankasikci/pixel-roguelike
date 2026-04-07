#include "editor/ui/inspectors/DoorGroupInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/core/EditorCommand.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

#include <cstring>

void drawDoorGroupInspector(LevelDoorPlacement& dg,
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
        std::strncpy(buf, dg.name.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        const auto before = document.captureState();
        if (ImGui::InputText("##value", buf, sizeof(buf))) {
            dg.name = buf;
        }
        trackItem(before, "Door Group Name", ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Position
    renderInspectorPropertyRow("Position", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat3("##value", &dg.position.x, 0.05f);
        trackItem(before, "Door Group Position", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Yaw
    renderInspectorPropertyRow("Yaw", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat("##value", &dg.yawDegrees, 0.5f, -360.0f, 360.0f);
        trackItem(before, "Door Group Yaw", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    ImGui::Separator();
    ImGui::TextUnformatted("Door Behavior");

    // Open Angle (negative = opposite direction)
    renderInspectorPropertyRow("Open Angle", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat("##value", &dg.openAngle, 0.5f, -180.0f, 180.0f, "%.1f deg");
        trackItem(before, "Open Angle", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Open Duration
    renderInspectorPropertyRow("Open Duration", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat("##value", &dg.openDuration, 0.01f, 0.1f, 5.0f);
        trackItem(before, "Open Duration", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Interact Distance
    renderInspectorPropertyRow("Interact Dist", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::DragFloat("##value", &dg.interactDistance, 0.05f, 0.5f, 10.0f);
        trackItem(before, "Interact Distance", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    // Interact Dot
    renderInspectorPropertyRow("Interact Dot", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::SliderFloat("##value", &dg.interactDotThreshold, 0.0f, 1.0f);
        trackItem(before, "Interact Dot", changed && ImGui::IsItemDeactivatedAfterEdit());
        return false;
    });

    ImGui::Separator();

    // Locked
    renderInspectorPropertyRow("Locked", [&]() {
        const auto before = document.captureState();
        bool changed = ImGui::Checkbox("##value", &dg.locked);
        trackItem(before, "Locked", changed);
        return false;
    });

    // Locked Prompt (only when locked)
    if (dg.locked) {
        renderInspectorPropertyRow("Lock Prompt", [&]() {
            char buf[256];
            std::strncpy(buf, dg.lockedPrompt.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            const auto before = document.captureState();
            if (ImGui::InputText("##value", buf, sizeof(buf))) {
                dg.lockedPrompt = buf;
            }
            trackItem(before, "Locked Prompt", ImGui::IsItemDeactivatedAfterEdit());
            return false;
        });
    }

    endInspectorPropertyTable();
}
