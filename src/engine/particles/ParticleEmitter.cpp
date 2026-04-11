#include "engine/particles/ParticleEmitter.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace particles {

ParticleEmitter::ParticleEmitter(const ParticleEmitterConfig& config)
    : config_(config), pool_(config.maxParticles)
    , instanceBuffer_(config.maxParticles), sortIndices_(config.maxParticles) {}

void ParticleEmitter::setShape(std::unique_ptr<EmitterShape> shape) { shape_ = std::move(shape); }
void ParticleEmitter::addForce(std::unique_ptr<ForceFunction> force) { forces_.push_back(std::move(force)); }
void ParticleEmitter::setColorAnimator(std::unique_ptr<ValueAnimator<glm::vec4>> a) { colorAnimator_ = std::move(a); }
void ParticleEmitter::setSizeAnimator(std::unique_ptr<ValueAnimator<float>> a) { sizeAnimator_ = std::move(a); }
void ParticleEmitter::setOpacityAnimator(std::unique_ptr<ValueAnimator<float>> a) { opacityAnimator_ = std::move(a); }

void ParticleEmitter::stop() {
    if (state_ == EmitterState::Playing) state_ = EmitterState::Stopping;
}

void ParticleEmitter::warmUp(float duration, float fixedDt) {
    const int steps = static_cast<int>(duration / fixedDt);
    for (int i = 0; i < steps; ++i) update(fixedDt, glm::mat4(1.0f), glm::vec3(0.0f));
}

void ParticleEmitter::update(float dt, const glm::mat4& emitterTransform, const glm::vec3& cameraPos) {
    lastCameraPos_ = cameraPos;
    if (state_ == EmitterState::Stopped) return;

    if (state_ == EmitterState::Playing) {
        spawnParticles(dt, emitterTransform);
        emitterAge_ += dt;
        if (config_.duration > 0.0f && emitterAge_ >= config_.duration) {
            if (config_.looping) emitterAge_ = 0.0f;
            else state_ = EmitterState::Stopping;
        }
    }

    updateParticles(dt);
    killDeadParticles();

    if (state_ == EmitterState::Stopping && pool_.aliveCount() == 0) state_ = EmitterState::Stopped;
}

void ParticleEmitter::spawnParticles(float dt, const glm::mat4& emitterTransform) {
    if (!shape_) return;
    emissionAccumulator_ += config_.emissionRate * dt;
    int burstCount = 0;
    for (const auto& burst : config_.bursts) {
        if (emitterAge_ <= burst.time && emitterAge_ + dt > burst.time) burstCount += burst.count;
    }
    int toSpawn = static_cast<int>(emissionAccumulator_) + burstCount;
    emissionAccumulator_ -= std::floor(emissionAccumulator_);

    std::uniform_real_distribution<float> speedDist(config_.initialSpeedMin, config_.initialSpeedMax);
    std::uniform_real_distribution<float> lifeDist(config_.lifetimeMin, config_.lifetimeMax);
    std::uniform_real_distribution<float> rotDist(config_.rotationSpeedMin, config_.rotationSpeedMax);

    for (int i = 0; i < toSpawn; ++i) {
        glm::vec3 localPos = shape_->samplePosition(rng_);
        glm::vec3 localDir = shape_->sampleDirection(rng_);
        glm::vec3 worldPos, worldVel;
        if (config_.simulationSpace == SimulationSpace::World) {
            worldPos = glm::vec3(emitterTransform * glm::vec4(localPos, 1.0f));
            worldVel = glm::vec3(emitterTransform * glm::vec4(localDir, 0.0f)) * speedDist(rng_);
        } else {
            worldPos = localPos;
            worldVel = localDir * speedDist(rng_);
        }
        glm::vec4 color = colorAnimator_ ? colorAnimator_->evaluate(0.0f) : glm::vec4(1.0f);
        float size = sizeAnimator_ ? sizeAnimator_->evaluate(0.0f) : 0.1f;
        pool_.spawn(worldPos, worldVel, color, size, lifeDist(rng_), rotDist(rng_));
    }
}

void ParticleEmitter::updateParticles(float dt) {
    const int count = pool_.aliveCount();
    auto* pos = pool_.positions(); auto* vel = pool_.velocities();
    auto* colors = pool_.colors(); auto* sizes = pool_.sizes();
    auto* ages = pool_.ages(); auto* lifetimes = pool_.lifetimes();
    auto* rots = pool_.rotations(); auto* rotSpeeds = pool_.rotationSpeeds();

    for (int i = 0; i < count; ++i) {
        ages[i] += dt;
        for (const auto& force : forces_) vel[i] = force->apply(pos[i], vel[i], dt);
        pos[i] += vel[i] * dt;
        rots[i] += rotSpeeds[i] * dt;
        float t = glm::clamp((lifetimes[i] > 0.0f) ? ages[i] / lifetimes[i] : 1.0f, 0.0f, 1.0f);
        if (colorAnimator_) colors[i] = colorAnimator_->evaluate(t);
        if (sizeAnimator_) sizes[i] = sizeAnimator_->evaluate(t);
        if (opacityAnimator_) colors[i].a *= opacityAnimator_->evaluate(t);
    }
}

void ParticleEmitter::killDeadParticles() {
    const auto* ages = pool_.ages();
    const auto* lifetimes = pool_.lifetimes();
    for (int i = pool_.aliveCount() - 1; i >= 0; --i) {
        if (ages[i] >= lifetimes[i]) pool_.kill(i);
    }
}

ParticleRenderBatch ParticleEmitter::buildRenderBatch() {
    const int count = pool_.aliveCount();
    if (count == 0) return {};
    if (config_.blendMode == BlendMode::AlphaBlend) {
        pool_.sortByDistance(lastCameraPos_, sortIndices_.data());
        pool_.packInstancesSorted(instanceBuffer_.data(), sortIndices_.data(), count);
    } else {
        pool_.packInstances(instanceBuffer_.data(), count);
    }
    ParticleRenderBatch batch;
    batch.instances = instanceBuffer_.data();
    batch.count = count;
    batch.blendMode = config_.blendMode;
    batch.texture = config_.texture;
    batch.emissiveStrength = config_.emissiveStrength;
    batch.softParticleFade = config_.softParticleFade;
    return batch;
}

} // namespace particles
