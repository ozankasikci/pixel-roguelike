#pragma once

#include "engine/particles/ParticleTypes.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace particles {

class ParticlePool {
public:
    explicit ParticlePool(int capacity);

    int capacity() const { return capacity_; }
    int aliveCount() const { return aliveCount_; }

    bool spawn(const glm::vec3& position, const glm::vec3& velocity, const glm::vec4& color,
               float size, float lifetime, float rotationSpeed);
    void kill(int index);
    void packInstances(ParticleInstance* out, int count) const;
    void sortByDistance(const glm::vec3& cameraPos, uint32_t* out) const;
    void packInstancesSorted(ParticleInstance* out, const uint32_t* sortedIndices, int count) const;

    glm::vec3* positions() { return positions_.data(); }
    glm::vec3* velocities() { return velocities_.data(); }
    glm::vec4* colors() { return colors_.data(); }
    float* sizes() { return sizes_.data(); }
    float* ages() { return ages_.data(); }
    float* lifetimes() { return lifetimes_.data(); }
    float* rotations() { return rotations_.data(); }
    float* rotationSpeeds() { return rotationSpeeds_.data(); }
    const glm::vec3* positions() const { return positions_.data(); }
    const float* ages() const { return ages_.data(); }
    const float* lifetimes() const { return lifetimes_.data(); }

private:
    void packOne(ParticleInstance& out, int index) const;

    int capacity_;
    int aliveCount_ = 0;
    std::vector<glm::vec3> positions_;
    std::vector<glm::vec3> velocities_;
    std::vector<glm::vec4> colors_;
    std::vector<float> sizes_;
    std::vector<float> ages_;
    std::vector<float> lifetimes_;
    std::vector<float> rotations_;
    std::vector<float> rotationSpeeds_;
};

} // namespace particles
