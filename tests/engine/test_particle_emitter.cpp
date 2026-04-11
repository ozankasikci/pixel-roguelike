#include "engine/particles/ParticleEmitter.h"
#include "engine/particles/EmitterShape.h"
#include "engine/particles/ForceFunction.h"
#include "engine/particles/ValueAnimator.h"

#include <cassert>
#include <cmath>

int main() {
    using namespace particles;

    // Basic emission at constant rate
    {
        ParticleEmitterConfig cfg; cfg.maxParticles=128; cfg.emissionRate=100.0f;
        cfg.lifetimeMin=1.0f; cfg.lifetimeMax=1.0f; cfg.initialSpeedMin=1.0f; cfg.initialSpeedMax=1.0f;
        ParticleEmitter e(cfg);
        e.setShape(std::make_unique<PointShape>());
        e.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        e.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));
        e.update(0.1f, glm::mat4(1.0f), glm::vec3(0.0f));
        assert(e.pool().aliveCount() >= 9 && e.pool().aliveCount() <= 11);
    }

    // Gravity affects particles
    {
        ParticleEmitterConfig cfg; cfg.maxParticles=4; cfg.emissionRate=1000.0f;
        cfg.lifetimeMin=10.0f; cfg.lifetimeMax=10.0f; cfg.initialSpeedMin=0.0f; cfg.initialSpeedMax=0.0f;
        ParticleEmitter e(cfg);
        e.setShape(std::make_unique<PointShape>());
        e.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        e.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));
        e.addForce(std::make_unique<GravityForce>(glm::vec3(0.0f, -10.0f, 0.0f)));
        e.update(1.0f, glm::mat4(1.0f), glm::vec3(0.0f));
        for (int i = 0; i < e.pool().aliveCount(); ++i) assert(e.pool().positions()[i].y < -1.0f);
    }

    // State transitions
    {
        ParticleEmitterConfig cfg; cfg.maxParticles=32; cfg.emissionRate=100.0f;
        cfg.lifetimeMin=0.1f; cfg.lifetimeMax=0.1f;
        ParticleEmitter e(cfg);
        e.setShape(std::make_unique<PointShape>());
        e.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        e.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));
        assert(e.state() == EmitterState::Playing);
        e.stop();
        assert(e.state() == EmitterState::Stopping);
        for (int i = 0; i < 20; ++i) e.update(0.05f, glm::mat4(1.0f), glm::vec3(0.0f));
        assert(e.state() == EmitterState::Stopped);
        assert(e.pool().aliveCount() == 0);
    }

    // Capacity limit
    {
        ParticleEmitterConfig cfg; cfg.maxParticles=8; cfg.emissionRate=10000.0f;
        cfg.lifetimeMin=100.0f; cfg.lifetimeMax=100.0f;
        ParticleEmitter e(cfg);
        e.setShape(std::make_unique<PointShape>());
        e.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        e.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));
        e.update(1.0f, glm::mat4(1.0f), glm::vec3(0.0f));
        assert(e.pool().aliveCount() == 8);
    }

    // buildRenderBatch returns valid data
    {
        ParticleEmitterConfig cfg; cfg.maxParticles=16; cfg.emissionRate=100.0f;
        cfg.lifetimeMin=5.0f; cfg.lifetimeMax=5.0f; cfg.initialSpeedMin=1.0f; cfg.initialSpeedMax=1.0f;
        ParticleEmitter e(cfg);
        e.setShape(std::make_unique<PointShape>());
        e.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1,0,0,1)));
        e.setSizeAnimator(std::make_unique<Constant<float>>(0.2f));
        e.update(0.1f, glm::mat4(1.0f), glm::vec3(0.0f));
        auto batch = e.buildRenderBatch();
        assert(batch.count > 0);
        assert(batch.instances != nullptr);
        assert(batch.blendMode == BlendMode::Additive);
    }

    return 0;
}
