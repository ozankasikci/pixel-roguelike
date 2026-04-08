#include "game/modules/checkpoint/CheckpointSerializer.h"

#include "game/content/ParseUtils.h"
#include "game/level/LevelDef.h"

#include <iomanip>
#include <stdexcept>

namespace {

// Local helpers mirroring the anonymous-namespace helpers in LevelDef.cpp.

bool tryParseFloatToken(const std::string& token, float& value) {
    std::size_t parsed = 0;
    try {
        value = std::stof(token, &parsed);
    } catch (const std::exception&) {
        return false;
    }
    return parsed == token.size();
}

// Returns true if the current token was a "node" or "parent" keyword and was consumed.
bool parseNodeMetadata(const std::string& path, int lineNumber,
                       const std::vector<std::string>& tokens, std::size_t& index,
                       std::string& outNodeId, std::string& outParentNodeId) {
    if (tokens[index] == "node") {
        if (index + 1 >= tokens.size())
            throwParseError(path, lineNumber, "missing node id after 'node'");
        outNodeId = tokens[index + 1];
        index += 2;
        return true;
    }
    if (tokens[index] == "parent") {
        if (index + 1 >= tokens.size())
            throwParseError(path, lineNumber, "missing parent node id after 'parent'");
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
    LevelCheckpointPlacement cp;
    cp.name = tokens[0];
    try {
        cp.position = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
    } catch (const std::exception&) {
        throwParseError(path, lineNumber, "invalid checkpoint position values");
    }

    bool hasRespawn = false;
    std::size_t index = 4;
    while (index < tokens.size()) {
        if (tokens[index] == "respawn" && index + 3 < tokens.size()) {
            float rx, ry, rz;
            if (!tryParseFloatToken(tokens[index + 1], rx) ||
                !tryParseFloatToken(tokens[index + 2], ry) ||
                !tryParseFloatToken(tokens[index + 3], rz))
                throwParseError(path, lineNumber, "invalid respawn values");
            cp.respawnPosition = glm::vec3(rx, ry, rz);
            hasRespawn = true;
            index += 4;
        } else if (tokens[index] == "interact_distance" && index + 1 < tokens.size()) {
            float val;
            if (!tryParseFloatToken(tokens[index + 1], val))
                throwParseError(path, lineNumber, "invalid interact_distance value");
            cp.interactDistance = val;
            index += 2;
        } else if (tokens[index] == "interact_dot" && index + 1 < tokens.size()) {
            float val;
            if (!tryParseFloatToken(tokens[index + 1], val))
                throwParseError(path, lineNumber, "invalid interact_dot value");
            cp.interactDotThreshold = val;
            index += 2;
        } else if (tokens[index] == "light_offset" && index + 3 < tokens.size()) {
            float lx, ly, lz;
            if (!tryParseFloatToken(tokens[index + 1], lx) ||
                !tryParseFloatToken(tokens[index + 2], ly) ||
                !tryParseFloatToken(tokens[index + 3], lz))
                throwParseError(path, lineNumber, "invalid light_offset values");
            cp.lightOffset = glm::vec3(lx, ly, lz);
            index += 4;
        } else if (tokens[index] == "light_color" && index + 3 < tokens.size()) {
            float r, g, b;
            if (!tryParseFloatToken(tokens[index + 1], r) ||
                !tryParseFloatToken(tokens[index + 2], g) ||
                !tryParseFloatToken(tokens[index + 3], b))
                throwParseError(path, lineNumber, "invalid light_color values");
            cp.lightColor = glm::vec3(r, g, b);
            index += 4;
        } else if (tokens[index] == "light_radius" && index + 1 < tokens.size()) {
            float val;
            if (!tryParseFloatToken(tokens[index + 1], val))
                throwParseError(path, lineNumber, "invalid light_radius value");
            cp.lightRadius = val;
            index += 2;
        } else if (tokens[index] == "light_intensity" && index + 1 < tokens.size()) {
            float val;
            if (!tryParseFloatToken(tokens[index + 1], val))
                throwParseError(path, lineNumber, "invalid light_intensity value");
            cp.lightIntensity = val;
            index += 2;
        } else if (parseNodeMetadata(path, lineNumber, tokens, index,
                                     cp.nodeId, cp.parentNodeId)) {
            // consumed by parseNodeMetadata
        } else {
            ++index; // skip unknown
        }
    }

    // Default respawn position if not explicitly specified
    if (!hasRespawn) {
        cp.respawnPosition = cp.position + glm::vec3(0.0f, 1.6f, 2.5f);
    }

    data.checkpoints.push_back(std::move(cp));
}

void serializeCheckpoints(std::ostringstream& out, const LevelDef& data) {
    for (const auto& cp : data.checkpoints) {
        out << "checkpoint " << cp.name << ' '
            << formatFloat(cp.position.x) << ' '
            << formatFloat(cp.position.y) << ' '
            << formatFloat(cp.position.z);
        // Always write respawn (important data)
        out << " respawn "
            << formatFloat(cp.respawnPosition.x) << ' '
            << formatFloat(cp.respawnPosition.y) << ' '
            << formatFloat(cp.respawnPosition.z);
        if (cp.interactDistance != 2.4f)
            out << " interact_distance " << formatFloat(cp.interactDistance);
        if (cp.interactDotThreshold != 0.55f)
            out << " interact_dot " << formatFloat(cp.interactDotThreshold);
        if (cp.lightOffset != glm::vec3(0.0f, 0.65f, -1.0f))
            out << " light_offset "
                << formatFloat(cp.lightOffset.x) << ' '
                << formatFloat(cp.lightOffset.y) << ' '
                << formatFloat(cp.lightOffset.z);
        if (cp.lightColor != glm::vec3(1.0f, 0.7f, 0.42f))
            out << " light_color "
                << formatFloat(cp.lightColor.x) << ' '
                << formatFloat(cp.lightColor.y) << ' '
                << formatFloat(cp.lightColor.z);
        if (cp.lightRadius != 7.0f)
            out << " light_radius " << formatFloat(cp.lightRadius);
        if (cp.lightIntensity != 8.05f)
            out << " light_intensity " << formatFloat(cp.lightIntensity);
        appendNodeMetadata(out, cp.nodeId, cp.parentNodeId);
        out << '\n';
    }
}
