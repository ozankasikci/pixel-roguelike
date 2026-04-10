#include "game/modules/checkpoint/CheckpointSerializer.h"

#include "game/content/ParseUtils.h"
#include "game/level/LevelDef.h"

#include <iomanip>
#include <stdexcept>

namespace {

bool tryParseFloatToken(const std::string& token, float& value) {
    std::size_t parsed = 0;
    try {
        value = std::stof(token, &parsed);
    } catch (const std::exception&) {
        return false;
    }
    return parsed == token.size();
}

bool parseNodeMetadata(const std::string& path,
                       int lineNumber,
                       const std::vector<std::string>& tokens,
                       std::size_t& index,
                       std::string& outNodeId,
                       std::string& outParentNodeId) {
    if (tokens[index] == "node") {
        if (index + 1 >= tokens.size()) {
            throwParseError(path, lineNumber, "missing node id after 'node'");
        }
        outNodeId = tokens[index + 1];
        index += 2;
        return true;
    }
    if (tokens[index] == "parent") {
        if (index + 1 >= tokens.size()) {
            throwParseError(path, lineNumber, "missing parent node id after 'parent'");
        }
        outParentNodeId = tokens[index + 1];
        index += 2;
        return true;
    }
    return false;
}

std::string formatFloat(float value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    std::string text = stream.str();
    while (!text.empty() && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.push_back('0');
    }
    return text;
}

void appendNodeMetadata(std::ostringstream& out,
                        const std::string& nodeId,
                        const std::string& parentNodeId) {
    if (!nodeId.empty()) {
        out << " node " << nodeId;
    }
    if (!parentNodeId.empty()) {
        out << " parent " << parentNodeId;
    }
}

} // namespace

void parseCheckpoint(LevelDef& data,
                     const std::string& path,
                     int lineNumber,
                     const std::vector<std::string>& tokens) {
    if (tokens.size() < 4) {
        throwParseError(path, lineNumber, "invalid checkpoint: need name x y z");
    }

    LevelCheckpointPlacement checkpoint;
    checkpoint.name = tokens[0];
    try {
        checkpoint.position = glm::vec3(std::stof(tokens[1]),
                                        std::stof(tokens[2]),
                                        std::stof(tokens[3]));
    } catch (const std::exception&) {
        throwParseError(path, lineNumber, "invalid checkpoint position values");
    }

    bool respawnSpecified = false;
    std::size_t index = 4;
    while (index < tokens.size()) {
        if (tokens[index] == "respawn" && index + 3 < tokens.size()) {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (!tryParseFloatToken(tokens[index + 1], x)
                || !tryParseFloatToken(tokens[index + 2], y)
                || !tryParseFloatToken(tokens[index + 3], z)) {
                throwParseError(path, lineNumber, "invalid respawn values");
            }
            checkpoint.respawnPosition = glm::vec3(x, y, z);
            respawnSpecified = true;
            index += 4;
            continue;
        }
        if (tokens[index] == "interact_distance" && index + 1 < tokens.size()) {
            float value = 0.0f;
            if (!tryParseFloatToken(tokens[index + 1], value)) {
                throwParseError(path, lineNumber, "invalid interact_distance value");
            }
            checkpoint.interactDistance = value;
            index += 2;
            continue;
        }
        if (tokens[index] == "interact_dot" && index + 1 < tokens.size()) {
            float value = 0.0f;
            if (!tryParseFloatToken(tokens[index + 1], value)) {
                throwParseError(path, lineNumber, "invalid interact_dot value");
            }
            checkpoint.interactDotThreshold = value;
            index += 2;
            continue;
        }
        if (tokens[index] == "light_offset" && index + 3 < tokens.size()) {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            if (!tryParseFloatToken(tokens[index + 1], x)
                || !tryParseFloatToken(tokens[index + 2], y)
                || !tryParseFloatToken(tokens[index + 3], z)) {
                throwParseError(path, lineNumber, "invalid light_offset values");
            }
            checkpoint.lightOffset = glm::vec3(x, y, z);
            index += 4;
            continue;
        }
        if (tokens[index] == "light_color" && index + 3 < tokens.size()) {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;
            if (!tryParseFloatToken(tokens[index + 1], r)
                || !tryParseFloatToken(tokens[index + 2], g)
                || !tryParseFloatToken(tokens[index + 3], b)) {
                throwParseError(path, lineNumber, "invalid light_color values");
            }
            checkpoint.lightColor = glm::vec3(r, g, b);
            index += 4;
            continue;
        }
        if (tokens[index] == "light_radius" && index + 1 < tokens.size()) {
            float value = 0.0f;
            if (!tryParseFloatToken(tokens[index + 1], value)) {
                throwParseError(path, lineNumber, "invalid light_radius value");
            }
            checkpoint.lightRadius = value;
            index += 2;
            continue;
        }
        if (tokens[index] == "light_intensity" && index + 1 < tokens.size()) {
            float value = 0.0f;
            if (!tryParseFloatToken(tokens[index + 1], value)) {
                throwParseError(path, lineNumber, "invalid light_intensity value");
            }
            checkpoint.lightIntensity = value;
            index += 2;
            continue;
        }
        if (parseNodeMetadata(path,
                              lineNumber,
                              tokens,
                              index,
                              checkpoint.nodeId,
                              checkpoint.parentNodeId)) {
            continue;
        }
        ++index;
    }

    if (!respawnSpecified) {
        checkpoint.respawnPosition = checkpoint.position + glm::vec3(0.0f, 1.6f, 2.5f);
    }

    data.checkpoints.push_back(std::move(checkpoint));
}

void serializeCheckpoints(std::ostringstream& out, const LevelDef& data) {
    for (const auto& checkpoint : data.checkpoints) {
        out << "checkpoint " << checkpoint.name << ' '
            << formatFloat(checkpoint.position.x) << ' '
            << formatFloat(checkpoint.position.y) << ' '
            << formatFloat(checkpoint.position.z);
        out << " respawn "
            << formatFloat(checkpoint.respawnPosition.x) << ' '
            << formatFloat(checkpoint.respawnPosition.y) << ' '
            << formatFloat(checkpoint.respawnPosition.z);
        if (checkpoint.interactDistance != 2.4f) {
            out << " interact_distance " << formatFloat(checkpoint.interactDistance);
        }
        if (checkpoint.interactDotThreshold != 0.55f) {
            out << " interact_dot " << formatFloat(checkpoint.interactDotThreshold);
        }
        if (checkpoint.lightOffset != glm::vec3(0.0f, 0.65f, -1.0f)) {
            out << " light_offset "
                << formatFloat(checkpoint.lightOffset.x) << ' '
                << formatFloat(checkpoint.lightOffset.y) << ' '
                << formatFloat(checkpoint.lightOffset.z);
        }
        if (checkpoint.lightColor != glm::vec3(1.0f, 0.7f, 0.42f)) {
            out << " light_color "
                << formatFloat(checkpoint.lightColor.x) << ' '
                << formatFloat(checkpoint.lightColor.y) << ' '
                << formatFloat(checkpoint.lightColor.z);
        }
        if (checkpoint.lightRadius != 7.0f) {
            out << " light_radius " << formatFloat(checkpoint.lightRadius);
        }
        if (checkpoint.lightIntensity != 8.05f) {
            out << " light_intensity " << formatFloat(checkpoint.lightIntensity);
        }
        appendNodeMetadata(out, checkpoint.nodeId, checkpoint.parentNodeId);
        out << '\n';
    }
}
