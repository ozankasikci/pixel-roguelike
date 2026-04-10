#include "engine/audio/mix/ReverbManager.h"

#include <cassert>
#include <cmath>
#include <string>

static bool approx(float a, float b, float eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

int main() {
    // --- All 4 default presets exist ---
    {
        engine::audio::ReverbManager mgr;
        assert(mgr.hasPreset("None"));
        assert(mgr.hasPreset("Cell"));
        assert(mgr.hasPreset("Corridor"));
        assert(mgr.hasPreset("OpenArea"));
        assert(!mgr.hasPreset("NonExistent"));
    }

    // --- Default preset is "None" ---
    {
        engine::audio::ReverbManager mgr;
        assert(mgr.currentPreset() == "None");
    }

    // --- setPreset changes current ---
    {
        engine::audio::ReverbManager mgr;
        mgr.setPreset("Cell");
        assert(mgr.currentPreset() == "Cell");
        mgr.setPreset("Corridor");
        assert(mgr.currentPreset() == "Corridor");
    }

    // --- setPreset with unknown name does not change current ---
    {
        engine::audio::ReverbManager mgr;
        mgr.setPreset("Cell");
        mgr.setPreset("Bogus");
        assert(mgr.currentPreset() == "Cell");
    }

    // --- Cell decay < Corridor decay (verify preset values) ---
    {
        engine::audio::ReverbManager mgr;
        engine::audio::ReverbParams cell = mgr.presetParams("Cell");
        engine::audio::ReverbParams corridor = mgr.presetParams("Corridor");
        engine::audio::ReverbParams openArea = mgr.presetParams("OpenArea");
        engine::audio::ReverbParams none = mgr.presetParams("None");

        assert(cell.decayTime < corridor.decayTime);
        assert(corridor.decayTime < openArea.decayTime);
        assert(approx(none.gain, 0.0f));
        assert(approx(none.reflectionsGain, 0.0f));
        assert(approx(none.lateReverbGain, 0.0f));
    }

    // --- beginTransition + updateTransition: progress ramps from 0 to 1 ---
    {
        engine::audio::ReverbManager mgr;
        mgr.setPreset("Cell");
        mgr.beginTransition("Corridor", 1.0f);

        assert(mgr.isTransitioning());
        assert(approx(mgr.transitionProgress(), 0.0f));
        assert(mgr.targetPreset() == "Corridor");

        mgr.updateTransition(0.25f);
        assert(mgr.isTransitioning());
        assert(approx(mgr.transitionProgress(), 0.25f));

        mgr.updateTransition(0.25f);
        assert(approx(mgr.transitionProgress(), 0.5f));

        mgr.updateTransition(0.5f);
        assert(approx(mgr.transitionProgress(), 1.0f));
        assert(!mgr.isTransitioning());
    }

    // --- After transition completes, currentPreset is the target ---
    {
        engine::audio::ReverbManager mgr;
        mgr.setPreset("None");
        mgr.beginTransition("OpenArea", 0.5f);

        mgr.updateTransition(0.5f);
        assert(!mgr.isTransitioning());
        assert(mgr.currentPreset() == "OpenArea");
        assert(mgr.targetPreset().empty());
    }

    // --- currentParams returns interpolated values during transition ---
    {
        engine::audio::ReverbManager mgr;
        mgr.setPreset("Cell");
        engine::audio::ReverbParams cellParams = mgr.presetParams("Cell");
        engine::audio::ReverbParams corridorParams = mgr.presetParams("Corridor");

        mgr.beginTransition("Corridor", 1.0f);
        mgr.updateTransition(0.5f);

        engine::audio::ReverbParams blended = mgr.currentParams();
        float expectedDecay = 0.5f * cellParams.decayTime + 0.5f * corridorParams.decayTime;
        assert(approx(blended.decayTime, expectedDecay));

        float expectedDensity = 0.5f * cellParams.density + 0.5f * corridorParams.density;
        assert(approx(blended.density, expectedDensity));
    }

    // --- currentParams returns source params when not transitioning ---
    {
        engine::audio::ReverbManager mgr;
        mgr.setPreset("Cell");
        engine::audio::ReverbParams params = mgr.currentParams();
        engine::audio::ReverbParams cellParams = mgr.presetParams("Cell");
        assert(approx(params.decayTime, cellParams.decayTime));
        assert(approx(params.density, cellParams.density));
        assert(approx(params.gain, cellParams.gain));
    }

    // --- addPreset and use in transition ---
    {
        engine::audio::ReverbManager mgr;
        engine::audio::ReverbParams custom{};
        custom.decayTime = 5.0f;
        custom.gain = 0.8f;
        mgr.addPreset("Custom", custom);
        assert(mgr.hasPreset("Custom"));

        mgr.setPreset("Custom");
        assert(mgr.currentPreset() == "Custom");
        engine::audio::ReverbParams p = mgr.currentParams();
        assert(approx(p.decayTime, 5.0f));
    }

    // --- Zero or negative duration in beginTransition does immediate switch ---
    {
        engine::audio::ReverbManager mgr;
        mgr.setPreset("Cell");
        mgr.beginTransition("Corridor", 0.0f);
        assert(!mgr.isTransitioning());
        assert(mgr.currentPreset() == "Corridor");
    }

    // --- Overshoot transition clamps at 1.0 ---
    {
        engine::audio::ReverbManager mgr;
        mgr.setPreset("Cell");
        mgr.beginTransition("OpenArea", 0.5f);
        mgr.updateTransition(10.0f); // way past duration
        assert(!mgr.isTransitioning());
        assert(mgr.currentPreset() == "OpenArea");
        assert(approx(mgr.transitionProgress(), 1.0f));
    }

    return 0;
}
