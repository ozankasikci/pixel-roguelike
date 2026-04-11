#include "game/particles/ParticleEmitterDefinition.h"

#include "game/content/ParseUtils.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace {

float parseFloatRecord(const std::vector<std::string>& tokens,
                       const std::string& path,
                       int lineNumber,
                       const std::string& label) {
    if (tokens.size() != 2) {
        throwParseError(path, lineNumber, "invalid " + label + " record");
    }
    return std::stof(tokens[1]);
}

void writeOptionalFloat(std::ostringstream& out,
                        const char* key,
                        const std::optional<float>& value) {
    if (value.has_value()) {
        out << key << ' ' << *value << '\n';
    }
}

void writeOptionalString(std::ostringstream& out,
                         const char* key,
                         const std::optional<std::string>& value) {
    if (value.has_value() && !value->empty()) {
        out << key << ' ' << *value << '\n';
    }
}

void writeOptionalBool(std::ostringstream& out,
                       const char* key,
                       const std::optional<bool>& value) {
    if (value.has_value()) {
        out << key << ' ' << (*value ? "true" : "false") << '\n';
    }
}

void writeOptionalInt(std::ostringstream& out,
                      const char* key,
                      const std::optional<int>& value) {
    if (value.has_value()) {
        out << key << ' ' << *value << '\n';
    }
}

ResolvedParticleEmitterDefinition resolveRecursive(
    const std::string& id,
    const std::unordered_map<std::string, ParticleEmitterDefinition>& definitions,
    std::unordered_map<std::string, ResolvedParticleEmitterDefinition>& cache,
    std::unordered_set<std::string>& visiting) {
    auto cached = cache.find(id);
    if (cached != cache.end()) {
        return cached->second;
    }

    auto defIt = definitions.find(id);
    if (defIt == definitions.end()) {
        throw std::runtime_error("Unknown particle emitter id: " + id);
    }

    if (!visiting.insert(id).second) {
        throw std::runtime_error("Particle emitter inheritance cycle detected at: " + id);
    }

    const ParticleEmitterDefinition& definition = defIt->second;
    ResolvedParticleEmitterDefinition resolved;
    resolved.id = definition.id;

    if (definition.parent.has_value()) {
        auto parent = resolveRecursive(*definition.parent, definitions, cache, visiting);
        resolved = parent;
        resolved.id = definition.id;
    }

    if (definition.maxParticles.has_value()) {
        resolved.maxParticles = *definition.maxParticles;
    }
    if (definition.emissionRate.has_value()) {
        resolved.emissionRate = *definition.emissionRate;
    }
    if (definition.looping.has_value()) {
        resolved.looping = *definition.looping;
    }
    if (definition.duration.has_value()) {
        resolved.duration = *definition.duration;
    }
    if (definition.simulationSpace.has_value()) {
        resolved.simulationSpace = *definition.simulationSpace;
    }
    if (definition.blendMode.has_value()) {
        resolved.blendMode = *definition.blendMode;
    }
    if (definition.texture.has_value()) {
        resolved.texture = *definition.texture;
    }
    if (definition.warmUp.has_value()) {
        resolved.warmUp = *definition.warmUp;
    }
    if (definition.softParticleFade.has_value()) {
        resolved.softParticleFade = *definition.softParticleFade;
    }
    if (definition.emissiveStrength.has_value()) {
        resolved.emissiveStrength = *definition.emissiveStrength;
    }
    if (definition.lifetimeMin.has_value()) {
        resolved.lifetimeMin = *definition.lifetimeMin;
    }
    if (definition.lifetimeMax.has_value()) {
        resolved.lifetimeMax = *definition.lifetimeMax;
    }
    if (definition.initialSpeedMin.has_value()) {
        resolved.initialSpeedMin = *definition.initialSpeedMin;
    }
    if (definition.initialSpeedMax.has_value()) {
        resolved.initialSpeedMax = *definition.initialSpeedMax;
    }
    if (definition.rotationSpeedMin.has_value()) {
        resolved.rotationSpeedMin = *definition.rotationSpeedMin;
    }
    if (definition.rotationSpeedMax.has_value()) {
        resolved.rotationSpeedMax = *definition.rotationSpeedMax;
    }
    if (definition.shapeType.has_value()) {
        resolved.shapeType = *definition.shapeType;
    }
    if (definition.shapeParam0.has_value()) {
        resolved.shapeParam0 = *definition.shapeParam0;
    }
    if (definition.shapeParam1.has_value()) {
        resolved.shapeParam1 = *definition.shapeParam1;
    }
    if (!definition.forces.empty()) {
        resolved.forces = definition.forces;
    }
    if (!definition.colorStops.empty()) {
        resolved.colorStops = definition.colorStops;
    }
    if (!definition.sizeKeyframes.empty()) {
        resolved.sizeKeyframes = definition.sizeKeyframes;
    }

    visiting.erase(id);
    cache.emplace(id, resolved);
    return resolved;
}

} // namespace

