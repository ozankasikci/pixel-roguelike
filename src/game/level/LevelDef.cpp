#include "game/level/LevelDef.h"

#include "engine/core/MathUtils.h"
#include "game/content/ParseUtils.h"
#include "game/rendering/EnvironmentProfile.h"
#include <filesystem>
#include <cctype>
#include <functional>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <unordered_map>
#include <unordered_set>

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

bool tryParseBoolToken(const std::string& token, bool& value) {
    if (token == "1" || token == "true" || token == "TRUE") {
        value = true;
        return true;
    }
    if (token == "0" || token == "false" || token == "FALSE") {
        value = false;
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

std::string resolvedEnvironmentId(const LevelDef& data) {
    if (!data.environmentId.empty()) {
        return data.environmentId;
    }
    return environmentProfileName(data.environmentProfile);
}

std::vector<std::string> collectRemainingTokens(std::istream& stream) {
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

bool tryParseVec3Tokens(const std::vector<std::string>& tokens,
                        std::size_t index,
                        glm::vec3& value) {
    if (index + 2 >= tokens.size()) {
        return false;
    }
    return tryParseFloatToken(tokens[index], value.x)
        && tryParseFloatToken(tokens[index + 1], value.y)
        && tryParseFloatToken(tokens[index + 2], value.z);
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

bool decomposeTransformMatrix(const glm::mat4& matrix,
                              glm::vec3& position,
                              glm::vec3& rotationDegrees,
                              glm::vec3& scale) {
    glm::vec3 skew(0.0f);
    glm::vec4 perspective(0.0f);
    glm::quat orientation;
    if (!glm::decompose(matrix, scale, orientation, position, skew, perspective)) {
        return false;
    }
    float rx, ry, rz;
    glm::extractEulerAngleXYZ(glm::mat4_cast(orientation), rx, ry, rz);
    rotationDegrees = glm::degrees(glm::vec3(rx, ry, rz));
    return true;
}

bool hasNonZeroVec3(const glm::vec3& value) {
    return std::abs(value.x) > 0.0001f
        || std::abs(value.y) > 0.0001f
        || std::abs(value.z) > 0.0001f;
}

template <typename PlacementT>
const std::string& placementNodeId(const PlacementT& placement) {
    return placement.nodeId;
}

template <typename PlacementT>
const std::string& placementParentNodeId(const PlacementT& placement) {
    return placement.parentNodeId;
}

struct LevelNodeRef {
    enum class Kind {
        Mesh,
        Light,
        Collider,
        ReflectionProbe,
        PlayerSpawn,
        Archetype,
        Group,
    };

    Kind kind = Kind::Mesh;
    std::size_t index = 0;
    std::string nodeId;
    std::string parentNodeId;
};

bool isIndentedLine(const std::string& line) {
    return !line.empty() && (line[0] == ' ' || line[0] == '\t');
}

ActionType parseActionTypeName(const std::string& path, int lineNumber, const std::string& name) {
    if (name == "open_door")       return ActionType::OpenDoor;
    if (name == "close_door")      return ActionType::CloseDoor;
    if (name == "toggle_door")     return ActionType::ToggleDoor;
    if (name == "play_sound")      return ActionType::PlaySound;
    if (name == "set_light")       return ActionType::SetLight;
    if (name == "flicker_light")   return ActionType::FlickerLight;
    if (name == "show_message")    return ActionType::ShowMessage;
    if (name == "delay")           return ActionType::Delay;
    if (name == "enable_entity")   return ActionType::EnableEntity;
    if (name == "disable_entity")  return ActionType::DisableEntity;
    if (name == "emit_event")      return ActionType::EmitEvent;
    if (name == "lock_player")     return ActionType::LockPlayer;
    if (name == "unlock_player")   return ActionType::UnlockPlayer;
    if (name == "teleport_player") return ActionType::TeleportPlayer;
    throwParseError(path, lineNumber, "unknown action type '" + name + "'");
    return ActionType::Delay; // unreachable
}

ActionEntry parseActionEntry(const std::string& path,
                             int lineNumber,
                             const std::string& actionTypeName,
                             const std::vector<std::string>& tokens,
                             std::size_t startIndex) {
    ActionEntry entry;
    entry.type = parseActionTypeName(path, lineNumber, actionTypeName);

    // Initialize default params based on type
    switch (entry.type) {
    case ActionType::OpenDoor:
    case ActionType::CloseDoor:
    case ActionType::ToggleDoor:
        entry.params = DoorActionParams{};
        break;
    case ActionType::PlaySound:
        entry.params = SoundActionParams{};
        break;
    case ActionType::SetLight:
        entry.params = LightActionParams{};
        break;
    case ActionType::FlickerLight:
        entry.params = FlickerLightParams{};
        break;
    case ActionType::ShowMessage:
        entry.params = MessageActionParams{};
        break;
    case ActionType::Delay:
        entry.params = DelayActionParams{};
        break;
    case ActionType::EnableEntity:
    case ActionType::DisableEntity:
        entry.params = EntityToggleParams{};
        break;
    case ActionType::EmitEvent:
        entry.params = EventActionParams{};
        break;
    case ActionType::LockPlayer:
        entry.params = PlayerLockParams{};
        break;
    case ActionType::UnlockPlayer:
        entry.params = PlayerLockParams{};
        break;
    case ActionType::TeleportPlayer:
        entry.params = TeleportPlayerParams{};
        break;
    }

    for (std::size_t i = startIndex; i < tokens.size();) {
        const std::string& tok = tokens[i];

        if (tok == "target") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing target node id");
            entry.targetNodeId = tokens[i + 1];
            i += 2;
            continue;
        }
        if (tok == "delay") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing delay value");
            float v = 0.0f;
            if (!tryParseFloatToken(tokens[i + 1], v)) throwParseError(path, lineNumber, "invalid delay value");
            entry.delay = v;
            i += 2;
            continue;
        }
        if (tok == "fire_once") {
            entry.fireOnce = true;
            i += 1;
            continue;
        }
        if (tok == "duration") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing duration value");
            float v = 0.0f;
            if (!tryParseFloatToken(tokens[i + 1], v)) throwParseError(path, lineNumber, "invalid duration value");
            if (auto* p = std::get_if<DoorActionParams>(&entry.params)) { p->duration = v; }
            else if (auto* p2 = std::get_if<FlickerLightParams>(&entry.params)) { p2->duration = v; }
            else if (auto* p3 = std::get_if<MessageActionParams>(&entry.params)) { p3->duration = v; }
            else if (auto* p4 = std::get_if<PlayerLockParams>(&entry.params)) { p4->duration = v; }
            else if (auto* p5 = std::get_if<DelayActionParams>(&entry.params)) { p5->seconds = v; }
            i += 2;
            continue;
        }
        if (tok == "sound") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing sound id");
            if (auto* p = std::get_if<SoundActionParams>(&entry.params)) { p->soundId = tokens[i + 1]; }
            i += 2;
            continue;
        }
        if (tok == "volume") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing volume value");
            float v = 0.0f;
            if (!tryParseFloatToken(tokens[i + 1], v)) throwParseError(path, lineNumber, "invalid volume value");
            if (auto* p = std::get_if<SoundActionParams>(&entry.params)) { p->volume = v; }
            i += 2;
            continue;
        }
        if (tok == "intensity") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing intensity value");
            float v = 0.0f;
            if (!tryParseFloatToken(tokens[i + 1], v)) throwParseError(path, lineNumber, "invalid intensity value");
            if (auto* p = std::get_if<LightActionParams>(&entry.params)) { p->intensity = v; }
            i += 2;
            continue;
        }
        if (tok == "color") {
            if (i + 3 >= tokens.size()) throwParseError(path, lineNumber, "missing color values");
            glm::vec3 col{1.0f};
            if (!tryParseVec3Tokens(tokens, i + 1, col)) throwParseError(path, lineNumber, "invalid color values");
            if (auto* p = std::get_if<LightActionParams>(&entry.params)) { p->color = col; }
            i += 4;
            continue;
        }
        if (tok == "radius") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing radius value");
            float v = 0.0f;
            if (!tryParseFloatToken(tokens[i + 1], v)) throwParseError(path, lineNumber, "invalid radius value");
            if (auto* p = std::get_if<LightActionParams>(&entry.params)) { p->radius = v; }
            i += 2;
            continue;
        }
        if (tok == "rate") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing rate value");
            float v = 0.0f;
            if (!tryParseFloatToken(tokens[i + 1], v)) throwParseError(path, lineNumber, "invalid rate value");
            if (auto* p = std::get_if<FlickerLightParams>(&entry.params)) { p->rate = v; }
            i += 2;
            continue;
        }
        if (tok == "text") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing text value");
            if (auto* p = std::get_if<MessageActionParams>(&entry.params)) { p->text = tokens[i + 1]; }
            i += 2;
            continue;
        }
        if (tok == "event") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing event name");
            if (auto* p = std::get_if<EventActionParams>(&entry.params)) { p->eventName = tokens[i + 1]; }
            i += 2;
            continue;
        }
        if (tok == "position") {
            if (i + 3 >= tokens.size()) throwParseError(path, lineNumber, "missing position values");
            glm::vec3 pos{0.0f};
            if (!tryParseVec3Tokens(tokens, i + 1, pos)) throwParseError(path, lineNumber, "invalid position values");
            if (auto* p = std::get_if<TeleportPlayerParams>(&entry.params)) { p->position = pos; }
            i += 4;
            continue;
        }
        throwParseError(path, lineNumber, "unknown action parameter '" + tok + "'");
    }

    return entry;
}

