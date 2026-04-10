#include "engine/audio/mix/OcclusionProcessor.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "engine/audio/mix/Voice.h"

namespace audio {

OcclusionProcessor::OcclusionProcessor(float queryRate)
    : queryInterval_(1.0f / std::max(queryRate, 1.0f)) {
}

void OcclusionProcessor::setRaycastFunc(RaycastFunc func) {
    raycastFunc_ = std::move(func);
}

void OcclusionProcessor::update(std::vector<Voice>& voices, const glm::vec3& listenerPos,
                                float deltaTime) {
    // If no raycast function is installed, clear all occlusion and bail.
    if (!raycastFunc_) {
        for (auto& voice : voices) {
            voice.occlusion = 0.0f;
        }
        return;
    }

    // Rate-limit the raycast queries.
    accumulator_ += deltaTime;
    bool should_query = accumulator_ >= queryInterval_;
    if (should_query) {
        accumulator_ -= queryInterval_;
        // Prevent spiral if frames are very long.
        if (accumulator_ > queryInterval_) {
            accumulator_ = 0.0f;
        }
    }

    for (auto& voice : voices) {
        // Skip voices that are not active 3D sources.
        if (voice.state == VoiceState::Stopped || voice.state == VoiceState::Virtual ||
            !voice.is3D) {
            continue;
        }

        float target = voice.occlusion; // default: keep current value

        if (should_query) {
            glm::vec3 to_voice = voice.position - listenerPos;
            float dist = glm::length(to_voice);

            if (dist < 1e-4f) {
                // Voice is essentially at the listener position — not occluded.
                target = 0.0f;
            } else {
                glm::vec3 direction = to_voice / dist;
                bool blocked = raycastFunc_(listenerPos, direction, dist);
                target = blocked ? 1.0f : 0.0f;
            }
        }

        // Smooth blend toward the target occlusion value.
        float diff = target - voice.occlusion;
        float max_step = kBlendSpeed * deltaTime;

        if (std::abs(diff) <= max_step) {
            voice.occlusion = target;
        } else {
            voice.occlusion += (diff > 0.0f ? max_step : -max_step);
        }

        // Clamp to valid range.
        voice.occlusion = std::clamp(voice.occlusion, 0.0f, 1.0f);
    }
}

} // namespace audio
