#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include <random>

namespace particles {

class EmitterShape {
public:
    virtual ~EmitterShape() = default;
    virtual glm::vec3 samplePosition(std::mt19937& rng) const = 0;
    virtual glm::vec3 sampleDirection(std::mt19937& rng) const = 0;
};

inline glm::vec3 randomOnUnitSphere(std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    glm::vec3 v;
    float lengthSq;
    do {
        v = glm::vec3(dist(rng), dist(rng), dist(rng));
        lengthSq = glm::dot(v, v);
    } while (lengthSq > 1.0f || lengthSq < 1e-6f);
    return v / std::sqrt(lengthSq);
}

class PointShape : public EmitterShape {
public:
    glm::vec3 samplePosition(std::mt19937&) const override { return glm::vec3(0.0f); }
    glm::vec3 sampleDirection(std::mt19937& rng) const override { return randomOnUnitSphere(rng); }
};

class SphereShape : public EmitterShape {
public:
    explicit SphereShape(float radius, bool surfaceOnly = false)
        : radius_(radius), surfaceOnly_(surfaceOnly) {}

    glm::vec3 samplePosition(std::mt19937& rng) const override {
        glm::vec3 dir = randomOnUnitSphere(rng);
        if (surfaceOnly_) return dir * radius_;
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float r = radius_ * std::cbrt(dist(rng));
        return dir * r;
    }

    glm::vec3 sampleDirection(std::mt19937& rng) const override {
        glm::vec3 pos = samplePosition(rng);
        float len = glm::length(pos);
        return (len > 1e-6f) ? pos / len : randomOnUnitSphere(rng);
    }

private:
    float radius_;
    bool surfaceOnly_;
};

class ConeShape : public EmitterShape {
public:
    ConeShape(float angleDegrees, float baseRadius)
        : cosAngle_(std::cos(glm::radians(angleDegrees))), baseRadius_(baseRadius) {}

    glm::vec3 samplePosition(std::mt19937& rng) const override {
        std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
        std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
        float angle = angleDist(rng);
        float r = baseRadius_ * std::sqrt(radiusDist(rng));
        return glm::vec3(r * std::cos(angle), 0.0f, r * std::sin(angle));
    }

    glm::vec3 sampleDirection(std::mt19937& rng) const override {
        std::uniform_real_distribution<float> cosThetaDist(cosAngle_, 1.0f);
        std::uniform_real_distribution<float> phiDist(0.0f, glm::two_pi<float>());
        float cosTheta = cosThetaDist(rng);
        float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
        float phi = phiDist(rng);
        return glm::normalize(glm::vec3(sinTheta * std::cos(phi), cosTheta, sinTheta * std::sin(phi)));
    }

private:
    float cosAngle_;
    float baseRadius_;
};

} // namespace particles
