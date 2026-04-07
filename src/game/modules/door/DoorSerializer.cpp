#include "game/modules/door/DoorSerializer.h"

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

void parseDoorGroup(LevelDef& data,
                    const std::string& path,
                    int lineNumber,
                    const std::vector<std::string>& tokens) {
    if (tokens.size() < 5) {
        throwParseError(path, lineNumber, "invalid door_group: need name x y z yaw");
    }
    LevelDoorPlacement dg;
    dg.name = tokens[0];
    try {
        dg.position = glm::vec3(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
        dg.yawDegrees = std::stof(tokens[4]);
    } catch (const std::exception&) {
        throwParseError(path, lineNumber, "invalid door_group position/yaw values");
    }
    std::size_t index = 5;
    while (index < tokens.size()) {
        if (tokens[index] == "open_angle" && index + 1 < tokens.size()) {
            dg.openAngle = std::stof(tokens[index + 1]);
            index += 2;
        } else if (tokens[index] == "open_duration" && index + 1 < tokens.size()) {
            dg.openDuration = std::stof(tokens[index + 1]);
            index += 2;
        } else if (tokens[index] == "interact_distance" && index + 1 < tokens.size()) {
            dg.interactDistance = std::stof(tokens[index + 1]);
            index += 2;
        } else if (tokens[index] == "interact_dot" && index + 1 < tokens.size()) {
            dg.interactDotThreshold = std::stof(tokens[index + 1]);
            index += 2;
        } else if (tokens[index] == "locked") {
            dg.locked = true;
            ++index;
            // Collect remaining tokens as prompt until hitting "node" or "parent"
            std::string prompt;
            while (index < tokens.size() && tokens[index] != "node" && tokens[index] != "parent") {
                if (!prompt.empty()) prompt += ' ';
                prompt += tokens[index++];
            }
            if (!prompt.empty()) dg.lockedPrompt = prompt;
        } else if (parseNodeMetadata(path, lineNumber, tokens, index,
                                     dg.nodeId, dg.parentNodeId)) {
            // consumed by parseNodeMetadata
        } else {
            ++index; // skip unknown
        }
    }
    data.doors.push_back(std::move(dg));
}

void serializeDoorGroups(std::ostringstream& out, const LevelDef& data) {
    for (const auto& dg : data.doors) {
        out << "door_group " << dg.name << ' '
            << formatFloat(dg.position.x) << ' '
            << formatFloat(dg.position.y) << ' '
            << formatFloat(dg.position.z) << ' '
            << formatFloat(dg.yawDegrees);
        if (dg.openAngle != 90.0f) out << " open_angle " << formatFloat(dg.openAngle);
        if (dg.openDuration != 1.2f) out << " open_duration " << formatFloat(dg.openDuration);
        if (dg.interactDistance != 2.5f) out << " interact_distance " << formatFloat(dg.interactDistance);
        if (dg.interactDotThreshold != 0.55f) out << " interact_dot " << formatFloat(dg.interactDotThreshold);
        if (dg.locked) out << " locked " << dg.lockedPrompt;
        appendNodeMetadata(out, dg.nodeId, dg.parentNodeId);
        out << '\n';
    }
}