BehaviorDeclaration parseSubLineBehavior(const std::string& path,
                                         int lineNumber,
                                         const std::string& trimmedLine) {
    std::istringstream stream(trimmedLine);
    std::string eventType;
    stream >> eventType;

    // Normalize: "behavior on_activate <action>" or "on_activate <action>"
    std::string actionTypeName;
    if (eventType == "behavior") {
        stream >> eventType >> actionTypeName;
    } else {
        // eventType is already "on_activate", "on_enter", etc.
        stream >> actionTypeName;
    }

    const auto tokens = collectRemainingTokens(stream);
    BehaviorDeclaration decl;
    decl.eventType = eventType;
    decl.action = parseActionEntry(path, lineNumber, actionTypeName, tokens, 0);
    return decl;
}

InteractableDeclaration parseSubLineInteractable(const std::string& path,
                                                  int lineNumber,
                                                  const std::string& trimmedLine) {
    // Format: interactable "<prompt>" distance <float> [dot_threshold <float>]
    // Or:     interactable <prompt_no_spaces> distance <float>
    std::istringstream stream(trimmedLine);
    std::string keyword;
    stream >> keyword; // "interactable"

    InteractableDeclaration decl;

    // Try to read quoted or unquoted prompt text
    stream >> std::ws;
    if (stream.peek() == '"') {
        stream.ignore(); // consume opening quote
        std::getline(stream, decl.promptText, '"');
    } else {
        stream >> decl.promptText;
    }

    // Parse remaining keyword tokens
    const auto tokens = collectRemainingTokens(stream);
    for (std::size_t i = 0; i < tokens.size();) {
        if (tokens[i] == "distance") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing interactable distance");
            if (!tryParseFloatToken(tokens[i + 1], decl.distance)) throwParseError(path, lineNumber, "invalid interactable distance");
            i += 2;
            continue;
        }
        if (tokens[i] == "dot_threshold") {
            if (i + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing interactable dot_threshold");
            if (!tryParseFloatToken(tokens[i + 1], decl.dotThreshold)) throwParseError(path, lineNumber, "invalid interactable dot_threshold");
            i += 2;
            continue;
        }
        throwParseError(path, lineNumber, "unknown interactable parameter '" + tokens[i] + "'");
    }
    return decl;
}

