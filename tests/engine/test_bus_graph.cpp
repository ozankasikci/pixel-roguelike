#include "engine/audio/mix/BusGraph.h"

#include <cassert>
#include <cmath>

static bool approx(float a, float b, float eps = 0.0001f) {
    return std::fabs(a - b) < eps;
}

int main() {
    // --- Default hierarchy exists ---
    {
        audio::BusGraph graph;
        assert(graph.busExists("Master"));
        assert(graph.busExists("Music"));
        assert(graph.busExists("SFX"));
        assert(graph.busExists("Ambience"));
        assert(graph.busExists("UI"));
        assert(graph.busExists("Footsteps"));
        assert(graph.busExists("Doors"));
        assert(graph.busExists("Environment"));
    }

    // --- Default volumes are 1.0 ---
    {
        audio::BusGraph graph;
        assert(approx(graph.busVolume("Master"), 1.0f));
        assert(approx(graph.busVolume("Music"), 1.0f));
        assert(approx(graph.busVolume("SFX"), 1.0f));
        assert(approx(graph.busVolume("Footsteps"), 1.0f));
        assert(approx(graph.busVolume("Ambience"), 1.0f));
        assert(approx(graph.busVolume("UI"), 1.0f));
    }

    // --- effectiveVolume walks the chain (Footsteps -> SFX -> Master) ---
    {
        audio::BusGraph graph;
        // All at 1.0 — effective should be 1.0
        assert(approx(graph.effectiveVolume("Footsteps"), 1.0f));
        assert(approx(graph.effectiveVolume("SFX"), 1.0f));
        assert(approx(graph.effectiveVolume("Master"), 1.0f));
    }

    // --- Setting parent volume affects children's effective volume ---
    {
        audio::BusGraph graph;
        graph.setVolume("Master", 0.5f);
        assert(approx(graph.effectiveVolume("Master"), 0.5f));
        assert(approx(graph.effectiveVolume("Music"), 0.5f));
        assert(approx(graph.effectiveVolume("SFX"), 0.5f));
        assert(approx(graph.effectiveVolume("Footsteps"), 0.5f));
    }

    // --- Nested multiplication (SFX=0.8, Master=0.5 => Footsteps effective=0.4) ---
    {
        audio::BusGraph graph;
        graph.setVolume("SFX", 0.8f);
        graph.setVolume("Master", 0.5f);
        assert(approx(graph.effectiveVolume("Footsteps"), 0.4f));
        assert(approx(graph.effectiveVolume("Doors"), 0.4f));
        assert(approx(graph.effectiveVolume("Environment"), 0.4f));
        // Music is under Master but not SFX
        assert(approx(graph.effectiveVolume("Music"), 0.5f));
    }

    // --- Mute propagates (muting SFX zeros Footsteps/Doors but not Music) ---
    {
        audio::BusGraph graph;
        graph.setMute("SFX", true);
        assert(approx(graph.effectiveVolume("SFX"), 0.0f));
        assert(approx(graph.effectiveVolume("Footsteps"), 0.0f));
        assert(approx(graph.effectiveVolume("Doors"), 0.0f));
        assert(approx(graph.effectiveVolume("Environment"), 0.0f));
        // Music is not under SFX
        assert(approx(graph.effectiveVolume("Music"), 1.0f));
        assert(approx(graph.effectiveVolume("Ambience"), 1.0f));
    }

    // --- Volume clamping (1.5 -> 1.0, -0.5 -> 0.0) ---
    {
        audio::BusGraph graph;
        graph.setVolume("Master", 1.5f);
        assert(approx(graph.busVolume("Master"), 1.0f));
        graph.setVolume("Master", -0.5f);
        assert(approx(graph.busVolume("Master"), 0.0f));
    }

    // --- Unknown bus returns 0 ---
    {
        audio::BusGraph graph;
        assert(approx(graph.busVolume("NonExistent"), 0.0f));
        assert(approx(graph.effectiveVolume("NonExistent"), 0.0f));
        assert(!graph.busExists("NonExistent"));
    }

    return 0;
}
