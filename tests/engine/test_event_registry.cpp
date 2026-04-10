#include <cassert>
#include <cstdio>
#include <cmath>

#include "engine/audio/events/EventRegistry.h"

using engine::audio::EventRegistry;
using engine::audio::PickMode;
using engine::audio::SoundEventDef;

#ifndef TEST_SOUND_EVENTS_PATH
#error "TEST_SOUND_EVENTS_PATH must be defined"
#endif

int main() {
    // Load fixture
    EventRegistry registry;
    bool loaded = registry.loadFromFile(TEST_SOUND_EVENTS_PATH);
    assert(loaded);

    // Event count
    {
        auto names = registry.eventNames();
        assert(names.size() == 3);
    }

    // door_open: 2 sounds, RandomNoRepeat, pitch range, bus, 3D
    {
        const SoundEventDef* door = registry.find("door_open");
        assert(door != nullptr);
        assert(door->name == "door_open");
        assert(door->sounds.size() == 2);
        assert(door->sounds[0] == "sfx/doors/door_open_01.wav");
        assert(door->sounds[1] == "sfx/doors/door_open_02.wav");
        assert(door->pickMode == PickMode::RandomNoRepeat);
        assert(std::fabs(door->pitchRange.x - 0.95f) < 0.001f);
        assert(std::fabs(door->pitchRange.y - 1.05f) < 0.001f);
        assert(std::fabs(door->volumeRange.x - 0.9f) < 0.001f);
        assert(std::fabs(door->volumeRange.y - 1.0f) < 0.001f);
        assert(door->maxInstances == 2);
        assert(std::fabs(door->cooldown - 0.1f) < 0.001f);
        assert(door->busName == "Doors");
        assert(door->is3D == true);
        assert(std::fabs(door->refDistance - 1.0f) < 0.001f);
        assert(std::fabs(door->maxDistance - 30.0f) < 0.001f);
    }

    // ui_click: 2D, UI bus
    {
        const SoundEventDef* ui = registry.find("ui_click");
        assert(ui != nullptr);
        assert(ui->sounds.size() == 1);
        assert(ui->pickMode == PickMode::Random);
        assert(ui->busName == "UI");
        assert(ui->is3D == false);
        assert(ui->maxInstances == 4);
    }

    // ambient_hum: Sequential pick
    {
        const SoundEventDef* amb = registry.find("ambient_hum");
        assert(amb != nullptr);
        assert(amb->sounds.size() == 1);
        assert(amb->pickMode == PickMode::Sequential);
        assert(amb->busName == "Environment");
        assert(amb->is3D == true);
        assert(std::fabs(amb->volumeRange.x - 0.6f) < 0.001f);
        assert(std::fabs(amb->volumeRange.y - 0.6f) < 0.001f);
        assert(std::fabs(amb->refDistance - 2.0f) < 0.001f);
        assert(std::fabs(amb->maxDistance - 15.0f) < 0.001f);
    }

    // Non-existent event returns nullptr
    {
        const SoundEventDef* none = registry.find("nonexistent");
        assert(none == nullptr);
    }

    // pickSound returns valid index
    {
        const SoundEventDef* door = registry.find("door_open");
        for (int i = 0; i < 20; ++i) {
            int idx = door->pickSound();
            assert(idx >= 0 && idx < static_cast<int>(door->sounds.size()));
        }
    }

    // RandomNoRepeat never picks same index twice in a row (with 2 sounds)
    {
        const SoundEventDef* door = registry.find("door_open");
        door->lastPickIndex = -1;
        int prev = door->pickSound();
        for (int i = 0; i < 50; ++i) {
            int next = door->pickSound();
            assert(next != prev);
            prev = next;
        }
    }

    // Sequential cycles through in order
    {
        const SoundEventDef* amb = registry.find("ambient_hum");
        amb->sequentialIndex = 0;
        // Single sound — should always return 0
        for (int i = 0; i < 5; ++i) {
            assert(amb->pickSound() == 0);
        }
    }

    // allSoundPaths returns unique sorted paths
    {
        auto paths = registry.allSoundPaths();
        assert(paths.size() == 4);
        // Sorted: ambience/..., sfx/doors/..._01, sfx/doors/..._02, sfx/ui/...
        assert(paths[0] == "ambience/electrical_hum.ogg");
        assert(paths[1] == "sfx/doors/door_open_01.wav");
        assert(paths[2] == "sfx/doors/door_open_02.wav");
        assert(paths[3] == "sfx/ui/click.wav");
    }

    // randomPitch and randomVolume return values within range
    {
        const SoundEventDef* door = registry.find("door_open");
        for (int i = 0; i < 100; ++i) {
            float p = door->randomPitch();
            assert(p >= 0.95f - 0.001f && p <= 1.05f + 0.001f);
            float v = door->randomVolume();
            assert(v >= 0.9f - 0.001f && v <= 1.0f + 0.001f);
        }
    }

    // Constant volume/pitch returns exact value
    {
        const SoundEventDef* amb = registry.find("ambient_hum");
        float v = amb->randomVolume();
        assert(std::fabs(v - 0.6f) < 0.001f);
        // Default pitch range is {1,1}
        float p = amb->randomPitch();
        assert(std::fabs(p - 1.0f) < 0.001f);
    }

    std::printf("test_event_registry: all assertions passed\n");
    return 0;
}
