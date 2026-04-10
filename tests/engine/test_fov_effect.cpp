#include "engine/camera/effects/FOVEffect.h"

#include <cassert>
#include <cmath>

namespace {

bool approxEqual(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a - b) < epsilon;
}

} // namespace

int main() {
    // --- At t=0 full FOV offset applied ---
    {
        FOVEffect effect(10.0f, 1.0f, EasingType::Linear);
        CameraState input;
        input.fov = 70.0f;
        const CameraState result = effect.apply(input);
        assert(approxEqual(result.fov, 80.0f));
    }

    // --- At t=1 no FOV offset (decayed) ---
    {
        FOVEffect effect(10.0f, 1.0f, EasingType::Linear);
        effect.update(1.0f);
        assert(effect.isFinished());

        CameraState input;
        input.fov = 70.0f;
        const CameraState result = effect.apply(input);
        assert(approxEqual(result.fov, 70.0f));
    }

    // --- Linear midpoint is half offset ---
    {
        FOVEffect effect(10.0f, 1.0f, EasingType::Linear);
        effect.update(0.5f);

        CameraState input;
        input.fov = 70.0f;
        const CameraState result = effect.apply(input);
        assert(approxEqual(result.fov, 75.0f));
    }

    // --- Negative delta narrows FOV ---
    {
        FOVEffect effect(-8.0f, 0.5f, EasingType::Linear);
        CameraState input;
        input.fov = 70.0f;
        const CameraState result = effect.apply(input);
        assert(approxEqual(result.fov, 62.0f));
    }

    // --- Finished after duration ---
    {
        FOVEffect effect(5.0f, 0.3f, EasingType::EaseOutCubic);
        assert(!effect.isFinished());
        effect.update(0.3f);
        assert(effect.isFinished());
    }

    return 0;
}
