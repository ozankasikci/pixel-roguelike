#pragma once

#include <string>

struct AudioSettings {
    float masterVolume = 1.0f;
    float sfxVolume = 1.0f;
    float musicVolume = 1.0f;
    float ambienceVolume = 1.0f;
    float uiVolume = 1.0f;
    float footstepInterval = 0.5f;
    float footstepVolume = 0.4f;
    std::string reverbPreset = "None";
};

struct GameSettings {
    AudioSettings audio;
};

bool loadGameSettings(const std::string& path, GameSettings& out);
bool saveGameSettings(const std::string& path, const GameSettings& settings);