std::string trimLeadingWhitespace(const std::string& line) {
    std::size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
        ++start;
    }
    return line.substr(start);
}

const char* actionTypeName(ActionType type) {
    switch (type) {
    case ActionType::OpenDoor:       return "open_door";
    case ActionType::CloseDoor:      return "close_door";
    case ActionType::ToggleDoor:     return "toggle_door";
    case ActionType::PlaySound:      return "play_sound";
    case ActionType::SetLight:       return "set_light";
    case ActionType::FlickerLight:   return "flicker_light";
    case ActionType::ShowMessage:    return "show_message";
    case ActionType::Delay:          return "delay";
    case ActionType::EnableEntity:   return "enable_entity";
    case ActionType::DisableEntity:  return "disable_entity";
    case ActionType::EmitEvent:      return "emit_event";
    case ActionType::LockPlayer:     return "lock_player";
    case ActionType::UnlockPlayer:   return "unlock_player";
    case ActionType::TeleportPlayer: return "teleport_player";
    }
    return "delay";
}

void serializeActionEntry(std::ostringstream& out, const std::string& eventType, const ActionEntry& entry) {
    out << "  " << eventType << " " << actionTypeName(entry.type);
    if (!entry.targetNodeId.empty()) {
        out << " target " << entry.targetNodeId;
    }
    if (entry.delay > 0.0f) {
        out << " delay " << formatFloat(entry.delay);
    }
    if (entry.fireOnce) {
        out << " fire_once";
    }
    std::visit([&](const auto& params) {
        using T = std::decay_t<decltype(params)>;
        if constexpr (std::is_same_v<T, DoorActionParams>) {
            out << " duration " << formatFloat(params.duration);
        } else if constexpr (std::is_same_v<T, SoundActionParams>) {
            if (!params.soundId.empty()) out << " sound " << params.soundId;
            out << " volume " << formatFloat(params.volume);
        } else if constexpr (std::is_same_v<T, LightActionParams>) {
            out << " intensity " << formatFloat(params.intensity);
            out << " color " << formatFloat(params.color.r) << ' '
                << formatFloat(params.color.g) << ' '
                << formatFloat(params.color.b);
            if (params.radius > 0.0f) out << " radius " << formatFloat(params.radius);
        } else if constexpr (std::is_same_v<T, FlickerLightParams>) {
            out << " duration " << formatFloat(params.duration);
            out << " rate " << formatFloat(params.rate);
        } else if constexpr (std::is_same_v<T, MessageActionParams>) {
            if (!params.text.empty()) out << " text " << params.text;
            out << " duration " << formatFloat(params.duration);
        } else if constexpr (std::is_same_v<T, DelayActionParams>) {
            out << " duration " << formatFloat(params.seconds);
        } else if constexpr (std::is_same_v<T, EntityToggleParams>) {
            // nothing
        } else if constexpr (std::is_same_v<T, EventActionParams>) {
            if (!params.eventName.empty()) out << " event " << params.eventName;
        } else if constexpr (std::is_same_v<T, PlayerLockParams>) {
            if (params.duration > 0.0f) out << " duration " << formatFloat(params.duration);
        } else if constexpr (std::is_same_v<T, TeleportPlayerParams>) {
            out << " position " << formatFloat(params.position.x) << ' '
                << formatFloat(params.position.y) << ' '
                << formatFloat(params.position.z);
        }
    }, entry.params);
    out << '\n';
}

void serializeBehaviors(std::ostringstream& out, const std::vector<BehaviorDeclaration>& behaviors) {
    for (const auto& decl : behaviors) {
        serializeActionEntry(out, decl.eventType, decl.action);
    }
}

} // namespace

