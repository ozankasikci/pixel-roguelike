#include "engine/audio/mix/BusGraph.h"

#include <algorithm>

namespace audio {

BusGraph::BusGraph() {
    // Default hierarchy:
    //   Master (1.0)
    //   +-- Music (1.0)
    //   +-- SFX (1.0)
    //   |   +-- Footsteps (1.0)
    //   |   +-- Doors (1.0)
    //   |   +-- Environment (1.0)
    //   +-- Ambience (1.0)
    //   +-- UI (1.0)
    addBus("Master", "", 1.0f);
    addBus("Music", "Master", 1.0f);
    addBus("SFX", "Master", 1.0f);
    addBus("Footsteps", "SFX", 1.0f);
    addBus("Doors", "SFX", 1.0f);
    addBus("Environment", "SFX", 1.0f);
    addBus("Ambience", "Master", 1.0f);
    addBus("UI", "Master", 1.0f);
}

void BusGraph::addBus(const std::string& name, const std::string& parent, float volume) {
    Bus bus;
    bus.name = name;
    bus.parent = parent;
    bus.volume = std::clamp(volume, 0.0f, 1.0f);
    bus.muted = false;
    buses_[name] = bus;
}

void BusGraph::setVolume(const std::string& name, float volume) {
    auto it = buses_.find(name);
    if (it != buses_.end()) {
        it->second.volume = std::clamp(volume, 0.0f, 1.0f);
    }
}

void BusGraph::setMute(const std::string& name, bool muted) {
    auto it = buses_.find(name);
    if (it != buses_.end()) {
        it->second.muted = muted;
    }
}

float BusGraph::busVolume(const std::string& name) const {
    auto it = buses_.find(name);
    if (it == buses_.end()) {
        return 0.0f;
    }
    return it->second.volume;
}

float BusGraph::effectiveVolume(const std::string& name) const {
    auto it = buses_.find(name);
    if (it == buses_.end()) {
        return 0.0f;
    }

    float result = 1.0f;
    const Bus* current = &it->second;

    // Walk up the chain, multiplying volumes; zero out if any bus is muted.
    while (current != nullptr) {
        if (current->muted) {
            return 0.0f;
        }
        result *= current->volume;

        if (current->parent.empty()) {
            break;
        }
        auto parent_it = buses_.find(current->parent);
        if (parent_it == buses_.end()) {
            break;
        }
        current = &parent_it->second;
    }

    return result;
}

bool BusGraph::busExists(const std::string& name) const {
    return buses_.find(name) != buses_.end();
}

bool BusGraph::isMuted(const std::string& name) const {
    auto it = buses_.find(name);
    if (it == buses_.end()) {
        return false;
    }
    return it->second.muted;
}

} // namespace audio
