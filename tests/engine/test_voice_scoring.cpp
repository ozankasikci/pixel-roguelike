#include "engine/audio/mix/Voice.h"
#include "engine/audio/mix/BusGraph.h"

#include <cassert>
#include <cmath>

static bool approx(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

int main() {
    // --- Voice at listener position: audibility = 1.0 ---
    {
        engine::audio::BusGraph graph;
        engine::audio::Voice v;
        v.volume = 1.0f;
        v.priority = 128;
        v.busName = "SFX";
        v.is3D = true;
        v.position = glm::vec3(0.0f);
        v.refDistance = 1.0f;
        v.maxDistance = 50.0f;

        glm::vec3 listener(0.0f, 0.0f, 0.0f);
        float atten = v.computeDistanceAttenuation(listener);
        assert(approx(atten, 1.0f));

        float audibility = v.computeAudibility(listener, graph);
        assert(approx(audibility, 1.0f));
    }

    // --- Voice at distance 10, refDistance=1: attenuation = 0.1 ---
    {
        engine::audio::Voice v;
        v.is3D = true;
        v.position = glm::vec3(10.0f, 0.0f, 0.0f);
        v.refDistance = 1.0f;
        v.maxDistance = 50.0f;

        glm::vec3 listener(0.0f, 0.0f, 0.0f);
        float atten = v.computeDistanceAttenuation(listener);
        assert(approx(atten, 0.1f));
    }

    // --- Voice at or beyond maxDistance: attenuation = 0 ---
    {
        engine::audio::Voice v;
        v.is3D = true;
        v.position = glm::vec3(50.0f, 0.0f, 0.0f);
        v.refDistance = 1.0f;
        v.maxDistance = 50.0f;

        glm::vec3 listener(0.0f, 0.0f, 0.0f);
        float atten = v.computeDistanceAttenuation(listener);
        assert(approx(atten, 0.0f));
    }

    // --- Bus volume affects score ---
    {
        engine::audio::BusGraph graph;
        graph.setVolume("SFX", 0.5f);

        engine::audio::Voice v;
        v.volume = 1.0f;
        v.priority = 128;
        v.busName = "SFX";
        v.is3D = true;
        v.position = glm::vec3(0.0f);
        v.refDistance = 1.0f;
        v.maxDistance = 50.0f;

        glm::vec3 listener(0.0f, 0.0f, 0.0f);
        float audibility = v.computeAudibility(listener, graph);
        assert(approx(audibility, 0.5f));
    }

    // --- Muted bus => score 0 ---
    {
        engine::audio::BusGraph graph;
        graph.setMute("SFX", true);

        engine::audio::Voice v;
        v.volume = 1.0f;
        v.priority = 128;
        v.busName = "SFX";
        v.is3D = true;
        v.position = glm::vec3(0.0f);
        v.refDistance = 1.0f;
        v.maxDistance = 50.0f;

        glm::vec3 listener(0.0f, 0.0f, 0.0f);
        float audibility = v.computeAudibility(listener, graph);
        assert(approx(audibility, 0.0f));
    }

    // --- 2D voice ignores distance ---
    {
        engine::audio::BusGraph graph;

        engine::audio::Voice v;
        v.volume = 1.0f;
        v.priority = 128;
        v.busName = "SFX";
        v.is3D = false;
        v.position = glm::vec3(1000.0f, 0.0f, 0.0f);
        v.refDistance = 1.0f;
        v.maxDistance = 50.0f;

        glm::vec3 listener(0.0f, 0.0f, 0.0f);
        float atten = v.computeDistanceAttenuation(listener);
        assert(approx(atten, 1.0f));

        float audibility = v.computeAudibility(listener, graph);
        assert(approx(audibility, 1.0f));
    }

    // --- Higher priority boosts score ---
    {
        engine::audio::BusGraph graph;
        glm::vec3 listener(0.0f, 0.0f, 0.0f);

        engine::audio::Voice low;
        low.volume = 1.0f;
        low.priority = 64;
        low.busName = "SFX";
        low.is3D = true;
        low.position = glm::vec3(0.0f);
        low.refDistance = 1.0f;
        low.maxDistance = 50.0f;

        engine::audio::Voice high;
        high.volume = 1.0f;
        high.priority = 255;
        high.busName = "SFX";
        high.is3D = true;
        high.position = glm::vec3(0.0f);
        high.refDistance = 1.0f;
        high.maxDistance = 50.0f;

        float low_score = low.computeAudibility(listener, graph);
        float high_score = high.computeAudibility(listener, graph);

        // priority 64 => 64/128 = 0.5
        assert(approx(low_score, 0.5f));
        // priority 255 => 255/128 ~= 1.9921875
        assert(approx(high_score, 255.0f / 128.0f));
        assert(high_score > low_score);
    }

    return 0;
}