LevelDef loadLevelDef(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open level definition: " + path);
    }

    LevelDef data;
    int lineNumber = 0;

    // Read all lines first so we can look ahead for indented sub-lines
    std::vector<std::string> allLines;
    {
        std::string line;
        while (std::getline(file, line)) {
            allLines.push_back(line);
        }
    }

    // Track which entity type we're currently appending sub-lines to
    enum class CurrentEntityKind { None, Mesh, Light, Collider };
    CurrentEntityKind currentKind = CurrentEntityKind::None;
    std::size_t currentIndex = 0;

    auto attachSubLine = [&](const std::string& rawSubLine, int subLineNumber) {
        const std::string trimmed = trimLeadingWhitespace(rawSubLine);
        if (isCommentOrEmpty(trimmed)) return;

        std::istringstream st(trimmed);
        std::string subKeyword;
        st >> subKeyword;

        if (subKeyword == "on_activate" || subKeyword == "on_enter"
            || subKeyword == "on_exit" || subKeyword == "on_timer"
            || subKeyword == "behavior") {
            BehaviorDeclaration decl = parseSubLineBehavior(path, subLineNumber, trimmed);
            if (currentKind == CurrentEntityKind::Mesh) {
                data.meshes[currentIndex].behaviors.push_back(std::move(decl));
            } else if (currentKind == CurrentEntityKind::Light) {
                data.lights[currentIndex].behaviors.push_back(std::move(decl));
            } else if (currentKind == CurrentEntityKind::Collider) {
                data.colliders[currentIndex].behaviors.push_back(std::move(decl));
            }
            return;
        }

        if (subKeyword == "interactable") {
            if (currentKind == CurrentEntityKind::Mesh) {
                data.meshes[currentIndex].interactable = parseSubLineInteractable(path, subLineNumber, trimmed);
            }
            return;
        }

        throwParseError(path, subLineNumber, "unknown sub-line keyword '" + subKeyword + "'");
    };

    for (std::size_t lineIdx = 0; lineIdx < allLines.size(); ++lineIdx) {
        const std::string& line = allLines[lineIdx];
        ++lineNumber;

        // If this line is indented, it's a sub-line
        if (isIndentedLine(line)) {
            attachSubLine(line, lineNumber);
            continue;
        }

        if (isCommentOrEmpty(line)) {
            continue;
        }

        std::istringstream stream(line);
        std::string kind;
        stream >> kind;

        if (kind == "mesh") {
            LevelMeshPlacement placement;
            if (!(stream >> placement.meshId
                         >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.scale.x >> placement.scale.y >> placement.scale.z
                         >> placement.rotation.x >> placement.rotation.y >> placement.rotation.z)) {
                throwParseError(path, lineNumber, "invalid mesh record");
            }
            stream >> std::ws;
            if (!stream.eof()) {
                const auto tokens = collectRemainingTokens(stream);

                if (!tokens.empty()) {
                    std::size_t index = 0;
                    while (index < tokens.size()) {
                        glm::vec3 tint{1.0f};

                        if (tokens[index] == "material") {
                            if (index + 1 >= tokens.size()) {
                                throwParseError(path, lineNumber, "missing material id");
                            }
                            placement.materialId = tokens[index + 1];
                            index += 2;
                            continue;
                        }
                        if (tokens[index] == "tint") {
                            if (!tryParseVec3Tokens(tokens, index + 1, tint)) {
                                throwParseError(path, lineNumber, "invalid mesh tint");
                            }
                            placement.tint = tint;
                            index += 4;
                            continue;
                        }
                        if (tokens[index] == "node") {
                            if (index + 1 >= tokens.size()) {
                                throwParseError(path, lineNumber, "missing mesh node id");
                            }
                            placement.nodeId = tokens[index + 1];
                            index += 2;
                            continue;
                        }
                        if (tokens[index] == "parent") {
                            if (index + 1 >= tokens.size()) {
                                throwParseError(path, lineNumber, "missing mesh parent node id");
                            }
                            placement.parentNodeId = tokens[index + 1];
                            index += 2;
                            continue;
                        }
                        if (tryParseVec3Tokens(tokens, index, tint)) {
                            placement.tint = tint;
                            index += 3;
                            continue;
                        }
                        throwParseError(path, lineNumber, "invalid mesh metadata");
                    }
                }
            }
            currentKind = CurrentEntityKind::Mesh;
            currentIndex = data.meshes.size();
            data.meshes.push_back(std::move(placement));
            continue;
        }

        if (kind == "environment_profile") {
            std::string token;
            if (!(stream >> token)) {
                throwParseError(path, lineNumber, "invalid environment_profile record");
            }
            data.environmentId = token;
            if (tryParseEnvironmentProfileToken(token, data.environmentProfile)) {
                continue;
            }
            continue;
        }

        if (kind == "light") {
            LevelLightPlacement placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.color.r >> placement.color.g >> placement.color.b
                         >> placement.radius >> placement.intensity)) {
                throwParseError(path, lineNumber, "invalid light record");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing light node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing light parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid light metadata");
            }
            currentKind = CurrentEntityKind::Light;
            currentIndex = data.lights.size();
            data.lights.push_back(placement);
            continue;
        }

        if (kind == "spot_light") {
            LevelLightPlacement placement;
            placement.type = LightType::Spot;
            std::string shadowToken;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.direction.x >> placement.direction.y >> placement.direction.z
                         >> placement.color.r >> placement.color.g >> placement.color.b
                         >> placement.radius >> placement.intensity
                         >> placement.innerConeDegrees >> placement.outerConeDegrees
                         >> shadowToken)) {
                throwParseError(path, lineNumber, "invalid spot_light record");
            }
            if (!tryParseBoolToken(shadowToken, placement.castsShadows)) {
                throwParseError(path, lineNumber, "invalid spot_light shadow flag");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing spot_light node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing spot_light parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid spot_light metadata");
            }
            currentKind = CurrentEntityKind::Light;
            currentIndex = data.lights.size();
            data.lights.push_back(placement);
            continue;
        }

        if (kind == "dir_light") {
            LevelLightPlacement placement;
            placement.type = LightType::Directional;
            if (!(stream >> placement.direction.x >> placement.direction.y >> placement.direction.z
                         >> placement.color.r >> placement.color.g >> placement.color.b
                         >> placement.intensity)) {
                throwParseError(path, lineNumber, "invalid dir_light record");
            }
            placement.radius = 0.0f;
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing dir_light node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing dir_light parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid dir_light metadata");
            }
            currentKind = CurrentEntityKind::Light;
            currentIndex = data.lights.size();
            data.lights.push_back(placement);
            continue;
        }

        if (kind == "collider_box") {
            LevelColliderPlacement placement;
            placement.shape = ColliderShape::Box;
            placement.mode = ColliderMode::Solid;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.halfExtents.x >> placement.halfExtents.y >> placement.halfExtents.z)) {
                throwParseError(path, lineNumber, "invalid collider_box record");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "rotation") {
                    if (!tryParseVec3Tokens(tokens, index + 1, placement.rotation)) {
                        throwParseError(path, lineNumber, "invalid collider_box rotation");
                    }
                    index += 4;
                    continue;
                }
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing collider_box node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing collider_box parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid collider_box metadata");
            }
            currentKind = CurrentEntityKind::None;
            data.colliders.push_back(placement);
            continue;
        }

        if (kind == "collider_cylinder") {
            LevelColliderPlacement placement;
            placement.shape = ColliderShape::Cylinder;
            placement.mode = ColliderMode::Solid;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.radius >> placement.halfHeight)) {
                throwParseError(path, lineNumber, "invalid collider_cylinder record");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "rotation") {
                    if (!tryParseVec3Tokens(tokens, index + 1, placement.rotation)) {
                        throwParseError(path, lineNumber, "invalid collider_cylinder rotation");
                    }
                    index += 4;
                    continue;
                }
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing collider_cylinder node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing collider_cylinder parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid collider_cylinder metadata");
            }
            currentKind = CurrentEntityKind::None;
            data.colliders.push_back(placement);
            continue;
        }

        if (kind == "collider") {
            // New unified format: collider <shape> <mode> <px> <py> <pz> <shape_params...> [options]
            LevelColliderPlacement placement;
            std::string shapeToken, modeToken;
            if (!(stream >> shapeToken >> modeToken)) {
                throwParseError(path, lineNumber, "invalid collider record: missing shape/mode tokens");
            }
            // Parse shape
            if (shapeToken == "box")          { placement.shape = ColliderShape::Box; }
            else if (shapeToken == "sphere")   { placement.shape = ColliderShape::Sphere; }
            else if (shapeToken == "cylinder") { placement.shape = ColliderShape::Cylinder; }
            else if (shapeToken == "capsule")  { placement.shape = ColliderShape::Capsule; }
            else { throwParseError(path, lineNumber, "unknown collider shape '" + shapeToken + "'"); }
            // Parse mode
            if (modeToken == "solid")                 { placement.mode = ColliderMode::Solid; }
            else if (modeToken == "trigger")           { placement.mode = ColliderMode::Trigger; }
            else if (modeToken == "solidandtrigger")   { placement.mode = ColliderMode::SolidAndTrigger; }
            else { throwParseError(path, lineNumber, "unknown collider mode '" + modeToken + "'"); }
            // Parse position
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z)) {
                throwParseError(path, lineNumber, "invalid collider record: missing position");
            }
            // Parse shape-specific params
            if (placement.shape == ColliderShape::Box) {
                if (!(stream >> placement.halfExtents.x >> placement.halfExtents.y >> placement.halfExtents.z)) {
                    throwParseError(path, lineNumber, "invalid collider box: missing halfExtents");
                }
            } else if (placement.shape == ColliderShape::Sphere) {
                if (!(stream >> placement.radius)) {
                    throwParseError(path, lineNumber, "invalid collider sphere: missing radius");
                }
            } else {
                // Cylinder or Capsule
                if (!(stream >> placement.radius >> placement.halfHeight)) {
                    throwParseError(path, lineNumber, "invalid collider cylinder/capsule: missing radius/halfHeight");
                }
            }
            // Parse optional modifiers
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "rotation") {
                    if (!tryParseVec3Tokens(tokens, index + 1, placement.rotation)) {
                        throwParseError(path, lineNumber, "invalid collider rotation");
                    }
                    index += 4;
                    continue;
                }
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing collider node id");
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing collider parent node id");
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "fire_once") {
                    placement.fireOnce = true;
                    index += 1;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid collider metadata '" + tokens[index] + "'");
            }
            const bool isTriggerLike = (placement.mode == ColliderMode::Trigger
                                        || placement.mode == ColliderMode::SolidAndTrigger);
            if (isTriggerLike) {
                currentKind = CurrentEntityKind::Collider;
                currentIndex = data.colliders.size();
            } else {
                currentKind = CurrentEntityKind::None;
            }
            data.colliders.push_back(std::move(placement));
            continue;
        }

        if (kind == "reflection_probe") {
            LevelReflectionProbePlacement placement;
            std::string boxProjectionToken;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.extents.x >> placement.extents.y >> placement.extents.z
                         >> placement.blendDistance >> placement.intensity
                         >> boxProjectionToken)) {
                throwParseError(path, lineNumber, "invalid reflection_probe record");
            }
            if (!tryParseBoolToken(boxProjectionToken, placement.boxProjection)) {
                throwParseError(path, lineNumber, "invalid reflection_probe box_projection flag");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing reflection_probe node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing reflection_probe parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid reflection_probe metadata");
            }
            currentKind = CurrentEntityKind::None;
            data.reflectionProbes.push_back(std::move(placement));
            continue;
        }

        if (kind == "player_spawn") {
            LevelPlayerSpawn placement;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.fallRespawnY)) {
                throwParseError(path, lineNumber, "invalid player_spawn record");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing player_spawn node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing player_spawn parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid player_spawn metadata");
            }
            currentKind = CurrentEntityKind::None;
            data.playerSpawn = placement;
            data.hasPlayerSpawn = true;
            continue;
        }

        if (kind == "archetype_instance") {
            LevelArchetypePlacement placement;
            if (!(stream >> placement.archetypeId
                         >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.yawDegrees)) {
                throwParseError(path, lineNumber, "invalid archetype_instance record");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing archetype node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing archetype parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid archetype metadata");
            }
            currentKind = CurrentEntityKind::None;
            data.archetypes.push_back(std::move(placement));
            continue;
        }

        if (kind == "group") {
            LevelGroupNode placement;
            if (!(stream >> placement.name
                         >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.scale.x >> placement.scale.y >> placement.scale.z
                         >> placement.rotation.x >> placement.rotation.y >> placement.rotation.z)) {
                throwParseError(path, lineNumber, "invalid group record");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing group node id");
                    }
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) {
                        throwParseError(path, lineNumber, "missing group parent node id");
                    }
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid group metadata");
            }
            currentKind = CurrentEntityKind::None;
            data.groups.push_back(std::move(placement));
            continue;
        }

        if (kind == "trigger_box") {
            // Legacy format — read as unified collider with Trigger mode
            LevelColliderPlacement placement;
            placement.shape = ColliderShape::Box;
            placement.mode = ColliderMode::Trigger;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.halfExtents.x >> placement.halfExtents.y >> placement.halfExtents.z)) {
                throwParseError(path, lineNumber, "invalid trigger_box record");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing trigger_box node id");
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing trigger_box parent node id");
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "fire_once") {
                    placement.fireOnce = true;
                    index += 1;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid trigger_box metadata");
            }
            currentKind = CurrentEntityKind::Collider;
            currentIndex = data.colliders.size();
            data.colliders.push_back(std::move(placement));
            continue;
        }

        if (kind == "trigger_sphere") {
            // Legacy format — read as unified collider with Trigger mode
            LevelColliderPlacement placement;
            placement.shape = ColliderShape::Sphere;
            placement.mode = ColliderMode::Trigger;
            if (!(stream >> placement.position.x >> placement.position.y >> placement.position.z
                         >> placement.radius)) {
                throwParseError(path, lineNumber, "invalid trigger_sphere record");
            }
            const auto tokens = collectRemainingTokens(stream);
            for (std::size_t index = 0; index < tokens.size();) {
                if (tokens[index] == "node") {
                    if (index + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing trigger_sphere node id");
                    placement.nodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "parent") {
                    if (index + 1 >= tokens.size()) throwParseError(path, lineNumber, "missing trigger_sphere parent node id");
                    placement.parentNodeId = tokens[index + 1];
                    index += 2;
                    continue;
                }
                if (tokens[index] == "fire_once") {
                    placement.fireOnce = true;
                    index += 1;
                    continue;
                }
                throwParseError(path, lineNumber, "invalid trigger_sphere metadata");
            }
            currentKind = CurrentEntityKind::Collider;
            currentIndex = data.colliders.size();
            data.colliders.push_back(std::move(placement));
            continue;
        }

        throwParseError(path, lineNumber, "unknown record type '" + kind + "'");
    }

    return data;
}

