#include "engine/audio/mix/Voice.h"

#include <algorithm>
#include <cmath>

#include <glm/geometric.hpp>

#include "engine/audio/mix/BusGraph.h"

namespace engine::audio {

float Voice::computeDistanceAttenuation(const glm::vec3& listenerPos) const {
    if (!is3D) {
        return 1.0f;
    }

    float dist = glm::distance(position, listenerPos);
    dist = std::max(dist, refDistance);

    if (dist >= maxDistance) {
        return 0.0f;
    }

    return refDistance / dist;
}

float Voice::computeAudibility(const glm::vec3& listenerPos, const BusGraph& graph) const {
    float attenuation = computeDistanceAttenuation(listenerPos);
    float bus_vol = graph.effectiveVolume(busName);
    float priority_factor = static_cast<float>(priority) / 128.0f;
    return volume * attenuation * bus_vol * priority_factor;
}

} // namespace engine::audio
