#pragma once

#include "engine/particles/EmitterShape.h"
#include "engine/particles/ForceFunction.h"
#include "engine/particles/ParticlePool.h"
#include "engine/particles/ParticleTypes.h"
#include "engine/particles/ValueAnimator.h"

#include <glm/glm.hpp>

#include <memory>
#include <random>
#include <vector>

namespace particles {

struct BurstEvent { float time; int count; };

struct ParticleEmitterConfig {
    int maxParticles = 256;
    float emissionRate = 10.0f;
    std::vector<BurstEvent> bursts;
    bool looping = true;
    float duration = 0.0f;
    SimulationSpace simulationSpace = SimulationSpace::World;
    BlendMode blendMode = BlendMode::Additive;
    float initialSpeedMin = 1.0f;
    float initialSpeedMax = 3.0f;
    float lifetimeMin = 0.5f;
    float lifetimeMax = 2.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;
    bool warmUp = false;
    float softParticleFade = 0.5f;
    float emissiveStrength = 1.0f;
    GLuint texture = 0;
};

class ParticleEmitter {
public:
    explicit ParticleEmitter(const ParticleEmitterConfig& config);

    void setShape(std::unique_ptr<EmitterShape> shape);
    void addForce(std::unique_ptr<ForceFunction> force);
    void setColorAnimator(std::unique_ptr<ValueAnimator<glm::vec4>> animator);
    void setSizeAnimator(std::unique_ptr<ValueAnimator<float>> animator);
    void setOpacityAnimator(std::unique_ptr<ValueAnimator<float>> animator);

    void update(float dt, const glm::mat4& emitterTransform, const glm::vec3& cameraPos);
    void stop();
    void warmUp(float duration, float fixedDt = 1.0f / 60.0f);

    EmitterState state() const { return state_; }
    const ParticlePool& pool() const { return pool_; }
    const ParticleEmitterConfig& config() const { return config_; }
    ParticleRenderBatch buildRenderBatch();

private:
    void spawnParticles(float dt, const glm::mat4& emitterTransform);
    void updateParticles(float dt);
    void killDeadParticles();

    ParticleEmitterConfig config_;
    ParticlePool pool_;
    EmitterState state_ = EmitterState::Playing;
    float emitterAge_ = 0.0f;
    float emissionAccumulator_ = 0.0f;
    std::mt19937 rng_{42};
    glm::vec3 lastCameraPos_{0.0f};

    std::unique_ptr<EmitterShape> shape_;
    std::vector<std::unique_ptr<ForceFunction>> forces_;
    std::unique_ptr<ValueAnimator<glm::vec4>> colorAnimator_;
    std::unique_ptr<ValueAnimator<float>> sizeAnimator_;
    std::unique_ptr<ValueAnimator<float>> opacityAnimator_;

    std::vector<ParticleInstance> instanceBuffer_;
    std::vector<uint32_t> sortIndices_;
};

} // namespace particles
