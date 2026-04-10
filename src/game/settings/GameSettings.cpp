#include "game/settings/GameSettings.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

bool loadGameSettings(const std::string& path, GameSettings& out) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error& e) {
        spdlog::warn("Failed to parse game settings '{}': {}", path, e.what());
        return false;
    }

    if (!root.is_object()) {
        return false;
    }

    if (root.contains("audio") && root["audio"].is_object()) {
        const auto& audio = root["audio"];
        if (audio.contains("masterVolume") && audio["masterVolume"].is_number()) {
            out.audio.masterVolume = audio["masterVolume"].get<float>();
        }
        if (audio.contains("sfxVolume") && audio["sfxVolume"].is_number()) {
            out.audio.sfxVolume = audio["sfxVolume"].get<float>();
        }
        if (audio.contains("musicVolume") && audio["musicVolume"].is_number()) {
            out.audio.musicVolume = audio["musicVolume"].get<float>();
        }
        if (audio.contains("ambienceVolume") && audio["ambienceVolume"].is_number()) {
            out.audio.ambienceVolume = audio["ambienceVolume"].get<float>();
        }
        if (audio.contains("uiVolume") && audio["uiVolume"].is_number()) {
            out.audio.uiVolume = audio["uiVolume"].get<float>();
        }
        if (audio.contains("footstepInterval") && audio["footstepInterval"].is_number()) {
            out.audio.footstepInterval = audio["footstepInterval"].get<float>();
        }
        if (audio.contains("footstepVolume") && audio["footstepVolume"].is_number()) {
            out.audio.footstepVolume = audio["footstepVolume"].get<float>();
        }
        if (audio.contains("reverbPreset") && audio["reverbPreset"].is_string()) {
            out.audio.reverbPreset = audio["reverbPreset"].get<std::string>();
        }
    }

    return true;
}

bool saveGameSettings(const std::string& path, const GameSettings& settings) {
    nlohmann::json root;
    nlohmann::json audio;
    audio["masterVolume"] = settings.audio.masterVolume;
    audio["sfxVolume"] = settings.audio.sfxVolume;
    audio["musicVolume"] = settings.audio.musicVolume;
    audio["ambienceVolume"] = settings.audio.ambienceVolume;
    audio["uiVolume"] = settings.audio.uiVolume;
    audio["footstepInterval"] = settings.audio.footstepInterval;
    audio["footstepVolume"] = settings.audio.footstepVolume;
    audio["reverbPreset"] = settings.audio.reverbPreset;
    root["audio"] = audio;

    std::ofstream file(path);
    if (!file.is_open()) {
        spdlog::error("Failed to write game settings to '{}'", path);
        return false;
    }

    file << root.dump(4) << std::endl;
    return true;
}
