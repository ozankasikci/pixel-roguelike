#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace engine::audio {

enum class PickMode { Random, RandomNoRepeat, RoundRobin, Sequential };

struct SoundEventDef {
    std::string name;
    std::vector<std::string> sounds;
    PickMode pickMode = PickMode::Random;
    glm::vec2 pitchRange{1.0f, 1.0f};
    glm::vec2 volumeRange{1.0f, 1.0f};
    int maxInstances = 8;
    float cooldown = 0.0f;
    std::string busName = "SFX";
    bool is3D = true;
    float refDistance = 1.0f;
    float maxDistance = 50.0f;

    // Mutable pick state — modified by const pickSound()
    mutable int lastPickIndex = -1;
    mutable int sequentialIndex = 0;

    /// Pick a sound index based on the current pick mode.
    /// Returns -1 if sounds is empty.
    int pickSound() const {
        if (sounds.empty()) {
            return -1;
        }
        int count = static_cast<int>(sounds.size());
        int picked = 0;

        switch (pickMode) {
        case PickMode::Random:
            picked = std::rand() % count;
            break;

        case PickMode::RandomNoRepeat:
            if (count == 1) {
                picked = 0;
            } else {
                do {
                    picked = std::rand() % count;
                } while (picked == lastPickIndex);
            }
            break;

        case PickMode::RoundRobin:
            picked = sequentialIndex % count;
            sequentialIndex = (sequentialIndex + 1) % count;
            break;

        case PickMode::Sequential:
            picked = sequentialIndex % count;
            sequentialIndex = (sequentialIndex + 1) % count;
            break;
        }

        lastPickIndex = picked;
        return picked;
    }

    /// Return a random pitch within pitchRange.
    float randomPitch() const {
        if (pitchRange.x == pitchRange.y) {
            return pitchRange.x;
        }
        float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        return pitchRange.x + t * (pitchRange.y - pitchRange.x);
    }

    /// Return a random volume within volumeRange.
    float randomVolume() const {
        if (volumeRange.x == volumeRange.y) {
            return volumeRange.x;
        }
        float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        return volumeRange.x + t * (volumeRange.y - volumeRange.x);
    }
};

} // namespace engine::audio
