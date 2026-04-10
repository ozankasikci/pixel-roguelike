#pragma once
#include <cstdint>
#include <string>

struct AudioSourceComponent {
    std::string eventName;       // Sound event name resolved by AudioEngine
    float volume = 1.0f;         // Per-source volume multiplier
    float pitch = 1.0f;          // Playback pitch
    bool loop = false;           // Loop playback
    bool is3D = true;            // 3D positional (true) or 2D (false)
    bool playing = false;        // Current playback state
    bool triggerPlay = false;    // Set to true to trigger a play on next update
};
