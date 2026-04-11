#pragma once

#include <glm/glm.hpp>

#include <algorithm>

namespace particles {

class ForceFunction {
public:
    virtual ~ForceFunction() = default;
    virtual glm::vec3 apply(const glm::vec3& position, const glm::vec3& velocity, float dt) const = 0;
};

class GravityForce : public ForceFunction {
public:
    explicit GravityForce(glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f)) : gravity_(gravity) {}
    glm::vec3 apply(const glm::vec3&, const glm::vec3& velocity, float dt) const override {
        return velocity + gravity_ * dt;
    }
private:
    glm::vec3 gravity_;
};

class DragForce : public ForceFunction {
public:
    explicit DragForce(float coefficient) : coefficient_(coefficient) {}
    glm::vec3 apply(const glm::vec3&, const glm::vec3& velocity, float dt) const override {
        float factor = std::max(0.0f, 1.0f - coefficient_ * dt);
        return velocity * factor;
    }
private:
    float coefficient_;
};

} // namespace particles
