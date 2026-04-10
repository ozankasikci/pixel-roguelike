#include "engine/camera/effects/ShakeEffect.h"

#include <cassert>
#include <cmath>

namespace {

bool approxEqual(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) < epsilon;
}

} // namespace

int main() {
    // --- Zero trauma produces no offset ---
    {
        ShakeEffect shake;
        CameraState input;
        input.position = glm::vec3(1.0f, 2.0f, 3.0f);
        input.yaw = 45.0f;

        const CameraState result = shake.apply(input);
        assert(approxEqual(result.position.x, 1.0f));
        assert(approxEqual(result.position.y, 2.0f));
        assert(approxEqual(result.yaw, 45.0f));
    }

    // --- Trauma produces offsets ---
    {
        ShakeEffect shake;
        shake.addTrauma(0.8f);
        shake.update(0.1f);

        CameraState input;
        input.position = glm::vec3(0.0f);
        const CameraState result = shake.apply(input);

        const bool positionChanged = result.position.x != 0.0f || result.position.y != 0.0f;
        const bool rotationChanged = result.yaw != 0.0f || result.pitch != 0.0f;
        assert(positionChanged || rotationChanged);
    }

    // --- Trauma decays over time ---
    {
        ShakeEffect shake;
        shake.addTrauma(1.0f);
        assert(approxEqual(shake.getTrauma(), 1.0f));

        shake.update(0.5f);
        assert(shake.getTrauma() < 1.0f);
        assert(shake.getTrauma() > 0.0f);
    }

    // --- Trauma clamps to [0, 1] ---
    {
        ShakeEffect shake;
        shake.addTrauma(0.8f);
        shake.addTrauma(0.8f);
        assert(approxEqual(shake.getTrauma(), 1.0f));
    }

    // --- Never finishes (persistent) ---
    {
        ShakeEffect shake;
        shake.update(100.0f);
        assert(!shake.isFinished());
    }

    return 0;
}
