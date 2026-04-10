#include "game/modules/player_control/PlayerControlSerializer.h"

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

void parsePlayerSpawn(LevelDef& data,
                      const std::string& path,
                      int lineNumber,
                      const std::vector<std::string>& tokens) {
    if (tokens.size() < 4) {
        throwParseError(path, lineNumber, "invalid player_spawn record");
    }

    LevelPlayerSpawn placement;
    if (!tryParseFloatToken(tokens[0], placement.position.x)
        || !tryParseFloatToken(tokens[1], placement.position.y)
        || !tryParseFloatToken(tokens[2], placement.position.z)
        || !tryParseFloatToken(tokens[3], placement.fallRespawnY)) {
        throwParseError(path, lineNumber, "invalid player_spawn record");
    }

    for (std::size_t index = 4; index < tokens.size();) {
        if (parseNodeMetadata(path, lineNumber, tokens, index,
                              placement.nodeId, placement.parentNodeId)) {
            continue;
        }
        throwParseError(path, lineNumber, "invalid player_spawn metadata");
    }

    data.playerSpawn = placement;
    data.hasPlayerSpawn = true;
}

void serializePlayerSpawn(std::ostringstream& out, const LevelDef& data) {
    if (!data.hasPlayerSpawn) {
        return;
    }

    out << "player_spawn "
        << formatFloat(data.playerSpawn.position.x) << ' '
        << formatFloat(data.playerSpawn.position.y) << ' '
        << formatFloat(data.playerSpawn.position.z) << ' '
        << formatFloat(data.playerSpawn.fallRespawnY);
    appendNodeMetadata(out, data.playerSpawn.nodeId, data.playerSpawn.parentNodeId);
    out << '\n';
}
