# Audio Engine Module Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the existing AudioSystem with a 3-layer audio engine module (backend / mix engine / game audio) that provides voice management, bus hierarchy, occlusion, and reverb — designed for a prison horror game.

**Architecture:** Three layers with strict downward dependency. Backend wraps OpenAL Soft into RAII types. Mix engine handles voice allocation, bus mixing, occlusion, and reverb. Game audio layer provides data-driven event mapping and the public command API. A facade class `AudioEngine` owns all layers and is the sole public interface.

**Tech Stack:** C++20, OpenAL Soft 1.25.1 (EFX extensions), stb_vorbis, nlohmann/json 3.11.3, Jolt Physics (raycasts for occlusion), GLM

**Design document:** `docs/plans/2026-04-10-audio-engine-module-design.md`

**Worktree:** `.worktrees/audio-engine` on branch `feature/audio-engine-module`

---

## Task 1: VoiceHandle — Opaque Handle Type

**Files:**
- Create: `src/engine/audio/VoiceHandle.h`
- Test: `tests/engine/test_voice_handle.cpp`

**Step 1: Write the test**

```cpp
// tests/engine/test_voice_handle.cpp
#include "engine/audio/VoiceHandle.h"
#include <cassert>
#include <cstdio>
#include <unordered_set>

int main() {
    // Default handle is invalid
    VoiceHandle h1;
    assert(!h1.valid());

    // Constructed handle is valid
    VoiceHandle h2(1);
    assert(h2.valid());

    // Equality
    VoiceHandle h3(1);
    VoiceHandle h4(2);
    assert(h2 == h3);
    assert(h2 != h4);

    // Usable as unordered_set key
    std::unordered_set<VoiceHandle> set;
    set.insert(h2);
    set.insert(h4);
    assert(set.size() == 2);
    assert(set.count(h3) == 1); // same as h2

    // Generation counter distinguishes reused IDs
    VoiceHandle h5(1, 1);
    VoiceHandle h6(1, 2);
    assert(h5 != h6);

    printf("test_voice_handle: all assertions passed\n");
    return 0;
}
```

**Step 2: Write the implementation**

```cpp
// src/engine/audio/VoiceHandle.h
#pragma once

#include <cstdint>
#include <functional>

struct VoiceHandle {
    uint32_t id = 0;
    uint32_t generation = 0;

    VoiceHandle() = default;
    explicit VoiceHandle(uint32_t id, uint32_t gen = 0) : id(id), generation(gen) {}

    bool valid() const { return id != 0; }

    bool operator==(const VoiceHandle& o) const {
        return id == o.id && generation == o.generation;
    }
    bool operator!=(const VoiceHandle& o) const { return !(*this == o); }
};

template <>
struct std::hash<VoiceHandle> {
    size_t operator()(const VoiceHandle& h) const noexcept {
        return std::hash<uint64_t>{}(
            (static_cast<uint64_t>(h.id) << 32) | h.generation);
    }
};
```

**Step 3: Register test in CMakeLists.txt**

Add to `tests/engine/CMakeLists.txt`:
```cmake
pixel_roguelike_add_test(test_voice_handle
    SOURCES test_voice_handle.cpp
    LIBRARIES glm::glm
    LABELS engine audio
)
```

**Step 4: Build and run test**

```bash
cd .worktrees/audio-engine && cmake --build build --target test_voice_handle && ctest --test-dir build -R test_voice_handle -V
```
Expected: PASS

**Step 5: Commit**

```bash
git add src/engine/audio/VoiceHandle.h tests/engine/test_voice_handle.cpp tests/engine/CMakeLists.txt
git commit -m "Add VoiceHandle opaque handle type for audio voice tracking"
```

---

## Task 2: AudioCommand — Command Types

**Files:**
- Create: `src/engine/audio/AudioCommand.h`
- Test: `tests/engine/test_audio_command.cpp`

**Step 1: Write the test**

```cpp
// tests/engine/test_audio_command.cpp
#include "engine/audio/AudioCommand.h"
#include <cassert>
#include <cstdio>
#include <vector>

int main() {
    using namespace audio;

    // Play command stores event name and position
    AudioCommand cmd = PlayCommand{"door_open", glm::vec3(1, 2, 3), 0.8f, 1.0f};
    assert(std::holds_alternative<PlayCommand>(cmd));

    auto& play = std::get<PlayCommand>(cmd);
    assert(play.eventName == "door_open");
    assert(play.position.x == 1.0f);
    assert(play.volume == 0.8f);

    // PlayLooping returns a handle
    PlayLoopingCommand loop{"ambient_hum", glm::vec3(0), 1.0f, 1.0f};
    assert(loop.eventName == "ambient_hum");

    // Stop command
    VoiceHandle h(42);
    AudioCommand stop = StopCommand{h, 0.1f};
    assert(std::holds_alternative<StopCommand>(stop));
    assert(std::get<StopCommand>(stop).handle == h);

    // SetBusVolume command
    AudioCommand bus = SetBusVolumeCommand{"Music", 0.5f};
    assert(std::get<SetBusVolumeCommand>(bus).busName == "Music");

    // SetListenerTransform command
    AudioCommand listener = SetListenerCommand{
        glm::vec3(0, 1, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0)};
    assert(std::holds_alternative<SetListenerCommand>(listener));

    // SetReverbPreset command
    AudioCommand reverb = SetReverbPresetCommand{"Cell"};
    assert(std::get<SetReverbPresetCommand>(reverb).presetName == "Cell");

    // UpdateVoicePosition command
    AudioCommand update = UpdateVoicePositionCommand{h, glm::vec3(5, 0, 5)};
    assert(std::get<UpdateVoicePositionCommand>(update).handle == h);

    // Command queue (vector swap pattern)
    std::vector<AudioCommand> queue;
    queue.push_back(PlayCommand{"click", glm::vec3(0), 1.0f, 1.0f});
    queue.push_back(SetBusVolumeCommand{"SFX", 0.7f});
    assert(queue.size() == 2);

    std::vector<AudioCommand> processing;
    std::swap(queue, processing);
    assert(queue.empty());
    assert(processing.size() == 2);

    printf("test_audio_command: all assertions passed\n");
    return 0;
}
```

**Step 2: Write the implementation**

```cpp
// src/engine/audio/AudioCommand.h
#pragma once

#include "engine/audio/VoiceHandle.h"
#include <glm/vec3.hpp>
#include <string>
#include <variant>

namespace audio {

struct PlayCommand {
    std::string eventName;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float pitch = 1.0f;
    bool is3D = true;
};

struct PlayLoopingCommand {
    std::string eventName;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float pitch = 1.0f;
    bool is3D = true;
};

struct StopCommand {
    VoiceHandle handle;
    float fadeTime = 0.0f;
};

struct UpdateVoicePositionCommand {
    VoiceHandle handle;
    glm::vec3 position;
};

struct SetBusVolumeCommand {
    std::string busName;
    float volume = 1.0f;
};

struct SetBusMuteCommand {
    std::string busName;
    bool mute = false;
};

struct SetListenerCommand {
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 up;
};

struct SetReverbPresetCommand {
    std::string presetName;
};

using AudioCommand = std::variant<
    PlayCommand,
    PlayLoopingCommand,
    StopCommand,
    UpdateVoicePositionCommand,
    SetBusVolumeCommand,
    SetBusMuteCommand,
    SetListenerCommand,
    SetReverbPresetCommand>;

} // namespace audio
```

**Step 3: Register test**

Add to `tests/engine/CMakeLists.txt`:
```cmake
pixel_roguelike_add_test(test_audio_command
    SOURCES test_audio_command.cpp
    LIBRARIES glm::glm
    LABELS engine audio
)
```

**Step 4: Build and run**

```bash
cmake --build build --target test_audio_command && ctest --test-dir build -R test_audio_command -V
```
Expected: PASS

**Step 5: Commit**

```bash
git add src/engine/audio/AudioCommand.h tests/engine/test_audio_command.cpp tests/engine/CMakeLists.txt
git commit -m "Add AudioCommand variant types for command queue pattern"
```

---

## Task 3: BusGraph — Hierarchical Bus Mixing

**Files:**
- Create: `src/engine/audio/mix/BusGraph.h`
- Create: `src/engine/audio/mix/BusGraph.cpp`
- Test: `tests/engine/test_bus_graph.cpp`

**Step 1: Write the test**

```cpp
// tests/engine/test_bus_graph.cpp
#include "engine/audio/mix/BusGraph.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float a, float b, float eps = 0.0001f) {
    return std::fabs(a - b) < eps;
}

int main() {
    using namespace audio;

    BusGraph graph;

    // Default hierarchy exists
    assert(graph.busExists("Master"));
    assert(graph.busExists("Music"));
    assert(graph.busExists("SFX"));
    assert(graph.busExists("Ambience"));
    assert(graph.busExists("UI"));
    assert(graph.busExists("Footsteps"));
    assert(graph.busExists("Doors"));
    assert(graph.busExists("Environment"));

    // Default volumes are 1.0
    assert(near(graph.busVolume("Master"), 1.0f));
    assert(near(graph.busVolume("SFX"), 1.0f));

    // Effective volume walks the chain
    // SFX -> Master, both at 1.0 => effective = 1.0
    assert(near(graph.effectiveVolume("SFX"), 1.0f));

    // Footsteps -> SFX -> Master
    assert(near(graph.effectiveVolume("Footsteps"), 1.0f));

    // Setting bus volume
    graph.setVolume("Master", 0.5f);
    assert(near(graph.busVolume("Master"), 0.5f));
    // Effective volume of children reflects parent
    assert(near(graph.effectiveVolume("SFX"), 0.5f));
    assert(near(graph.effectiveVolume("Footsteps"), 0.5f));

    // Nested multiplication
    graph.setVolume("SFX", 0.8f);
    // Footsteps -> SFX(0.8) -> Master(0.5) = 0.4
    assert(near(graph.effectiveVolume("Footsteps"), 0.4f));

    // Mute
    graph.setMute("SFX", true);
    assert(near(graph.effectiveVolume("Footsteps"), 0.0f));
    assert(near(graph.effectiveVolume("Doors"), 0.0f));
    // Music is unaffected
    assert(near(graph.effectiveVolume("Music"), 0.5f));

    graph.setMute("SFX", false);
    assert(near(graph.effectiveVolume("Footsteps"), 0.4f));

    // Volume clamping
    graph.setVolume("Music", 1.5f);
    assert(near(graph.busVolume("Music"), 1.0f));
    graph.setVolume("Music", -0.5f);
    assert(near(graph.busVolume("Music"), 0.0f));

    // Unknown bus returns 0 effective volume
    assert(near(graph.effectiveVolume("NonExistent"), 0.0f));

    // Reset
    graph.setVolume("Master", 1.0f);
    graph.setVolume("SFX", 1.0f);
    graph.setVolume("Music", 1.0f);

    printf("test_bus_graph: all assertions passed\n");
    return 0;
}
```

**Step 2: Write the implementation**

```cpp
// src/engine/audio/mix/BusGraph.h
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace audio {

class BusGraph {
public:
    BusGraph();

    void setVolume(const std::string& name, float volume);
    void setMute(const std::string& name, bool mute);

    float busVolume(const std::string& name) const;
    float effectiveVolume(const std::string& name) const;
    bool busExists(const std::string& name) const;
    bool isMuted(const std::string& name) const;

private:
    struct Bus {
        std::string name;
        std::string parent; // empty for root
        float volume = 1.0f;
        bool muted = false;
    };

    void addBus(const std::string& name, const std::string& parent);

    std::unordered_map<std::string, Bus> buses_;
};

} // namespace audio
```

```cpp
// src/engine/audio/mix/BusGraph.cpp
#include "engine/audio/mix/BusGraph.h"

namespace audio {

BusGraph::BusGraph() {
    addBus("Master", "");
    addBus("Music", "Master");
    addBus("SFX", "Master");
    addBus("Footsteps", "SFX");
    addBus("Doors", "SFX");
    addBus("Environment", "SFX");
    addBus("Ambience", "Master");
    addBus("UI", "Master");
}

void BusGraph::addBus(const std::string& name, const std::string& parent) {
    buses_[name] = Bus{name, parent, 1.0f, false};
}

void BusGraph::setVolume(const std::string& name, float volume) {
    auto it = buses_.find(name);
    if (it != buses_.end()) {
        it->second.volume = std::clamp(volume, 0.0f, 1.0f);
    }
}

void BusGraph::setMute(const std::string& name, bool mute) {
    auto it = buses_.find(name);
    if (it != buses_.end()) {
        it->second.muted = mute;
    }
}

float BusGraph::busVolume(const std::string& name) const {
    auto it = buses_.find(name);
    if (it == buses_.end()) return 0.0f;
    return it->second.volume;
}

float BusGraph::effectiveVolume(const std::string& name) const {
    auto it = buses_.find(name);
    if (it == buses_.end()) return 0.0f;

    float vol = 1.0f;
    const Bus* bus = &it->second;
    while (bus) {
        if (bus->muted) return 0.0f;
        vol *= bus->volume;
        if (bus->parent.empty()) break;
        auto pit = buses_.find(bus->parent);
        bus = (pit != buses_.end()) ? &pit->second : nullptr;
    }
    return vol;
}

bool BusGraph::busExists(const std::string& name) const {
    return buses_.find(name) != buses_.end();
}

bool BusGraph::isMuted(const std::string& name) const {
    auto it = buses_.find(name);
    if (it == buses_.end()) return false;
    return it->second.muted;
}

} // namespace audio
```

**Step 3: Register test and update engine CMakeLists**

Add to `tests/engine/CMakeLists.txt`:
```cmake
pixel_roguelike_add_test(test_bus_graph
    SOURCES test_bus_graph.cpp
    LIBRARIES engine_audio
    LABELS engine audio
)
```

Add `audio/mix/BusGraph.cpp` to the engine_audio target sources in `src/engine/CMakeLists.txt`.

**Step 4: Build and run**

```bash
cmake --build build --target test_bus_graph && ctest --test-dir build -R test_bus_graph -V
```
Expected: PASS

**Step 5: Commit**

```bash
git add src/engine/audio/mix/BusGraph.h src/engine/audio/mix/BusGraph.cpp tests/engine/test_bus_graph.cpp tests/engine/CMakeLists.txt src/engine/CMakeLists.txt
git commit -m "Add BusGraph hierarchical bus mixing with volume chain resolution"
```

