#include "engine/particles/ParticlePool.h"

#include <algorithm>
#include <numeric>

namespace particles {

ParticlePool::ParticlePool(int capacity)
    : capacity_(capacity)
    , positions_(capacity), velocities_(capacity), colors_(capacity)
    , sizes_(capacity), ages_(capacity), lifetimes_(capacity)
    , rotations_(capacity), rotationSpeeds_(capacity) {}

bool ParticlePool::spawn(const glm::vec3& position, const glm::vec3& velocity,
                         const glm::vec4& color, float size, float lifetime, float rotationSpeed) {
    if (aliveCount_ >= capacity_) return false;
    const int i = aliveCount_;
    positions_[i] = position; velocities_[i] = velocity; colors_[i] = color;
    sizes_[i] = size; ages_[i] = 0.0f; lifetimes_[i] = lifetime;
    rotations_[i] = 0.0f; rotationSpeeds_[i] = rotationSpeed;
    ++aliveCount_;
    return true;
}

void ParticlePool::kill(int index) {
    if (index < 0 || index >= aliveCount_) return;
    const int last = aliveCount_ - 1;
    if (index != last) {
        positions_[index] = positions_[last]; velocities_[index] = velocities_[last];
        colors_[index] = colors_[last]; sizes_[index] = sizes_[last];
        ages_[index] = ages_[last]; lifetimes_[index] = lifetimes_[last];
        rotations_[index] = rotations_[last]; rotationSpeeds_[index] = rotationSpeeds_[last];
    }
    --aliveCount_;
}

void ParticlePool::packOne(ParticleInstance& out, int index) const {
    out.position = positions_[index];
    out.colorPacked = packColorRGBA8(colors_[index]);
    out.size = sizes_[index];
    out.rotation = rotations_[index];
    out.normalizedAge = (lifetimes_[index] > 0.0f) ? ages_[index] / lifetimes_[index] : 1.0f;
}

void ParticlePool::packInstances(ParticleInstance* out, int count) const {
    const int n = std::min(count, aliveCount_);
    for (int i = 0; i < n; ++i) packOne(out[i], i);
}

void ParticlePool::sortByDistance(const glm::vec3& cameraPos, uint32_t* out) const {
    std::iota(out, out + aliveCount_, 0u);
    // Insertion sort: nearly O(n) for mostly-sorted data
    for (int i = 1; i < aliveCount_; ++i) {
        uint32_t key = out[i];
        float keyDist = glm::dot(positions_[key] - cameraPos, positions_[key] - cameraPos);
        int j = i - 1;
        while (j >= 0) {
            float jDist = glm::dot(positions_[out[j]] - cameraPos, positions_[out[j]] - cameraPos);
            if (jDist >= keyDist) break;
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = key;
    }
}

void ParticlePool::packInstancesSorted(ParticleInstance* out, const uint32_t* sortedIndices, int count) const {
    for (int i = 0; i < count; ++i) packOne(out[i], static_cast<int>(sortedIndices[i]));
}

} // namespace particles
