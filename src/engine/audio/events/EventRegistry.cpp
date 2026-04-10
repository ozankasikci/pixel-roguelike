#include "engine/audio/events/EventRegistry.h"

#include <fstream>
#include <algorithm>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace engine::audio {

namespace {

PickMode parsePickMode(const std::string& str) {
    if (str == "random_no_repeat") {
        return PickMode::RandomNoRepeat;
    }
    if (str == "round_robin") {
        return PickMode::RoundRobin;
    }
    if (str == "sequential") {
        return PickMode::Sequential;
    }
    return PickMode::Random;
}

} // namespace

bool EventRegistry::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(file);
    } catch (const nlohmann::json::parse_error&) {
        return false;
    }

    if (!root.is_object()) {
        return false;
    }

    for (auto it = root.begin(); it != root.end(); ++it) {
        SoundEventDef def;
        def.name = it.key();

        const auto& obj = it.value();

        if (obj.contains("sounds") && obj["sounds"].is_array()) {
            for (const auto& s : obj["sounds"]) {
                if (s.is_string()) {
                    def.sounds.push_back(s.get<std::string>());
                }
            }
        }

        if (obj.contains("pick") && obj["pick"].is_string()) {
            def.pickMode = parsePickMode(obj["pick"].get<std::string>());
        }

        if (obj.contains("pitch_range") && obj["pitch_range"].is_array() &&
            obj["pitch_range"].size() == 2) {
            def.pitchRange.x = obj["pitch_range"][0].get<float>();
            def.pitchRange.y = obj["pitch_range"][1].get<float>();
        }

        if (obj.contains("volume_range") && obj["volume_range"].is_array() &&
            obj["volume_range"].size() == 2) {
            def.volumeRange.x = obj["volume_range"][0].get<float>();
            def.volumeRange.y = obj["volume_range"][1].get<float>();
        }

        if (obj.contains("max_instances") && obj["max_instances"].is_number_integer()) {
            def.maxInstances = obj["max_instances"].get<int>();
        }

        if (obj.contains("cooldown") && obj["cooldown"].is_number()) {
            def.cooldown = obj["cooldown"].get<float>();
        }

        if (obj.contains("bus") && obj["bus"].is_string()) {
            def.busName = obj["bus"].get<std::string>();
        }

        if (obj.contains("spatial") && obj["spatial"].is_string()) {
            def.is3D = (obj["spatial"].get<std::string>() == "3D");
        }

        if (obj.contains("ref_distance") && obj["ref_distance"].is_number()) {
            def.refDistance = obj["ref_distance"].get<float>();
        }

        if (obj.contains("max_distance") && obj["max_distance"].is_number()) {
            def.maxDistance = obj["max_distance"].get<float>();
        }

        events_.emplace(def.name, std::move(def));
    }

    return true;
}

const SoundEventDef* EventRegistry::find(const std::string& name) const {
    auto it = events_.find(name);
    if (it == events_.end()) {
        return nullptr;
    }
    return &it->second;
}

SoundEventDef* EventRegistry::find(const std::string& name) {
    auto it = events_.find(name);
    if (it == events_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> EventRegistry::eventNames() const {
    std::vector<std::string> names;
    names.reserve(events_.size());
    for (const auto& [name, def] : events_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> EventRegistry::allSoundPaths() const {
    std::unordered_set<std::string> unique;
    for (const auto& [name, def] : events_) {
        for (const auto& path : def.sounds) {
            unique.insert(path);
        }
    }
    std::vector<std::string> paths(unique.begin(), unique.end());
    std::sort(paths.begin(), paths.end());
    return paths;
}

} // namespace engine::audio