ParticleEmitterDefinition loadParticleEmitterDefinition(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open particle emitter definition: " + path);
    }

    ParticleEmitterDefinition definition;
    std::string line;
    int lineNumber = 0;

    while (std::getline(file, line)) {
        ++lineNumber;
        if (isCommentOrEmpty(line)) {
            continue;
        }

        const auto tokens = tokenizeRecord(line);
        if (tokens.empty()) {
            continue;
        }

        const std::string& key = tokens[0];
        if (key == "id" && tokens.size() == 2) {
            definition.id = tokens[1];
            continue;
        }
        if (key == "parent" && tokens.size() == 2) {
            definition.parent = tokens[1];
            continue;
        }
        if (key == "max_particles" && tokens.size() == 2) {
            definition.maxParticles = std::stoi(tokens[1]);
            continue;
        }
        if (key == "emission_rate" && tokens.size() == 2) {
            definition.emissionRate = std::stof(tokens[1]);
            continue;
        }
        if (key == "looping" && tokens.size() == 2) {
            definition.looping = (tokens[1] == "true");
            continue;
        }
        if (key == "duration" && tokens.size() == 2) {
            definition.duration = std::stof(tokens[1]);
            continue;
        }
        if (key == "simulation_space" && tokens.size() == 2) {
            if (tokens[1] != "world" && tokens[1] != "local") {
                throwParseError(path, lineNumber, "unknown simulation_space");
            }
            definition.simulationSpace = tokens[1];
            continue;
        }
        if (key == "blend_mode" && tokens.size() == 2) {
            if (tokens[1] != "additive" && tokens[1] != "alpha") {
                throwParseError(path, lineNumber, "unknown blend_mode");
            }
            definition.blendMode = tokens[1];
            continue;
        }
        if (key == "texture" && tokens.size() == 2) {
            definition.texture = tokens[1];
            continue;
        }
        if (key == "warm_up" && tokens.size() == 2) {
            definition.warmUp = (tokens[1] == "true");
            continue;
        }
        if (key == "soft_particle_fade") {
            definition.softParticleFade = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "emissive_strength") {
            definition.emissiveStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "lifetime" && tokens.size() == 3) {
            definition.lifetimeMin = std::stof(tokens[1]);
            definition.lifetimeMax = std::stof(tokens[2]);
            continue;
        }
        if (key == "initial_speed" && tokens.size() == 3) {
            definition.initialSpeedMin = std::stof(tokens[1]);
            definition.initialSpeedMax = std::stof(tokens[2]);
            continue;
        }
        if (key == "rotation_speed" && tokens.size() == 3) {
            definition.rotationSpeedMin = std::stof(tokens[1]);
            definition.rotationSpeedMax = std::stof(tokens[2]);
            continue;
        }
        if (key == "shape") {
            if (tokens.size() < 2) {
                throwParseError(path, lineNumber, "invalid shape record");
            }
            definition.shapeType = tokens[1];
            if (tokens[1] == "point") {
                if (tokens.size() != 2) {
                    throwParseError(path, lineNumber, "invalid shape record");
                }
            } else if (tokens[1] == "sphere") {
                if (tokens.size() != 3) {
                    throwParseError(path, lineNumber, "invalid shape record");
                }
                definition.shapeParam0 = std::stof(tokens[2]);
            } else if (tokens[1] == "cone") {
                if (tokens.size() != 4) {
                    throwParseError(path, lineNumber, "invalid shape record");
                }
                definition.shapeParam0 = std::stof(tokens[2]);
                definition.shapeParam1 = std::stof(tokens[3]);
            } else {
                throwParseError(path, lineNumber, "unknown shape type");
            }
            continue;
        }
        if (key == "force") {
            if (tokens.size() < 2) {
                throwParseError(path, lineNumber, "invalid force record");
            }
            ParticleForceDeclaration force;
            force.type = tokens[1];
            if (tokens[1] == "gravity") {
                if (tokens.size() != 5) {
                    throwParseError(path, lineNumber, "invalid gravity force record");
                }
                force.value = glm::vec3(std::stof(tokens[2]), std::stof(tokens[3]),
                                        std::stof(tokens[4]));
            } else if (tokens[1] == "drag") {
                if (tokens.size() != 3) {
                    throwParseError(path, lineNumber, "invalid drag force record");
                }
                force.coefficient = std::stof(tokens[2]);
            } else {
                throwParseError(path, lineNumber, "unknown force type");
            }
            definition.forces.push_back(std::move(force));
            continue;
        }
        if (key == "color_over_lifetime" && tokens.size() == 6) {
            float t = std::stof(tokens[1]);
            glm::vec4 color(std::stof(tokens[2]), std::stof(tokens[3]), std::stof(tokens[4]),
                            std::stof(tokens[5]));
            definition.colorStops.emplace_back(t, color);
            continue;
        }
        if (key == "size_over_lifetime" && tokens.size() == 3) {
            float t = std::stof(tokens[1]);
            float v = std::stof(tokens[2]);
            definition.sizeKeyframes.emplace_back(t, v);
            continue;
        }

        throwParseError(path, lineNumber, "invalid particle emitter definition record");
    }

    if (definition.id.empty()) {
        throw std::runtime_error("Particle emitter definition missing id: " + path);
    }

    return definition;
}