---

## Task 4: Voice — Voice State and Audibility Scoring

**Files:**
- Create: `src/engine/audio/mix/Voice.h`
- Test: `tests/engine/test_voice_scoring.cpp`

**Step 1: Write the test**

```cpp
// tests/engine/test_voice_scoring.cpp
#include "engine/audio/mix/Voice.h"
#include "engine/audio/mix/BusGraph.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

int main() {
    using namespace audio;

    BusGraph graph;

    // Voice with full volume, at ref distance => audibility = 1.0
    Voice v;
    v.handle = VoiceHandle(1);
    v.eventName = "test";
    v.position = glm::vec3(0, 0, 0);
    v.volume = 1.0f;
    v.pitch = 1.0f;
    v.priority = 128;
    v.busName = "SFX";
    v.is3D = true;
    v.looping = false;
    v.state = VoiceState::Playing;
    v.refDistance = 1.0f;
    v.maxDistance = 50.0f;
    v.elapsedTime = 0.0f;

    glm::vec3 listener(0, 0, 0);

    float score = v.computeAudibility(listener, graph);
    assert(near(score, 1.0f));

    // Voice at distance 10, ref=1, max=50 (inverse distance clamped)
    // gain = refDist / max(dist, refDist) = 1/10 = 0.1
    v.position = glm::vec3(10, 0, 0);
    score = v.computeAudibility(listener, graph);
    assert(near(score, 0.1f));

    // Bus volume affects score
    graph.setVolume("SFX", 0.5f);
    score = v.computeAudibility(listener, graph);
    assert(near(score, 0.05f));
    graph.setVolume("SFX", 1.0f);

    // Muted bus => score 0
    graph.setMute("SFX", true);
    score = v.computeAudibility(listener, graph);
    assert(near(score, 0.0f));
    graph.setMute("SFX", false);

    // 2D voice ignores distance
    v.is3D = false;
    v.position = glm::vec3(100, 0, 0);
    score = v.computeAudibility(listener, graph);
    assert(near(score, 1.0f));

    // Priority weight: higher priority boosts score
    v.is3D = true;
    v.position = glm::vec3(10, 0, 0);
    v.priority = 255; // max priority
    score = v.computeAudibility(listener, graph);
    float highPriScore = score;

    v.priority = 1; // low priority
    score = v.computeAudibility(listener, graph);
    assert(highPriScore > score);

    printf("test_voice_scoring: all assertions passed\n");
    return 0;
}
```

**Step 2: Write the implementation**

```cpp
// src/engine/audio/mix/Voice.h
#pragma once

#include "engine/audio/VoiceHandle.h"
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>
#include <string>
#include <algorithm>
#include <cmath>

namespace audio {

class BusGraph;

enum class VoiceState {
    Playing,
    Virtual,
    Stopping,
    Stopped
};

struct Voice {
    VoiceHandle handle;
    std::string eventName;
    glm::vec3 position{0.0f};
    float volume = 1.0f;
    float pitch = 1.0f;
    uint8_t priority = 128; // 0=lowest, 255=highest
    std::string busName = "SFX";
    bool is3D = true;
    bool looping = false;
    VoiceState state = VoiceState::Stopped;

    float refDistance = 1.0f;
    float maxDistance = 50.0f;
    float elapsedTime = 0.0f;

    // Occlusion factor set by OcclusionProcessor (0=clear, 1=blocked)
    float occlusion = 0.0f;

    // Backend source index (-1 = virtual, no source assigned)
    int sourceIndex = -1;

    // Buffer handle for the selected sound
    uint32_t bufferHandle = 0;

    float computeAudibility(const glm::vec3& listenerPos,
                            const BusGraph& graph) const;

    float computeDistanceAttenuation(const glm::vec3& listenerPos) const;
};

// Inline implementations

inline float Voice::computeDistanceAttenuation(
    const glm::vec3& listenerPos) const {
    if (!is3D) return 1.0f;
    float dist = glm::distance(position, listenerPos);
    dist = std::max(dist, refDistance);
    if (dist >= maxDistance) return 0.0f;
    // AL_INVERSE_DISTANCE_CLAMPED: gain = refDist / max(dist, refDist)
    return refDistance / dist;
}

inline float Voice::computeAudibility(const glm::vec3& listenerPos,
                                       const BusGraph& graph) const;
```

The `computeAudibility` implementation needs `BusGraph.h`, so put it in a small .cpp or keep it inline with a forward-include pattern. Since `Voice.h` already forward-declares `BusGraph`, provide a non-inline definition:

```cpp
// Add to src/engine/audio/mix/Voice.cpp (new file, ~15 lines)
#include "engine/audio/mix/Voice.h"
#include "engine/audio/mix/BusGraph.h"

namespace audio {

float Voice::computeAudibility(const glm::vec3& listenerPos,
                                const BusGraph& graph) const {
    float distAtten = computeDistanceAttenuation(listenerPos);
    float busVol = graph.effectiveVolume(busName);
    float priorityWeight = static_cast<float>(priority) / 128.0f;
    return volume * distAtten * busVol * priorityWeight;
}

} // namespace audio
```

Remove the trailing inline declaration from the header (replace with just the declaration):
```cpp
    float computeAudibility(const glm::vec3& listenerPos,
                            const BusGraph& graph) const;
```

**Step 3: Register test, add Voice.cpp to engine_audio sources**

Add `audio/mix/Voice.cpp` to engine_audio sources in `src/engine/CMakeLists.txt`.

Add to `tests/engine/CMakeLists.txt`:
```cmake
pixel_roguelike_add_test(test_voice_scoring
    SOURCES test_voice_scoring.cpp
    LIBRARIES engine_audio
    LABELS engine audio
)
```

**Step 4: Build and run**

```bash
cmake --build build --target test_voice_scoring && ctest --test-dir build -R test_voice_scoring -V
```
Expected: PASS

**Step 5: Commit**

```bash
git add src/engine/audio/mix/Voice.h src/engine/audio/mix/Voice.cpp tests/engine/test_voice_scoring.cpp tests/engine/CMakeLists.txt src/engine/CMakeLists.txt
git commit -m "Add Voice state struct with audibility scoring for voice management"
```

---

## Task 5: SoundEventDef + EventRegistry — Data-Driven Event Mapping

**Files:**
- Create: `src/engine/audio/events/SoundEventDef.h`
- Create: `src/engine/audio/events/EventRegistry.h`
- Create: `src/engine/audio/events/EventRegistry.cpp`
- Create: `tests/engine/test_fixtures/test_sound_events.json`
- Test: `tests/engine/test_event_registry.cpp`

**Step 1: Write test fixture JSON**

```json
{
    "door_open": {
        "sounds": ["sfx/doors/door_open_01.wav", "sfx/doors/door_open_02.wav"],
        "pick": "random_no_repeat",
        "pitch_range": [0.95, 1.05],
        "volume_range": [0.9, 1.0],
        "max_instances": 2,
        "cooldown": 0.1,
        "bus": "Doors",
        "spatial": "3D",
        "ref_distance": 1.0,
        "max_distance": 30.0
    },
    "ui_click": {
        "sounds": ["sfx/ui/click.wav"],
        "pick": "random",
        "pitch_range": [1.0, 1.0],
        "volume_range": [1.0, 1.0],
        "max_instances": 4,
        "cooldown": 0.0,
        "bus": "UI",
        "spatial": "2D",
        "ref_distance": 1.0,
        "max_distance": 50.0
    },
    "ambient_hum": {
        "sounds": ["ambience/electrical_hum.ogg"],
        "pick": "sequential",
        "pitch_range": [1.0, 1.0],
        "volume_range": [0.6, 0.6],
        "max_instances": 1,
        "cooldown": 0.0,
        "bus": "Environment",
        "spatial": "3D",
        "ref_distance": 2.0,
        "max_distance": 15.0
    }
}
```

**Step 2: Write the test**

```cpp
// tests/engine/test_event_registry.cpp
#include "engine/audio/events/EventRegistry.h"
#include <cassert>
#include <cmath>
#include <cstdio>

#ifndef TEST_SOUND_EVENTS_PATH
#error "TEST_SOUND_EVENTS_PATH must be defined"
#endif

static bool near(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

int main() {
    using namespace audio;

    EventRegistry registry;
    bool loaded = registry.loadFromFile(TEST_SOUND_EVENTS_PATH);
    assert(loaded);

    // door_open event exists
    auto* doorOpen = registry.find("door_open");
    assert(doorOpen != nullptr);
    assert(doorOpen->sounds.size() == 2);
    assert(doorOpen->sounds[0] == "sfx/doors/door_open_01.wav");
    assert(doorOpen->pickMode == PickMode::RandomNoRepeat);
    assert(near(doorOpen->pitchRange.x, 0.95f));
    assert(near(doorOpen->pitchRange.y, 1.05f));
    assert(doorOpen->maxInstances == 2);
    assert(doorOpen->busName == "Doors");
    assert(doorOpen->is3D == true);
    assert(near(doorOpen->refDistance, 1.0f));
    assert(near(doorOpen->maxDistance, 30.0f));

    // ui_click is 2D
    auto* click = registry.find("ui_click");
    assert(click != nullptr);
    assert(click->is3D == false);
    assert(click->busName == "UI");
    assert(click->maxInstances == 4);

    // ambient_hum
    auto* hum = registry.find("ambient_hum");
    assert(hum != nullptr);
    assert(hum->pickMode == PickMode::Sequential);
    assert(near(hum->refDistance, 2.0f));

    // Non-existent event returns nullptr
    assert(registry.find("nonexistent") == nullptr);

    // Sound picking: random returns valid index
    int idx = doorOpen->pickSound();
    assert(idx >= 0 && idx < 2);

    // Sequential increments
    int first = hum->pickSound();
    assert(first == 0);
    // Only one sound, wraps
    int second = hum->pickSound();
    assert(second == 0);

    // All event names
    auto names = registry.eventNames();
    assert(names.size() == 3);

    printf("test_event_registry: all assertions passed\n");
    return 0;
}
```

**Step 3: Write the implementation**

```cpp
// src/engine/audio/events/SoundEventDef.h
#pragma once

#include <glm/vec2.hpp>
#include <string>
#include <vector>
#include <cstdlib>

namespace audio {

enum class PickMode {
    Random,
    RandomNoRepeat,
    RoundRobin,
    Sequential
};

struct SoundEventDef {
    std::string name;
    std::vector<std::string> sounds;
    PickMode pickMode = PickMode::Random;
    glm::vec2 pitchRange{1.0f, 1.0f};
    glm::vec2 volumeRange{1.0f, 1.0f};
    int maxInstances = 8;
    float cooldown = 0.0f;
    std::string busName = "SFX";
    bool is3D = true;
    float refDistance = 1.0f;
    float maxDistance = 50.0f;

    // Mutable pick state
    mutable int lastPickIndex = -1;
    mutable int sequentialIndex = 0;

    int pickSound() const {
        if (sounds.empty()) return -1;
        int n = static_cast<int>(sounds.size());
        switch (pickMode) {
            case PickMode::Random:
                return std::rand() % n;
            case PickMode::RandomNoRepeat: {
                if (n == 1) return 0;
                int idx;
                do { idx = std::rand() % n; } while (idx == lastPickIndex);
                lastPickIndex = idx;
                return idx;
            }
            case PickMode::RoundRobin:
            case PickMode::Sequential: {
                int idx = sequentialIndex;
                sequentialIndex = (sequentialIndex + 1) % n;
                return idx;
            }
        }
        return 0;
    }

    float randomPitch() const {
        float t = static_cast<float>(std::rand()) / RAND_MAX;
        return pitchRange.x + t * (pitchRange.y - pitchRange.x);
    }

    float randomVolume() const {
        float t = static_cast<float>(std::rand()) / RAND_MAX;
        return volumeRange.x + t * (volumeRange.y - volumeRange.x);
    }
};

} // namespace audio
```

```cpp
// src/engine/audio/events/EventRegistry.h
#pragma once

#include "engine/audio/events/SoundEventDef.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace audio {

class EventRegistry {
public:
    bool loadFromFile(const std::string& path);

    const SoundEventDef* find(const std::string& eventName) const;
    SoundEventDef* find(const std::string& eventName);

    std::vector<std::string> eventNames() const;

    // Returns all unique sound file paths (for preloading)
    std::vector<std::string> allSoundPaths() const;

private:
    std::unordered_map<std::string, SoundEventDef> events_;
};

} // namespace audio
```

```cpp
// src/engine/audio/events/EventRegistry.cpp
#include "engine/audio/events/EventRegistry.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace audio {

static PickMode parsePickMode(const std::string& s) {
    if (s == "random") return PickMode::Random;
    if (s == "random_no_repeat") return PickMode::RandomNoRepeat;
    if (s == "round_robin") return PickMode::RoundRobin;
    if (s == "sequential") return PickMode::Sequential;
    return PickMode::Random;
}

bool EventRegistry::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    nlohmann::json root;
    try {
        file >> root;
    } catch (...) {
        return false;
    }

    for (auto& [name, obj] : root.items()) {
        SoundEventDef def;
        def.name = name;

        for (auto& s : obj["sounds"]) {
            def.sounds.push_back(s.get<std::string>());
        }

        if (obj.contains("pick")) {
            def.pickMode = parsePickMode(obj["pick"].get<std::string>());
        }
        if (obj.contains("pitch_range")) {
            def.pitchRange.x = obj["pitch_range"][0].get<float>();
            def.pitchRange.y = obj["pitch_range"][1].get<float>();
        }
        if (obj.contains("volume_range")) {
            def.volumeRange.x = obj["volume_range"][0].get<float>();
            def.volumeRange.y = obj["volume_range"][1].get<float>();
        }
        if (obj.contains("max_instances")) {
            def.maxInstances = obj["max_instances"].get<int>();
        }
        if (obj.contains("cooldown")) {
            def.cooldown = obj["cooldown"].get<float>();
        }
        if (obj.contains("bus")) {
            def.busName = obj["bus"].get<std::string>();
        }
        if (obj.contains("spatial")) {
            def.is3D = (obj["spatial"].get<std::string>() == "3D");
        }
        if (obj.contains("ref_distance")) {
            def.refDistance = obj["ref_distance"].get<float>();
        }
        if (obj.contains("max_distance")) {
            def.maxDistance = obj["max_distance"].get<float>();
        }

        events_[name] = std::move(def);
    }

    return true;
}

const SoundEventDef* EventRegistry::find(const std::string& eventName) const {
    auto it = events_.find(eventName);
    return (it != events_.end()) ? &it->second : nullptr;
}

SoundEventDef* EventRegistry::find(const std::string& eventName) {
    auto it = events_.find(eventName);
    return (it != events_.end()) ? &it->second : nullptr;
}

std::vector<std::string> EventRegistry::eventNames() const {
    std::vector<std::string> names;
    names.reserve(events_.size());
    for (auto& [name, _] : events_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> EventRegistry::allSoundPaths() const {
    std::vector<std::string> paths;
    for (auto& [_, def] : events_) {
        for (auto& s : def.sounds) {
            paths.push_back(s);
        }
    }
    return paths;
}

} // namespace audio
```