LevelDef resolveLevelHierarchy(const LevelDef& data) {
    LevelDef resolved = data;

    std::vector<LevelNodeRef> refs;
    refs.reserve(data.meshes.size()
                 + data.lights.size()
                 + data.colliders.size()
                 + data.reflectionProbes.size()
                 + data.archetypes.size()
                 + data.groups.size()
                 + (data.hasPlayerSpawn ? 1u : 0u));

    for (std::size_t i = 0; i < data.meshes.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::Mesh, i, data.meshes[i].nodeId, data.meshes[i].parentNodeId});
    }
    for (std::size_t i = 0; i < data.lights.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::Light, i, data.lights[i].nodeId, data.lights[i].parentNodeId});
    }
    for (std::size_t i = 0; i < data.colliders.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::Collider, i, data.colliders[i].nodeId, data.colliders[i].parentNodeId});
    }
    for (std::size_t i = 0; i < data.reflectionProbes.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::ReflectionProbe, i, data.reflectionProbes[i].nodeId, data.reflectionProbes[i].parentNodeId});
    }
    if (data.hasPlayerSpawn) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::PlayerSpawn, 0, data.playerSpawn.nodeId, data.playerSpawn.parentNodeId});
    }
    for (std::size_t i = 0; i < data.archetypes.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::Archetype, i, data.archetypes[i].nodeId, data.archetypes[i].parentNodeId});
    }
    for (std::size_t i = 0; i < data.groups.size(); ++i) {
        refs.push_back(LevelNodeRef{LevelNodeRef::Kind::Group, i, data.groups[i].nodeId, data.groups[i].parentNodeId});
    }

    std::unordered_map<std::string, std::size_t> nodeLookup;
    for (std::size_t i = 0; i < refs.size(); ++i) {
        if (refs[i].nodeId.empty()) {
            continue;
        }
        const auto [it, inserted] = nodeLookup.emplace(refs[i].nodeId, i);
        if (!inserted) {
            throw std::runtime_error("Duplicate level node id: " + refs[i].nodeId);
        }
    }

    auto localMatrixFor = [&](const LevelNodeRef& ref) {
        switch (ref.kind) {
        case LevelNodeRef::Kind::Mesh: {
            const auto& placement = data.meshes[ref.index];
            return makeTransformMatrix(placement.position, placement.rotation, placement.scale);
        }
        case LevelNodeRef::Kind::Light: {
            const auto& placement = data.lights[ref.index];
            if (placement.type == LightType::Directional) {
                return glm::mat4(1.0f);
            }
            return makeTransformMatrix(placement.position, glm::vec3(0.0f), glm::vec3(1.0f));
        }
        case LevelNodeRef::Kind::Collider: {
            const auto& placement = data.colliders[ref.index];
            if (placement.shape == ColliderShape::Box) {
                return makeTransformMatrix(placement.position, placement.rotation, placement.halfExtents * 2.0f);
            }
            // Sphere, Cylinder, Capsule — use radius-based scale
            return makeTransformMatrix(placement.position,
                                       placement.rotation,
                                       glm::vec3(placement.radius * 2.0f, placement.halfHeight * 2.0f, placement.radius * 2.0f));
        }
        case LevelNodeRef::Kind::ReflectionProbe: {
            const auto& placement = data.reflectionProbes[ref.index];
            return makeTransformMatrix(placement.position, glm::vec3(0.0f), placement.extents * 2.0f);
        }
        case LevelNodeRef::Kind::PlayerSpawn:
            return makeTransformMatrix(data.playerSpawn.position, glm::vec3(0.0f), glm::vec3(1.0f));
        case LevelNodeRef::Kind::Archetype: {
            const auto& placement = data.archetypes[ref.index];
            return makeTransformMatrix(placement.position, glm::vec3(0.0f, placement.yawDegrees, 0.0f), glm::vec3(1.0f));
        }
        case LevelNodeRef::Kind::Group: {
            const auto& placement = data.groups[ref.index];
            return makeTransformMatrix(placement.position, placement.rotation, placement.scale);
        }
        }
        return glm::mat4(1.0f);
    };

    std::vector<glm::mat4> worldMatrices(refs.size(), glm::mat4(1.0f));
    std::vector<glm::mat3> worldRotations(refs.size(), glm::mat3(1.0f));
    std::vector<bool> resolvedFlags(refs.size(), false);
    std::vector<bool> visiting(refs.size(), false);

    std::function<void(std::size_t)> resolveRef = [&](std::size_t idx) {
        if (resolvedFlags[idx]) {
            return;
        }
        if (visiting[idx]) {
            throw std::runtime_error("Cycle detected in level hierarchy");
        }
        visiting[idx] = true;

        glm::mat4 parentMatrix(1.0f);
        glm::mat3 parentRotation(1.0f);
        const std::string& parentNodeId = refs[idx].parentNodeId;
        if (!parentNodeId.empty()) {
            const auto parentIt = nodeLookup.find(parentNodeId);
            if (parentIt != nodeLookup.end()) {
                resolveRef(parentIt->second);
                parentMatrix = worldMatrices[parentIt->second];
                parentRotation = worldRotations[parentIt->second];
            }
        }

        worldMatrices[idx] = parentMatrix * localMatrixFor(refs[idx]);
        worldRotations[idx] = extractRotationMatrix(worldMatrices[idx]);

        switch (refs[idx].kind) {
        case LevelNodeRef::Kind::Mesh: {
            auto& placement = resolved.meshes[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                placement.scale = scale;
            }
            break;
        }
        case LevelNodeRef::Kind::Light: {
            auto& placement = resolved.lights[refs[idx].index];
            if (placement.type != LightType::Directional) {
                placement.position = glm::vec3(worldMatrices[idx][3]);
            }
            placement.direction = safeNormalize(parentRotation * data.lights[refs[idx].index].direction,
                                               placement.type == LightType::Directional
                                                   ? glm::vec3(0.0f, -1.0f, 0.0f)
                                                   : glm::vec3(0.0f, 0.0f, -1.0f));
            break;
        }
        case LevelNodeRef::Kind::Collider: {
            auto& placement = resolved.colliders[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                if (placement.shape == ColliderShape::Box) {
                    placement.halfExtents = glm::max(scale * 0.5f, glm::vec3(0.001f));
                } else {
                    placement.radius = std::max(0.001f, (std::abs(scale.x) + std::abs(scale.z)) * 0.25f);
                    placement.halfHeight = std::max(0.001f, std::abs(scale.y) * 0.5f);
                }
            }
            break;
        }
        case LevelNodeRef::Kind::ReflectionProbe: {
            auto& placement = resolved.reflectionProbes[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                (void)rotation;
                placement.position = position;
                placement.extents = glm::max(scale * 0.5f, glm::vec3(0.05f));
            }
            break;
        }
        case LevelNodeRef::Kind::PlayerSpawn:
            resolved.playerSpawn.position = glm::vec3(worldMatrices[idx][3]);
            break;
        case LevelNodeRef::Kind::Archetype: {
            auto& placement = resolved.archetypes[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                placement.position = position;
                placement.yawDegrees = rotation.y;
            }
            break;
        }
        case LevelNodeRef::Kind::Group: {
            auto& placement = resolved.groups[refs[idx].index];
            glm::vec3 position(0.0f), rotation(0.0f), scale(1.0f);
            if (decomposeTransformMatrix(worldMatrices[idx], position, rotation, scale)) {
                placement.position = position;
                placement.rotation = rotation;
                placement.scale = glm::max(scale, glm::vec3(0.01f));
            }
            break;
        }
        }

        visiting[idx] = false;
        resolvedFlags[idx] = true;
    };

    for (std::size_t i = 0; i < refs.size(); ++i) {
        resolveRef(i);
    }

    return resolved;
}

