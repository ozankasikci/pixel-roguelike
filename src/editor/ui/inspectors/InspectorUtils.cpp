#include "editor/ui/inspectors/InspectorUtils.h"

#include "game/behavior/ActionTypes.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct ActionCategoryDef {
    const char* name;
    struct Entry {
        const char* label;
        ActionType type;
    };
    std::vector<Entry> actions;
};

const std::vector<ActionCategoryDef>& coreActionCategories() {
    static const std::vector<ActionCategoryDef> categories = {
        {"Door", {{"Open", ActionType::OpenDoor}, {"Close", ActionType::CloseDoor}, {"Toggle", ActionType::ToggleDoor}}},
        {"Lighting", {{"SetLight", ActionType::SetLight}}},
        {"Audio", {{"PlaySound", ActionType::PlaySound}}},
        {"Entity", {{"EnableEntity", ActionType::EnableEntity}, {"DisableEntity", ActionType::DisableEntity}}},
        {"Timing", {{"Delay", ActionType::Delay}}},
    };
    return categories;
}

int findCategoryIndex(ActionType type) {
    const auto& cats = coreActionCategories();
    for (int c = 0; c < static_cast<int>(cats.size()); ++c) {
        for (const auto& entry : cats[c].actions) {
            if (entry.type == type) return c;
        }
    }
    return 0;
}

int findActionIndexInCategory(ActionType type, int categoryIndex) {
    const auto& cats = coreActionCategories();
    if (categoryIndex < 0 || categoryIndex >= static_cast<int>(cats.size())) return 0;
    for (int a = 0; a < static_cast<int>(cats[categoryIndex].actions.size()); ++a) {
        if (cats[categoryIndex].actions[a].type == type) return a;
    }
    return 0;
}

ActionParams defaultParamsForType(ActionType type) {
    switch (type) {
    case ActionType::OpenDoor:
    case ActionType::CloseDoor:
    case ActionType::ToggleDoor:
        return DoorActionParams{};
    case ActionType::SetLight:
        return LightActionParams{};
    case ActionType::PlaySound:
        return SoundActionParams{};
    case ActionType::Delay:
        return DelayActionParams{};
    case ActionType::EnableEntity:
    case ActionType::DisableEntity:
        return EntityToggleParams{};
    default:
        return DelayActionParams{};
    }
}