**Step 4: Register test**

Add to `tests/engine/CMakeLists.txt`:
```cmake
set(TEST_SOUND_EVENTS_FILE "${CMAKE_SOURCE_DIR}/tests/engine/test_fixtures/test_sound_events.json")

pixel_roguelike_add_test(test_event_registry
    SOURCES test_event_registry.cpp
    LIBRARIES engine_audio nlohmann_json::nlohmann_json
    DEFINITIONS TEST_SOUND_EVENTS_PATH="${TEST_SOUND_EVENTS_FILE}"
    LABELS engine audio
)
```

Add `audio/events/EventRegistry.cpp` to engine_audio sources in `src/engine/CMakeLists.txt`. Add `nlohmann_json::nlohmann_json` to engine_audio's PRIVATE link libraries.

**Step 5: Build and run**

```bash
cmake --build build --target test_event_registry && ctest --test-dir build -R test_event_registry -V
```
Expected: PASS

**Step 6: Commit**

```bash
git add src/engine/audio/events/ tests/engine/test_event_registry.cpp tests/engine/test_fixtures/test_sound_events.json tests/engine/CMakeLists.txt src/engine/CMakeLists.txt
git commit -m "Add SoundEventDef and EventRegistry with JSON loading and sound picking"
```

---

## Task 6: ALDevice — RAII OpenAL Device and Context

**Files:**
- Create: `src/engine/audio/backend/ALDevice.h`
- Create: `src/engine/audio/backend/ALDevice.cpp`

This is an OpenAL wrapper — tested via integration in later tasks rather than standalone (requires audio hardware).

**Step 1: Write the implementation**

```cpp
// src/engine/audio/backend/ALDevice.h
#pragma once

#include <string>

// Forward declarations for OpenAL types
struct ALCdevice;
struct ALCcontext;

namespace audio {

struct DeviceCapabilities {
    int maxSources = 0;
    bool hasEFX = false;
    int maxAuxSends = 0;
    bool hasHRTF = false;
};

class ALDevice {
public:
    ALDevice();
    ~ALDevice();

    ALDevice(const ALDevice&) = delete;
    ALDevice& operator=(const ALDevice&) = delete;
    ALDevice(ALDevice&& other) noexcept;
    ALDevice& operator=(ALDevice&& other) noexcept;

    bool open();
    void close();
    bool isOpen() const;

    const DeviceCapabilities& capabilities() const { return caps_; }

private:
    void queryCapabilities();

    ALCdevice* device_ = nullptr;
    ALCcontext* context_ = nullptr;
    DeviceCapabilities caps_;
};

} // namespace audio
```

```cpp
// src/engine/audio/backend/ALDevice.cpp
#include "engine/audio/backend/ALDevice.h"

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <spdlog/spdlog.h>

namespace audio {

ALDevice::ALDevice() = default;

ALDevice::~ALDevice() {
    close();
}

ALDevice::ALDevice(ALDevice&& other) noexcept
    : device_(other.device_), context_(other.context_), caps_(other.caps_) {
    other.device_ = nullptr;
    other.context_ = nullptr;
}

ALDevice& ALDevice::operator=(ALDevice&& other) noexcept {
    if (this != &other) {
        close();
        device_ = other.device_;
        context_ = other.context_;
        caps_ = other.caps_;
        other.device_ = nullptr;
        other.context_ = nullptr;
    }
    return *this;
}

bool ALDevice::open() {
    device_ = alcOpenDevice(nullptr);
    if (!device_) {
        spdlog::error("ALDevice: failed to open default audio device");
        return false;
    }

    context_ = alcCreateContext(device_, nullptr);
    if (!context_) {
        spdlog::error("ALDevice: failed to create OpenAL context");
        alcCloseDevice(device_);
        device_ = nullptr;
        return false;
    }

    alcMakeContextCurrent(context_);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

    queryCapabilities();

    spdlog::info("ALDevice: opened '{}' (maxSources={}, EFX={}, auxSends={})",
                 alcGetString(device_, ALC_DEVICE_SPECIFIER),
                 caps_.maxSources, caps_.hasEFX, caps_.maxAuxSends);
    return true;
}

void ALDevice::close() {
    if (context_) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context_);
        context_ = nullptr;
    }
    if (device_) {
        alcCloseDevice(device_);
        device_ = nullptr;
    }
}

bool ALDevice::isOpen() const {
    return device_ != nullptr && context_ != nullptr;
}

void ALDevice::queryCapabilities() {
    // Query mono source limit as proxy for max sources
    ALCint monoSources = 0;
    alcGetIntegerv(device_, ALC_MONO_SOURCES, 1, &monoSources);
    caps_.maxSources = monoSources;

    // EFX extension
    caps_.hasEFX = (alcIsExtensionPresent(device_, "ALC_EXT_EFX") == ALC_TRUE);
    if (caps_.hasEFX) {
        ALCint maxSends = 0;
        alcGetIntegerv(device_, ALC_MAX_AUXILIARY_SENDS, 1, &maxSends);
        caps_.maxAuxSends = maxSends;
    }

    // HRTF
    caps_.hasHRTF = (alcIsExtensionPresent(device_, "ALC_SOFT_HRTF") == ALC_TRUE);
}

} // namespace audio
```

**Step 2: Add to engine_audio sources in `src/engine/CMakeLists.txt`**

Add `audio/backend/ALDevice.cpp` to the engine_audio source list.

**Step 3: Build (compile check)**

```bash
cmake --build build --target engine_audio
```
Expected: compiles without error

**Step 4: Commit**

```bash
git add src/engine/audio/backend/ALDevice.h src/engine/audio/backend/ALDevice.cpp src/engine/CMakeLists.txt
git commit -m "Add ALDevice RAII wrapper for OpenAL device and context"
```

---

## Task 7: ALSourcePool — Fixed Source Pool

**Files:**
- Create: `src/engine/audio/backend/ALSourcePool.h`
- Create: `src/engine/audio/backend/ALSourcePool.cpp`

**Step 1: Write the implementation**

```cpp
// src/engine/audio/backend/ALSourcePool.h
#pragma once

#include <AL/al.h>
#include <array>
#include <cstdint>

namespace audio {

class ALSourcePool {
public:
    static constexpr int kDefaultPoolSize = 32;

    explicit ALSourcePool(int poolSize = kDefaultPoolSize);
    ~ALSourcePool();

    ALSourcePool(const ALSourcePool&) = delete;
    ALSourcePool& operator=(const ALSourcePool&) = delete;
    ALSourcePool(ALSourcePool&& other) noexcept;
    ALSourcePool& operator=(ALSourcePool&& other) noexcept;

    bool init();
    void shutdown();

    // Claim/release a source by pool index
    ALuint source(int index) const;
    int poolSize() const { return poolSize_; }

    // Stop and reset a source to default state
    void resetSource(int index);

private:
    int poolSize_;
    ALuint* sources_ = nullptr;
    bool initialized_ = false;
};

} // namespace audio
```

```cpp
// src/engine/audio/backend/ALSourcePool.cpp
#include "engine/audio/backend/ALSourcePool.h"
#include <spdlog/spdlog.h>
#include <cstring>

namespace audio {

ALSourcePool::ALSourcePool(int poolSize) : poolSize_(poolSize) {
    sources_ = new ALuint[poolSize_]();
}

ALSourcePool::~ALSourcePool() {
    shutdown();
    delete[] sources_;
}

ALSourcePool::ALSourcePool(ALSourcePool&& other) noexcept
    : poolSize_(other.poolSize_), sources_(other.sources_),
      initialized_(other.initialized_) {
    other.sources_ = nullptr;
    other.initialized_ = false;
}

ALSourcePool& ALSourcePool::operator=(ALSourcePool&& other) noexcept {
    if (this != &other) {
        shutdown();
        delete[] sources_;
        poolSize_ = other.poolSize_;
        sources_ = other.sources_;
        initialized_ = other.initialized_;
        other.sources_ = nullptr;
        other.initialized_ = false;
    }
    return *this;
}

bool ALSourcePool::init() {
    alGenSources(poolSize_, sources_);
    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::error("ALSourcePool: failed to generate {} sources (err=0x{:x})",
                      poolSize_, err);
        return false;
    }
    initialized_ = true;
    spdlog::info("ALSourcePool: allocated {} sources", poolSize_);
    return true;
}

void ALSourcePool::shutdown() {
    if (initialized_ && sources_) {
        for (int i = 0; i < poolSize_; ++i) {
            alSourceStop(sources_[i]);
        }
        alDeleteSources(poolSize_, sources_);
        initialized_ = false;
    }
}

ALuint ALSourcePool::source(int index) const {
    if (index < 0 || index >= poolSize_ || !initialized_) return 0;
    return sources_[index];
}

void ALSourcePool::resetSource(int index) {
    if (index < 0 || index >= poolSize_ || !initialized_) return;
    ALuint src = sources_[index];
    alSourceStop(src);
    alSourcei(src, AL_BUFFER, 0);
    alSourcef(src, AL_GAIN, 1.0f);
    alSourcef(src, AL_PITCH, 1.0f);
    alSource3f(src, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(src, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alSourcei(src, AL_LOOPING, AL_FALSE);
    alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
    alSourcei(src, AL_DIRECT_FILTER, AL_FILTER_NULL);
    alSource3i(src, AL_AUXILIARY_SEND_FILTER, AL_EFFECTSLOT_NULL, 0, AL_FILTER_NULL);
}

} // namespace audio
```

**Step 2: Add to engine_audio in CMakeLists, build**

```bash
cmake --build build --target engine_audio
```

**Step 3: Commit**

```bash
git add src/engine/audio/backend/ALSourcePool.h src/engine/audio/backend/ALSourcePool.cpp src/engine/CMakeLists.txt
git commit -m "Add ALSourcePool fixed-size OpenAL source pool with RAII"
```

---

## Task 8: ALBufferCache — WAV Loading and Caching

**Files:**
- Create: `src/engine/audio/backend/ALBufferCache.h`
- Create: `src/engine/audio/backend/ALBufferCache.cpp`

Reuses the WAV parsing logic from the existing AudioSystem.cpp (lines 333-394).

**Step 1: Write the implementation**

```cpp
// src/engine/audio/backend/ALBufferCache.h
#pragma once

#include <AL/al.h>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace audio {

class ALBufferCache {
public:
    ALBufferCache() = default;
    ~ALBufferCache();

    ALBufferCache(const ALBufferCache&) = delete;
    ALBufferCache& operator=(const ALBufferCache&) = delete;

    // Load a WAV file, cache it, return AL buffer handle. Returns 0 on failure.
    ALuint getOrLoad(const std::string& path);

    // Preload a list of paths (for SFX at startup)
    void preload(const std::vector<std::string>& paths,
                 const std::string& baseDir);

    bool contains(const std::string& path) const;
    void clear();

private:
    ALuint loadWav(const std::string& path);

    std::unordered_map<std::string, ALuint> cache_;
};

} // namespace audio
```

```cpp
// src/engine/audio/backend/ALBufferCache.cpp
#include "engine/audio/backend/ALBufferCache.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <vector>
#include <cstring>

namespace audio {

ALBufferCache::~ALBufferCache() {
    clear();
}

void ALBufferCache::clear() {
    for (auto& [path, buf] : cache_) {
        alDeleteBuffers(1, &buf);
    }
    cache_.clear();
}

ALuint ALBufferCache::getOrLoad(const std::string& path) {
    auto it = cache_.find(path);
    if (it != cache_.end()) return it->second;

    ALuint buf = loadWav(path);
    if (buf != 0) {
        cache_[path] = buf;
    }
    return buf;
}

void ALBufferCache::preload(const std::vector<std::string>& paths,
                             const std::string& baseDir) {
    for (auto& p : paths) {
        std::string fullPath = baseDir.empty() ? p : (baseDir + "/" + p);
        getOrLoad(fullPath);
    }
}

bool ALBufferCache::contains(const std::string& path) const {
    return cache_.find(path) != cache_.end();
}

static ALenum wavFormat(int channels, int bitsPerSample) {
    if (channels == 1 && bitsPerSample == 8) return AL_FORMAT_MONO8;
    if (channels == 1 && bitsPerSample == 16) return AL_FORMAT_MONO16;
    if (channels == 2 && bitsPerSample == 8) return AL_FORMAT_STEREO8;
    if (channels == 2 && bitsPerSample == 16) return AL_FORMAT_STEREO16;
    return 0;
}

ALuint ALBufferCache::loadWav(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        spdlog::warn("ALBufferCache: cannot open '{}'", path);
        return 0;
    }

    // RIFF header
    char riff[4];
    file.read(riff, 4);
    if (std::memcmp(riff, "RIFF", 4) != 0) {
        spdlog::warn("ALBufferCache: '{}' not a RIFF file", path);
        return 0;
    }

    uint32_t fileSize;
    file.read(reinterpret_cast<char*>(&fileSize), 4);

    char wave[4];
    file.read(wave, 4);
    if (std::memcmp(wave, "WAVE", 4) != 0) {
        spdlog::warn("ALBufferCache: '{}' not a WAVE file", path);
        return 0;
    }

    // Find fmt and data chunks
    int channels = 0, sampleRate = 0, bitsPerSample = 0;
    std::vector<char> audioData;

    while (file.good()) {
        char chunkId[4];
        uint32_t chunkSize;
        file.read(chunkId, 4);
        file.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (!file.good()) break;

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat;
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            if (audioFormat != 1) { // PCM only
                spdlog::warn("ALBufferCache: '{}' is not PCM (format={})",
                             path, audioFormat);
                return 0;
            }
            uint16_t ch, bps;
            uint32_t sr, byteRate;
            uint16_t blockAlign;
            file.read(reinterpret_cast<char*>(&ch), 2);
            file.read(reinterpret_cast<char*>(&sr), 4);
            file.read(reinterpret_cast<char*>(&byteRate), 4);
            file.read(reinterpret_cast<char*>(&blockAlign), 2);
            file.read(reinterpret_cast<char*>(&bps), 2);
            channels = ch;
            sampleRate = sr;
            bitsPerSample = bps;
            // Skip any extra fmt bytes
            if (chunkSize > 16) {
                file.seekg(chunkSize - 16, std::ios::cur);
            }
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            audioData.resize(chunkSize);
            file.read(audioData.data(), chunkSize);
        } else {
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (audioData.empty() || channels == 0) {
        spdlog::warn("ALBufferCache: '{}' has no audio data", path);
        return 0;
    }

    ALenum format = wavFormat(channels, bitsPerSample);
    if (format == 0) {
        spdlog::warn("ALBufferCache: '{}' unsupported format (ch={}, bps={})",
                     path, channels, bitsPerSample);
        return 0;
    }

    ALuint buffer;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, audioData.data(),
                 static_cast<ALsizei>(audioData.size()), sampleRate);

    ALenum err = alGetError();
    if (err != AL_NO_ERROR) {
        spdlog::warn("ALBufferCache: alBufferData failed for '{}' (err=0x{:x})",
                     path, err);
        alDeleteBuffers(1, &buffer);
        return 0;
    }

    spdlog::debug("ALBufferCache: loaded '{}' ({}Hz, {}ch, {}bit, {} bytes)",
                  path, sampleRate, channels, bitsPerSample, audioData.size());
    return buffer;
}

} // namespace audio
```

