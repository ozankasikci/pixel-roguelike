#pragma once

#include <string>
#include <variant>

#include <glm/vec3.hpp>

#include "engine/audio/VoiceHandle.h"

namespace engine::audio {

struct PlayCommand {
    std::string eventName;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float pitch = 1.0f;
    bool is3D = true;
};

struct PlayLoopingCommand {
    std::string eventName;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float pitch = 1.0f;
    bool is3D = true;
};

struct StopCommand {
    VoiceHandle handle;
    float fadeTime = 0.0f;
};

struct UpdateVoicePositionCommand {
    VoiceHandle handle;
    glm::vec3 position{0.0f};
};

struct SetBusVolumeCommand {
    std::string busName;
    float volume = 1.0f;
};

struct SetBusMuteCommand {
    std::string busName;
    bool mute = false;
};

struct SetListenerCommand {
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
};

struct SetReverbPresetCommand {
    std::string presetName;
};

using AudioCommand = std::variant<PlayCommand, PlayLoopingCommand, StopCommand,
                                  UpdateVoicePositionCommand, SetBusVolumeCommand,
                                  SetBusMuteCommand, SetListenerCommand, SetReverbPresetCommand>;

} // namespace engine::audio
