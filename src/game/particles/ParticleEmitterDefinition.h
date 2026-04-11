#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct ParticleForceDeclaration {
    std::string type;
    glm::vec3 value{0.0f};
    float coefficient = 0.0f;
};

struct ParticleEmitterDefinition {
    std::string id;
    std::optional<std::string> parent;
    std::optional<int> maxParticles;
    std::optional<float> emissionRate;
    std::optional<bool> looping;
    std::optional<float> duration;
    std::optional<std::string> simulationSpace;
    std::optional<std::string> blendMode;
    std::optional<std::string> texture;
    std::optional<bool> warmUp;
    std::optional<float> softParticleFade;
    std::optional<float> emissiveStrength;
    std::optional<float> lifetimeMin;
    std::optional<float> lifetimeMax;
    std::optional<float> initialSpeedMin;
    std::optional<float> initialSpeedMax;
    std::optional<float> rotationSpeedMin;
    std::optional<float> rotationSpeedMax;
    std::optional<std::string> shapeType;
    std::optional<float> shapeParam0;
    std::optional<float> shapeParam1;
    std::vector<ParticleForceDeclaration> forces;
    std::vector<std::pair<float, glm::vec4>> colorStops;
    std::vector<std::pair<float, float>> sizeKeyframes;
};

struct ResolvedParticleEmitterDefinition {
    std::string id;
    int maxParticles = 256;
    float emissionRate = 10.0f;
    bool looping = true;
    float duration = 0.0f;
    std::string simulationSpace = "world";
    std::string blendMode = "additive";
    std::string texture;
    bool warmUp = false;
    float softParticleFade = 0.5f;
    float emissiveStrength = 1.0f;
    float lifetimeMin = 0.5f;
    float lifetimeMax = 2.0f;
    float initialSpeedMin = 1.0f;
    float initialSpeedMax = 3.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;
    std::string shapeType = "point";
    float shapeParam0 = 0.0f;
    float shapeParam1 = 0.0f;
    std::vector<ParticleForceDeclaration> forces;
    std::vector<std::pair<float, glm::vec4>> colorStops;
    std::vector<std::pair<float, float>> sizeKeyframes;
};

ParticleEmitterDefinition loadParticleEmitterDefinition(const std::string& path);
void saveParticleEmitterDefinition(const std::string& path, const ParticleEmitterDefinition& def);
ResolvedParticleEmitterDefinition resolveParticleEmitterDefinition(
    const std::string& id,
    const std::unordered_map<std::string, ParticleEmitterDefinition>& definitions);