**Step 2: Add to engine_audio, build**

Add `audio/backend/ALBufferCache.cpp` to engine_audio sources.

```bash
cmake --build build --target engine_audio
```

**Step 3: Commit**

```bash
git add src/engine/audio/backend/ALBufferCache.h src/engine/audio/backend/ALBufferCache.cpp src/engine/CMakeLists.txt
git commit -m "Add ALBufferCache WAV loading with RIFF parsing and caching"
```

---

## Task 9: ALStreamPlayer — OGG Vorbis Double-Buffer Streaming

**Files:**
- Create: `src/engine/audio/backend/ALStreamPlayer.h`
- Create: `src/engine/audio/backend/ALStreamPlayer.cpp`

Adapts streaming logic from existing AudioSystem.cpp (StreamState, fillVorbisBuffer, refillStream, startStream, destroyStream).

**Step 1: Write the implementation**

```cpp
// src/engine/audio/backend/ALStreamPlayer.h
#pragma once

#include <AL/al.h>
#include <string>
#include <array>

struct stb_vorbis;

namespace audio {

class ALStreamPlayer {
public:
    static constexpr int kBufferCount = 4;
    static constexpr int kPcmSamples = 65536;

    ALStreamPlayer();
    ~ALStreamPlayer();

    ALStreamPlayer(const ALStreamPlayer&) = delete;
    ALStreamPlayer& operator=(const ALStreamPlayer&) = delete;
    ALStreamPlayer(ALStreamPlayer&& other) noexcept;
    ALStreamPlayer& operator=(ALStreamPlayer&& other) noexcept;

    bool play(const std::string& path, bool loop = true);
    void stop();
    void pause();
    void resume();

    // Call each frame to refill processed buffers
    void update();

    void setGain(float gain);
    float gain() const { return gain_; }

    bool isPlaying() const;
    bool isActive() const { return active_; }

    const std::string& currentPath() const { return path_; }

private:
    bool fillBuffer(ALuint buffer);
    void cleanup();

    ALuint source_ = 0;
    std::array<ALuint, kBufferCount> buffers_{};
    stb_vorbis* vorbis_ = nullptr;
    int channels_ = 0;
    int sampleRate_ = 0;
    bool looping_ = false;
    bool active_ = false;
    float gain_ = 1.0f;
    std::string path_;
    bool ownsSource_ = false;
};

} // namespace audio
```

```cpp
// src/engine/audio/backend/ALStreamPlayer.cpp
#include "engine/audio/backend/ALStreamPlayer.h"
#include <spdlog/spdlog.h>
#include <stb_vorbis.c> // included via external/
// Note: stb_vorbis is compiled separately as C in CMakeLists.
// This file uses the stb_vorbis API through the header declarations.

// Actually, we need to include just the header declarations.
// stb_vorbis.c is compiled as its own translation unit.
// We include stb_vorbis.h or use extern declarations.

// stb_vorbis doesn't have a separate .h — it's all in stb_vorbis.c
// The existing project compiles stb_vorbis.c separately and the
// AudioSystem.cpp includes the header portion. We do the same.
```

Actually, let me revise — the existing project handles stb_vorbis by compiling `external/stb_vorbis.c` as a separate C translation unit. The header declarations are obtained by including the file with a guard. Let me check the existing pattern and match it.

```cpp
// src/engine/audio/backend/ALStreamPlayer.cpp
#include "engine/audio/backend/ALStreamPlayer.h"

// stb_vorbis: declarations only (implementation compiled separately in CMake)
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#include <spdlog/spdlog.h>
#include <vector>

namespace audio {

ALStreamPlayer::ALStreamPlayer() = default;

ALStreamPlayer::~ALStreamPlayer() {
    cleanup();
}

ALStreamPlayer::ALStreamPlayer(ALStreamPlayer&& other) noexcept
    : source_(other.source_), buffers_(other.buffers_),
      vorbis_(other.vorbis_), channels_(other.channels_),
      sampleRate_(other.sampleRate_), looping_(other.looping_),
      active_(other.active_), gain_(other.gain_),
      path_(std::move(other.path_)), ownsSource_(other.ownsSource_) {
    other.source_ = 0;
    other.vorbis_ = nullptr;
    other.active_ = false;
    other.ownsSource_ = false;
}

ALStreamPlayer& ALStreamPlayer::operator=(ALStreamPlayer&& other) noexcept {
    if (this != &other) {
        cleanup();
        source_ = other.source_;
        buffers_ = other.buffers_;
        vorbis_ = other.vorbis_;
        channels_ = other.channels_;
        sampleRate_ = other.sampleRate_;
        looping_ = other.looping_;
        active_ = other.active_;
        gain_ = other.gain_;
        path_ = std::move(other.path_);
        ownsSource_ = other.ownsSource_;
        other.source_ = 0;
        other.vorbis_ = nullptr;
        other.active_ = false;
        other.ownsSource_ = false;
    }
    return *this;
}

void ALStreamPlayer::cleanup() {
    if (active_) {
        alSourceStop(source_);
        ALint queued = 0;
        alGetSourcei(source_, AL_BUFFERS_QUEUED, &queued);
        while (queued-- > 0) {
            ALuint buf;
            alSourceUnqueueBuffers(source_, 1, &buf);
        }
    }
    if (vorbis_) {
        stb_vorbis_close(vorbis_);
        vorbis_ = nullptr;
    }
    if (ownsSource_ && source_) {
        alDeleteSources(1, &source_);
    }
    if (buffers_[0]) {
        alDeleteBuffers(kBufferCount, buffers_.data());
        buffers_.fill(0);
    }
    source_ = 0;
    active_ = false;
}

bool ALStreamPlayer::fillBuffer(ALuint buffer) {
    std::vector<short> pcm(kPcmSamples);
    int samplesRead = stb_vorbis_get_samples_short_interleaved(
        vorbis_, channels_, pcm.data(), kPcmSamples);

    if (samplesRead == 0) {
        if (looping_) {
            stb_vorbis_seek_start(vorbis_);
            samplesRead = stb_vorbis_get_samples_short_interleaved(
                vorbis_, channels_, pcm.data(), kPcmSamples);
        }
        if (samplesRead == 0) return false;
    }

    ALenum format = (channels_ == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    int byteCount = samplesRead * channels_ * sizeof(short);
    alBufferData(buffer, format, pcm.data(), byteCount, sampleRate_);
    return true;
}

bool ALStreamPlayer::play(const std::string& path, bool loop) {
    cleanup();

    int error = 0;
    vorbis_ = stb_vorbis_open_filename(path.c_str(), &error, nullptr);
    if (!vorbis_) {
        spdlog::warn("ALStreamPlayer: failed to open '{}' (err={})", path, error);
        return false;
    }

    stb_vorbis_info info = stb_vorbis_get_info(vorbis_);
    channels_ = info.channels;
    sampleRate_ = info.sample_rate;
    looping_ = loop;
    path_ = path;

    alGenSources(1, &source_);
    ownsSource_ = true;
    alGenBuffers(kBufferCount, buffers_.data());

    alSourcef(source_, AL_GAIN, gain_);
    alSourcei(source_, AL_SOURCE_RELATIVE, AL_TRUE);

    // Prime all buffers
    for (int i = 0; i < kBufferCount; ++i) {
        if (!fillBuffer(buffers_[i])) break;
        alSourceQueueBuffers(source_, 1, &buffers_[i]);
    }

    alSourcePlay(source_);
    active_ = true;

    spdlog::debug("ALStreamPlayer: playing '{}' ({}Hz, {}ch, loop={})",
                  path, sampleRate_, channels_, loop);
    return true;
}

void ALStreamPlayer::stop() {
    cleanup();
}

void ALStreamPlayer::pause() {
    if (active_ && source_) {
        alSourcePause(source_);
    }
}

void ALStreamPlayer::resume() {
    if (active_ && source_) {
        alSourcePlay(source_);
    }
}

void ALStreamPlayer::update() {
    if (!active_ || !source_) return;

    ALint processed = 0;
    alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed);

    while (processed-- > 0) {
        ALuint buf;
        alSourceUnqueueBuffers(source_, 1, &buf);
        if (!fillBuffer(buf)) {
            active_ = false;
            return;
        }
        alSourceQueueBuffers(source_, 1, &buf);
    }

    // Recover from underrun
    ALint state;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);
    if (state == AL_STOPPED && active_) {
        alSourcePlay(source_);
    }
}

void ALStreamPlayer::setGain(float gain) {
    gain_ = gain;
    if (source_) {
        alSourcef(source_, AL_GAIN, gain_);
    }
}

bool ALStreamPlayer::isPlaying() const {
    if (!active_ || !source_) return false;
    ALint state;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

} // namespace audio
```

**Step 2: Add to engine_audio, build**

Add `audio/backend/ALStreamPlayer.cpp` to engine_audio sources.

```bash
cmake --build build --target engine_audio
```

**Step 3: Commit**

```bash
git add src/engine/audio/backend/ALStreamPlayer.h src/engine/audio/backend/ALStreamPlayer.cpp src/engine/CMakeLists.txt
git commit -m "Add ALStreamPlayer OGG Vorbis streaming with double-buffer ring"
```

---

## Task 10: ALEffectSlots — EFX Reverb

**Files:**
- Create: `src/engine/audio/backend/ALEffectSlots.h`
- Create: `src/engine/audio/backend/ALEffectSlots.cpp`

**Step 1: Write the implementation**

```cpp
// src/engine/audio/backend/ALEffectSlots.h
#pragma once

#include <AL/al.h>
#include <AL/efx.h>
#include <vector>
#include <string>

namespace audio {

struct ReverbParams {
    float density = 1.0f;
    float diffusion = 1.0f;
    float gain = 0.32f;
    float gainHF = 0.89f;
    float decayTime = 1.49f;
    float decayHFRatio = 0.83f;
    float reflectionsGain = 0.05f;
    float reflectionsDelay = 0.007f;
    float lateReverbGain = 1.26f;
    float lateReverbDelay = 0.011f;
    float airAbsorptionGainHF = 0.994f;
    float roomRolloffFactor = 0.0f;
};

class ALEffectSlots {
public:
    ALEffectSlots() = default;
    ~ALEffectSlots();

    ALEffectSlots(const ALEffectSlots&) = delete;
    ALEffectSlots& operator=(const ALEffectSlots&) = delete;

    // Initialize with given number of slots. Returns actual count allocated.
    int init(int requestedSlots);
    void shutdown();

    // Set reverb parameters on a slot
    void setReverb(int slotIndex, const ReverbParams& params);

    // Get the ALuint effect slot handle for connecting sources
    ALuint slotHandle(int slotIndex) const;

    int slotCount() const { return static_cast<int>(slots_.size()); }

    // Create a direct low-pass filter for occlusion
    ALuint createFilter();
    void setFilterParams(ALuint filter, float gain, float gainHF);
    void destroyFilter(ALuint filter);

private:
    struct Slot {
        ALuint effectSlot = 0;
        ALuint effect = 0;
    };
    std::vector<Slot> slots_;
};

} // namespace audio
```

