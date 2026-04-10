#include "engine/audio/mix/VoiceManager.h"

#include <algorithm>

#include "engine/audio/mix/BusGraph.h"

namespace engine::audio {

VoiceManager::VoiceManager(int poolSize)
    : poolSize_(poolSize) {
}

VoiceHandle VoiceManager::spawn(const std::string& eventName,
                                               const glm::vec3& position, float volume,
                                               float pitch, uint8_t priority,
                                               const std::string& busName, bool is3D,
                                               bool looping, float refDistance, float maxDistance,
                                               uint32_t bufferHandle) {
    VoiceHandle handle(nextId_++, generation_);

    Voice v;
    v.handle = handle;
    v.eventName = eventName;
    v.position = position;
    v.volume = volume;
    v.pitch = pitch;
    v.priority = priority;
    v.busName = busName;
    v.is3D = is3D;
    v.looping = looping;
    v.state = VoiceState::Playing;
    v.refDistance = refDistance;
    v.maxDistance = maxDistance;
    v.bufferHandle = bufferHandle;
    v.elapsedTime = 0.0f;
    v.occlusion = 0.0f;
    v.sourceIndex = -1;

    voices_.push_back(std::move(v));
    return handle;
}

void VoiceManager::stop(VoiceHandle handle, float fadeTime) {
    Voice* v = findVoice(handle);
    if (!v) {
        return;
    }

    if (fadeTime <= 0.0f) {
        v->state = VoiceState::Stopped;
    } else {
        v->state = VoiceState::Stopping;
        v->fadeTime = fadeTime;
        v->fadeElapsed = 0.0f;
        v->fadeVolume = 1.0f;
    }
}

void VoiceManager::stopAllByEvent(const std::string& eventName) {
    for (auto& v : voices_) {
        if (v.eventName == eventName && v.state != VoiceState::Stopped) {
            v.state = VoiceState::Stopped;
        }
    }
}

void VoiceManager::stopAll() {
    for (auto& v : voices_) {
        v.state = VoiceState::Stopped;
    }
    voices_.clear();
}

void VoiceManager::updatePosition(VoiceHandle handle, const glm::vec3& position) {
    Voice* v = findVoice(handle);
    if (v) {
        v->position = position;
    }
}

void VoiceManager::update(const glm::vec3& listenerPos, const BusGraph& graph, float deltaTime) {
    // 1. Advance fade for Stopping voices; mark as Stopped when fade completes
    for (auto& v : voices_) {
        if (v.state == VoiceState::Stopping) {
            v.fadeElapsed += deltaTime;
            if (v.fadeTime > 0.0f) {
                v.fadeVolume = 1.0f - (v.fadeElapsed / v.fadeTime);
            } else {
                v.fadeVolume = 0.0f;
            }
            if (v.fadeVolume <= 0.0f) {
                v.fadeVolume = 0.0f;
                v.state = VoiceState::Stopped;
            }
        }
    }

    // 2. Remove stopped voices
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                 [](const Voice& v) {
                                     return v.state == VoiceState::Stopped;
                                 }),
                  voices_.end());

    if (voices_.empty()) {
        return;
    }

    // 3. Compute audibility scores and sort descending
    struct ScoredIndex {
        int index;
        float score;
    };

    std::vector<ScoredIndex> scored;
    scored.reserve(voices_.size());
    for (int i = 0; i < static_cast<int>(voices_.size()); ++i) {
        float score = voices_[i].computeAudibility(listenerPos, graph);
        scored.push_back({i, score});
    }

    std::sort(scored.begin(), scored.end(),
              [](const ScoredIndex& a, const ScoredIndex& b) { return a.score > b.score; });

    // 4. Stable source assignment: keep existing assignments, only reassign as needed
    std::vector<bool> usedSources(poolSize_, false);

    // First pass: determine which voices are in the top N (eligible for sources)
    std::vector<bool> inTopN(voices_.size(), false);
    for (int rank = 0; rank < std::min(static_cast<int>(scored.size()), poolSize_); ++rank) {
        inTopN[scored[rank].index] = true;
    }

    // Second pass: voices that already have a source and are still in top N keep it
    for (int i = 0; i < static_cast<int>(voices_.size()); ++i) {
        Voice& v = voices_[i];
        if (v.sourceIndex >= 0 && v.sourceIndex < poolSize_ && inTopN[i]) {
            usedSources[v.sourceIndex] = true;
        } else if (!inTopN[i]) {
            // Voice dropped out of top N — free its source
            if (v.state != VoiceState::Stopping) {
                v.state = VoiceState::Virtual;
            }
            v.sourceIndex = -1;
        }
    }

    // Third pass: assign free sources to voices in top N that don't have one yet
    int nextFree = 0;
    for (int rank = 0; rank < std::min(static_cast<int>(scored.size()), poolSize_); ++rank) {
        Voice& v = voices_[scored[rank].index];
        if (v.sourceIndex >= 0 && usedSources[v.sourceIndex]) {
            // Already has a stable assignment
            if (v.state == VoiceState::Virtual) {
                v.state = VoiceState::Playing;
            }
            continue;
        }
        // Find next free source
        while (nextFree < poolSize_ && usedSources[nextFree]) {
            ++nextFree;
        }
        if (nextFree < poolSize_) {
            v.sourceIndex = nextFree;
            usedSources[nextFree] = true;
            if (v.state == VoiceState::Virtual) {
                v.state = VoiceState::Playing;
            } else if (v.state != VoiceState::Stopping) {
                v.state = VoiceState::Playing;
            }
            ++nextFree;
        }
    }

    // 5. Advance elapsed time
    for (auto& v : voices_) {
        v.elapsedTime += deltaTime;
    }
}

Voice* VoiceManager::findVoice(VoiceHandle handle) {
    for (auto& v : voices_) {
        if (v.handle == handle) {
            return &v;
        }
    }
    return nullptr;
}

const Voice* VoiceManager::findVoice(VoiceHandle handle) const {
    for (const auto& v : voices_) {
        if (v.handle == handle) {
            return &v;
        }
    }
    return nullptr;
}

int VoiceManager::activeVoiceCount() const {
    int count = 0;
    for (const auto& v : voices_) {
        if (v.state == VoiceState::Playing || v.state == VoiceState::Stopping) {
            ++count;
        }
    }
    return count;
}

int VoiceManager::countByEvent(const std::string& eventName) const {
    int count = 0;
    for (const auto& v : voices_) {
        if (v.eventName == eventName && v.state != VoiceState::Stopped) {
            ++count;
        }
    }
    return count;
}

std::vector<Voice>& VoiceManager::voices() {
    return voices_;
}

const std::vector<Voice>& VoiceManager::voices() const {
    return voices_;
}

} // namespace engine::audio
