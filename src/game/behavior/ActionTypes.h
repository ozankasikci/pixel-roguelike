#pragma once

#include "game/modules/checkpoint/CheckpointActionTypes.h"
#include "game/modules/door/DoorActionTypes.h"
#include "game/modules/player_control/PlayerControlActionTypes.h"

#include <glm/glm.hpp>
#include <string>
#include <variant>
#include <cstdint>

enum class ActionType : uint8_t {
    OpenDoor,
    CloseDoor,
    ToggleDoor,
    ActivateCheckpoint,
    PlaySound,
    SetLight,
    FlickerLight,
    ShowMessage,
    Delay,
    EnableEntity,
    DisableEntity,
    EmitEvent,
    LockPlayer,
    UnlockPlayer,
    TeleportPlayer,
};

// DoorActionParams is defined in game/modules/door/DoorActionTypes.h

struct SoundActionParams {
    std::string soundId;
    float volume = 1.0f;
};

struct LightActionParams {
    float intensity = 1.0f;
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float radius = 0.0f;  // 0 = don't change
};

struct FlickerLightParams {
    float duration = 1.0f;
    float rate = 10.0f;
};

struct MessageActionParams {
    std::string text;
    float duration = 3.0f;
};

struct DelayActionParams {
    float seconds = 1.0f;
};

struct EntityToggleParams {
    // No extra params needed -- target is in ActionEntry
};

struct EventActionParams {
    std::string eventName;
};

using ActionParams = std::variant<
    DoorActionParams,
    ActivateCheckpointParams,
    SoundActionParams,
    LightActionParams,
    FlickerLightParams,
    MessageActionParams,
    DelayActionParams,
    EntityToggleParams,
    EventActionParams,
    PlayerLockParams,
    TeleportPlayerParams
>;

struct ActionEntry {
    ActionType type = ActionType::Delay;
    std::string targetNodeId;        // "self" or named node ID
    float delay = 0.0f;             // seconds before action fires
    bool fireOnce = false;
    ActionParams params;
    bool fired = false;              // runtime state for fireOnce tracking
};