```cpp
// src/engine/audio/backend/ALEffectSlots.cpp
#include "engine/audio/backend/ALEffectSlots.h"
#include <spdlog/spdlog.h>

namespace audio {

ALEffectSlots::~ALEffectSlots() {
    shutdown();
}

int ALEffectSlots::init(int requestedSlots) {
    for (int i = 0; i < requestedSlots; ++i) {
        Slot s;
        alGenAuxiliaryEffectSlots(1, &s.effectSlot);
        if (alGetError() != AL_NO_ERROR) break;

        alGenEffects(1, &s.effect);
        if (alGetError() != AL_NO_ERROR) {
            alDeleteAuxiliaryEffectSlots(1, &s.effectSlot);
            break;
        }

        alEffecti(s.effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
        if (alGetError() != AL_NO_ERROR) {
            // Fallback to standard reverb
            alEffecti(s.effect, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
            if (alGetError() != AL_NO_ERROR) {
                alDeleteEffects(1, &s.effect);
                alDeleteAuxiliaryEffectSlots(1, &s.effectSlot);
                break;
            }
        }

        alAuxiliaryEffectSloti(s.effectSlot, AL_EFFECTSLOT_EFFECT, s.effect);
        slots_.push_back(s);
    }

    spdlog::info("ALEffectSlots: allocated {} of {} requested slots",
                 slots_.size(), requestedSlots);
    return static_cast<int>(slots_.size());
}

void ALEffectSlots::shutdown() {
    for (auto& s : slots_) {
        alAuxiliaryEffectSloti(s.effectSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
        alDeleteEffects(1, &s.effect);
        alDeleteAuxiliaryEffectSlots(1, &s.effectSlot);
    }
    slots_.clear();
}

void ALEffectSlots::setReverb(int slotIndex, const ReverbParams& params) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size())) return;
    ALuint effect = slots_[slotIndex].effect;

    alEffectf(effect, AL_EAXREVERB_DENSITY, params.density);
    alEffectf(effect, AL_EAXREVERB_DIFFUSION, params.diffusion);
    alEffectf(effect, AL_EAXREVERB_GAIN, params.gain);
    alEffectf(effect, AL_EAXREVERB_GAINHF, params.gainHF);
    alEffectf(effect, AL_EAXREVERB_DECAY_TIME, params.decayTime);
    alEffectf(effect, AL_EAXREVERB_DECAY_HFRATIO, params.decayHFRatio);
    alEffectf(effect, AL_EAXREVERB_REFLECTIONS_GAIN, params.reflectionsGain);
    alEffectf(effect, AL_EAXREVERB_REFLECTIONS_DELAY, params.reflectionsDelay);
    alEffectf(effect, AL_EAXREVERB_LATE_REVERB_GAIN, params.lateReverbGain);
    alEffectf(effect, AL_EAXREVERB_LATE_REVERB_DELAY, params.lateReverbDelay);
    alEffectf(effect, AL_EAXREVERB_AIR_ABSORPTION_GAINHF,
              params.airAbsorptionGainHF);
    alEffectf(effect, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, params.roomRolloffFactor);

    // Commit to slot
    alAuxiliaryEffectSloti(slots_[slotIndex].effectSlot,
                           AL_EFFECTSLOT_EFFECT, effect);
}

ALuint ALEffectSlots::slotHandle(int slotIndex) const {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size())) return 0;
    return slots_[slotIndex].effectSlot;
}

ALuint ALEffectSlots::createFilter() {
    ALuint filter;
    alGenFilters(1, &filter);
    alFilteri(filter, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
    return filter;
}

void ALEffectSlots::setFilterParams(ALuint filter, float gain, float gainHF) {
    alFilterf(filter, AL_LOWPASS_GAIN, gain);
    alFilterf(filter, AL_LOWPASS_GAINHF, gainHF);
}

void ALEffectSlots::destroyFilter(ALuint filter) {
    alDeleteFilters(1, &filter);
}

} // namespace audio
```

**Step 2: Add to engine_audio, build**

Add `audio/backend/ALEffectSlots.cpp` to engine_audio sources.

```bash
cmake --build build --target engine_audio
```

**Step 3: Commit**

```bash
git add src/engine/audio/backend/ALEffectSlots.h src/engine/audio/backend/ALEffectSlots.cpp src/engine/CMakeLists.txt
git commit -m "Add ALEffectSlots EFX reverb and low-pass filter management"
```

---

## Task 11: VoiceManager — Voice Allocation, Scoring, Virtualization

**Files:**
- Create: `src/engine/audio/mix/VoiceManager.h`
- Create: `src/engine/audio/mix/VoiceManager.cpp`
- Test: `tests/engine/test_voice_manager.cpp`

**Step 1: Write the test**

```cpp
// tests/engine/test_voice_manager.cpp
#include "engine/audio/mix/VoiceManager.h"
#include "engine/audio/mix/BusGraph.h"
#include <cassert>
#include <cstdio>

int main() {
    using namespace audio;

    BusGraph graph;
    VoiceManager mgr(4); // small pool for testing

    glm::vec3 listener(0, 0, 0);

    // Spawn a voice
    VoiceHandle h1 = mgr.spawn("test_sound", glm::vec3(0), 1.0f, 1.0f,
                                128, "SFX", true, false, 1.0f, 50.0f, 42);
    assert(h1.valid());

    // Voice exists and is playing
    auto* v1 = mgr.findVoice(h1);
    assert(v1 != nullptr);
    assert(v1->state == VoiceState::Playing);
    assert(v1->bufferHandle == 42);

    // Spawn more voices than pool size (4)
    VoiceHandle h2 = mgr.spawn("s2", glm::vec3(5, 0, 0), 1.0f, 1.0f,
                                128, "SFX", true, false, 1.0f, 50.0f, 43);
    VoiceHandle h3 = mgr.spawn("s3", glm::vec3(10, 0, 0), 1.0f, 1.0f,
                                128, "SFX", true, false, 1.0f, 50.0f, 44);
    VoiceHandle h4 = mgr.spawn("s4", glm::vec3(15, 0, 0), 1.0f, 1.0f,
                                128, "SFX", true, false, 1.0f, 50.0f, 45);
    assert(h2.valid());
    assert(h3.valid());
    assert(h4.valid());

    // Pool is full (4 voices). Next spawn should virtualize the farthest.
    VoiceHandle h5 = mgr.spawn("s5", glm::vec3(1, 0, 0), 1.0f, 1.0f,
                                128, "SFX", true, false, 1.0f, 50.0f, 46);
    assert(h5.valid());

    // After update, sort by audibility: h4 (dist=15) should be virtual
    mgr.update(listener, graph, 0.016f);
    auto* v4 = mgr.findVoice(h4);
    assert(v4 != nullptr);
    assert(v4->state == VoiceState::Virtual);

    // Stop a voice
    mgr.stop(h1, 0.0f);
    v1 = mgr.findVoice(h1);
    assert(v1 == nullptr || v1->state == VoiceState::Stopped);

    // Concurrency check
    int active = mgr.activeVoiceCount();
    assert(active == 4); // h2, h3, h4(virtual), h5

    // Invalid handle returns nullptr
    VoiceHandle bogus(999);
    assert(mgr.findVoice(bogus) == nullptr);

    printf("test_voice_manager: all assertions passed\n");
    return 0;
}
```

**Step 2: Write the implementation**

```cpp
// src/engine/audio/mix/VoiceManager.h
#pragma once

#include "engine/audio/mix/Voice.h"
#include "engine/audio/VoiceHandle.h"
#include <glm/vec3.hpp>
#include <vector>
#include <string>
#include <cstdint>

namespace audio {

class BusGraph;

class VoiceManager {
public:
    explicit VoiceManager(int poolSize = 32);

    // Spawn a new voice. Returns handle (may start virtual if pool is full).
    VoiceHandle spawn(const std::string& eventName,
                      const glm::vec3& position,
                      float volume, float pitch,
                      uint8_t priority,
                      const std::string& busName,
                      bool is3D, bool looping,
                      float refDistance, float maxDistance,
                      uint32_t bufferHandle);

    // Stop a voice (immediate or with fade)
    void stop(VoiceHandle handle, float fadeTime);

    // Stop all voices matching an event name
    void stopAllByEvent(const std::string& eventName);

    // Update voice positions
    void updatePosition(VoiceHandle handle, const glm::vec3& position);

    // Main update: score, sort, assign real/virtual, advance elapsed time
    // Returns list of source indices that need to start/stop playing
    void update(const glm::vec3& listenerPos, const BusGraph& graph,
                float deltaTime);

    Voice* findVoice(VoiceHandle handle);
    const Voice* findVoice(VoiceHandle handle) const;

    int activeVoiceCount() const;
    int countByEvent(const std::string& eventName) const;

    // Access voices for backend to read source assignments
    const std::vector<Voice>& voices() const { return voices_; }
    std::vector<Voice>& voices() { return voices_; }

private:
    int poolSize_;
    uint32_t nextId_ = 1;
    uint32_t generation_ = 0;
    std::vector<Voice> voices_;
};

} // namespace audio
```

```cpp
// src/engine/audio/mix/VoiceManager.cpp
#include "engine/audio/mix/VoiceManager.h"
#include "engine/audio/mix/BusGraph.h"
#include <algorithm>

namespace audio {

VoiceManager::VoiceManager(int poolSize) : poolSize_(poolSize) {}

VoiceHandle VoiceManager::spawn(const std::string& eventName,
                                 const glm::vec3& position,
                                 float volume, float pitch,
                                 uint8_t priority,
                                 const std::string& busName,
                                 bool is3D, bool looping,
                                 float refDistance, float maxDistance,
                                 uint32_t bufferHandle) {
    VoiceHandle handle(nextId_++, generation_);

    Voice v;
    v.handle = handle;
    v.eventName = eventName;
    v.position = position;
    v.volume = volume;
    v.pitch = pitch;
    v.priority = priority;
    v.busName = busName;
    v.is3D = is3D;
    v.looping = looping;
    v.state = VoiceState::Playing;
    v.refDistance = refDistance;
    v.maxDistance = maxDistance;
    v.bufferHandle = bufferHandle;
    v.sourceIndex = -1; // assigned during update

    voices_.push_back(std::move(v));
    return handle;
}

void VoiceManager::stop(VoiceHandle handle, float fadeTime) {
    auto it = std::find_if(voices_.begin(), voices_.end(),
        [&](const Voice& v) { return v.handle == handle; });
    if (it != voices_.end()) {
        if (fadeTime <= 0.0f) {
            it->state = VoiceState::Stopped;
        } else {
            it->state = VoiceState::Stopping;
        }
    }
}

void VoiceManager::stopAllByEvent(const std::string& eventName) {
    for (auto& v : voices_) {
        if (v.eventName == eventName && v.state != VoiceState::Stopped) {
            v.state = VoiceState::Stopped;
        }
    }
}

void VoiceManager::updatePosition(VoiceHandle handle, const glm::vec3& position) {
    auto* v = findVoice(handle);
    if (v) v->position = position;
}

void VoiceManager::update(const glm::vec3& listenerPos,
                           const BusGraph& graph, float deltaTime) {
    // Remove stopped voices
    voices_.erase(
        std::remove_if(voices_.begin(), voices_.end(),
            [](const Voice& v) { return v.state == VoiceState::Stopped; }),
        voices_.end());

    // Compute audibility for all voices
    struct Scored {
        int index;
        float score;
    };
    std::vector<Scored> scored;
    scored.reserve(voices_.size());
    for (int i = 0; i < static_cast<int>(voices_.size()); ++i) {
        float s = voices_[i].computeAudibility(listenerPos, graph);
        scored.push_back({i, s});
    }

    // Sort by audibility descending
    std::sort(scored.begin(), scored.end(),
        [](const Scored& a, const Scored& b) { return a.score > b.score; });

    // Assign real/virtual: top poolSize_ get sources, rest go virtual
    int realCount = 0;
    for (auto& s : scored) {
        Voice& v = voices_[s.index];
        if (realCount < poolSize_ && v.state != VoiceState::Stopping) {
            if (v.state == VoiceState::Virtual) {
                v.state = VoiceState::Playing;
            }
            v.sourceIndex = realCount;
            ++realCount;
        } else if (v.state == VoiceState::Playing) {
            v.state = VoiceState::Virtual;
            v.sourceIndex = -1;
        }
    }

    // Advance elapsed time
    for (auto& v : voices_) {
        v.elapsedTime += deltaTime;
    }
}

Voice* VoiceManager::findVoice(VoiceHandle handle) {
    auto it = std::find_if(voices_.begin(), voices_.end(),
        [&](const Voice& v) { return v.handle == handle; });
    return (it != voices_.end()) ? &(*it) : nullptr;
}

const Voice* VoiceManager::findVoice(VoiceHandle handle) const {
    auto it = std::find_if(voices_.begin(), voices_.end(),
        [&](const Voice& v) { return v.handle == handle; });
    return (it != voices_.end()) ? &(*it) : nullptr;
}

int VoiceManager::activeVoiceCount() const {
    int count = 0;
    for (auto& v : voices_) {
        if (v.state != VoiceState::Stopped) ++count;
    }
    return count;
}

int VoiceManager::countByEvent(const std::string& eventName) const {
    int count = 0;
    for (auto& v : voices_) {
        if (v.eventName == eventName && v.state != VoiceState::Stopped) ++count;
    }
    return count;
}

} // namespace audio
```

**Step 3: Register test, add to engine_audio**

Add `audio/mix/VoiceManager.cpp` to engine_audio sources.

Add to `tests/engine/CMakeLists.txt`:
```cmake
pixel_roguelike_add_test(test_voice_manager
    SOURCES test_voice_manager.cpp
    LIBRARIES engine_audio
    LABELS engine audio
)
```

**Step 4: Build and run**

```bash
cmake --build build --target test_voice_manager && ctest --test-dir build -R test_voice_manager -V
```
Expected: PASS

**Step 5: Commit**

```bash
git add src/engine/audio/mix/VoiceManager.h src/engine/audio/mix/VoiceManager.cpp tests/engine/test_voice_manager.cpp tests/engine/CMakeLists.txt src/engine/CMakeLists.txt
git commit -m "Add VoiceManager with audibility scoring and virtualization"
```

---

## Task 12: ReverbManager — Preset Management and Crossfade

**Files:**
- Create: `src/engine/audio/mix/ReverbManager.h`
- Create: `src/engine/audio/mix/ReverbManager.cpp`
- Test: `tests/engine/test_reverb_manager.cpp`

**Step 1: Write the test**

```cpp
// tests/engine/test_reverb_manager.cpp
#include "engine/audio/mix/ReverbManager.h"
#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) < eps;
}

int main() {
    using namespace audio;

    ReverbManager mgr;

    // Default presets exist
    assert(mgr.hasPreset("Cell"));
    assert(mgr.hasPreset("Corridor"));
    assert(mgr.hasPreset("OpenArea"));
    assert(mgr.hasPreset("None"));
    assert(!mgr.hasPreset("Nonexistent"));

    // Current preset starts as None
    assert(mgr.currentPreset() == "None");

    // Set preset
    mgr.setPreset("Cell");
    assert(mgr.currentPreset() == "Cell");

    // Crossfade: after setting, blend starts at 0 and ramps to 1
    mgr.beginTransition("Corridor", 0.5f);
    assert(mgr.isTransitioning());
    assert(near(mgr.transitionProgress(), 0.0f));

    // Advance half the fade time
    mgr.updateTransition(0.25f);
    assert(near(mgr.transitionProgress(), 0.5f));
    assert(mgr.isTransitioning());

    // Advance past end
    mgr.updateTransition(0.3f);
    assert(!mgr.isTransitioning());
    assert(near(mgr.transitionProgress(), 1.0f));
    assert(mgr.currentPreset() == "Corridor");

    // Get preset params
    auto* cellParams = mgr.presetParams("Cell");
    assert(cellParams != nullptr);
    assert(cellParams->decayTime < 1.0f); // small room

    auto* corridorParams = mgr.presetParams("Corridor");
    assert(corridorParams != nullptr);
    assert(corridorParams->decayTime > cellParams->decayTime); // larger space

    printf("test_reverb_manager: all assertions passed\n");
    return 0;
}
```

