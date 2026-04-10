#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "engine/audio/events/SoundEventDef.h"

namespace engine::audio {

class EventRegistry {
public:
    EventRegistry() = default;
    ~EventRegistry() = default;

    // Non-copyable
    EventRegistry(const EventRegistry&) = delete;
    EventRegistry& operator=(const EventRegistry&) = delete;

    /// Load sound event definitions from a JSON file.
    /// Returns true on success, false on parse or I/O error.
    bool loadFromFile(const std::string& path);

    /// Find a sound event by name. Returns nullptr if not found.
    const SoundEventDef* find(const std::string& name) const;
    SoundEventDef* find(const std::string& name);

    /// Return all registered event names.
    std::vector<std::string> eventNames() const;

    /// Return all unique sound file paths across all events.
    std::vector<std::string> allSoundPaths() const;

private:
    std::unordered_map<std::string, SoundEventDef> events_;
};

} // namespace engine::audio