std::string serializeLevelDef(const LevelDef& data) {
    std::ostringstream out;
    out << "environment_profile " << resolvedEnvironmentId(data) << "\n\n";

    for (const auto& placement : data.meshes) {
        out << "mesh " << placement.meshId << ' '
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.scale.x) << ' '
            << formatFloat(placement.scale.y) << ' '
            << formatFloat(placement.scale.z) << ' '
            << formatFloat(placement.rotation.x) << ' '
            << formatFloat(placement.rotation.y) << ' '
            << formatFloat(placement.rotation.z) << ' '
            << "material " << (placement.materialId.empty() ? "stone_default" : placement.materialId);
        if (placement.tint.has_value()) {
            out << " tint "
                << formatFloat(placement.tint->r) << ' '
                << formatFloat(placement.tint->g) << ' '
                << formatFloat(placement.tint->b);
        }
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
        if (placement.interactable.has_value()) {
            const auto& ia = *placement.interactable;
            out << "  interactable \"" << ia.promptText << "\" distance " << formatFloat(ia.distance);
            if (std::abs(ia.dotThreshold - 0.55f) > 0.0001f) {
                out << " dot_threshold " << formatFloat(ia.dotThreshold);
            }
            out << '\n';
        }
        serializeBehaviors(out, placement.behaviors);
    }

    for (const auto& placement : data.lights) {
        if (placement.type == LightType::Spot) {
            out << "spot_light "
                << formatFloat(placement.position.x) << ' '
                << formatFloat(placement.position.y) << ' '
                << formatFloat(placement.position.z) << ' '
                << formatFloat(placement.direction.x) << ' '
                << formatFloat(placement.direction.y) << ' '
                << formatFloat(placement.direction.z) << ' '
                << formatFloat(placement.color.r) << ' '
                << formatFloat(placement.color.g) << ' '
                << formatFloat(placement.color.b) << ' '
                << formatFloat(placement.radius) << ' '
                << formatFloat(placement.intensity) << ' '
                << formatFloat(placement.innerConeDegrees) << ' '
                << formatFloat(placement.outerConeDegrees) << ' '
                << (placement.castsShadows ? "true" : "false");
            appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
            out << '\n';
            serializeBehaviors(out, placement.behaviors);
            continue;
        }

        if (placement.type == LightType::Directional) {
            out << "dir_light "
                << formatFloat(placement.direction.x) << ' '
                << formatFloat(placement.direction.y) << ' '
                << formatFloat(placement.direction.z) << ' '
                << formatFloat(placement.color.r) << ' '
                << formatFloat(placement.color.g) << ' '
                << formatFloat(placement.color.b) << ' '
                << formatFloat(placement.intensity);
            appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
            out << '\n';
            serializeBehaviors(out, placement.behaviors);
            continue;
        }

        out << "light "
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.color.r) << ' '
            << formatFloat(placement.color.g) << ' '
            << formatFloat(placement.color.b) << ' '
            << formatFloat(placement.radius) << ' '
            << formatFloat(placement.intensity);
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
        serializeBehaviors(out, placement.behaviors);
    }

    for (const auto& c : data.colliders) {
        out << "collider ";
        // shape token
        switch (c.shape) {
            case ColliderShape::Box:      out << "box ";      break;
            case ColliderShape::Sphere:   out << "sphere ";   break;
            case ColliderShape::Cylinder: out << "cylinder "; break;
            case ColliderShape::Capsule:  out << "capsule ";  break;
        }
        // mode token
        switch (c.mode) {
            case ColliderMode::Solid:          out << "solid ";          break;
            case ColliderMode::Trigger:        out << "trigger ";        break;
            case ColliderMode::SolidAndTrigger: out << "solidandtrigger "; break;
        }
        // position
        out << formatFloat(c.position.x) << ' '
            << formatFloat(c.position.y) << ' '
            << formatFloat(c.position.z) << ' ';
        // shape-specific params
        if (c.shape == ColliderShape::Box) {
            out << formatFloat(c.halfExtents.x) << ' '
                << formatFloat(c.halfExtents.y) << ' '
                << formatFloat(c.halfExtents.z);
        } else if (c.shape == ColliderShape::Sphere) {
            out << formatFloat(c.radius);
        } else {
            // Cylinder or Capsule
            out << formatFloat(c.radius) << ' ' << formatFloat(c.halfHeight);
        }
        // optional modifiers
        if (hasNonZeroVec3(c.rotation)) {
            out << " rotation "
                << formatFloat(c.rotation.x) << ' '
                << formatFloat(c.rotation.y) << ' '
                << formatFloat(c.rotation.z);
        }
        appendNodeMetadata(out, c.nodeId, c.parentNodeId);
        if (c.fireOnce) {
            out << " fire_once";
        }
        out << '\n';
        serializeBehaviors(out, c.behaviors);
    }

    for (const auto& placement : data.reflectionProbes) {
        out << "reflection_probe "
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.extents.x) << ' '
            << formatFloat(placement.extents.y) << ' '
            << formatFloat(placement.extents.z) << ' '
            << formatFloat(placement.blendDistance) << ' '
            << formatFloat(placement.intensity) << ' '
            << (placement.boxProjection ? "true" : "false");
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
    }

    if (data.hasPlayerSpawn) {
        out << "player_spawn "
            << formatFloat(data.playerSpawn.position.x) << ' '
            << formatFloat(data.playerSpawn.position.y) << ' '
            << formatFloat(data.playerSpawn.position.z) << ' '
            << formatFloat(data.playerSpawn.fallRespawnY);
        appendNodeMetadata(out, data.playerSpawn.nodeId, data.playerSpawn.parentNodeId);
        out << '\n';
    }

    for (const auto& placement : data.archetypes) {
        out << "archetype_instance " << placement.archetypeId << ' '
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.yawDegrees);
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
    }

    for (const auto& placement : data.groups) {
        out << "group " << placement.name << ' '
            << formatFloat(placement.position.x) << ' '
            << formatFloat(placement.position.y) << ' '
            << formatFloat(placement.position.z) << ' '
            << formatFloat(placement.scale.x) << ' '
            << formatFloat(placement.scale.y) << ' '
            << formatFloat(placement.scale.z) << ' '
            << formatFloat(placement.rotation.x) << ' '
            << formatFloat(placement.rotation.y) << ' '
            << formatFloat(placement.rotation.z);
        appendNodeMetadata(out, placement.nodeId, placement.parentNodeId);
        out << '\n';
    }

    return out.str();
}

void saveLevelDef(const std::string& path, const LevelDef& data) {
    namespace fs = std::filesystem;
    const fs::path target(path);
    if (target.has_parent_path()) {
        fs::create_directories(target.parent_path());
    }
    std::ofstream out(target);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to write level definition: " + target.string());
    }
    out << serializeLevelDef(data);
}