**Step 2: Write the implementation**

```cpp
// src/engine/audio/mix/ReverbManager.h
#pragma once

#include "engine/audio/backend/ALEffectSlots.h"
#include <string>
#include <unordered_map>
#include <algorithm>

namespace audio {

class ReverbManager {
public:
    ReverbManager();

    void addPreset(const std::string& name, const ReverbParams& params);
    bool hasPreset(const std::string& name) const;
    const ReverbParams* presetParams(const std::string& name) const;

    // Immediate switch
    void setPreset(const std::string& name);

    // Crossfade over duration seconds
    void beginTransition(const std::string& targetPreset, float duration);
    void updateTransition(float deltaTime);

    bool isTransitioning() const { return transitioning_; }
    float transitionProgress() const { return transitionProgress_; }
    const std::string& currentPreset() const { return currentPreset_; }
    const std::string& targetPreset() const { return targetPreset_; }

    // Interpolated params during transition (for backend to apply)
    ReverbParams currentParams() const;

private:
    std::unordered_map<std::string, ReverbParams> presets_;
    std::string currentPreset_ = "None";
    std::string targetPreset_;
    bool transitioning_ = false;
    float transitionDuration_ = 0.5f;
    float transitionElapsed_ = 0.0f;
    float transitionProgress_ = 1.0f;
};

} // namespace audio
```

```cpp
// src/engine/audio/mix/ReverbManager.cpp
#include "engine/audio/mix/ReverbManager.h"
#include <cmath>

namespace audio {

ReverbManager::ReverbManager() {
    // "None" preset — minimal reverb
    ReverbParams none;
    none.gain = 0.0f;
    none.decayTime = 0.1f;
    none.reflectionsGain = 0.0f;
    none.lateReverbGain = 0.0f;
    presets_["None"] = none;

    // "Cell" — small enclosed room, short decay
    ReverbParams cell;
    cell.density = 0.8f;
    cell.diffusion = 0.9f;
    cell.gain = 0.4f;
    cell.gainHF = 0.7f;
    cell.decayTime = 0.6f;
    cell.decayHFRatio = 0.6f;
    cell.reflectionsGain = 0.15f;
    cell.reflectionsDelay = 0.002f;
    cell.lateReverbGain = 0.6f;
    cell.lateReverbDelay = 0.008f;
    presets_["Cell"] = cell;

    // "Corridor" — medium length, narrow
    ReverbParams corridor;
    corridor.density = 0.5f;
    corridor.diffusion = 0.7f;
    corridor.gain = 0.35f;
    corridor.gainHF = 0.65f;
    corridor.decayTime = 1.8f;
    corridor.decayHFRatio = 0.7f;
    corridor.reflectionsGain = 0.12f;
    corridor.reflectionsDelay = 0.005f;
    corridor.lateReverbGain = 0.9f;
    corridor.lateReverbDelay = 0.02f;
    presets_["Corridor"] = corridor;

    // "OpenArea" — large space, long decay
    ReverbParams open;
    open.density = 0.3f;
    open.diffusion = 0.6f;
    open.gain = 0.3f;
    open.gainHF = 0.5f;
    open.decayTime = 3.2f;
    open.decayHFRatio = 0.5f;
    open.reflectionsGain = 0.08f;
    open.reflectionsDelay = 0.012f;
    open.lateReverbGain = 1.0f;
    open.lateReverbDelay = 0.035f;
    presets_["OpenArea"] = open;
}

void ReverbManager::addPreset(const std::string& name,
                                const ReverbParams& params) {
    presets_[name] = params;
}

bool ReverbManager::hasPreset(const std::string& name) const {
    return presets_.find(name) != presets_.end();
}

const ReverbParams* ReverbManager::presetParams(const std::string& name) const {
    auto it = presets_.find(name);
    return (it != presets_.end()) ? &it->second : nullptr;
}

void ReverbManager::setPreset(const std::string& name) {
    if (hasPreset(name)) {
        currentPreset_ = name;
        transitioning_ = false;
        transitionProgress_ = 1.0f;
    }
}

void ReverbManager::beginTransition(const std::string& targetPreset,
                                      float duration) {
    if (!hasPreset(targetPreset) || targetPreset == currentPreset_) return;
    targetPreset_ = targetPreset;
    transitionDuration_ = std::max(duration, 0.01f);
    transitionElapsed_ = 0.0f;
    transitionProgress_ = 0.0f;
    transitioning_ = true;
}

void ReverbManager::updateTransition(float deltaTime) {
    if (!transitioning_) return;
    transitionElapsed_ += deltaTime;
    transitionProgress_ = std::clamp(
        transitionElapsed_ / transitionDuration_, 0.0f, 1.0f);
    if (transitionProgress_ >= 1.0f) {
        currentPreset_ = targetPreset_;
        transitioning_ = false;
    }
}

static float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

ReverbParams ReverbManager::currentParams() const {
    auto* current = presetParams(currentPreset_);
    if (!current) return {};

    if (!transitioning_) return *current;

    auto* target = presetParams(targetPreset_);
    if (!target) return *current;

    float t = transitionProgress_;
    ReverbParams result;
    result.density = lerp(current->density, target->density, t);
    result.diffusion = lerp(current->diffusion, target->diffusion, t);
    result.gain = lerp(current->gain, target->gain, t);
    result.gainHF = lerp(current->gainHF, target->gainHF, t);
    result.decayTime = lerp(current->decayTime, target->decayTime, t);
    result.decayHFRatio = lerp(current->decayHFRatio, target->decayHFRatio, t);
    result.reflectionsGain = lerp(current->reflectionsGain,
                                   target->reflectionsGain, t);
    result.reflectionsDelay = lerp(current->reflectionsDelay,
                                    target->reflectionsDelay, t);
    result.lateReverbGain = lerp(current->lateReverbGain,
                                  target->lateReverbGain, t);
    result.lateReverbDelay = lerp(current->lateReverbDelay,
                                   target->lateReverbDelay, t);
    result.airAbsorptionGainHF = lerp(current->airAbsorptionGainHF,
                                       target->airAbsorptionGainHF, t);
    result.roomRolloffFactor = lerp(current->roomRolloffFactor,
                                     target->roomRolloffFactor, t);
    return result;
}

} // namespace audio
```

**Step 3: Register test**

```cmake
pixel_roguelike_add_test(test_reverb_manager
    SOURCES test_reverb_manager.cpp
    LIBRARIES engine_audio
    LABELS engine audio
)
```

**Step 4: Build and run**

```bash
cmake --build build --target test_reverb_manager && ctest --test-dir build -R test_reverb_manager -V
```

**Step 5: Commit**

```bash
git add src/engine/audio/mix/ReverbManager.h src/engine/audio/mix/ReverbManager.cpp tests/engine/test_reverb_manager.cpp tests/engine/CMakeLists.txt src/engine/CMakeLists.txt
git commit -m "Add ReverbManager with preset crossfading for room-based reverb"
```

---

## Task 13: OcclusionProcessor — Raycast-Based Occlusion

**Files:**
- Create: `src/engine/audio/mix/OcclusionProcessor.h`
- Create: `src/engine/audio/mix/OcclusionProcessor.cpp`

This depends on PhysicsSystem raycast API. The processor will accept a function pointer/callback for raycasting to stay decoupled from Jolt directly.

**Step 1: Write the implementation**

```cpp
// src/engine/audio/mix/OcclusionProcessor.h
#pragma once

#include "engine/audio/mix/Voice.h"
#include <glm/vec3.hpp>
#include <vector>
#include <functional>

namespace audio {

// Raycast callback: (origin, direction, maxDistance) -> hit?
// Returns true if geometry blocks the path, false if clear
using RaycastFunc = std::function<bool(const glm::vec3& origin,
                                        const glm::vec3& direction,
                                        float maxDistance)>;

class OcclusionProcessor {
public:
    // queryRate: how often to recast per voice (Hz)
    explicit OcclusionProcessor(float queryRate = 10.0f);

    // Set the raycast function (provided by game layer via PhysicsSystem)
    void setRaycastFunc(RaycastFunc func);

    // Process all active 3D voices, updating their occlusion factor
    void update(std::vector<Voice>& voices,
                const glm::vec3& listenerPos,
                float deltaTime);

private:
    RaycastFunc raycast_;
    float queryRate_;
    float queryInterval_;
    float timeSinceLastQuery_ = 0.0f;
};

} // namespace audio
```

```cpp
// src/engine/audio/mix/OcclusionProcessor.cpp
#include "engine/audio/mix/OcclusionProcessor.h"
#include <glm/geometric.hpp>
#include <cmath>

namespace audio {

OcclusionProcessor::OcclusionProcessor(float queryRate)
    : queryRate_(queryRate),
      queryInterval_(queryRate > 0.0f ? 1.0f / queryRate : 0.1f) {}

void OcclusionProcessor::setRaycastFunc(RaycastFunc func) {
    raycast_ = std::move(func);
}

void OcclusionProcessor::update(std::vector<Voice>& voices,
                                  const glm::vec3& listenerPos,
                                  float deltaTime) {
    timeSinceLastQuery_ += deltaTime;
    if (timeSinceLastQuery_ < queryInterval_) return;
    timeSinceLastQuery_ = 0.0f;

    if (!raycast_) {
        // No raycast function — clear all occlusion
        for (auto& v : voices) v.occlusion = 0.0f;
        return;
    }

    for (auto& v : voices) {
        // Skip non-3D, virtual, or stopped voices
        if (!v.is3D || v.state == VoiceState::Stopped ||
            v.state == VoiceState::Virtual) {
            continue;
        }

        glm::vec3 diff = v.position - listenerPos;
        float dist = glm::length(diff);
        if (dist < 0.01f) {
            v.occlusion = 0.0f;
            continue;
        }

        glm::vec3 dir = diff / dist;
        bool blocked = raycast_(listenerPos, dir, dist);
        // Simple binary occlusion for now. Smooth transitions
        // by blending toward target (avoids abrupt filter jumps).
        float target = blocked ? 1.0f : 0.0f;
        float blendSpeed = 8.0f; // per second
        float step = blendSpeed * queryInterval_;
        if (v.occlusion < target) {
            v.occlusion = std::min(v.occlusion + step, target);
        } else {
            v.occlusion = std::max(v.occlusion - step, target);
        }
    }
}

} // namespace audio
```

**Step 2: Add to engine_audio, build**

```bash
cmake --build build --target engine_audio
```

**Step 3: Commit**

```bash
git add src/engine/audio/mix/OcclusionProcessor.h src/engine/audio/mix/OcclusionProcessor.cpp src/engine/CMakeLists.txt
git commit -m "Add OcclusionProcessor with raycast-based occlusion and smooth blending"
```

---

## Task 14: PhysicsSystem Raycast Extension

**Files:**
- Modify: `src/engine/physics/PhysicsSystem.h`
- Modify: `src/engine/physics/PhysicsSystem.cpp`

Add a public raycast method to PhysicsSystem that the audio module can use.

**Step 1: Add to header**

Add to the public section of `PhysicsSystem`:
```cpp
// Raycast: returns true if any static geometry blocks the ray
bool raycastStatic(const glm::vec3& origin, const glm::vec3& direction,
                   float maxDistance) const;
```

**Step 2: Implement using Jolt**

Add to `PhysicsSystem.cpp` (inside the Impl or as a method that accesses Impl):
```cpp
bool PhysicsSystem::raycastStatic(const glm::vec3& origin,
                                   const glm::vec3& direction,
                                   float maxDistance) const {
    JPH::RRayCast ray;
    ray.mOrigin = JPH::Vec3(origin.x, origin.y, origin.z);
    ray.mDirection = JPH::Vec3(direction.x, direction.y, direction.z) * maxDistance;

    JPH::RayCastResult result;
    // Cast against broadphase, static layer only
    auto& bpQuery = impl_->physicsSystem_.GetNarrowPhaseQuery();
    JPH::ObjectLayerFilter layerFilter; // accept all layers
    return bpQuery.CastRay(ray, result);
}
```

Note: The exact Jolt API calls depend on the existing Impl structure. The implementing engineer should check the Jolt layer definitions in the Impl and may need to filter to only static bodies. Reference `impl_->physicsSystem_` field name — verify against actual Impl struct.

**Step 3: Build**

```bash
cmake --build build --target engine_physics
```

**Step 4: Commit**

```bash
git add src/engine/physics/PhysicsSystem.h src/engine/physics/PhysicsSystem.cpp
git commit -m "Add raycastStatic to PhysicsSystem for audio occlusion queries"
```

---

## Task 15: AudioEngine — Facade Class

**Files:**
- Create: `src/engine/audio/AudioEngine.h`
- Create: `src/engine/audio/AudioEngine.cpp`

This is the main public API that owns all three layers and processes commands.

**Step 1: Write the implementation**

