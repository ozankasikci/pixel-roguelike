#pragma once

#include <string>
#include <unordered_map>

namespace engine::audio {

class BusGraph {
public:
    BusGraph();

    // Non-copyable
    BusGraph(const BusGraph&) = delete;
    BusGraph& operator=(const BusGraph&) = delete;

    /// Set the direct volume of a bus, clamped to [0, 1].
    void setVolume(const std::string& name, float volume);

    /// Set the mute state of a bus.
    void setMute(const std::string& name, bool muted);

    /// Return the direct volume of a bus (0 if unknown).
    float busVolume(const std::string& name) const;

    /// Return the product of all ancestor volumes (0 if any ancestor is muted, or bus unknown).
    float effectiveVolume(const std::string& name) const;

    /// Return true if a bus with the given name exists.
    bool busExists(const std::string& name) const;

    /// Return true if the bus is directly muted.
    bool isMuted(const std::string& name) const;

private:
    struct Bus {
        std::string name;
        std::string parent; // empty for root
        float volume = 1.0f;
        bool muted = false;
    };

    void addBus(const std::string& name, const std::string& parent, float volume);

    std::unordered_map<std::string, Bus> buses_;
};

} // namespace engine::audio
