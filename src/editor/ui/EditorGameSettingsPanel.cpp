#include "editor/ui/EditorGameSettingsPanel.h"

#include "editor/ui/LevelEditorUi.h"
#include "game/settings/GameSettings.h"

#include <imgui.h>

bool renderGameSettingsPanel(GameSettings& settings, bool* open) {
    if (open != nullptr && !*open) {
        return false;
    }
    if (!beginCompactEditorPanelWindow("Game Settings", open)) {
        ImGui::End();
        return false;
    }

    bool changed = false;

    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SeparatorText("Volume");

        float masterPct = settings.audio.masterVolume * 100.0f;
        if (ImGui::SliderFloat("Master", &masterPct, 0.0f, 100.0f, "%.0f%%")) {
            settings.audio.masterVolume = masterPct / 100.0f;
            changed = true;
        }

        float sfxPct = settings.audio.sfxVolume * 100.0f;
        if (ImGui::SliderFloat("SFX", &sfxPct, 0.0f, 100.0f, "%.0f%%")) {
            settings.audio.sfxVolume = sfxPct / 100.0f;
            changed = true;
        }

        float musicPct = settings.audio.musicVolume * 100.0f;
        if (ImGui::SliderFloat("Music", &musicPct, 0.0f, 100.0f, "%.0f%%")) {
            settings.audio.musicVolume = musicPct / 100.0f;
            changed = true;
        }

        float ambiencePct = settings.audio.ambienceVolume * 100.0f;
        if (ImGui::SliderFloat("Ambience", &ambiencePct, 0.0f, 100.0f, "%.0f%%")) {
            settings.audio.ambienceVolume = ambiencePct / 100.0f;
            changed = true;
        }

        float uiPct = settings.audio.uiVolume * 100.0f;
        if (ImGui::SliderFloat("UI", &uiPct, 0.0f, 100.0f, "%.0f%%")) {
            settings.audio.uiVolume = uiPct / 100.0f;
            changed = true;
        }

        ImGui::SeparatorText("Footsteps");

        if (ImGui::SliderFloat("Interval", &settings.audio.footstepInterval, 0.1f, 2.0f, "%.2f s")) {
            changed = true;
        }

        float footstepPct = settings.audio.footstepVolume * 100.0f;
        if (ImGui::SliderFloat("Volume##footstep", &footstepPct, 0.0f, 100.0f, "%.0f%%")) {
            settings.audio.footstepVolume = footstepPct / 100.0f;
            changed = true;
        }

        ImGui::SeparatorText("Reverb");

        static const char* kReverbPresets[] = {"None", "Cell", "Corridor", "OpenArea"};
        int currentPreset = 0;
        for (int i = 0; i < 4; ++i) {
            if (settings.audio.reverbPreset == kReverbPresets[i]) {
                currentPreset = i;
                break;
            }
        }
        if (ImGui::Combo("Reverb Preset", &currentPreset, kReverbPresets, 4)) {
            settings.audio.reverbPreset = kReverbPresets[currentPreset];
            changed = true;
        }
    }

    ImGui::End();
    return changed;
}