void saveParticleEmitterDefinition(const std::string& path,
                                   const ParticleEmitterDefinition& def) {
    if (def.id.empty()) {
        throw std::runtime_error("Cannot save particle emitter definition without id");
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to save particle emitter definition: " + path);
    }

    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    out << "id " << def.id << '\n';
    writeOptionalString(out, "parent", def.parent);
    writeOptionalInt(out, "max_particles", def.maxParticles);
    writeOptionalFloat(out, "emission_rate", def.emissionRate);
    writeOptionalBool(out, "looping", def.looping);
    writeOptionalFloat(out, "duration", def.duration);
    writeOptionalString(out, "simulation_space", def.simulationSpace);
    writeOptionalString(out, "blend_mode", def.blendMode);
    writeOptionalString(out, "texture", def.texture);
    writeOptionalBool(out, "warm_up", def.warmUp);
    writeOptionalFloat(out, "soft_particle_fade", def.softParticleFade);
    writeOptionalFloat(out, "emissive_strength", def.emissiveStrength);
    if (def.lifetimeMin.has_value() && def.lifetimeMax.has_value()) {
        out << "lifetime " << *def.lifetimeMin << ' ' << *def.lifetimeMax << '\n';
    }
    if (def.initialSpeedMin.has_value() && def.initialSpeedMax.has_value()) {
        out << "initial_speed " << *def.initialSpeedMin << ' ' << *def.initialSpeedMax << '\n';
    }
    if (def.rotationSpeedMin.has_value() && def.rotationSpeedMax.has_value()) {
        out << "rotation_speed " << *def.rotationSpeedMin << ' ' << *def.rotationSpeedMax << '\n';
    }
    if (def.shapeType.has_value()) {
        out << "shape " << *def.shapeType;
        if (*def.shapeType == "sphere" && def.shapeParam0.has_value()) {
            out << ' ' << *def.shapeParam0;
        } else if (*def.shapeType == "cone") {
            if (def.shapeParam0.has_value()) {
                out << ' ' << *def.shapeParam0;
            }
            if (def.shapeParam1.has_value()) {
                out << ' ' << *def.shapeParam1;
            }
        }
        out << '\n';
    }
    for (const auto& force : def.forces) {
        if (force.type == "gravity") {
            out << "force gravity " << force.value.x << ' ' << force.value.y << ' '
                << force.value.z << '\n';
        } else if (force.type == "drag") {
            out << "force drag " << force.coefficient << '\n';
        }
    }
    for (const auto& [t, color] : def.colorStops) {
        out << "color_over_lifetime " << t << ' ' << color.r << ' ' << color.g << ' ' << color.b
            << ' ' << color.a << '\n';
    }
    for (const auto& [t, value] : def.sizeKeyframes) {
        out << "size_over_lifetime " << t << ' ' << value << '\n';
    }

    file << out.str();
}

ResolvedParticleEmitterDefinition resolveParticleEmitterDefinition(
    const std::string& id,
    const std::unordered_map<std::string, ParticleEmitterDefinition>& definitions) {
    std::unordered_map<std::string, ResolvedParticleEmitterDefinition> cache;
    std::unordered_set<std::string> visiting;
    return resolveRecursive(id, definitions, cache, visiting);
}
