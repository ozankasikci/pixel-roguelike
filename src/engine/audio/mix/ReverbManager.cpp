#include "engine/audio/mix/ReverbManager.h"

#include <algorithm>

namespace audio {

ReverbManager::ReverbManager() {
    // "None" — silence, no reverb
    ReverbParams none{};
    none.gain = 0.0f;
    none.decayTime = 0.1f;
    none.reflectionsGain = 0.0f;
    none.lateReverbGain = 0.0f;
    presets_["None"] = none;

    // "Cell" — small room, short decay, high density
    ReverbParams cell{};
    cell.density = 1.0f;
    cell.diffusion = 0.9f;
    cell.gain = 0.4f;
    cell.gainHF = 0.8f;
    cell.decayTime = 0.6f;
    cell.decayHFRatio = 0.7f;
    cell.reflectionsGain = 0.15f;
    cell.reflectionsDelay = 0.003f;
    cell.lateReverbGain = 0.6f;
    cell.lateReverbDelay = 0.005f;
    cell.airAbsorption = 0.994f;
    cell.roomRolloff = 0.0f;
    presets_["Cell"] = cell;

    // "Corridor" — medium decay, narrow, lower density
    ReverbParams corridor{};
    corridor.density = 0.4f;
    corridor.diffusion = 0.7f;
    corridor.gain = 0.35f;
    corridor.gainHF = 0.75f;
    corridor.decayTime = 1.8f;
    corridor.decayHFRatio = 0.6f;
    corridor.reflectionsGain = 0.1f;
    corridor.reflectionsDelay = 0.01f;
    corridor.lateReverbGain = 0.9f;
    corridor.lateReverbDelay = 0.02f;
    corridor.airAbsorption = 0.992f;
    corridor.roomRolloff = 0.0f;
    presets_["Corridor"] = corridor;

    // "OpenArea" — long decay, low density
    ReverbParams openArea{};
    openArea.density = 0.2f;
    openArea.diffusion = 0.5f;
    openArea.gain = 0.3f;
    openArea.gainHF = 0.6f;
    openArea.decayTime = 3.2f;
    openArea.decayHFRatio = 0.5f;
    openArea.reflectionsGain = 0.05f;
    openArea.reflectionsDelay = 0.02f;
    openArea.lateReverbGain = 0.7f;
    openArea.lateReverbDelay = 0.04f;
    openArea.airAbsorption = 0.990f;
    openArea.roomRolloff = 0.0f;
    presets_["OpenArea"] = openArea;

    current_ = "None";
}

void ReverbManager::addPreset(const std::string& name, const ReverbParams& params) {
    presets_[name] = params;
}

bool ReverbManager::hasPreset(const std::string& name) const {
    return presets_.find(name) != presets_.end();
}

ReverbParams ReverbManager::presetParams(const std::string& name) const {
    auto it = presets_.find(name);
    if (it != presets_.end()) {
        return it->second;
    }
    return ReverbParams{};
}

void ReverbManager::setPreset(const std::string& name) {
    if (presets_.find(name) != presets_.end()) {
        current_ = name;
        transitioning_ = false;
        progress_ = 0.0f;
        duration_ = 0.0f;
        target_.clear();
    }
}

void ReverbManager::beginTransition(const std::string& targetPreset, float duration) {
    if (presets_.find(targetPreset) == presets_.end()) {
        return;
    }
    if (duration <= 0.0f) {
        setPreset(targetPreset);
        return;
    }
    target_ = targetPreset;
    duration_ = duration;
    progress_ = 0.0f;
    transitioning_ = true;
}

void ReverbManager::updateTransition(float deltaTime) {
    if (!transitioning_) {
        return;
    }
    progress_ += deltaTime / duration_;
    if (progress_ >= 1.0f) {
        progress_ = 1.0f;
        current_ = target_;
        target_.clear();
        transitioning_ = false;
    }
}

bool ReverbManager::isTransitioning() const {
    return transitioning_;
}

float ReverbManager::transitionProgress() const {
    return progress_;
}

const std::string& ReverbManager::currentPreset() const {
    return current_;
}

const std::string& ReverbManager::targetPreset() const {
    return target_;
}

ReverbParams ReverbManager::currentParams() const {
    auto srcIt = presets_.find(current_);
    ReverbParams src = (srcIt != presets_.end()) ? srcIt->second : ReverbParams{};

    if (!transitioning_) {
        return src;
    }

    auto dstIt = presets_.find(target_);
    ReverbParams dst = (dstIt != presets_.end()) ? dstIt->second : ReverbParams{};

    return lerp(src, dst, progress_);
}

ReverbParams ReverbManager::lerp(const ReverbParams& a, const ReverbParams& b, float t) {
    float s = 1.0f - t;
    ReverbParams result{};
    result.density = s * a.density + t * b.density;
    result.diffusion = s * a.diffusion + t * b.diffusion;
    result.gain = s * a.gain + t * b.gain;
    result.gainHF = s * a.gainHF + t * b.gainHF;
    result.decayTime = s * a.decayTime + t * b.decayTime;
    result.decayHFRatio = s * a.decayHFRatio + t * b.decayHFRatio;
    result.reflectionsGain = s * a.reflectionsGain + t * b.reflectionsGain;
    result.reflectionsDelay = s * a.reflectionsDelay + t * b.reflectionsDelay;
    result.lateReverbGain = s * a.lateReverbGain + t * b.lateReverbGain;
    result.lateReverbDelay = s * a.lateReverbDelay + t * b.lateReverbDelay;
    result.airAbsorption = s * a.airAbsorption + t * b.airAbsorption;
    result.roomRolloff = s * a.roomRolloff + t * b.roomRolloff;
    return result;
}

} // namespace audio
