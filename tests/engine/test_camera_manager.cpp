#include "engine/camera/CameraManager.h"
#include "engine/camera/CameraMath.h"

#include <cassert>
#include <cmath>

namespace {

bool approxEqual(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) < epsilon;
}

bool approxEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.001f) {
    return approxEqual(a.x, b.x, epsilon)
        && approxEqual(a.y, b.y, epsilon)
        && approxEqual(a.z, b.z, epsilon);
}

} // namespace

int main() {
    // --- Base state passes through with no effects ---
    {
        CameraManager manager;
        manager.setBaseState(glm::vec3(1.0f, 2.0f, 3.0f), -90.0f, 15.0f);
        manager.setProjection(70.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        manager.update(0.016f);

        const auto& state = manager.getState();
        assert(approxEqualVec3(state.position, glm::vec3(1.0f, 2.0f, 3.0f)));
        assert(approxEqual(state.yaw, -90.0f));
        assert(approxEqual(state.pitch, 15.0f));
        assert(approxEqual(state.fov, 70.0f));
        assert(state.viewMatrix != glm::mat4(1.0f));
        assert(state.projectionMatrix != glm::mat4(1.0f));
    }

    // --- getBaseState returns input state ---
    {
        CameraManager manager;
        manager.setBaseState(glm::vec3(5.0f, 0.0f, 0.0f), 45.0f, -10.0f);
        manager.setProjection(90.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        manager.update(0.016f);

        const auto& base = manager.getBaseState();
        assert(approxEqualVec3(base.position, glm::vec3(5.0f, 0.0f, 0.0f)));
        assert(approxEqual(base.yaw, 45.0f));
        assert(approxEqual(base.pitch, -10.0f));
    }

    // --- No active effects initially ---
    {
        CameraManager manager;
        assert(!manager.hasActiveEffects());
        assert(!manager.isTransitioning());
    }

    // --- clearEffects removes all effects ---
    {
        CameraManager manager;
        manager.shake(0.5f);
        assert(manager.hasActiveEffects());
        manager.clearEffects();
        assert(!manager.hasActiveEffects());
    }

    return 0;
}
