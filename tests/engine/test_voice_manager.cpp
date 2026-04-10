#include "engine/audio/mix/VoiceManager.h"
#include "engine/audio/mix/BusGraph.h"

#include <cassert>
#include <cmath>

static bool approx(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

int main() {
    // --- Spawn a voice, verify it's Playing with correct bufferHandle ---
    {
        engine::audio::VoiceManager mgr(4);
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f);

        auto handle = mgr.spawn("footstep", glm::vec3(0.0f), 1.0f, 1.0f, 128, "SFX", true, false,
                                1.0f, 50.0f, 42);

        assert(handle.valid());

        const engine::audio::Voice* v = mgr.findVoice(handle);
        assert(v != nullptr);
        assert(v->bufferHandle == 42);
        assert(v->eventName == "footstep");
        assert(v->state == engine::audio::VoiceState::Playing);
        assert(approx(v->volume, 1.0f));
        assert(approx(v->pitch, 1.0f));

        // After update, should still be Playing with sourceIndex assigned
        mgr.update(listener, graph, 0.016f);
        v = mgr.findVoice(handle);
        assert(v != nullptr);
        assert(v->state == engine::audio::VoiceState::Playing);
        assert(v->sourceIndex >= 0);
    }

    // --- Fill pool (poolSize=4), spawn 5th; after update, farthest goes Virtual ---
    {
        engine::audio::VoiceManager mgr(4);
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f);

        // Spawn 4 voices at increasing distances
        auto h1 = mgr.spawn("sfx", glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 1.0f, 128, "SFX", true,
                             false, 1.0f, 100.0f, 1);
        auto h2 = mgr.spawn("sfx", glm::vec3(5.0f, 0.0f, 0.0f), 1.0f, 1.0f, 128, "SFX", true,
                             false, 1.0f, 100.0f, 2);
        auto h3 = mgr.spawn("sfx", glm::vec3(10.0f, 0.0f, 0.0f), 1.0f, 1.0f, 128, "SFX", true,
                             false, 1.0f, 100.0f, 3);
        auto h4 = mgr.spawn("sfx", glm::vec3(20.0f, 0.0f, 0.0f), 1.0f, 1.0f, 128, "SFX", true,
                             false, 1.0f, 100.0f, 4);

        // Spawn 5th voice even farther
        auto h5 = mgr.spawn("sfx", glm::vec3(40.0f, 0.0f, 0.0f), 1.0f, 1.0f, 128, "SFX", true,
                             false, 1.0f, 100.0f, 5);

        mgr.update(listener, graph, 0.016f);

        // The farthest voice (h5 at 40 units) should be Virtual
        const engine::audio::Voice* v5 = mgr.findVoice(h5);
        assert(v5 != nullptr);
        assert(v5->state == engine::audio::VoiceState::Virtual);
        assert(v5->sourceIndex == -1);

        // The closest 4 should be Playing with source indices
        const engine::audio::Voice* v1 = mgr.findVoice(h1);
        assert(v1 != nullptr);
        assert(v1->state == engine::audio::VoiceState::Playing);
        assert(v1->sourceIndex >= 0);

        const engine::audio::Voice* v4 = mgr.findVoice(h4);
        assert(v4 != nullptr);
        assert(v4->state == engine::audio::VoiceState::Playing);
        assert(v4->sourceIndex >= 0);
    }

    // --- Stop a voice, verify removed after update ---
    {
        engine::audio::VoiceManager mgr(4);
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f);

        auto h1 = mgr.spawn("door_open", glm::vec3(0.0f), 1.0f, 1.0f, 128, "Doors", true, false,
                             1.0f, 50.0f, 10);
        auto h2 = mgr.spawn("door_close", glm::vec3(0.0f), 1.0f, 1.0f, 128, "Doors", true, false,
                             1.0f, 50.0f, 11);

        assert(mgr.voices().size() == 2);

        // Stop immediately (fadeTime = 0)
        mgr.stop(h1, 0.0f);

        const engine::audio::Voice* v1 = mgr.findVoice(h1);
        assert(v1 != nullptr);
        assert(v1->state == engine::audio::VoiceState::Stopped);

        // After update, stopped voice is removed
        mgr.update(listener, graph, 0.016f);
        assert(mgr.findVoice(h1) == nullptr);
        assert(mgr.voices().size() == 1);

        // The other voice is still alive
        assert(mgr.findVoice(h2) != nullptr);
    }

    // --- Stop with fadeTime sets Stopping state ---
    {
        engine::audio::VoiceManager mgr(4);
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f);

        auto handle = mgr.spawn("ambience", glm::vec3(0.0f), 1.0f, 1.0f, 128, "Ambience", false,
                                true, 1.0f, 50.0f, 20);

        mgr.stop(handle, 0.5f);

        const engine::audio::Voice* v = mgr.findVoice(handle);
        assert(v != nullptr);
        assert(v->state == engine::audio::VoiceState::Stopping);
    }

    // --- activeVoiceCount tracks correctly ---
    {
        engine::audio::VoiceManager mgr(4);
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f);

        assert(mgr.activeVoiceCount() == 0);

        mgr.spawn("sfx1", glm::vec3(0.0f), 1.0f, 1.0f, 128, "SFX", true, false, 1.0f, 50.0f, 1);
        mgr.spawn("sfx2", glm::vec3(0.0f), 1.0f, 1.0f, 128, "SFX", true, false, 1.0f, 50.0f, 2);

        // Before update, both are Playing (initial state from spawn)
        assert(mgr.activeVoiceCount() == 2);

        mgr.update(listener, graph, 0.016f);
        assert(mgr.activeVoiceCount() == 2);
    }

    // --- Invalid handle returns nullptr ---
    {
        engine::audio::VoiceManager mgr(4);

        engine::audio::VoiceHandle invalid(999, 0);
        assert(mgr.findVoice(invalid) == nullptr);

        engine::audio::VoiceHandle zero;
        assert(mgr.findVoice(zero) == nullptr);
    }

    // --- countByEvent ---
    {
        engine::audio::VoiceManager mgr(8);
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f);

        mgr.spawn("footstep", glm::vec3(0.0f), 1.0f, 1.0f, 128, "Footsteps", true, false, 1.0f,
                   50.0f, 1);
        mgr.spawn("footstep", glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 1.0f, 128, "Footsteps", true,
                   false, 1.0f, 50.0f, 2);
        mgr.spawn("door_open", glm::vec3(0.0f), 1.0f, 1.0f, 128, "Doors", true, false, 1.0f,
                   50.0f, 3);

        assert(mgr.countByEvent("footstep") == 2);
        assert(mgr.countByEvent("door_open") == 1);
        assert(mgr.countByEvent("nonexistent") == 0);
    }

    // --- stopAllByEvent ---
    {
        engine::audio::VoiceManager mgr(8);
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f);

        mgr.spawn("footstep", glm::vec3(0.0f), 1.0f, 1.0f, 128, "Footsteps", true, false, 1.0f,
                   50.0f, 1);
        mgr.spawn("footstep", glm::vec3(1.0f, 0.0f, 0.0f), 1.0f, 1.0f, 128, "Footsteps", true,
                   false, 1.0f, 50.0f, 2);
        mgr.spawn("door_open", glm::vec3(0.0f), 1.0f, 1.0f, 128, "Doors", true, false, 1.0f,
                   50.0f, 3);

        mgr.stopAllByEvent("footstep");
        assert(mgr.countByEvent("footstep") == 0);
        assert(mgr.countByEvent("door_open") == 1);

        // After update, the stopped footsteps are removed
        mgr.update(listener, graph, 0.016f);
        assert(mgr.voices().size() == 1);
    }

    // --- updatePosition changes voice position ---
    {
        engine::audio::VoiceManager mgr(4);

        auto handle = mgr.spawn("sfx", glm::vec3(0.0f), 1.0f, 1.0f, 128, "SFX", true, false,
                                1.0f, 50.0f, 1);

        mgr.updatePosition(handle, glm::vec3(10.0f, 5.0f, 3.0f));

        const engine::audio::Voice* v = mgr.findVoice(handle);
        assert(v != nullptr);
        assert(approx(v->position.x, 10.0f));
        assert(approx(v->position.y, 5.0f));
        assert(approx(v->position.z, 3.0f));
    }

    // --- Elapsed time advances ---
    {
        engine::audio::VoiceManager mgr(4);
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f);

        auto handle = mgr.spawn("sfx", glm::vec3(0.0f), 1.0f, 1.0f, 128, "SFX", true, false,
                                1.0f, 50.0f, 1);

        mgr.update(listener, graph, 0.1f);

        const engine::audio::Voice* v = mgr.findVoice(handle);
        assert(v != nullptr);
        assert(approx(v->elapsedTime, 0.1f));

        mgr.update(listener, graph, 0.05f);
        v = mgr.findVoice(handle);
        assert(approx(v->elapsedTime, 0.15f));
    }

    return 0;
}
