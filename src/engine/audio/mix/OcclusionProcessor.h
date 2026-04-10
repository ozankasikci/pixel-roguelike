#pragma once

#include <functional>
#include <vector>

#include <glm/vec3.hpp>

namespace engine::audio {

struct Voice;

/// Callback type for line-of-sight raycasts.  Returns true if the ray hits
/// geometry (i.e. the path is blocked).
using RaycastFunc =
    std::function<bool(const glm::vec3& origin, const glm::vec3& direction, float maxDistance)>;

/// Rate-limited, raycast-based occlusion processor for spatial audio voices.
///
/// Each active 3D voice is periodically tested for line-of-sight to the
/// listener.  The resulting occlusion factor is smoothly blended rather than
/// snapping instantly, producing gradual muffling as sources move behind walls.
///
/// The game layer provides a RaycastFunc (typically backed by Jolt Physics);
/// if none is set, all occlusion values are cleared to zero.
class OcclusionProcessor {
public:
    /// @param queryRate  How many raycast queries per second (default 10 Hz).
    explicit OcclusionProcessor(float queryRate = 10.0f);

    // Non-copyable
    OcclusionProcessor(const OcclusionProcessor&) = delete;
    OcclusionProcessor& operator=(const OcclusionProcessor&) = delete;

    /// Install the raycast callback.  Pass nullptr / empty function to disable.
    void setRaycastFunc(RaycastFunc func);

    /// Process occlusion for all voices.
    ///
    /// @param voices       The active voice pool.
    /// @param listenerPos  Current listener world position.
    /// @param deltaTime    Frame delta in seconds.
    void update(std::vector<Voice>& voices, const glm::vec3& listenerPos, float deltaTime);

private:
    RaycastFunc raycastFunc_;
    float queryInterval_; // seconds between raycast batches
    float accumulator_ = 0.0f;

    static constexpr float kBlendSpeed = 8.0f; // occlusion blend rate (units/sec)
};

} // namespace engine::audio
