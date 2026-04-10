#include "engine/audio/mix/VoiceManager.h"

#include <algorithm>

#include "engine/audio/mix/BusGraph.h"

namespace audio {

VoiceManager::VoiceManager(int poolSize)
    : poolSize_(poolSize) {
}

engine::audio::VoiceHandle VoiceManager::spawn(const std::string& eventName,
                                               const glm::vec3& position, float volume,
                                               float pitch, uint8_t priority,
                                               const std::string& busName, bool is3D,
                                               bool looping, float refDistance, float maxDistance,
                                               uint32_t bufferHandle) {
    engine::audio::VoiceHandle handle(nextId_++, generation_);

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

void VoiceManager::stop(engine::audio::VoiceHandle handle, float fadeTime) {
    Voice* v = findVoice(handle);
    if (!v) {
        return;
    }

    if (fadeTime <= 0.0f) {
        v->state = VoiceState::Stopped;
    } else {
        v->state = VoiceState::Stopping;
    }
}

void VoiceManager::stopAllByEvent(const std::string& eventName) {
    for (auto& v : voices_) {
        if (v.eventName == eventName && v.state != VoiceState::Stopped) {
            v.state = VoiceState::Stopped;
        }
    }
}

void VoiceManager::updatePosition(engine::audio::VoiceHandle handle, const glm::vec3& position) {
    Voice* v = findVoice(handle);
    if (v) {
        v->position = position;
    }
}

void VoiceManager::update(const glm::vec3& listenerPos, const BusGraph& graph, float deltaTime) {
    // 1. Remove stopped voices
    voices_.erase(std::remove_if(voices_.begin(), voices_.end(),
                                 [](const Voice& v) {
                                     return v.state == VoiceState::Stopped;
                                 }),
                  voices_.end());

    if (voices_.empty()) {
        return;
    }

    // 2. Compute audibility scores and sort descending
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

    // 3. Top N get source indices (Playing), rest go Virtual
    for (int rank = 0; rank < static_cast<int>(scored.size()); ++rank) {
        Voice& v = voices_[scored[rank].index];
        if (v.state == VoiceState::Stopping) {
            // Stopping voices keep their state but may lose source
            if (rank < poolSize_) {
                v.sourceIndex = rank;
            } else {
                v.sourceIndex = -1;
            }
            continue;
        }
        if (rank < poolSize_) {
            v.state = VoiceState::Playing;
            v.sourceIndex = rank;
        } else {
            v.state = VoiceState::Virtual;
            v.sourceIndex = -1;
        }
    }

    // 4. Advance elapsed time
    for (auto& v : voices_) {
        v.elapsedTime += deltaTime;
    }
}

Voice* VoiceManager::findVoice(engine::audio::VoiceHandle handle) {
    for (auto& v : voices_) {
        if (v.handle == handle) {
            return &v;
        }
    }
    return nullptr;
}

const Voice* VoiceManager::findVoice(engine::audio::VoiceHandle handle) const {
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

} // namespace audio
