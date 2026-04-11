#include "game/modules/particles/ParticleSerializer.h"

#include "game/content/ParseUtils.h"
#include "game/level/LevelDef.h"

#include <sstream>
#include <string>

void registerParticleEmitterKeyword() {
    registerLevelDefKeyword("particle_emitter",
        // Parser — tokens do NOT include the keyword itself
        [](LevelDef& data, const std::string& path, int lineNumber,
           const std::vector<std::string>& tokens) {
            if (tokens.size() < 4) {
                throwParseError(path, lineNumber,
                    "particle_emitter requires: emitterId px py pz");
            }
            LevelParticleEmitterPlacement p;
            p.emitterId = tokens[0];
            p.position.x = std::stof(tokens[1]);
            p.position.y = std::stof(tokens[2]);
            p.position.z = std::stof(tokens[3]);
            // Optional node/parent
            for (std::size_t i = 4; i < tokens.size(); ++i) {
                if (tokens[i] == "node" && i + 1 < tokens.size()) {
                    p.nodeId = tokens[++i];
                } else if (tokens[i] == "parent" && i + 1 < tokens.size()) {
                    p.parentNodeId = tokens[++i];
                }
            }
            data.particleEmitters.push_back(p);
        },
        // Serializer
        [](std::ostringstream& out, const LevelDef& data) {
            for (const auto& p : data.particleEmitters) {
                out << "particle_emitter " << p.emitterId
                    << " " << p.position.x << " " << p.position.y
                    << " " << p.position.z;
                if (!p.nodeId.empty()) out << " node " << p.nodeId;
                if (!p.parentNodeId.empty()) out << " parent " << p.parentNodeId;
                out << "\n";
            }
        });
}
