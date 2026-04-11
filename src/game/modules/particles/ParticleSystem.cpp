#include "game/modules/particles/ParticleSystem.h"

#include "game/components/ParticleEmitterComponent.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/particles/ParticleEmitterDefinition.h"
#include "engine/particles/EmitterShape.h"
#include "engine/particles/ForceFunction.h"
#include "engine/particles/ValueAnimator.h"

void ParticleUpdateSystem::init(const ContentRegistry& content) {
    content_ = &content;
}

void ParticleUpdateSystem::ensureEmitter(entt::entity entity,
                                          const std::string& emitterId) {
    if (emitters_.count(entity)) return;
    if (!content_) return;

    const auto* def = content_->findParticleEmitter(emitterId);
    if (!def) return;

    auto resolved = resolveParticleEmitterDefinition(emitterId,
                                                      content_->particleEmitters());

    particles::ParticleEmitterConfig config;
    config.maxParticles = resolved.maxParticles;
    config.emissionRate = resolved.emissionRate;
    config.looping = resolved.looping;
    config.duration = resolved.duration;
    config.simulationSpace = (resolved.simulationSpace == "local")
        ? particles::SimulationSpace::Local
        : particles::SimulationSpace::World;
    config.blendMode = (resolved.blendMode == "alpha")
        ? particles::BlendMode::AlphaBlend
        : particles::BlendMode::Additive;
    config.initialSpeedMin = resolved.initialSpeedMin;
    config.initialSpeedMax = resolved.initialSpeedMax;
    config.lifetimeMin = resolved.lifetimeMin;
    config.lifetimeMax = resolved.lifetimeMax;
    config.rotationSpeedMin = resolved.rotationSpeedMin;
    config.rotationSpeedMax = resolved.rotationSpeedMax;
    config.warmUp = resolved.warmUp;
    config.softParticleFade = resolved.softParticleFade;
    config.emissiveStrength = resolved.emissiveStrength;

    auto emitter = std::make_unique<particles::ParticleEmitter>(config);

    if (resolved.shapeType == "sphere") {
        emitter->setShape(
            std::make_unique<particles::SphereShape>(resolved.shapeParam0));
    } else if (resolved.shapeType == "cone") {
        emitter->setShape(std::make_unique<particles::ConeShape>(
            resolved.shapeParam0, resolved.shapeParam1));
    } else {
        emitter->setShape(std::make_unique<particles::PointShape>());
    }

    for (const auto& f : resolved.forces) {
        if (f.type == "gravity") {
            emitter->addForce(
                std::make_unique<particles::GravityForce>(f.value));
        } else if (f.type == "drag") {
            emitter->addForce(
                std::make_unique<particles::DragForce>(f.coefficient));
        }
    }

    if (!resolved.colorStops.empty()) {
        emitter->setColorAnimator(
            std::make_unique<particles::ColorGradient>(resolved.colorStops));
    } else {
        emitter->setColorAnimator(
            std::make_unique<particles::Constant<glm::vec4>>(glm::vec4(1.0f)));
    }

    if (!resolved.sizeKeyframes.empty()) {
        emitter->setSizeAnimator(
            std::make_unique<particles::FloatCurve>(resolved.sizeKeyframes));
    } else {
        emitter->setSizeAnimator(
            std::make_unique<particles::Constant<float>>(0.1f));
    }

    if (resolved.warmUp) {
        float warmDuration = (resolved.duration > 0.0f)
            ? resolved.duration
            : resolved.lifetimeMax;
        emitter->warmUp(warmDuration);
    }

    emitters_.emplace(entity, std::move(emitter));
}

void ParticleUpdateSystem::update(GameRegistry& registry,
                                   float deltaTime,
                                   const glm::vec3& cameraPos) {
    renderBatches_.clear();

    auto view = registry.view<ParticleEmitterComponent, TransformComponent>();
    for (auto [entity, particle, transform] : view.each()) {
        if (!particle.enabled) continue;

        ensureEmitter(entity, particle.emitterId);
        auto it = emitters_.find(entity);
        if (it == emitters_.end()) continue;

        it->second->update(deltaTime, transform.modelMatrix(), cameraPos);
        auto batch = it->second->buildRenderBatch();
        if (batch.count > 0) {
            renderBatches_.push_back(batch);
        }
    }

    // Clean up emitters for destroyed entities
    for (auto it = emitters_.begin(); it != emitters_.end();) {
        if (!registry.valid(it->first)
            || !registry.all_of<ParticleEmitterComponent>(it->first)) {
            it = emitters_.erase(it);
        } else {
            ++it;
        }
    }
}
