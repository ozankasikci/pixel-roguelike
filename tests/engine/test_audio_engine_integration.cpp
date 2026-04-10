// Integration test: AudioEngine init-play-update-shutdown cycle.
// Requires OpenAL device -- skips gracefully if unavailable.

#include <cassert>
#include <cstdio>
#include <string>

#include <glm/vec3.hpp>

#include "engine/audio/AudioEngine.h"

#ifndef TEST_AUDIO_BASE_PATH
#error "TEST_AUDIO_BASE_PATH must be defined"
#endif

int main() {
    using namespace engine::audio;

    // Configure engine to use the project's audio assets.
    AudioEngineConfig config;
    config.audioBasePath = TEST_AUDIO_BASE_PATH;
    config.eventDefPath = std::string(TEST_AUDIO_BASE_PATH) + "/sound_events.json";
    config.voicePoolSize = 8;
    config.reverbSlots = 1;

    AudioEngine engine(config);

    // Init -- may fail if no audio device is available (e.g. headless CI).
    if (!engine.init()) {
        std::printf("test_audio_engine_integration: SKIPPED (no audio device)\n");
        return 0;
    }

    // -- Event registry loaded correctly ---------------------------------
    assert(engine.eventRegistry().find("door_open") != nullptr);
    assert(engine.eventRegistry().find("door_close") != nullptr);
    assert(engine.eventRegistry().find("nonexistent_event") == nullptr);

    // -- Set listener position -------------------------------------------
    engine.setListenerTransform(glm::vec3(0.0f, 1.7f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
                                glm::vec3(0.0f, 1.0f, 0.0f));

    // -- Play a 3D sound -------------------------------------------------
    auto handle = engine.play("door_open", glm::vec3(2.0f, 0.0f, -3.0f));
    assert(handle.valid());

    // -- Run several update frames ---------------------------------------
    for (int i = 0; i < 10; ++i) {
        engine.update(0.016f);
    }

    // -- Bus volume control ----------------------------------------------
    engine.setBusVolume("SFX", 0.5f);
    assert(engine.busGraph().busVolume("SFX") == 0.5f);
    engine.update(0.016f);

    // -- Reverb transition (SmallRoom is a built-in preset) --------------
    engine.setReverbPreset("SmallRoom", 0.3f);
    for (int i = 0; i < 30; ++i) {
        engine.update(0.016f);
    }

    // -- Update voice position -------------------------------------------
    engine.updateVoicePosition(handle, glm::vec3(5.0f, 0.0f, -1.0f));
    engine.update(0.016f);

    // -- Stop voice ------------------------------------------------------
    engine.stop(handle);
    engine.update(0.016f);

    // -- Play a second event to exercise variety -------------------------
    auto handle2 = engine.play("door_close", glm::vec3(-1.0f, 0.0f, 2.0f));
    assert(handle2.valid());
    engine.update(0.016f);
    engine.stop(handle2);
    engine.update(0.016f);

    // -- Shutdown --------------------------------------------------------
    engine.shutdown();

    std::printf("test_audio_engine_integration: all assertions passed\n");
    return 0;
}
