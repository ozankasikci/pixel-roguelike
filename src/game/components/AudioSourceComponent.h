#pragma once
#include <cstdint>

struct AudioSourceComponent {
    uint32_t soundHandle = 0;    // Handle from AudioSystem::loadSound()
    float volume = 1.0f;         // Per-source volume multiplier
    float pitch = 1.0f;          // Playback pitch
    float refDistance = 1.0f;    // Distance at which volume is 100% (per D-16)
    float maxDistance = 50.0f;   // Distance beyond which sound is silent (per D-16)
    bool loop = false;           // Loop playback
    bool is3D = true;            // 3D positional (true) or 2D (false)
    bool playing = false;        // Current playback state
    bool triggerPlay = false;    // Set to true to trigger a play on next update
};