// Returns true if any field was modified; sets removeRequested if X is clicked
bool renderActionEntryRow(int index, BehaviorDeclaration& decl,
                          const EditorSceneDocument& document,
                          bool& removeRequested) {
    bool changed = false;
    ImGui::PushID(index);

    if (ImGui::SmallButton("X")) {
        removeRequested = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Remove action");
    }
    ImGui::SameLine();

    const auto& cats = coreActionCategories();
    int catIndex = findCategoryIndex(decl.action.type);
    ImGui::SetNextItemWidth(80.0f);
    if (ImGui::BeginCombo("##cat", cats[catIndex].name)) {
        for (int c = 0; c < static_cast<int>(cats.size()); ++c) {
            if (ImGui::Selectable(cats[c].name, c == catIndex)) {
                catIndex = c;
                decl.action.type = cats[c].actions[0].type;
                decl.action.params = defaultParamsForType(decl.action.type);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();

    int actIndex = findActionIndexInCategory(decl.action.type, catIndex);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::BeginCombo("##act", cats[catIndex].actions[actIndex].label)) {
        for (int a = 0; a < static_cast<int>(cats[catIndex].actions.size()); ++a) {
            if (ImGui::Selectable(cats[catIndex].actions[a].label, a == actIndex)) {
                decl.action.type = cats[catIndex].actions[a].type;
                decl.action.params = defaultParamsForType(decl.action.type);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();

    // Target node ID combo
    ImGui::SetNextItemWidth(120.0f);
    const std::string& currentTarget = decl.action.targetNodeId;
    const char* previewTarget = (currentTarget.empty() || currentTarget == "self") ? "self" : currentTarget.c_str();
    if (ImGui::BeginCombo("##target", previewTarget)) {
        static char targetFilter[64] = {};
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##targetFilter", "Filter...", targetFilter, sizeof(targetFilter));
        ImGui::Separator();

        const std::string filterStr(targetFilter);
        const auto matchesFilter = [&](const std::string& nodeId) {
            if (filterStr.empty()) return true;
            std::string lower = nodeId;
            std::string lowerFilter = filterStr;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
            return lower.find(lowerFilter) != std::string::npos;
        };

        if (matchesFilter("self")) {
            if (ImGui::Selectable("self", currentTarget.empty() || currentTarget == "self")) {
                decl.action.targetNodeId = "self";
                changed = true;
            }
        }
        for (const auto& obj : document.objects()) {
            std::string nodeId;
            std::visit([&](const auto& p) { nodeId = p.nodeId; }, obj.payload);
            if (!nodeId.empty() && matchesFilter(nodeId)) {
                if (ImGui::Selectable(nodeId.c_str(), currentTarget == nodeId)) {
                    decl.action.targetNodeId = nodeId;
                    changed = true;
                }
            }
        }
        ImGui::EndCombo();
    }

    // Validation: stale target
    if (!currentTarget.empty() && currentTarget != "self") {
        bool found = false;
        for (const auto& obj : document.objects()) {
            std::string nodeId;
            std::visit([&](const auto& p) { nodeId = p.nodeId; }, obj.payload);
            if (nodeId == currentTarget) { found = true; break; }
        }
        if (!found) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Target node not found in scene");
        }
    }

    // Per-type parameter widgets
    switch (decl.action.type) {
    case ActionType::SetLight: {
        if (std::holds_alternative<LightActionParams>(decl.action.params)) {
            auto& params = std::get<LightActionParams>(decl.action.params);
            ImGui::Text("Intensity:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            changed |= ImGui::DragFloat("##intensity", &params.intensity, 0.05f, 0.0f, 10.0f, "%.2f");
            ImGui::Text("Color:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(180.0f);
            changed |= ImGui::ColorEdit3("##color", &params.color.x);
            ImGui::Text("Radius:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            changed |= ImGui::DragFloat("##radius", &params.radius, 0.1f, 0.0f, 50.0f, "%.1f");
        }
        break;
    }
    case ActionType::PlaySound: {
        if (std::holds_alternative<SoundActionParams>(decl.action.params)) {
            auto& params = std::get<SoundActionParams>(decl.action.params);
            char soundBuf[64];
            std::snprintf(soundBuf, sizeof(soundBuf), "%s", params.soundId.c_str());
            ImGui::Text("Sound:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputText("##soundId", soundBuf, sizeof(soundBuf))) {
                params.soundId = soundBuf;
                changed = true;
            }
            if (params.soundId.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Sound ID required for PlaySound");
            }
        }
        break;
    }
    case ActionType::Delay: {
        if (std::holds_alternative<DelayActionParams>(decl.action.params)) {
            auto& params = std::get<DelayActionParams>(decl.action.params);
            ImGui::Text("Duration:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            changed |= ImGui::DragFloat("##duration", &params.seconds, 0.05f, 0.0f, 60.0f, "%.2f s");
        }
        break;
    }
    default:
        break;
    }

    ImGui::PopID();
    return changed;
}

} // namespace

void drawBehaviorSections(std::vector<BehaviorDeclaration>& behaviors,
                          EditorSceneDocument& document,
                          EditorCommandStack& commandStack) {
    const char* eventTypes[] = {"on_activate", "on_enter", "on_exit", "on_timer"};
    const char* eventLabels[] = {"On Activate", "On Enter", "On Exit", "On Timer"};

    for (int e = 0; e < 4; ++e) {
        int count = 0;
        for (const auto& b : behaviors) {
            if (b.eventType == eventTypes[e]) ++count;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
        if (count > 0 && e == 0) flags |= ImGuiTreeNodeFlags_DefaultOpen;

        if (ImGui::CollapsingHeader(eventLabels[e], flags)) {
            if (count == 0) {
                ImGui::TextDisabled("(none)");
            }

            int removeIndex = -1;
            int actionIndex = 0;
            for (int i = 0; i < static_cast<int>(behaviors.size()); ++i) {
                if (behaviors[i].eventType != eventTypes[e]) continue;
                ImGui::PushID(i);
                bool removeRequested = false;
                auto beforeState = document.captureState();
                bool changed = renderActionEntryRow(actionIndex, behaviors[i], document, removeRequested);
                if (changed) {
                    commandStack.pushDocumentStateCommand(
                        "Edit Action", beforeState, document.captureState(), document);
                }
                if (removeRequested) {
                    removeIndex = i;
                }
                ImGui::PopID();
                ImGui::Spacing();
                ++actionIndex;
            }

            if (removeIndex >= 0) {
                auto beforeState = document.captureState();
                behaviors.erase(behaviors.begin() + removeIndex);
                commandStack.pushDocumentStateCommand(
                    "Remove Action", beforeState, document.captureState(), document);
            }

            if (ImGui::Button("+ Add Action", ImVec2(-1.0f, 0.0f))) {
                auto beforeState = document.captureState();
                BehaviorDeclaration newDecl;
                newDecl.eventType = eventTypes[e];
                newDecl.action.type = ActionType::ToggleDoor;
                newDecl.action.targetNodeId = "self";
                newDecl.action.params = DoorActionParams{};
                behaviors.push_back(newDecl);
                document.markSceneDirty();
                commandStack.pushDocumentStateCommand(
                    "Add Action", beforeState, document.captureState(), document);
            }
        }
    }
}
