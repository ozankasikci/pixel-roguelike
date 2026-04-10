#pragma once

#include <string>
#include <unordered_map>

#include "engine/audio/ReverbParams.h"

namespace engine::audio {

/// Manages reverb presets and crossfades between them.
/// Stores named ReverbParams presets and provides linear interpolation
/// during transitions for smooth room-to-room reverb changes.
class ReverbManager {
public:
    ReverbManager();

    // Non-copyable
    ReverbManager(const ReverbManager&) = delete;
    ReverbManager& operator=(const ReverbManager&) = delete;

    /// Add or overwrite a named preset.
    void addPreset(const std::string& name, const ReverbParams& params);

    /// Return true if a preset with the given name exists.
    bool hasPreset(const std::string& name) const;

    /// Return the parameters for a named preset.
    /// Returns default ReverbParams if the name is not found.
    ReverbParams presetParams(const std::string& name) const;

    /// Immediately switch to the named preset (no crossfade).
    void setPreset(const std::string& name);

    /// Begin a crossfade transition from the current preset to the target.
    /// Duration is in seconds.
    void beginTransition(const std::string& targetPreset, float duration);

    /// Advance the transition blend by deltaTime seconds.
    void updateTransition(float deltaTime);

    /// Return true if a crossfade transition is in progress.
    bool isTransitioning() const;

    /// Return the current transition progress in [0, 1].
    float transitionProgress() const;

    /// Return the name of the current (or source) preset.
    const std::string& currentPreset() const;

    /// Return the name of the target preset during a transition.
    /// Returns an empty string if no transition is active.
    const std::string& targetPreset() const;

    /// Return the interpolated ReverbParams for the current state.
    /// During a transition, linearly blends between source and target.
    ReverbParams currentParams() const;

private:
    static ReverbParams lerp(const ReverbParams& a, const ReverbParams& b, float t);

    std::unordered_map<std::string, ReverbParams> presets_;
    std::string current_;
    std::string target_;
    float progress_ = 0.0f;
    float duration_ = 0.0f;
    bool transitioning_ = false;
};

} // namespace engine::audio