```cpp
// src/engine/audio/AudioEngine.h
#pragma once

#include "engine/audio/AudioCommand.h"
#include "engine/audio/VoiceHandle.h"
#include "engine/audio/backend/ALDevice.h"
#include "engine/audio/backend/ALSourcePool.h"
#include "engine/audio/backend/ALBufferCache.h"
#include "engine/audio/backend/ALStreamPlayer.h"
#include "engine/audio/backend/ALEffectSlots.h"
#include "engine/audio/mix/VoiceManager.h"
#include "engine/audio/mix/BusGraph.h"
#include "engine/audio/mix/OcclusionProcessor.h"
#include "engine/audio/mix/ReverbManager.h"
#include "engine/audio/events/EventRegistry.h"

#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace audio {

struct AudioEngineConfig {
    int voicePoolSize = 32;
    int reverbSlots = 2;
    float occlusionQueryRate = 10.0f;
    std::string audioBasePath = "assets/audio";
    std::string eventDefPath = "assets/audio/sound_events.json";
};

class AudioEngine {
public:
    explicit AudioEngine(const AudioEngineConfig& config = {});
    ~AudioEngine();

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    bool init();
    void shutdown();

    // --- Public Command API ---

    // Fire-and-forget 3D sound
    VoiceHandle play(const std::string& eventName,
                     const glm::vec3& position,
                     float volume = 1.0f, float pitch = 1.0f);

    // Fire-and-forget 2D sound (no position)
    VoiceHandle play(const std::string& eventName);

    // Looping sound (returns handle for later stop)
    VoiceHandle playLooping(const std::string& eventName,
                            const glm::vec3& position,
                            float volume = 1.0f, float pitch = 1.0f);

    void stop(VoiceHandle handle, float fadeTime = 0.0f);
    void updateVoicePosition(VoiceHandle handle, const glm::vec3& position);

    // Music (streamed)
    void playMusic(const std::string& path, bool loop = true);
    void stopMusic();
    void setMusicVolume(float volume);

    // Ambience (streamed)
    void playAmbient(const std::string& path, bool loop = true);
    void stopAmbient();
    void setAmbientVolume(float volume);

    // Listener
    void setListenerTransform(const glm::vec3& position,
                               const glm::vec3& forward,
                               const glm::vec3& up);

    // Bus mixing
    void setBusVolume(const std::string& busName, float volume);
    void setBusMute(const std::string& busName, bool mute);

    // Reverb
    void setReverbPreset(const std::string& presetName,
                          float transitionTime = 0.5f);

    // Occlusion
    void setRaycastFunc(OcclusionProcessor::RaycastFunc func);

    // Frame update — processes commands, updates voices, streams
    void update(float deltaTime);

    // Accessors
    BusGraph& busGraph() { return busGraph_; }
    const BusGraph& busGraph() const { return busGraph_; }
    EventRegistry& eventRegistry() { return eventRegistry_; }

private:
    void processCommand(const audio::AudioCommand& cmd);
    VoiceHandle spawnVoice(const std::string& eventName,
                            const glm::vec3& position,
                            float volume, float pitch,
                            bool is3D, bool looping);
    void syncVoicesToSources();
    void applyOcclusionToSources();
    void applyReverbToSources();

    AudioEngineConfig config_;

    // Backend
    ALDevice device_;
    ALSourcePool sourcePool_;
    ALBufferCache bufferCache_;
    ALStreamPlayer musicStream_;
    ALStreamPlayer ambientStream_;
    ALEffectSlots effectSlots_;

    // Mix engine
    VoiceManager voiceManager_;
    BusGraph busGraph_;
    OcclusionProcessor occlusionProcessor_;
    ReverbManager reverbManager_;

    // Game audio
    EventRegistry eventRegistry_;

    // Command queue (double-buffered)
    std::vector<AudioCommand> pendingCommands_;

    // Listener state
    glm::vec3 listenerPos_{0.0f};
    glm::vec3 listenerFwd_{0.0f, 0.0f, -1.0f};
    glm::vec3 listenerUp_{0.0f, 1.0f, 0.0f};

    bool initialized_ = false;
};

} // namespace audio
```

```cpp
// src/engine/audio/AudioEngine.cpp
#include "engine/audio/AudioEngine.h"
#include <AL/al.h>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace audio {

AudioEngine::AudioEngine(const AudioEngineConfig& config)
    : config_(config),
      sourcePool_(config.voicePoolSize),
      voiceManager_(config.voicePoolSize),
      occlusionProcessor_(config.occlusionQueryRate) {}

AudioEngine::~AudioEngine() {
    shutdown();
}

bool AudioEngine::init() {
    if (!device_.open()) return false;

    if (!sourcePool_.init()) {
        device_.close();
        return false;
    }

    // Initialize EFX reverb slots if supported
    if (device_.capabilities().hasEFX) {
        int allocated = effectSlots_.init(config_.reverbSlots);
        if (allocated > 0) {
            // Apply default reverb
            reverbManager_.setPreset("None");
            effectSlots_.setReverb(0, reverbManager_.currentParams());
        }
    }

    // Load event definitions
    if (!eventRegistry_.loadFromFile(config_.eventDefPath)) {
        spdlog::warn("AudioEngine: no sound_events.json at '{}' — "
                     "events won't resolve", config_.eventDefPath);
    }

    // Preload all SFX WAV files
    auto paths = eventRegistry_.allSoundPaths();
    std::vector<std::string> wavPaths;
    for (auto& p : paths) {
        if (p.size() > 4 && p.substr(p.size() - 4) == ".wav") {
            wavPaths.push_back(p);
        }
    }
    bufferCache_.preload(wavPaths, config_.audioBasePath);

    initialized_ = true;
    spdlog::info("AudioEngine: initialized (pool={}, reverb_slots={}, events={})",
                 config_.voicePoolSize, effectSlots_.slotCount(),
                 eventRegistry_.eventNames().size());
    return true;
}

void AudioEngine::shutdown() {
    if (!initialized_) return;
    musicStream_.stop();
    ambientStream_.stop();
    // Stop all sources
    for (int i = 0; i < sourcePool_.poolSize(); ++i) {
        sourcePool_.resetSource(i);
    }
    effectSlots_.shutdown();
    bufferCache_.clear();
    sourcePool_.shutdown();
    device_.close();
    initialized_ = false;
}

// --- Public API (queue commands) ---

VoiceHandle AudioEngine::play(const std::string& eventName,
                                const glm::vec3& position,
                                float volume, float pitch) {
    return spawnVoice(eventName, position, volume, pitch, true, false);
}

VoiceHandle AudioEngine::play(const std::string& eventName) {
    return spawnVoice(eventName, glm::vec3(0), 1.0f, 1.0f, false, false);
}

VoiceHandle AudioEngine::playLooping(const std::string& eventName,
                                       const glm::vec3& position,
                                       float volume, float pitch) {
    return spawnVoice(eventName, position, volume, pitch, true, true);
}

void AudioEngine::stop(VoiceHandle handle, float fadeTime) {
    voiceManager_.stop(handle, fadeTime);
}

void AudioEngine::updateVoicePosition(VoiceHandle handle,
                                        const glm::vec3& position) {
    voiceManager_.updatePosition(handle, position);
}

void AudioEngine::playMusic(const std::string& path, bool loop) {
    std::string fullPath = config_.audioBasePath + "/" + path;
    musicStream_.play(fullPath, loop);
    musicStream_.setGain(busGraph_.effectiveVolume("Music"));
}

void AudioEngine::stopMusic() {
    musicStream_.stop();
}

void AudioEngine::setMusicVolume(float volume) {
    busGraph_.setVolume("Music", volume);
    musicStream_.setGain(busGraph_.effectiveVolume("Music"));
}

void AudioEngine::playAmbient(const std::string& path, bool loop) {
    std::string fullPath = config_.audioBasePath + "/" + path;
    ambientStream_.play(fullPath, loop);
    ambientStream_.setGain(busGraph_.effectiveVolume("Ambience"));
}

void AudioEngine::stopAmbient() {
    ambientStream_.stop();
}

void AudioEngine::setAmbientVolume(float volume) {
    busGraph_.setVolume("Ambience", volume);
    ambientStream_.setGain(busGraph_.effectiveVolume("Ambience"));
}

void AudioEngine::setListenerTransform(const glm::vec3& position,
                                         const glm::vec3& forward,
                                         const glm::vec3& up) {
    listenerPos_ = position;
    listenerFwd_ = forward;
    listenerUp_ = up;

    alListener3f(AL_POSITION, position.x, position.y, position.z);
    float orientation[6] = {forward.x, forward.y, forward.z,
                            up.x, up.y, up.z};
    alListenerfv(AL_ORIENTATION, orientation);
}

void AudioEngine::setBusVolume(const std::string& busName, float volume) {
    busGraph_.setVolume(busName, volume);
    // Update stream gains if relevant buses changed
    musicStream_.setGain(busGraph_.effectiveVolume("Music"));
    ambientStream_.setGain(busGraph_.effectiveVolume("Ambience"));
}

void AudioEngine::setBusMute(const std::string& busName, bool mute) {
    busGraph_.setMute(busName, mute);
}

void AudioEngine::setReverbPreset(const std::string& presetName,
                                    float transitionTime) {
    reverbManager_.beginTransition(presetName, transitionTime);
}

void AudioEngine::setRaycastFunc(OcclusionProcessor::RaycastFunc func) {
    occlusionProcessor_.setRaycastFunc(std::move(func));
}

// --- Frame Update ---

void AudioEngine::update(float deltaTime) {
    if (!initialized_) return;

    // Update voice manager (scoring, virtualization)
    voiceManager_.update(listenerPos_, busGraph_, deltaTime);

    // Update occlusion
    occlusionProcessor_.update(voiceManager_.voices(), listenerPos_, deltaTime);

    // Update reverb crossfade
    reverbManager_.updateTransition(deltaTime);
    if (effectSlots_.slotCount() > 0) {
        effectSlots_.setReverb(0, reverbManager_.currentParams());
    }

    // Sync voices to AL sources
    syncVoicesToSources();

    // Update streams
    musicStream_.update();
    ambientStream_.update();
}

VoiceHandle AudioEngine::spawnVoice(const std::string& eventName,
                                      const glm::vec3& position,
                                      float volume, float pitch,
                                      bool is3D, bool looping) {
    auto* def = eventRegistry_.find(eventName);
    if (!def) {
        spdlog::warn("AudioEngine: unknown event '{}'", eventName);
        return {};
    }

    // Check concurrency
    if (voiceManager_.countByEvent(eventName) >= def->maxInstances) {
        return {};
    }

    // Pick sound
    int soundIdx = def->pickSound();
    if (soundIdx < 0) return {};

    std::string soundPath = config_.audioBasePath + "/" + def->sounds[soundIdx];
    ALuint buffer = bufferCache_.getOrLoad(soundPath);
    if (buffer == 0) return {};

    float vol = volume * def->randomVolume();
    float pit = pitch * def->randomPitch();

    return voiceManager_.spawn(
        eventName, position, vol, pit, 128,
        def->busName, is3D ? def->is3D : false, looping,
        def->refDistance, def->maxDistance, buffer);
}

void AudioEngine::syncVoicesToSources() {
    for (auto& voice : voiceManager_.voices()) {
        if (voice.state == VoiceState::Playing && voice.sourceIndex >= 0) {
            ALuint src = sourcePool_.source(voice.sourceIndex);
            if (src == 0) continue;

            // Check if source is already playing this buffer
            ALint currentBuffer = 0;
            alGetSourcei(src, AL_BUFFER, &currentBuffer);
            if (static_cast<ALuint>(currentBuffer) != voice.bufferHandle) {
                // New assignment — set up source
                alSourceStop(src);
                alSourcei(src, AL_BUFFER, static_cast<ALint>(voice.bufferHandle));
                alSourcei(src, AL_LOOPING, voice.looping ? AL_TRUE : AL_FALSE);
                alSourcePlay(src);
            }

            // Update position
            if (voice.is3D) {
                alSource3f(src, AL_POSITION,
                           voice.position.x, voice.position.y, voice.position.z);
                alSourcei(src, AL_SOURCE_RELATIVE, AL_FALSE);
                alSourcef(src, AL_REFERENCE_DISTANCE, voice.refDistance);
                alSourcef(src, AL_MAX_DISTANCE, voice.maxDistance);
            } else {
                alSourcei(src, AL_SOURCE_RELATIVE, AL_TRUE);
                alSource3f(src, AL_POSITION, 0.0f, 0.0f, 0.0f);
            }

            // Volume: voice volume * bus effective volume
            float effectiveGain = voice.volume *
                busGraph_.effectiveVolume(voice.busName);

            // Apply occlusion
            if (voice.occlusion > 0.0f) {
                float occGain = 1.0f - (voice.occlusion * 0.3f);
                effectiveGain *= occGain;

                // Low-pass filter for occlusion
                float hfGain = 1.0f - (voice.occlusion * 0.7f);
                // Apply via direct filter if EFX is available
                if (effectSlots_.slotCount() > 0) {
                    // Use a transient filter — in production you'd cache these
                    ALuint filter = effectSlots_.createFilter();
                    effectSlots_.setFilterParams(filter, 1.0f, hfGain);
                    alSourcei(src, AL_DIRECT_FILTER, filter);
                    effectSlots_.destroyFilter(filter);
                }
            } else {
                alSourcei(src, AL_DIRECT_FILTER, AL_FILTER_NULL);
            }

            alSourcef(src, AL_GAIN, effectiveGain);
            alSourcef(src, AL_PITCH, voice.pitch);

            // Reverb auxiliary send
            if (voice.is3D && effectSlots_.slotCount() > 0) {
                float sendGain = 1.0f - (voice.occlusion * 0.5f);
                // Connect to reverb slot 0
                alSource3i(src, AL_AUXILIARY_SEND_FILTER,
                           effectSlots_.slotHandle(0), 0, AL_FILTER_NULL);
            }

        } else if (voice.state == VoiceState::Virtual && voice.sourceIndex == -1) {
            // Voice went virtual — nothing to do, source was already freed
        }
    }

    // Stop sources that are no longer assigned to any voice
    // (The voice manager reassigns sourceIndex each frame)
}

} // namespace audio
```

**Step 2: Add to engine_audio, build**

Add `audio/AudioEngine.cpp` to engine_audio sources. Note engine_audio now needs to link spdlog.

```bash
cmake --build build --target engine_audio
```

**Step 3: Commit**

```bash
git add src/engine/audio/AudioEngine.h src/engine/audio/AudioEngine.cpp src/engine/CMakeLists.txt
git commit -m "Add AudioEngine facade owning all three audio layers"
```

---

## Task 16: Update CMakeLists — Complete engine_audio Target

**Files:**
- Modify: `src/engine/CMakeLists.txt`

Replace the existing engine_audio target definition with the full source list:

