#include "game/particles/ParticleEmitterDefinition.h"
#include "common/TestSupport.h"

#include <cassert>
#include <filesystem>
#include <fstream>

int main() {
    // Parse basic definition
    {
        const auto def = loadParticleEmitterDefinition(PARTICLE_DEF_FILE);
        assert(def.id == "torch_sparks");
        assert(def.maxParticles.has_value() && *def.maxParticles == 256);
        assert(def.emissionRate.has_value() && test_support::nearlyEqual(*def.emissionRate, 30.0f));
        assert(def.looping.has_value() && *def.looping == true);
        assert(def.blendMode.has_value() && *def.blendMode == "additive");
        assert(def.lifetimeMin.has_value() && test_support::nearlyEqual(*def.lifetimeMin, 0.3f));
        assert(def.lifetimeMax.has_value() && test_support::nearlyEqual(*def.lifetimeMax, 0.8f));
        assert(def.shapeType.has_value() && *def.shapeType == "cone");
        assert(def.shapeParam0.has_value() && test_support::nearlyEqual(*def.shapeParam0, 25.0f));
        assert(def.shapeParam1.has_value() && test_support::nearlyEqual(*def.shapeParam1, 0.05f));
        assert(def.forces.size() == 2);
        assert(def.forces[0].type == "gravity");
        assert(test_support::nearlyEqual(def.forces[0].value.y, -4.0f));
        assert(def.forces[1].type == "drag");
        assert(test_support::nearlyEqual(def.forces[1].coefficient, 0.5f));
        assert(def.colorStops.size() == 3);
        assert(def.sizeKeyframes.size() == 2);
    }

    // Parent inheritance
    {
        const auto parent = loadParticleEmitterDefinition(PARTICLE_PARENT_FILE);
        const auto child = loadParticleEmitterDefinition(PARTICLE_CHILD_FILE);
        assert(child.parent.has_value() && *child.parent == "base_emitter");

        std::unordered_map<std::string, ParticleEmitterDefinition> defs;
        defs.emplace(parent.id, parent);
        defs.emplace(child.id, child);

        const auto resolved = resolveParticleEmitterDefinition("child_emitter", defs);
        assert(resolved.maxParticles == 512);
        assert(test_support::nearlyEqual(resolved.emissionRate, 50.0f));
        assert(resolved.shapeType == "point");
    }

    // Roundtrip
    {
        ParticleEmitterDefinition def;
        def.id = "roundtrip_test";
        def.maxParticles = 128;
        def.emissionRate = 20.0f;
        def.blendMode = "alpha";
        def.lifetimeMin = 1.0f;
        def.lifetimeMax = 3.0f;
        def.shapeType = "sphere";
        def.shapeParam0 = 2.0f;

        const auto path = test_support::tempPath("roundtrip_test.particle");
        saveParticleEmitterDefinition(path.string(), def);
        const auto loaded = loadParticleEmitterDefinition(path.string());
        std::filesystem::remove(path);

        assert(loaded.id == "roundtrip_test");
        assert(*loaded.maxParticles == 128);
        assert(test_support::nearlyEqual(*loaded.emissionRate, 20.0f));
        assert(*loaded.blendMode == "alpha");
        assert(*loaded.shapeType == "sphere");
        assert(test_support::nearlyEqual(*loaded.shapeParam0, 2.0f));
    }

    return 0;
}