```cmake
add_library(engine_audio STATIC
    audio/AudioEngine.cpp
    audio/backend/ALDevice.cpp
    audio/backend/ALSourcePool.cpp
    audio/backend/ALBufferCache.cpp
    audio/backend/ALStreamPlayer.cpp
    audio/backend/ALEffectSlots.cpp
    audio/mix/Voice.cpp
    audio/mix/VoiceManager.cpp
    audio/mix/BusGraph.cpp
    audio/mix/OcclusionProcessor.cpp
    audio/mix/ReverbManager.cpp
    audio/events/EventRegistry.cpp
    ${CMAKE_SOURCE_DIR}/external/stb_vorbis.c
)
target_include_directories(engine_audio PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(engine_audio PRIVATE ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(engine_audio PUBLIC engine_core glm::glm)
target_link_libraries(engine_audio PRIVATE OpenAL::OpenAL nlohmann_json::nlohmann_json spdlog::spdlog)
set_source_files_properties(${CMAKE_SOURCE_DIR}/external/stb_vorbis.c PROPERTIES LANGUAGE C)
```

**Step 1: Make the edit, build**

```bash
cmake --build build --target engine_audio
```

**Step 2: Commit**

```bash
git add src/engine/CMakeLists.txt
git commit -m "Update engine_audio CMake target with complete audio module sources"
```

---

## Task 17: ECS Integration — AudioEngineSystem

**Files:**
- Create: `src/game/systems/AudioEngineSystem.h`
- Create: `src/game/systems/AudioEngineSystem.cpp`
- Modify: `src/game/systems/AudioListenerSystem.h` (refactor to use new AudioEngine)
- Modify: `src/game/systems/AudioListenerSystem.cpp`

**Step 1: Write AudioEngineSystem**

```cpp
// src/game/systems/AudioEngineSystem.h
#pragma once

#include "engine/core/System.h"

namespace audio { class AudioEngine; }

class AudioEngineSystem : public System {
public:
    explicit AudioEngineSystem(audio::AudioEngine& engine);

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    audio::AudioEngine& engine_;
};
```

```cpp
// src/game/systems/AudioEngineSystem.cpp
#include "game/systems/AudioEngineSystem.h"
#include "engine/audio/AudioEngine.h"

AudioEngineSystem::AudioEngineSystem(audio::AudioEngine& engine)
    : engine_(engine) {}

void AudioEngineSystem::init(Application& app) {
    // AudioEngine::init() is called separately before system registration
}

void AudioEngineSystem::update(Application& app, float deltaTime) {
    engine_.update(deltaTime);
}

void AudioEngineSystem::shutdown() {
    engine_.shutdown();
}
```

**Step 2: Refactor AudioListenerSystem to use AudioEngine**

Update `AudioListenerSystem` to call `audioEngine.setListenerTransform()` instead of the old `AudioSystem::setListenerTransform()`. Also process `AudioSourceComponent` entities (the emitter role).

```cpp
// src/game/systems/AudioListenerSystem.h — updated
#pragma once

#include "engine/core/System.h"

namespace audio { class AudioEngine; }

class AudioListenerSystem : public System {
public:
    explicit AudioListenerSystem(audio::AudioEngine& engine);

    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

private:
    audio::AudioEngine& engine_;
};
```

```cpp
// src/game/systems/AudioListenerSystem.cpp — updated
#include "game/systems/AudioListenerSystem.h"
#include "engine/audio/AudioEngine.h"
#include "engine/core/Application.h"
#include "game/components/TransformComponent.h"
#include "game/components/CameraComponent.h"
#include "game/components/AudioSourceComponent.h"
#include "game/components/AudioListenerTag.h"
#include "game/ecs/GameRegistry.h"

#include <glm/trigonometric.hpp>
#include <cmath>

AudioListenerSystem::AudioListenerSystem(audio::AudioEngine& engine)
    : engine_(engine) {}

void AudioListenerSystem::init(Application& app) {}

void AudioListenerSystem::update(Application& app, float deltaTime) {
    auto& registry = app.registry();

    // Update listener from primary camera
    auto view = registry.view<PrimaryCameraTag, TransformComponent>();
    for (auto [entity, transform] : view.each()) {
        const float yawRad   = glm::radians(transform.rotation.y);
        const float pitchRad = glm::radians(transform.rotation.x);
        glm::vec3 forward{
            std::cos(pitchRad) * std::sin(yawRad),
           -std::sin(pitchRad),
            std::cos(pitchRad) * std::cos(yawRad)
        };
        glm::vec3 up{0.0f, 1.0f, 0.0f};
        engine_.setListenerTransform(transform.position, forward, up);
        break;
    }

    // Process audio source components (emitter role)
    auto sources = registry.view<AudioSourceComponent, TransformComponent>();
    for (auto [entity, audio, transform] : sources.each()) {
        if (audio.triggerPlay) {
            audio.triggerPlay = false;
            engine_.play(std::to_string(audio.soundHandle), transform.position,
                         audio.volume, audio.pitch);
        }
    }
}

void AudioListenerSystem::shutdown() {}
```

**Step 3: Add to game CMakeLists**

Add `systems/AudioEngineSystem.cpp` to the gameplay library sources.

**Step 4: Build**

```bash
cmake --build build --target gameplay
```

**Step 5: Commit**

```bash
git add src/game/systems/AudioEngineSystem.h src/game/systems/AudioEngineSystem.cpp src/game/systems/AudioListenerSystem.h src/game/systems/AudioListenerSystem.cpp src/game/CMakeLists.txt
git commit -m "Add AudioEngineSystem and refactor AudioListenerSystem for new audio module"
```

---

## Task 18: Runtime Bootstrap — Replace AudioSystem Registration

**Files:**
- Modify: `apps/runtime/main.cpp`

**Step 1: Replace the old AudioSystem registration**

Replace lines 92-94 (approximately):
```cpp
// OLD:
auto& audio = app.addSystem<AudioSystem>(Application::UpdatePhase::Gameplay);
app.emplaceService<AudioSystem*>(&audio);
auto& audioListener = app.addSystem<AudioListenerSystem>(Application::UpdatePhase::Gameplay, audio);
```

With:
```cpp
// NEW:
auto audioEngine = std::make_unique<audio::AudioEngine>();
audioEngine->init();
auto* audioEnginePtr = audioEngine.get();
app.emplaceService<audio::AudioEngine*>(audioEnginePtr);
app.addSystem<AudioEngineSystem>(Application::UpdatePhase::Gameplay, *audioEnginePtr);
app.addSystem<AudioListenerSystem>(Application::UpdatePhase::Gameplay, *audioEnginePtr);

// Wire up physics raycasting for audio occlusion
auto* physics = app.tryGetService<PhysicsSystem*>();
if (physics) {
    audioEnginePtr->setRaycastFunc(
        [physics](const glm::vec3& origin, const glm::vec3& dir, float maxDist) {
            return physics->raycastStatic(origin, dir, maxDist);
        });
}
```

Add required includes:
```cpp
#include "engine/audio/AudioEngine.h"
#include "game/systems/AudioEngineSystem.h"
```

Remove old include:
```cpp
// Remove: #include "engine/audio/AudioSystem.h"
```

**Step 2: Build the runtime**

```bash
cmake --build build --target pixel-roguelike
```

**Step 3: Commit**

```bash
git add apps/runtime/main.cpp
git commit -m "Replace AudioSystem with AudioEngine in runtime bootstrap"
```

---

## Task 19: Delete Old AudioSystem

**Files:**
- Delete: `src/engine/audio/AudioSystem.h`
- Delete: `src/engine/audio/AudioSystem.cpp`
- Modify: Any remaining references

**Step 1: Search for remaining references**

```bash
grep -r "AudioSystem" src/ apps/ --include="*.h" --include="*.cpp" -l
```

Update any remaining includes or references (level editor, procedural model viewer if applicable).

**Step 2: Delete old files**

```bash
git rm src/engine/audio/AudioSystem.h src/engine/audio/AudioSystem.cpp
```

**Step 3: Build all targets to verify nothing breaks**

```bash
cmake --build build
```

**Step 4: Commit**

```bash
git add -A
git commit -m "Remove old AudioSystem, replaced by AudioEngine module"
```

---

## Task 20: Asset Directory Setup + Default sound_events.json

**Files:**
- Create: `assets/audio/sound_events.json`
- Create: `assets/audio/sfx/doors/` (move existing door WAVs)
- Create: `assets/audio/sfx/footsteps/`
- Create: `assets/audio/sfx/interactions/`
- Create: `assets/audio/sfx/environment/`
- Create: `assets/audio/music/`
- Create: `assets/audio/ambience/`
- Create: `assets/audio/ui/`

**Step 1: Create directory structure and initial event definitions**

```json
{
    "door_open": {
        "sounds": ["sfx/doors/Door_Open.wav"],
        "pick": "random",
        "pitch_range": [0.95, 1.05],
        "volume_range": [0.85, 1.0],
        "max_instances": 2,
        "cooldown": 0.2,
        "bus": "Doors",
        "spatial": "3D",
        "ref_distance": 1.0,
        "max_distance": 25.0
    },
    "door_close": {
        "sounds": ["sfx/doors/Door_Close.wav"],
        "pick": "random",
        "pitch_range": [0.95, 1.05],
        "volume_range": [0.85, 1.0],
        "max_instances": 2,
        "cooldown": 0.2,
        "bus": "Doors",
        "spatial": "3D",
        "ref_distance": 1.0,
        "max_distance": 25.0
    }
}
```

**Step 2: Move existing door sound files**

```bash
cp "assets/packs/Free Wood Door Pack/Audio/Door_Open.wav" assets/audio/sfx/doors/
cp "assets/packs/Free Wood Door Pack/Audio/Door_Close.wav" assets/audio/sfx/doors/
```

**Step 3: Create .gitkeep files for empty directories**

```bash
touch assets/audio/sfx/footsteps/.gitkeep
touch assets/audio/sfx/interactions/.gitkeep
touch assets/audio/sfx/environment/.gitkeep
touch assets/audio/music/.gitkeep
touch assets/audio/ambience/.gitkeep
touch assets/audio/ui/.gitkeep
```

**Step 4: Commit**

```bash
git add assets/audio/
git commit -m "Set up audio asset directory structure with initial door sound events"
```

---

## Task 21: Integration Test — Full Audio Engine Smoke Test

**Files:**
- Create: `tests/engine/test_audio_engine_integration.cpp`

This test verifies the AudioEngine initializes, loads events, and the command flow works end-to-end. It requires OpenAL to be available (CI may need a null device).

```cpp
// tests/engine/test_audio_engine_integration.cpp
#include "engine/audio/AudioEngine.h"
#include <cassert>
#include <cstdio>

#ifndef TEST_AUDIO_BASE_PATH
#error "TEST_AUDIO_BASE_PATH must be defined"
#endif

int main() {
    using namespace audio;

    AudioEngineConfig config;
    config.audioBasePath = TEST_AUDIO_BASE_PATH;
    config.eventDefPath = std::string(TEST_AUDIO_BASE_PATH) + "/sound_events.json";
    config.voicePoolSize = 8;
    config.reverbSlots = 1;

    AudioEngine engine(config);

    // Init should succeed (requires OpenAL device)
    if (!engine.init()) {
        printf("test_audio_engine_integration: SKIPPED (no audio device)\n");
        return 0;
    }

    // Event registry loaded
    assert(engine.eventRegistry().find("door_open") != nullptr);

    // Set listener
    engine.setListenerTransform(
        glm::vec3(0, 1.7f, 0),
        glm::vec3(0, 0, -1),
        glm::vec3(0, 1, 0));

    // Play a sound event
    auto handle = engine.play("door_open", glm::vec3(2, 0, -3));
    assert(handle.valid());

    // Update a few frames
    for (int i = 0; i < 10; ++i) {
        engine.update(0.016f);
    }

    // Bus volume
    engine.setBusVolume("SFX", 0.5f);
    engine.update(0.016f);

    // Reverb preset
    engine.setReverbPreset("Cell", 0.3f);
    for (int i = 0; i < 30; ++i) {
        engine.update(0.016f);
    }

    // Stop
    engine.stop(handle);
    engine.update(0.016f);

    engine.shutdown();

    printf("test_audio_engine_integration: all assertions passed\n");
    return 0;
}
```

Register in `tests/engine/CMakeLists.txt`:
```cmake
pixel_roguelike_add_test(test_audio_engine_integration
    SOURCES test_audio_engine_integration.cpp
    LIBRARIES engine_audio
    DEFINITIONS TEST_AUDIO_BASE_PATH="${CMAKE_SOURCE_DIR}/assets/audio"
    LABELS engine audio integration
)
```

**Build, run, commit:**

```bash
cmake --build build --target test_audio_engine_integration && ctest --test-dir build -R test_audio_engine_integration -V
git add tests/engine/test_audio_engine_integration.cpp tests/engine/CMakeLists.txt
git commit -m "Add AudioEngine integration test with full init-play-update-shutdown cycle"
```

---

## Build Order Summary

Tasks must be executed in this order (dependencies shown):

```
Task 1: VoiceHandle          (no deps)
Task 2: AudioCommand          (depends on: 1)
Task 3: BusGraph              (no deps)
Task 4: Voice                 (depends on: 1, 3)
Task 5: EventRegistry         (no deps)
Task 6: ALDevice              (no deps — OpenAL)
Task 7: ALSourcePool          (depends on: 6)
Task 8: ALBufferCache          (depends on: 6)
Task 9: ALStreamPlayer         (depends on: 6)
Task 10: ALEffectSlots         (depends on: 6)
Task 11: VoiceManager          (depends on: 1, 3, 4)
Task 12: ReverbManager         (depends on: 10)
Task 13: OcclusionProcessor    (depends on: 4)
Task 14: PhysicsSystem raycast (no audio deps)
Task 15: AudioEngine           (depends on: 2-13)
Task 16: CMakeLists            (depends on: 6-15)
Task 17: ECS Systems           (depends on: 15)
Task 18: Runtime Bootstrap     (depends on: 14, 15, 17)
Task 19: Delete Old System     (depends on: 18)
Task 20: Asset Directory       (no code deps)
Task 21: Integration Test      (depends on: 15, 20)
```

**Parallelizable batches:**
- Batch 1: Tasks 1, 3, 5, 6, 14, 20 (independent foundations)
- Batch 2: Tasks 2, 4, 7, 8, 9, 10 (depend on batch 1)
- Batch 3: Tasks 11, 12, 13 (depend on batch 2)
- Batch 4: Task 15, 16 (depend on batch 3)
- Batch 5: Tasks 17, 18, 19, 21 (integration)
