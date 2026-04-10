# Camera Module Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Decouple the camera from the player entity into a standalone engine module with a composable effect pipeline (transitions, shake, FOV punch), replacing all direct CameraComponent/PrimaryCameraTag queries with a single authoritative CameraState output.

**Architecture:** CameraManager lives in `src/engine/camera/` as engine_camera CMake target. Game code feeds base state each frame via `setBaseState()`. Effects (TransitionEffect, ShakeEffect, FOVEffect) modify state in a composable pipeline. All consumers read `getState()` for the final camera output. CameraManager is owned by RuntimeGameSession and stored in the ECS registry context as `CameraManager*` for system access.

**Tech Stack:** C++20, GLM (matrices, vectors, Perlin noise), EnTT (ECS registry context), CMake (FetchContent)

**Spec:** `docs/superpowers/specs/2026-04-10-camera-module-design.md`

---

### Task 1: Core Types, CameraMath, and CMake Target

**Files:**
- Create: `src/engine/camera/CameraState.h`
- Create: `src/engine/camera/CameraEffect.h`
- Create: `src/engine/camera/CameraMath.h`
- Create: `src/engine/camera/CameraMath.cpp`
- Modify: `src/engine/CMakeLists.txt`
- Test: `tests/engine/test_camera_math.cpp`
- Modify: `tests/engine/CMakeLists.txt`

- [ ] **Step 1: Create `src/engine/camera/CameraState.h`**

```cpp
#pragma once

#include <glm/glm.hpp>

enum class EasingType {
    Linear,
    EaseOutQuad,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInOutQuad
};

struct CameraState {
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};
```

- [ ] **Step 2: Create `src/engine/camera/CameraEffect.h`**

```cpp
#pragma once

#include "engine/camera/CameraState.h"

class CameraEffect {
public:
    virtual ~CameraEffect() = default;
    virtual void update(float deltaTime) = 0;
    virtual CameraState apply(const CameraState& input) const = 0;
    virtual bool isFinished() const = 0;
};
```

- [ ] **Step 3: Create `src/engine/camera/CameraMath.h`**

```cpp
#pragma once

#include "engine/camera/CameraState.h"

#include <glm/glm.hpp>

float evaluateEasing(EasingType type, float t);

glm::vec3 buildCameraForward(float yawDegrees, float pitchDegrees);

void rebuildCameraVectors(CameraState& state);

void rebuildCameraMatrices(CameraState& state, float aspectRatio);
```

- [ ] **Step 4: Create `src/engine/camera/CameraMath.cpp`**

```cpp
#include "engine/camera/CameraMath.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

float evaluateEasing(EasingType type, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (type) {
        case EasingType::Linear:
            return t;
        case EasingType::EaseOutQuad:
            return t * (2.0f - t);
        case EasingType::EaseOutCubic:
            return 1.0f - std::pow(1.0f - t, 3.0f);
        case EasingType::EaseInOutCubic:
            return t < 0.5f
                ? 4.0f * t * t * t
                : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
        case EasingType::EaseInOutQuad:
            return t < 0.5f
                ? 2.0f * t * t
                : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    }
    return t;
}

glm::vec3 buildCameraForward(float yawDegrees, float pitchDegrees) {
    const float yawRad = glm::radians(yawDegrees);
    const float pitchRad = glm::radians(pitchDegrees);
    glm::vec3 forward;
    forward.x = std::cos(yawRad) * std::cos(pitchRad);
    forward.y = std::sin(pitchRad);
    forward.z = std::sin(yawRad) * std::cos(pitchRad);
    if (glm::dot(forward, forward) <= 0.0001f) {
        return glm::vec3(0.0f, 0.0f, -1.0f);
    }
    return glm::normalize(forward);
}

void rebuildCameraVectors(CameraState& state) {
    const glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    state.forward = buildCameraForward(state.yaw, state.pitch);
    state.right = glm::normalize(glm::cross(state.forward, worldUp));
    state.up = glm::normalize(glm::cross(state.right, state.forward));
}

void rebuildCameraMatrices(CameraState& state, float aspectRatio) {
    rebuildCameraVectors(state);
    state.viewMatrix = glm::lookAt(state.position,
                                   state.position + state.forward,
                                   glm::vec3(0.0f, 1.0f, 0.0f));
    state.projectionMatrix = glm::perspective(glm::radians(state.fov),
                                              std::max(aspectRatio, 0.0001f),
                                              state.nearPlane,
                                              state.farPlane);
}
```

- [ ] **Step 5: Write test `tests/engine/test_camera_math.cpp`**

```cpp
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
    // --- Easing: Linear ---
    {
        assert(approxEqual(evaluateEasing(EasingType::Linear, 0.0f), 0.0f));
        assert(approxEqual(evaluateEasing(EasingType::Linear, 0.5f), 0.5f));
        assert(approxEqual(evaluateEasing(EasingType::Linear, 1.0f), 1.0f));
    }

    // --- Easing: boundaries always 0 and 1 ---
    {
        const EasingType types[] = {
            EasingType::Linear, EasingType::EaseOutQuad, EasingType::EaseOutCubic,
            EasingType::EaseInOutCubic, EasingType::EaseInOutQuad
        };
        for (auto type : types) {
            assert(approxEqual(evaluateEasing(type, 0.0f), 0.0f));
            assert(approxEqual(evaluateEasing(type, 1.0f), 1.0f));
        }
    }

    // --- Easing: EaseOutCubic is fast at start, slow at end ---
    {
        const float mid = evaluateEasing(EasingType::EaseOutCubic, 0.5f);
        assert(mid > 0.5f); // Should be ahead of linear at midpoint
    }

    // --- Easing: clamped input ---
    {
        assert(approxEqual(evaluateEasing(EasingType::Linear, -0.5f), 0.0f));
        assert(approxEqual(evaluateEasing(EasingType::Linear, 1.5f), 1.0f));
    }

    // --- buildCameraForward: looking down -Z at yaw=-90, pitch=0 ---
    {
        const glm::vec3 forward = buildCameraForward(-90.0f, 0.0f);
        assert(approxEqualVec3(forward, glm::vec3(0.0f, 0.0f, -1.0f)));
    }

    // --- buildCameraForward: looking up ---
    {
        const glm::vec3 forward = buildCameraForward(-90.0f, 45.0f);
        assert(forward.y > 0.0f);
        assert(approxEqual(glm::length(forward), 1.0f));
    }

    // --- rebuildCameraMatrices: produces valid matrices ---
    {
        CameraState state;
        state.position = glm::vec3(1.0f, 2.0f, 3.0f);
        state.yaw = -90.0f;
        state.pitch = 0.0f;
        state.fov = 70.0f;
        rebuildCameraMatrices(state, 16.0f / 9.0f);

        assert(approxEqualVec3(state.forward, glm::vec3(0.0f, 0.0f, -1.0f)));
        assert(state.viewMatrix != glm::mat4(1.0f));
        assert(state.projectionMatrix != glm::mat4(1.0f));
    }

    return 0;
}
```

- [ ] **Step 6: Add engine_camera target to `src/engine/CMakeLists.txt`**

Insert after the existing `engine_audio` block (after line 78):

```cmake
add_library(engine_camera STATIC
    camera/CameraMath.cpp
)
target_include_directories(engine_camera PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_camera PUBLIC engine_core glm::glm)
```

- [ ] **Step 7: Register test in `tests/engine/CMakeLists.txt`**

Append:

```cmake
pixel_roguelike_add_test(test_camera_math
    SOURCES test_camera_math.cpp
    LIBRARIES engine_camera
    LABELS engine
)
```

- [ ] **Step 8: Build and run the test**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_camera_math && ctest --test-dir build -R camera_math -V`
Expected: PASS

- [ ] **Step 9: Commit**

```bash
git add src/engine/camera/ tests/engine/test_camera_math.cpp src/engine/CMakeLists.txt tests/engine/CMakeLists.txt
git commit -m "Add engine camera module: core types and CameraMath with tests"
```

---

### Task 2: CameraManager

**Files:**
- Create: `src/engine/camera/CameraManager.h`
- Create: `src/engine/camera/CameraManager.cpp`
- Modify: `src/engine/CMakeLists.txt` (add CameraManager.cpp to engine_camera)
- Test: `tests/engine/test_camera_manager.cpp`
- Modify: `tests/engine/CMakeLists.txt`

- [ ] **Step 1: Create `src/engine/camera/CameraManager.h`**

```cpp
#pragma once

#include "engine/camera/CameraEffect.h"
#include "engine/camera/CameraState.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

class ShakeEffect;

class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    CameraManager(CameraManager&&) noexcept;
    CameraManager& operator=(CameraManager&&) noexcept;

    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    void setBaseState(const glm::vec3& position, float yaw, float pitch);
    void setProjection(float fov, float aspectRatio, float nearPlane, float farPlane);

    template<typename T, typename... Args>
    T& addEffect(Args&&... args) {
        auto effect = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *effect;
        effects_.push_back(std::move(effect));
        return ref;
    }

    void removeEffect(CameraEffect* effect);
    void clearEffects();

    void transitionTo(const glm::vec3& position, float yaw, float pitch,
                      float duration, EasingType easing = EasingType::EaseOutCubic);
    void shake(float trauma);
    void punchFOV(float deltaFov, float duration,
                  EasingType easing = EasingType::EaseOutCubic);

    void update(float deltaTime);
    const CameraState& getState() const;
    const CameraState& getBaseState() const;

    bool isTransitioning() const;
    bool hasActiveEffects() const;

private:
    CameraState baseState_;
    CameraState finalState_;
    float aspectRatio_ = 16.0f / 9.0f;
    std::vector<std::unique_ptr<CameraEffect>> effects_;

    void pruneFinishedEffects();
};
```

- [ ] **Step 2: Create `src/engine/camera/CameraManager.cpp`**

```cpp
#include "engine/camera/CameraManager.h"

#include "engine/camera/CameraMath.h"
#include "engine/camera/effects/ShakeEffect.h"
#include "engine/camera/effects/TransitionEffect.h"
#include "engine/camera/effects/FOVEffect.h"

#include <algorithm>

CameraManager::CameraManager() {
    rebuildCameraMatrices(baseState_, aspectRatio_);
    finalState_ = baseState_;
}

CameraManager::~CameraManager() = default;

CameraManager::CameraManager(CameraManager&&) noexcept = default;
CameraManager& CameraManager::operator=(CameraManager&&) noexcept = default;

void CameraManager::setBaseState(const glm::vec3& position, float yaw, float pitch) {
    baseState_.position = position;
    baseState_.yaw = yaw;
    baseState_.pitch = pitch;
}

void CameraManager::setProjection(float fov, float aspectRatio, float nearPlane, float farPlane) {
    baseState_.fov = fov;
    baseState_.nearPlane = nearPlane;
    baseState_.farPlane = farPlane;
    aspectRatio_ = aspectRatio;
}

void CameraManager::removeEffect(CameraEffect* effect) {
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [effect](const auto& ptr) { return ptr.get() == effect; }),
        effects_.end());
}

void CameraManager::clearEffects() {
    effects_.clear();
}

void CameraManager::transitionTo(const glm::vec3& position, float yaw, float pitch,
                                 float duration, EasingType easing) {
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [](const auto& ptr) { return dynamic_cast<TransitionEffect*>(ptr.get()) != nullptr; }),
        effects_.end());

    addEffect<TransitionEffect>(finalState_, position, yaw, pitch, duration, easing);
}

void CameraManager::shake(float trauma) {
    for (auto& effect : effects_) {
        if (auto* shakeEffect = dynamic_cast<ShakeEffect*>(effect.get())) {
            shakeEffect->addTrauma(trauma);
            return;
        }
    }
    auto& effect = addEffect<ShakeEffect>();
    effect.addTrauma(trauma);
}

void CameraManager::punchFOV(float deltaFov, float duration, EasingType easing) {
    addEffect<FOVEffect>(deltaFov, duration, easing);
}

void CameraManager::update(float deltaTime) {
    rebuildCameraMatrices(baseState_, aspectRatio_);

    for (auto& effect : effects_) {
        effect->update(deltaTime);
    }

    CameraState current = baseState_;
    for (auto& effect : effects_) {
        current = effect->apply(current);
    }

    rebuildCameraMatrices(current, aspectRatio_);
    finalState_ = current;

    pruneFinishedEffects();
}

const CameraState& CameraManager::getState() const {
    return finalState_;
}

const CameraState& CameraManager::getBaseState() const {
    return baseState_;
}

bool CameraManager::isTransitioning() const {
    for (const auto& effect : effects_) {
        if (dynamic_cast<const TransitionEffect*>(effect.get()) != nullptr) {
            return true;
        }
    }
    return false;
}

bool CameraManager::hasActiveEffects() const {
    return !effects_.empty();
}

void CameraManager::pruneFinishedEffects() {
    effects_.erase(
        std::remove_if(effects_.begin(), effects_.end(),
                       [](const auto& ptr) { return ptr->isFinished(); }),
        effects_.end());
}
```

**Build order note:** CameraManager.cpp includes the effect headers from Tasks 3-5. Tasks 1-5 should be implemented sequentially and the first full build attempted after Task 5 completes. The test in this task (test_camera_manager) exercises convenience methods (`shake`, `clearEffects`) that depend on ShakeEffect, so it should only be built and run after Task 5.

- [ ] **Step 3: Add CameraManager.cpp to `src/engine/CMakeLists.txt`**

Change the engine_camera target source list:

```cmake
add_library(engine_camera STATIC
    camera/CameraMath.cpp
    camera/CameraManager.cpp
)
```

- [ ] **Step 4: Write test `tests/engine/test_camera_manager.cpp`**

```cpp
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
```

- [ ] **Step 5: Register test in `tests/engine/CMakeLists.txt`**

Append:

```cmake
pixel_roguelike_add_test(test_camera_manager
    SOURCES test_camera_manager.cpp
    LIBRARIES engine_camera
    LABELS engine
)
```

- [ ] **Step 6: Build and run (after Tasks 3-5 create the effect headers)**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_camera_manager && ctest --test-dir build -R camera_manager -V`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/engine/camera/CameraManager.h src/engine/camera/CameraManager.cpp src/engine/CMakeLists.txt tests/engine/test_camera_manager.cpp tests/engine/CMakeLists.txt
git commit -m "Add CameraManager with base state pipeline and effect management"
```

---

### Task 3: TransitionEffect

**Files:**
- Create: `src/engine/camera/effects/TransitionEffect.h`
- Create: `src/engine/camera/effects/TransitionEffect.cpp`
- Modify: `src/engine/CMakeLists.txt` (add source)
- Test: `tests/engine/test_transition_effect.cpp`
- Modify: `tests/engine/CMakeLists.txt`

- [ ] **Step 1: Create `src/engine/camera/effects/TransitionEffect.h`**

```cpp
#pragma once

#include "engine/camera/CameraEffect.h"

class TransitionEffect : public CameraEffect {
public:
    TransitionEffect(const CameraState& from, const glm::vec3& targetPos,
                     float targetYaw, float targetPitch,
                     float duration, EasingType easing);

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;

private:
    static float shortestAngleDelta(float from, float to);

    glm::vec3 fromPosition_;
    glm::vec3 targetPosition_;
    float fromYaw_;
    float targetYaw_;
    float fromPitch_;
    float targetPitch_;
    float duration_;
    float progress_ = 0.0f;
    EasingType easing_;
};
```

- [ ] **Step 2: Create `src/engine/camera/effects/TransitionEffect.cpp`**

```cpp
#include "engine/camera/effects/TransitionEffect.h"

#include "engine/camera/CameraMath.h"

#include <algorithm>
#include <cmath>

TransitionEffect::TransitionEffect(const CameraState& from, const glm::vec3& targetPos,
                                   float targetYaw, float targetPitch,
                                   float duration, EasingType easing)
    : fromPosition_(from.position)
    , targetPosition_(targetPos)
    , fromYaw_(from.yaw)
    , targetYaw_(targetYaw)
    , fromPitch_(from.pitch)
    , targetPitch_(targetPitch)
    , duration_(std::max(duration, 0.001f))
    , easing_(easing) {}

void TransitionEffect::update(float deltaTime) {
    progress_ = std::min(progress_ + deltaTime / duration_, 1.0f);
}

CameraState TransitionEffect::apply(const CameraState& input) const {
    const float t = evaluateEasing(easing_, progress_);

    CameraState result = input;
    result.position = glm::mix(fromPosition_, targetPosition_, t);
    result.yaw = fromYaw_ + shortestAngleDelta(fromYaw_, targetYaw_) * t;
    result.pitch = glm::mix(fromPitch_, targetPitch_, t);
    return result;
}

bool TransitionEffect::isFinished() const {
    return progress_ >= 1.0f;
}

float TransitionEffect::shortestAngleDelta(float from, float to) {
    float delta = std::fmod(to - from, 360.0f);
    if (delta > 180.0f) {
        delta -= 360.0f;
    } else if (delta < -180.0f) {
        delta += 360.0f;
    }
    return delta;
}
```

- [ ] **Step 3: Add source to `src/engine/CMakeLists.txt`**

Update engine_camera:

```cmake
add_library(engine_camera STATIC
    camera/CameraMath.cpp
    camera/CameraManager.cpp
    camera/effects/TransitionEffect.cpp
)
```

- [ ] **Step 4: Write test `tests/engine/test_transition_effect.cpp`**

```cpp
#include "engine/camera/effects/TransitionEffect.h"
#include "engine/camera/CameraMath.h"

#include <cassert>
#include <cmath>

namespace {

bool approxEqual(float a, float b, float epsilon = 0.01f) {
    return std::fabs(a - b) < epsilon;
}

bool approxEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.01f) {
    return approxEqual(a.x, b.x, epsilon)
        && approxEqual(a.y, b.y, epsilon)
        && approxEqual(a.z, b.z, epsilon);
}

} // namespace

int main() {
    // --- At t=0 output matches start state ---
    {
        CameraState from;
        from.position = glm::vec3(0.0f, 0.0f, 0.0f);
        from.yaw = 0.0f;
        from.pitch = 0.0f;

        TransitionEffect effect(from, glm::vec3(10.0f, 5.0f, 0.0f), 90.0f, 30.0f,
                                1.0f, EasingType::Linear);

        CameraState input;
        const CameraState result = effect.apply(input);
        assert(approxEqualVec3(result.position, glm::vec3(0.0f, 0.0f, 0.0f)));
        assert(approxEqual(result.yaw, 0.0f));
    }

    // --- At t=1 output matches target ---
    {
        CameraState from;
        from.position = glm::vec3(0.0f);
        from.yaw = 0.0f;
        from.pitch = 0.0f;

        TransitionEffect effect(from, glm::vec3(10.0f, 5.0f, 0.0f), 90.0f, 30.0f,
                                1.0f, EasingType::Linear);

        effect.update(1.0f);
        assert(effect.isFinished());

        CameraState input;
        const CameraState result = effect.apply(input);
        assert(approxEqualVec3(result.position, glm::vec3(10.0f, 5.0f, 0.0f)));
        assert(approxEqual(result.yaw, 90.0f));
        assert(approxEqual(result.pitch, 30.0f));
    }

    // --- Linear midpoint is halfway ---
    {
        CameraState from;
        from.position = glm::vec3(0.0f);
        from.yaw = 0.0f;
        from.pitch = 0.0f;

        TransitionEffect effect(from, glm::vec3(10.0f, 0.0f, 0.0f), 0.0f, 0.0f,
                                1.0f, EasingType::Linear);

        effect.update(0.5f);
        CameraState input;
        const CameraState result = effect.apply(input);
        assert(approxEqual(result.position.x, 5.0f));
    }

    // --- Yaw wraparound: 350 -> 10 goes through 0, not 340 degrees around ---
    {
        CameraState from;
        from.position = glm::vec3(0.0f);
        from.yaw = 350.0f;
        from.pitch = 0.0f;

        TransitionEffect effect(from, glm::vec3(0.0f), 10.0f, 0.0f,
                                1.0f, EasingType::Linear);

        effect.update(0.5f);
        CameraState input;
        const CameraState result = effect.apply(input);
        assert(approxEqual(result.yaw, 360.0f)); // Midpoint of 350->370 (wraps through 360)
    }

    return 0;
}
```

- [ ] **Step 5: Register test in `tests/engine/CMakeLists.txt`**

Append:

```cmake
pixel_roguelike_add_test(test_transition_effect
    SOURCES test_transition_effect.cpp
    LIBRARIES engine_camera
    LABELS engine
)
```

- [ ] **Step 6: Build and run test**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_transition_effect && ctest --test-dir build -R transition_effect -V`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/engine/camera/effects/TransitionEffect.h src/engine/camera/effects/TransitionEffect.cpp src/engine/CMakeLists.txt tests/engine/test_transition_effect.cpp tests/engine/CMakeLists.txt
git commit -m "Add TransitionEffect with yaw wraparound and easing"
```

---

### Task 4: ShakeEffect

**Files:**
- Create: `src/engine/camera/effects/ShakeEffect.h`
- Create: `src/engine/camera/effects/ShakeEffect.cpp`
- Modify: `src/engine/CMakeLists.txt` (add source)
- Test: `tests/engine/test_shake_effect.cpp`
- Modify: `tests/engine/CMakeLists.txt`

- [ ] **Step 1: Create `src/engine/camera/effects/ShakeEffect.h`**

```cpp
#pragma once

#include "engine/camera/CameraEffect.h"

class ShakeEffect : public CameraEffect {
public:
    float maxTranslationOffset = 0.05f;
    float maxYawOffset = 1.5f;
    float maxPitchOffset = 1.0f;
    float noiseFrequency = 8.0f;
    float traumaDecayRate = 1.5f;

    void addTrauma(float amount);
    float getTrauma() const;

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;

private:
    float trauma_ = 0.0f;
    float noiseTime_ = 0.0f;
};
```

- [ ] **Step 2: Create `src/engine/camera/effects/ShakeEffect.cpp`**

```cpp
#include "engine/camera/effects/ShakeEffect.h"

#include <glm/gtc/noise.hpp>

#include <algorithm>

void ShakeEffect::addTrauma(float amount) {
    trauma_ = std::clamp(trauma_ + amount, 0.0f, 1.0f);
}

float ShakeEffect::getTrauma() const {
    return trauma_;
}

void ShakeEffect::update(float deltaTime) {
    trauma_ = std::max(trauma_ - traumaDecayRate * deltaTime, 0.0f);
    noiseTime_ += deltaTime;
}

CameraState ShakeEffect::apply(const CameraState& input) const {
    if (trauma_ <= 0.0001f) {
        return input;
    }

    const float magnitude = trauma_ * trauma_;
    const float t = noiseTime_ * noiseFrequency;

    const float offsetX = glm::perlin(glm::vec2(t, 0.0f)) * maxTranslationOffset * magnitude;
    const float offsetY = glm::perlin(glm::vec2(0.0f, t)) * maxTranslationOffset * magnitude;
    const float yawOffset = glm::perlin(glm::vec2(t, 100.0f)) * maxYawOffset * magnitude;
    const float pitchOffset = glm::perlin(glm::vec2(t, 200.0f)) * maxPitchOffset * magnitude;

    CameraState result = input;
    result.position.x += offsetX;
    result.position.y += offsetY;
    result.yaw += yawOffset;
    result.pitch += pitchOffset;
    return result;
}

bool ShakeEffect::isFinished() const {
    return false;
}
```

- [ ] **Step 3: Add source to `src/engine/CMakeLists.txt`**

Update engine_camera:

```cmake
add_library(engine_camera STATIC
    camera/CameraMath.cpp
    camera/CameraManager.cpp
    camera/effects/TransitionEffect.cpp
    camera/effects/ShakeEffect.cpp
)
```

- [ ] **Step 4: Write test `tests/engine/test_shake_effect.cpp`**

```cpp
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
```

- [ ] **Step 5: Register test in `tests/engine/CMakeLists.txt`**

Append:

```cmake
pixel_roguelike_add_test(test_shake_effect
    SOURCES test_shake_effect.cpp
    LIBRARIES engine_camera
    LABELS engine
)
```

- [ ] **Step 6: Build and run test**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target test_shake_effect && ctest --test-dir build -R shake_effect -V`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/engine/camera/effects/ShakeEffect.h src/engine/camera/effects/ShakeEffect.cpp src/engine/CMakeLists.txt tests/engine/test_shake_effect.cpp tests/engine/CMakeLists.txt
git commit -m "Add ShakeEffect with trauma-based Perlin noise"
```

---

### Task 5: FOVEffect

**Files:**
- Create: `src/engine/camera/effects/FOVEffect.h`
- Create: `src/engine/camera/effects/FOVEffect.cpp`
- Modify: `src/engine/CMakeLists.txt` (add source)
- Test: `tests/engine/test_fov_effect.cpp`
- Modify: `tests/engine/CMakeLists.txt`

- [ ] **Step 1: Create `src/engine/camera/effects/FOVEffect.h`**

```cpp
#pragma once

#include "engine/camera/CameraEffect.h"

class FOVEffect : public CameraEffect {
public:
    FOVEffect(float deltaFov, float duration, EasingType easing);

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;

private:
    float deltaFov_;
    float duration_;
    float progress_ = 0.0f;
    EasingType easing_;
};
```

- [ ] **Step 2: Create `src/engine/camera/effects/FOVEffect.cpp`**

```cpp
#include "engine/camera/effects/FOVEffect.h"

#include "engine/camera/CameraMath.h"

#include <algorithm>

FOVEffect::FOVEffect(float deltaFov, float duration, EasingType easing)
    : deltaFov_(deltaFov)
    , duration_(std::max(duration, 0.001f))
    , easing_(easing) {}

void FOVEffect::update(float deltaTime) {
    progress_ = std::min(progress_ + deltaTime / duration_, 1.0f);
}

CameraState FOVEffect::apply(const CameraState& input) const {
    const float easedProgress = evaluateEasing(easing_, progress_);
    const float fovOffset = deltaFov_ * (1.0f - easedProgress);

    CameraState result = input;
    result.fov += fovOffset;
    return result;
}

bool FOVEffect::isFinished() const {
    return progress_ >= 1.0f;
}
```

- [ ] **Step 3: Add source to `src/engine/CMakeLists.txt`**

Update engine_camera:

```cmake
add_library(engine_camera STATIC
    camera/CameraMath.cpp
    camera/CameraManager.cpp
    camera/effects/TransitionEffect.cpp
    camera/effects/ShakeEffect.cpp
    camera/effects/FOVEffect.cpp
)
```

- [ ] **Step 4: Write test `tests/engine/test_fov_effect.cpp`**

```cpp
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
```

- [ ] **Step 5: Register test in `tests/engine/CMakeLists.txt`**

Append:

```cmake
pixel_roguelike_add_test(test_fov_effect
    SOURCES test_fov_effect.cpp
    LIBRARIES engine_camera
    LABELS engine
)
```

- [ ] **Step 6: Build and run all camera tests**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build && ctest --test-dir build -R "camera_math|camera_manager|transition_effect|shake_effect|fov_effect" -V`
Expected: All 5 tests PASS

- [ ] **Step 7: Commit**

```bash
git add src/engine/camera/effects/FOVEffect.h src/engine/camera/effects/FOVEffect.cpp src/engine/CMakeLists.txt tests/engine/test_fov_effect.cpp tests/engine/CMakeLists.txt
git commit -m "Add FOVEffect with easing decay"
```

---

### Task 6: Integrate CameraManager into RuntimeGameSession

**Files:**
- Modify: `src/game/runtime/RuntimeGameSession.h:1-106`
- Modify: `src/game/runtime/RuntimeGameSession.cpp:1-475`
- Modify: `src/game/CMakeLists.txt:27-51` (add engine_camera dependency)

- [ ] **Step 1: Add engine_camera dependency to game targets in `src/game/CMakeLists.txt`**

In the `game_rendering` target (line 20-24), add `engine_camera`:

```cmake
target_link_libraries(game_rendering PUBLIC
    game_content
    engine_camera
    engine_ui
)
```

In the `gameplay` target (line 43-51), add `engine_camera`:

```cmake
target_link_libraries(gameplay PUBLIC
    game_content
    game_rendering
    engine_audio
    engine_camera
    engine_input
    engine_physics
    EnTT::EnTT
)
```

- [ ] **Step 2: Add CameraManager member to `RuntimeGameSession.h`**

Add include at line 2 area (after existing includes):

```cpp
#include "engine/camera/CameraManager.h"
```

Add member variable in the private section (after `float elapsedTime_ = 0.0f;` at line 103):

```cpp
    CameraManager cameraManager_;
```

Add public accessor (after `float elapsedTime() const` at line 83):

```cpp
    CameraManager& cameraManager() { return cameraManager_; }
    const CameraManager& cameraManager() const { return cameraManager_; }
```

- [ ] **Step 3: Register CameraManager pointer in registry context during `rebuild()`**

In `RuntimeGameSession.cpp`, add to the `rebuild()` method after line 152 (`registry_.ctx().insert_or_assign<PhysicsSystem*>(&physics_);`):

```cpp
    registry_.ctx().insert_or_assign<CameraManager*>(&cameraManager_);
```

- [ ] **Step 4: Clear CameraManager pointer in `clearEntities()`**

In `RuntimeGameSession.cpp`, add to `clearEntities()` after line 471 (`if (ctx.contains<PhysicsSystem*>()) { ctx.erase<PhysicsSystem*>(); }`):

```cpp
    if (ctx.contains<CameraManager*>()) {
        ctx.erase<CameraManager*>();
    }
```

Also clear effects when clearing entities. Add after the new erase block:

```cpp
    cameraManager_.clearEffects();
```

- [ ] **Step 5: Wire CameraManager::update into tick()**

In `RuntimeGameSession.cpp`, modify the camera section of `tick()` (lines 247-250). Replace:

```cpp
    t0 = t1;
    tickPlayerCamera(registry_, inputSystem_, aspect, deltaTime);
    t1 = Clock::now();
    performanceStats_.cameraMs = elapsedMilliseconds(t0, t1);
```

With:

```cpp
    t0 = t1;
    tickPlayerCamera(registry_, inputSystem_, cameraManager_, aspect, deltaTime);
    cameraManager_.update(deltaTime);
    t1 = Clock::now();
    performanceStats_.cameraMs = elapsedMilliseconds(t0, t1);
```

Note: `tickPlayerCamera` signature changes in Task 8 (accepts `CameraManager&`). Build will break until Task 8 is completed.

- [ ] **Step 6: Commit**

```bash
git add src/game/runtime/RuntimeGameSession.h src/game/runtime/RuntimeGameSession.cpp src/game/CMakeLists.txt
git commit -m "Integrate CameraManager into RuntimeGameSession with registry context"
```

---

### Task 7: Migrate PlayerControlCamera

**Files:**
- Modify: `src/game/modules/player_control/PlayerControlCamera.h:1-14`
- Modify: `src/game/modules/player_control/PlayerControlCamera.cpp:1-59`

- [ ] **Step 1: Update `PlayerControlCamera.h`**

Replace the entire file contents:

```cpp
#pragma once

#include "engine/ecs/GameRegistry.h"

#include <glm/glm.hpp>

class CameraManager;
class InputSystem;

glm::vec3 buildPlayerCameraForward(float yawDegrees, float pitchDegrees);

void tickPlayerCamera(GameRegistry& registry,
                      const InputSystem& input,
                      CameraManager& cameraManager,
                      float aspect,
                      float deltaTime);
```

- [ ] **Step 2: Update `PlayerControlCamera.cpp`**

Replace the entire file contents:

```cpp
#include "game/modules/player_control/PlayerControlCamera.h"

#include "engine/camera/CameraManager.h"
#include "engine/camera/CameraMath.h"
#include "engine/input/InputSystem.h"
#include "game/components/ControllableTag.h"
#include "game/components/PlayerInteractionLockComponent.h"
#include "game/components/PlayerTag.h"
#include "game/components/TransformComponent.h"
#include "game/ui/InventoryMenuState.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>

glm::vec3 buildPlayerCameraForward(float yawDegrees, float pitchDegrees) {
    return buildCameraForward(yawDegrees, pitchDegrees);
}

namespace {

struct PlayerCameraInput {
    glm::vec3 position{0.0f};
    float yaw = -90.0f;
    float pitch = 0.0f;
    float fov = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    bool found = false;
};

PlayerCameraInput gatherPlayerCameraInput(GameRegistry& registry, const InputSystem& input) {
    const bool inventoryOpen = registry.ctx().contains<InventoryMenuState>()
        && registry.ctx().get<InventoryMenuState>().open;

    PlayerCameraInput result;

    auto view = registry.view<TransformComponent,
                              PlayerInteractionLockComponent,
                              ControllableTag,
                              PlayerTag>();
    for (auto [entity, transform, lock] : view.each()) {
        (void)entity;

        // Read current yaw/pitch from CameraManager base state
        auto* camManager = registry.ctx().contains<CameraManager*>()
            ? registry.ctx().get<CameraManager*>()
            : nullptr;

        float yaw = -90.0f;
        float pitch = 0.0f;
        float fov = 70.0f;
        float nearPlane = 0.1f;
        float farPlane = 100.0f;

        if (camManager != nullptr) {
            const auto& base = camManager->getBaseState();
            yaw = base.yaw;
            pitch = base.pitch;
            fov = base.fov;
            nearPlane = base.nearPlane;
            farPlane = base.farPlane;
        }

        if (input.isCursorLocked() && !lock.active && !inventoryOpen) {
            constexpr float sensitivity = 0.1f;
            const glm::vec2 delta = input.mouseDelta();
            yaw += delta.x * sensitivity;
            pitch -= delta.y * sensitivity;
        }

        pitch = std::clamp(pitch, -89.0f, 89.0f);

        result.position = transform.position;
        result.yaw = yaw;
        result.pitch = pitch;
        result.fov = fov;
        result.nearPlane = nearPlane;
        result.farPlane = farPlane;
        result.found = true;
        break;
    }

    return result;
}

} // namespace

void tickPlayerCamera(GameRegistry& registry,
                      const InputSystem& input,
                      CameraManager& cameraManager,
                      float aspect,
                      float deltaTime) {
    (void)deltaTime;

    const PlayerCameraInput cam = gatherPlayerCameraInput(registry, input);
    if (!cam.found) {
        return;
    }

    cameraManager.setBaseState(cam.position, cam.yaw, cam.pitch);
    cameraManager.setProjection(cam.fov, aspect, cam.nearPlane, cam.farPlane);
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build 2>&1 | head -40`
Expected: Compiles (or only errors from files not yet migrated -- not from PlayerControlCamera itself)

- [ ] **Step 4: Commit**

```bash
git add src/game/modules/player_control/PlayerControlCamera.h src/game/modules/player_control/PlayerControlCamera.cpp
git commit -m "Migrate PlayerControlCamera to feed CameraManager instead of CameraComponent"
```

---

### Task 8: Migrate RuntimeSceneRenderer

**Files:**
- Modify: `src/game/rendering/RuntimeSceneRenderer.h:1-78`
- Modify: `src/game/rendering/RuntimeSceneRenderer.cpp:1-384`

- [ ] **Step 1: Update `RuntimeSceneRenderer.h`**

Add include (after existing includes near line 7):

```cpp
#include "engine/camera/CameraState.h"
```

Remove the private `CameraState` struct (lines 41-49):

```cpp
    // DELETE these lines:
    struct CameraState {
        glm::vec3 position{0.0f};
        glm::mat4 viewMatrix{1.0f};
        glm::mat4 projectionMatrix{1.0f};
        glm::vec3 direction{0.0f, 0.0f, -1.0f};
        float nearPlane = 0.1f;
        float farPlane = 100.0f;
        float moveSpeed = 3.0f;
    };
```

Change the `captureCamera` method signature (line 51) to accept CameraState:

Replace:
```cpp
    CameraState captureCamera(GameRegistry& registry, float aspect) const;
```
With:
```cpp
    // captureCamera removed — CameraState passed in from CameraManager
```

Update `render()` signature to accept `const CameraState&` (line 24). Add it as the second parameter:

Replace:
```cpp
    void render(GameRegistry& registry,
                DebugParams& params,
```
With:
```cpp
    void render(GameRegistry& registry,
                const CameraState& camera,
                DebugParams& params,
```

Update private method signatures that used the old `CameraState` to use the new `CameraState` from the engine module. The includes already pull in `CameraState` from `engine/camera/CameraState.h`. The methods `collectViewmodelObjects`, `collectLights`, and `updateDebugParams` that take `const CameraState&` will work as-is because the engine `CameraState` has the same field names. But they need updating for field name changes: the old struct had `direction`, the new one has `forward`. Update the signatures and bodies:

In `collectLights`, change to accept `const CameraState&`:

Replace line 58-60:
```cpp
    void collectLights(GameRegistry& registry,
                       const DebugParams& params,
                       std::vector<RenderLight>& out) const;
```
With:
```cpp
    void collectLights(GameRegistry& registry,
                       const CameraState& camera,
                       const DebugParams& params,
                       std::vector<RenderLight>& out) const;
```

- [ ] **Step 2: Update `RuntimeSceneRenderer.cpp` — remove captureCamera**

Remove the include for `RuntimeCameraMath.h` (line 15):
```cpp
// DELETE: #include "game/rendering/RuntimeCameraMath.h"
```

Remove the includes for `CameraComponent.h` and `PrimaryCameraTag.h` (lines 6, 9):
```cpp
// DELETE: #include "game/components/CameraComponent.h"
// DELETE: #include "game/components/PrimaryCameraTag.h"
```

Remove the `captureCamera` method entirely (lines 101-112):
```cpp
// DELETE the entire captureCamera method
```

- [ ] **Step 3: Update `RuntimeSceneRenderer.cpp` — update collectLights**

Change method signature (line 182-184):

Replace:
```cpp
void RuntimeSceneRenderer::collectLights(GameRegistry& registry,
                                          const DebugParams& params,
                                          std::vector<RenderLight>& out) const {
```
With:
```cpp
void RuntimeSceneRenderer::collectLights(GameRegistry& registry,
                                          const CameraState& camera,
                                          const DebugParams& params,
                                          std::vector<RenderLight>& out) const {
```

Replace the ECS camera query block (lines 188-254). The old code queries `TransformComponent, CameraComponent, PrimaryCameraTag`. Replace with direct reads from the `camera` parameter:

Replace lines 188-254:
```cpp
    auto cameraView = registry.view<TransformComponent, CameraComponent, PrimaryCameraTag>();
    for (auto [entity, transform, camera] : cameraView.each()) {
        ...entire torch block...
        break;
    }
```

With:
```cpp
    if (params.lighting.torch.enabled) {
        const PlayerTorchOverride& torch = params.lighting.torch;
        const float timeSeconds = static_cast<float>(glfwGetTime());
        const float visualFlicker = playerTorchVisualFlicker(timeSeconds);
        const float lightFlicker = playerTorchLightFlicker(timeSeconds);
        const glm::vec3 cameraUp = safeNormalize(glm::cross(camera.right, camera.forward), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::vec3 torchDirection = safeNormalize(
            camera.forward * 0.14f + camera.right * 0.03f + cameraUp * -0.48f,
            glm::vec3(0.0f, 0.0f, -1.0f)
        );
        glm::vec3 flamePosition = camera.position
            + camera.forward * kPlayerTorchForwardOffset
            + camera.right * kPlayerTorchRightOffset
            + cameraUp * -kPlayerTorchDownOffset;
        flamePosition += camera.right * (std::sin(timeSeconds * 6.3f) * 0.010f)
            + cameraUp * (std::sin(timeSeconds * 8.7f + 0.8f) * 0.012f);

        glm::vec3 spillPosition = camera.position
            + camera.right * kPlayerTorchSpillOffset.x
            + cameraUp * kPlayerTorchSpillOffset.y
            + camera.forward * kPlayerTorchSpillOffset.z;

        RenderLight torchSpill;
        torchSpill.type = LightType::Point;
        torchSpill.position = spillPosition;
        torchSpill.color = torch.spillColor * (0.92f + visualFlicker * 0.14f);
        torchSpill.radius = torch.spillRadius * (0.95f + visualFlicker * 0.08f);
        torchSpill.intensity = torch.spillIntensity * torch.masterIntensity * (0.88f + visualFlicker * 0.22f);
        lights.push_back(torchSpill);

        RenderLight torchHalo;
        torchHalo.type = LightType::Point;
        torchHalo.position = camera.position + cameraUp * -0.22f;
        torchHalo.color = torch.haloColor * (0.92f + visualFlicker * 0.10f);
        torchHalo.radius = torch.haloRadius * (0.97f + visualFlicker * 0.05f);
        torchHalo.intensity = torch.haloIntensity * torch.masterIntensity * (0.92f + visualFlicker * 0.12f);
        lights.push_back(torchHalo);

        RenderLight torchLight;
        torchLight.type = LightType::Spot;
        torchLight.position = flamePosition;
        torchLight.direction = torchDirection;
        torchLight.color = torch.torchColor * (0.95f + lightFlicker * 0.04f);
        torchLight.radius = torch.torchRadius * (0.98f + lightFlicker * 0.05f);
        torchLight.intensity = torch.torchIntensity * torch.masterIntensity * lightFlicker;
        torchLight.innerConeDegrees = clampInnerCone(torch.torchInnerConeDegrees, torch.torchOuterConeDegrees);
        torchLight.outerConeDegrees = clampOuterCone(torchLight.innerConeDegrees, torch.torchOuterConeDegrees);
        torchLight.castsShadows = true;
        lights.push_back(torchLight);

        glm::vec3 handGlowPosition = camera.position
            + camera.forward * kPlayerHandGlowForwardOffset
            + camera.right * kPlayerHandGlowRightOffset
            + glm::vec3(0.0f, -kPlayerHandGlowDownOffset, 0.0f);
        RenderLight handGlow;
        handGlow.type = LightType::Point;
        handGlow.position = handGlowPosition;
        handGlow.color = torch.handGlowColor * (0.98f + visualFlicker * 0.03f);
        handGlow.radius = torch.handGlowRadius * (0.99f + lightFlicker * 0.03f);
        handGlow.intensity = torch.handGlowIntensity * torch.masterIntensity * (0.96f + lightFlicker * 0.05f);
        lights.push_back(handGlow);
    }
```

- [ ] **Step 4: Update `RuntimeSceneRenderer.cpp` — update render() method**

In `render()` (line 313+), change signature:

Replace:
```cpp
void RuntimeSceneRenderer::render(GameRegistry& registry,
                                  DebugParams& params,
```
With:
```cpp
void RuntimeSceneRenderer::render(GameRegistry& registry,
                                  const CameraState& camera,
                                  DebugParams& params,
```

Remove the `captureCamera` call (line 327):
```cpp
// DELETE: const CameraState camera = captureCamera(registry, aspect);
```

Remove the `aspect` local variable and `captureCamera` call (lines 326-327). The `aspect` variable was only used by `captureCamera`, which is now removed:

```cpp
// DELETE both lines:
    const float aspect = static_cast<float>(std::max(internalWidth, 1)) / static_cast<float>(std::max(internalHeight, 1));
    const CameraState camera = captureCamera(registry, aspect);
```

Update `collectLights` call (line 330):

Replace:
```cpp
    collectLights(registry, params, lights_);
```
With:
```cpp
    collectLights(registry, camera, params, lights_);
```

Update `updateDebugParams` (line 300-311) — two field name changes (`direction` -> `forward`, and `moveSpeed` removed from `CameraState`):

Replace:
```cpp
    params.camera.direction = camera.direction;
    params.camera.fov = glm::degrees(2.0f * std::atan(1.0f / camera.projectionMatrix[1][1]));
    params.camera.moveSpeed = camera.moveSpeed;
```
With:
```cpp
    params.camera.direction = camera.forward;
    params.camera.fov = glm::degrees(2.0f * std::atan(1.0f / camera.projectionMatrix[1][1]));
    params.camera.moveSpeed = 3.0f; // Player movement speed; not a camera property
```

- [ ] **Step 5: Update the call site in `RuntimeGameSession.cpp`**

In `RuntimeGameSession::render()` (line 332), add `cameraManager_.getState()`:

Replace:
```cpp
    renderer_.render(registry_,
                     debugParams_,
```
With:
```cpp
    renderer_.render(registry_,
                     cameraManager_.getState(),
                     debugParams_,
```

- [ ] **Step 6: Verify build compiles**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build 2>&1 | head -60`

- [ ] **Step 7: Commit**

```bash
git add src/game/rendering/RuntimeSceneRenderer.h src/game/rendering/RuntimeSceneRenderer.cpp src/game/runtime/RuntimeGameSession.cpp
git commit -m "Migrate RuntimeSceneRenderer to accept CameraState from CameraManager"
```

---

### Task 9: Migrate AudioListenerSystem, InteractionSystem, and PlayerControlMovement

**Files:**
- Modify: `src/game/systems/AudioListenerSystem.h:1-26`
- Modify: `src/game/systems/AudioListenerSystem.cpp:1-46`
- Modify: `src/game/modules/interaction/InteractionSystem.h:1-13`
- Modify: `src/game/modules/interaction/InteractionSystem.cpp:1-166`
- Modify: `src/game/modules/player_control/PlayerControlMovement.h:1-11`
- Modify: `src/game/modules/player_control/PlayerControlMovement.cpp:1-80`

- [ ] **Step 1: Update `AudioListenerSystem.h`**

Replace `updateAudioListener` signature (line 14):

```cpp
// Replace:
void updateAudioListener(GameRegistry& registry, AudioSystem& audio);
// With:
class CameraManager;
void updateAudioListener(const CameraManager& cameraManager, AudioSystem& audio);
```

- [ ] **Step 2: Update `AudioListenerSystem.cpp`**

Replace includes: remove `PrimaryCameraTag.h` and `TransformComponent.h`, add `CameraManager.h`:

```cpp
// DELETE: #include "game/components/PrimaryCameraTag.h"
// DELETE: #include "game/components/TransformComponent.h"
// ADD:
#include "engine/camera/CameraManager.h"
```

Remove unused includes `entt`, `glm/trigonometric.hpp`, `cmath`.

Replace `updateAudioListener` (lines 17-31):

```cpp
void updateAudioListener(const CameraManager& cameraManager, AudioSystem& audio) {
    const auto& cam = cameraManager.getState();
    audio.setListenerTransform(cam.position, cam.forward, cam.up);
}
```

Update `AudioListenerSystem::update` (line 42-44):

```cpp
void AudioListenerSystem::update(Application& app, float /*deltaTime*/) {
    auto* camManager = app.registry().ctx().contains<CameraManager*>()
        ? app.registry().ctx().get<CameraManager*>()
        : nullptr;
    if (camManager != nullptr) {
        updateAudioListener(*camManager, audio_);
    }
}
```

- [ ] **Step 3: Update `InteractionSystem.h`**

Replace `updateRuntimeInteraction` signature (line 12):

```cpp
// Replace:
void updateRuntimeInteraction(GameRegistry& registry, const InputSystem& input);
// With:
class CameraManager;
void updateRuntimeInteraction(GameRegistry& registry, const InputSystem& input,
                              const CameraManager& cameraManager);
```

- [ ] **Step 4: Update `InteractionSystem.cpp`**

Replace includes: remove `CameraComponent.h` and `PrimaryCameraTag.h`, add `CameraManager.h`:

```cpp
// DELETE: #include "game/components/CameraComponent.h"
// DELETE: #include "game/components/PrimaryCameraTag.h"
// ADD:
#include "engine/camera/CameraManager.h"
```

Replace `InteractionActorContext` struct (lines 50-55) — remove `CameraComponent*`:

```cpp
struct InteractionActorContext {
    entt::entity entity = entt::null;
    TransformComponent* transform = nullptr;
    PlayerInteractionLockComponent* lock = nullptr;
};
```

Replace `resolveInteractionActor` (lines 57-71) — remove CameraComponent/PrimaryCameraTag from query:

```cpp
InteractionActorContext resolveInteractionActor(GameRegistry& registry) {
    auto view = registry.view<TransformComponent,
                              ControllableTag,
                              PlayerTag>();
    for (auto entity : view) {
        return InteractionActorContext{
            .entity = entity,
            .transform = &view.get<TransformComponent>(entity),
            .lock = registry.try_get<PlayerInteractionLockComponent>(entity),
        };
    }
    return {};
}
```

Remove `buildInteractionForward` function (lines 37-48) — will use `cameraManager.getState().forward` directly.

Update `updateRuntimeInteraction` signature and body (lines 99-165):

```cpp
void updateRuntimeInteraction(GameRegistry& registry, const InputSystem& input,
                              const CameraManager& cameraManager) {
```

Replace line 113:
```cpp
    if (actor.entity == entt::null || actor.transform == nullptr || actor.camera == nullptr) {
```
With:
```cpp
    if (actor.entity == entt::null || actor.transform == nullptr) {
```

Replace line 118:
```cpp
    const glm::vec3 actorForward = buildInteractionForward(actor.camera->yaw, actor.camera->pitch);
```
With:
```cpp
    const glm::vec3 actorForward = cameraManager.getState().forward;
```

- [ ] **Step 5: Update the call site in `RuntimeGameSession.cpp`**

In `tick()` (line 211):

Replace:
```cpp
    updateRuntimeInteraction(registry_, inputSystem_);
```
With:
```cpp
    updateRuntimeInteraction(registry_, inputSystem_, cameraManager_);
```

Add the CameraManager include at top of RuntimeGameSession.cpp (it was already added in Task 6 via the header).

Also add the InteractionSystem header include if not present.

- [ ] **Step 6: Update `PlayerControlMovement.h`**

Add `CameraManager` forward declaration and parameter:

```cpp
#pragma once

#include "engine/ecs/GameRegistry.h"

class CameraManager;
class InputSystem;
class PhysicsSystem;

void tickPlayerMovement(GameRegistry& registry,
                        const InputSystem& input,
                        const CameraManager& cameraManager,
                        PhysicsSystem& physics,
                        float deltaTime);
```

- [ ] **Step 7: Update `PlayerControlMovement.cpp`**

Replace includes: remove `CameraComponent.h` and `PrimaryCameraTag.h`, add `CameraManager.h`:

```cpp
// DELETE: #include "game/components/CameraComponent.h"
// DELETE: #include "game/components/PrimaryCameraTag.h"
// ADD:
#include "engine/camera/CameraManager.h"
```

Update function signature (line 39-42):

```cpp
void tickPlayerMovement(GameRegistry& registry,
                        const InputSystem& input,
                        const CameraManager& cameraManager,
                        PhysicsSystem& physics,
                        float deltaTime) {
```

Update the ECS view (lines 46-54) — remove `CameraComponent` and `PrimaryCameraTag`:

```cpp
    auto view = registry.view<TransformComponent,
                              PlayerMovementComponent,
                              CharacterControllerComponent,
                              PlayerInteractionLockComponent,
                              PlayerSpawnComponent,
                              PlayerTag,
                              ControllableTag>();
    for (auto [entity, transform, movement, controller, lock, spawn] : view.each()) {
```

Update line 72 — get yaw from CameraManager base state instead of CameraComponent:

Replace:
```cpp
            const float yawRadians = glm::radians(camera.yaw);
```
With:
```cpp
            const float yawRadians = glm::radians(cameraManager.getBaseState().yaw);
```

- [ ] **Step 8: Update the call site in `RuntimeGameSession.cpp`**

In `tick()` (line 243):

Replace:
```cpp
    tickPlayerMovement(registry_, inputSystem_, physics_, deltaTime);
```
With:
```cpp
    tickPlayerMovement(registry_, inputSystem_, cameraManager_, physics_, deltaTime);
```

- [ ] **Step 9: Verify build compiles**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build 2>&1 | head -60`

- [ ] **Step 10: Commit**

```bash
git add src/game/systems/AudioListenerSystem.h src/game/systems/AudioListenerSystem.cpp src/game/modules/interaction/InteractionSystem.h src/game/modules/interaction/InteractionSystem.cpp src/game/modules/player_control/PlayerControlMovement.h src/game/modules/player_control/PlayerControlMovement.cpp src/game/runtime/RuntimeGameSession.cpp
git commit -m "Migrate audio, interaction, and movement systems to use CameraManager"
```

---

### Task 10: Migrate RuntimeGameSession Methods and PlayerControlSpawner

**Files:**
- Modify: `src/game/runtime/RuntimeGameSession.h:58-62`
- Modify: `src/game/runtime/RuntimeGameSession.cpp:36-51,268-300,347-430`
- Modify: `src/game/modules/player_control/PlayerControlSpawner.cpp:1-69`

- [ ] **Step 1: Update `RuntimeMutableSnapshot` in `RuntimeGameSession.cpp`**

Remove `CameraComponent camera{};` from the `PlayerState` struct (line 40). Add camera state fields instead:

Replace lines 36-51:
```cpp
struct RuntimeMutableSnapshot {
    struct PlayerState {
        entt::entity entity = entt::null;
        TransformComponent transform{};
        PlayerMovementComponent movement{};
        PlayerInteractionLockComponent interactionLock{};
        PlayerSpawnComponent spawn{};
        bool valid = false;

        // Camera baseline (captured from CameraManager)
        float cameraYaw = -90.0f;
        float cameraPitch = 0.0f;
        float cameraFov = 70.0f;
    };

    RunSession runSession{};
    PlayerState player{};
    std::vector<std::pair<entt::entity, DoorStateComponent>> doors;
    std::vector<std::pair<entt::entity, CheckpointComponent>> checkpoints;
};
```

Remove the `#include "game/components/CameraComponent.h"` (line 5) since it's no longer needed here. Also remove `#include "game/components/PrimaryCameraTag.h"` (line 18).

- [ ] **Step 2: Update `setPrimaryCameraView`**

Replace lines 268-300:

```cpp
bool RuntimeGameSession::setPrimaryCameraView(const glm::vec3& position,
                                              float yaw,
                                              float pitch,
                                              const std::optional<float>& fov) {
    cameraManager_.setBaseState(position, std::clamp(pitch, -89.0f, 89.0f) == pitch ? position : position,
                                yaw, std::clamp(pitch, -89.0f, 89.0f));
```

Actually, let me write this more clearly:

```cpp
bool RuntimeGameSession::setPrimaryCameraView(const glm::vec3& position,
                                              float yaw,
                                              float pitch,
                                              const std::optional<float>& fov) {
    cameraManager_.setBaseState(position, yaw, std::clamp(pitch, -89.0f, 89.0f));
    if (fov.has_value()) {
        const auto& base = cameraManager_.getBaseState();
        cameraManager_.setProjection(fov.value(), 16.0f / 9.0f, base.nearPlane, base.farPlane);
    }

    auto view = registry_.view<TransformComponent, PlayerTag>();
    for (auto [entity, transform] : view.each()) {
        transform.position = position;

        if (registry_.all_of<PlayerMovementComponent>(entity)) {
            auto& movement = registry_.get<PlayerMovementComponent>(entity);
            movement.velocity = glm::vec3(0.0f);
            movement.grounded = false;
            movement.jumpHeld = false;
            movement.jumpHoldTimer = 0.0f;
        }

        if (registry_.all_of<CharacterControllerComponent>(entity)) {
            const auto& controller = registry_.get<CharacterControllerComponent>(entity);
            physics_.setCharacterVelocity(entity, glm::vec3(0.0f));
            physics_.setCharacterPosition(entity,
                                          position - glm::vec3(0.0f, controller.eyeOffset(), 0.0f));
        }

        return true;
    }

    return false;
}
```

- [ ] **Step 3: Update `captureBaselineState`**

Replace lines 353-358 (the player view and camera capture):

Replace:
```cpp
    auto playerView = registry_.view<TransformComponent, CameraComponent, PlayerMovementComponent,
                                     PlayerInteractionLockComponent, PlayerSpawnComponent, PlayerTag>();
    for (auto [entity, transform, camera, movement, lock, spawn] : playerView.each()) {
        baselineSnapshot_->player.entity = entity;
        baselineSnapshot_->player.transform = transform;
        baselineSnapshot_->player.camera = camera;
        baselineSnapshot_->player.movement = movement;
```
With:
```cpp
    auto playerView = registry_.view<TransformComponent, PlayerMovementComponent,
                                     PlayerInteractionLockComponent, PlayerSpawnComponent, PlayerTag>();
    for (auto [entity, transform, movement, lock, spawn] : playerView.each()) {
        baselineSnapshot_->player.entity = entity;
        baselineSnapshot_->player.transform = transform;
        baselineSnapshot_->player.movement = movement;

        const auto& baseCam = cameraManager_.getBaseState();
        baselineSnapshot_->player.cameraYaw = baseCam.yaw;
        baselineSnapshot_->player.cameraPitch = baseCam.pitch;
        baselineSnapshot_->player.cameraFov = baseCam.fov;
```

Keep lines 359-363 the same (lock, spawn, valid, break).

- [ ] **Step 4: Update `restoreBaselineState`**

Replace lines 393-395 (the CameraComponent restore):

```cpp
        // DELETE:
        registry_.patch<CameraComponent>(player, [&](auto& component) {
            component = baselineSnapshot_->player.camera;
        });
```

And add after the TransformComponent restore (after line 391):

```cpp
        cameraManager_.setBaseState(baselineSnapshot_->player.transform.position,
                                    baselineSnapshot_->player.cameraYaw,
                                    baselineSnapshot_->player.cameraPitch);
        const auto& base = cameraManager_.getBaseState();
        cameraManager_.setProjection(baselineSnapshot_->player.cameraFov,
                                     16.0f / 9.0f, base.nearPlane, base.farPlane);
        cameraManager_.clearEffects();
```

- [ ] **Step 5: Update `PlayerControlSpawner.cpp`**

Remove includes for `CameraComponent.h` and `PrimaryCameraTag.h` (lines 3, 10):

```cpp
// DELETE: #include "game/components/CameraComponent.h"
// DELETE: #include "game/components/PrimaryCameraTag.h"
```

Remove the two `registry.emplace` calls (lines 61-62):

```cpp
// DELETE: registry.emplace<PrimaryCameraTag>(player);
// DELETE: registry.emplace<CameraComponent>(player);
```

- [ ] **Step 6: Verify build compiles**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build 2>&1 | head -60`

- [ ] **Step 7: Commit**

```bash
git add src/game/runtime/RuntimeGameSession.h src/game/runtime/RuntimeGameSession.cpp src/game/modules/player_control/PlayerControlSpawner.cpp
git commit -m "Migrate setPrimaryCameraView, baseline snapshot, and spawner to use CameraManager"
```

---

### Task 11: Migrate Editor Code

**Files:**
- Modify: `src/editor/debug/EditorCommander.cpp:179-224`
- Modify: `src/editor/debug/EditorInspector.cpp:257-277`
- Modify: `src/editor/core/LevelEditorCore.cpp:95-103`

- [ ] **Step 1: Update `EditorCommander.cpp`**

Replace includes: remove `CameraComponent.h` and `PrimaryCameraTag.h`, add `CameraManager.h`:

```cpp
// DELETE: #include "game/components/CameraComponent.h"
// DELETE: #include "game/components/PrimaryCameraTag.h"
// ADD:
#include "engine/camera/CameraManager.h"
```

Replace `setRuntimeCamera` method (lines 179-224):

```cpp
nlohmann::json EditorCommander::setRuntimeCamera(const nlohmann::json& args) {
    if (runtimePreviewSession_ == nullptr) {
        return {{"ok", false}, {"error", "Runtime preview session is not available"}};
    }

    auto& camManager = runtimePreviewSession_->cameraManager();
    const auto& base = camManager.getBaseState();

    const glm::vec3 requestedPosition(
        args.value("x", base.position.x),
        args.value("y", base.position.y),
        args.value("z", base.position.z));
    const float requestedYaw = args.value("yaw", base.yaw);
    const float requestedPitch = args.value("pitch", base.pitch);
    const bool hasFov = args.contains("fov");
    const std::optional<float> requestedFov = hasFov
        ? std::optional<float>(args["fov"].get<float>())
        : std::nullopt;

    if (!args.contains("x") && !args.contains("y") && !args.contains("z")
        && !args.contains("yaw") && !args.contains("pitch") && !args.contains("fov")) {
        return {{"ok", false}, {"error", "No runtime camera fields provided"}};
    }

    runtimePreviewSession_->setPrimaryCameraView(requestedPosition,
                                                 requestedYaw,
                                                 requestedPitch,
                                                 requestedFov);

    const auto& state = camManager.getBaseState();

    if (window_) {
        glfwPostEmptyEvent();
    }

    return {{"ok", true}, {"data", {
        {"position", {{"x", state.position.x}, {"y", state.position.y}, {"z", state.position.z}}},
        {"yaw", state.yaw},
        {"pitch", state.pitch},
        {"fov", state.fov}
    }}};
}
```

Note: The old code returned the entity ID. Since the camera is no longer on an entity, omit it from the response. If the debug harness protocol expects it, return a sentinel value.

- [ ] **Step 2: Update `EditorInspector.cpp`**

Replace includes: remove `CameraComponent.h` and `PrimaryCameraTag.h`, add `CameraManager.h`:

```cpp
// DELETE: #include "game/components/CameraComponent.h"
// DELETE: #include "game/components/PrimaryCameraTag.h"
// ADD:
#include "engine/camera/CameraManager.h"
```

Replace `runtimeCamera` method (lines 257-277):

```cpp
nlohmann::json EditorInspector::runtimeCamera() const {
    if (runtimePreviewSession_ == nullptr) {
        return {{"ok", false}, {"error", "Runtime preview session is not available"}};
    }

    const auto& state = runtimePreviewSession_->cameraManager().getState();
    return {{"ok", true}, {"data", {
        {"position", {{"x", state.position.x}, {"y", state.position.y}, {"z", state.position.z}}},
        {"yaw", state.yaw},
        {"pitch", state.pitch},
        {"fov", state.fov},
        {"near_plane", state.nearPlane},
        {"far_plane", state.farPlane},
        {"forward", {{"x", state.forward.x}, {"y", state.forward.y}, {"z", state.forward.z}}}
    }}};
}
```

- [ ] **Step 3: Update `LevelEditorCore.cpp`**

Replace include: remove `CameraComponent.h`, add `CameraState.h`:

```cpp
// DELETE: #include "game/components/CameraComponent.h"
// ADD:
#include "engine/camera/CameraState.h"
```

Replace `resetEditorCameraToRuntimeDefaults` (lines 95-103):

```cpp
void resetEditorCameraToRuntimeDefaults(EditorCamera& camera) {
    const CameraState defaults;
    camera.yawDegrees = defaults.yaw;
    camera.pitchDegrees = defaults.pitch;
    camera.fovDegrees = defaults.fov;
    camera.nearPlane = defaults.nearPlane;
    camera.farPlane = defaults.farPlane;
}
```

Note: `CameraComponent` had `moveSpeed = 3.0f` which was also copied. `CameraState` doesn't have `moveSpeed`. The editor `moveSpeed` defaults to `8.0f` already in `EditorCamera`, so dropping the `moveSpeed` reset is correct — the editor should keep its own movement speed.

- [ ] **Step 4: Verify build compiles**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build 2>&1 | head -60`

- [ ] **Step 5: Commit**

```bash
git add src/editor/debug/EditorCommander.cpp src/editor/debug/EditorInspector.cpp src/editor/core/LevelEditorCore.cpp
git commit -m "Migrate editor code to use CameraManager and CameraState"
```

---

### Task 12: Remove Deprecated Code and Final Cleanup

**Files:**
- Delete: `src/game/rendering/RuntimeCameraMath.h`
- Delete: `src/game/rendering/RuntimeCameraMath.cpp`
- Modify: `src/game/CMakeLists.txt:14-25` (remove RuntimeCameraMath.cpp from game_rendering)
- Verify: No remaining references to `CameraComponent` or `PrimaryCameraTag` in runtime code
- Verify: `CameraComponent.h` and `PrimaryCameraTag.h` still exist for potential editor/test usage but have no runtime consumers

- [ ] **Step 1: Remove RuntimeCameraMath from game_rendering target**

In `src/game/CMakeLists.txt`, remove `rendering/RuntimeCameraMath.cpp` from line 17:

```cmake
add_library(game_rendering STATIC
    rendering/EnvironmentDebugSync.cpp
    rendering/MaterialTextureLibrary.cpp
    rendering/RuntimeSceneRenderer.cpp
)
```

- [ ] **Step 2: Delete RuntimeCameraMath files**

```bash
git rm src/game/rendering/RuntimeCameraMath.h src/game/rendering/RuntimeCameraMath.cpp
```

- [ ] **Step 3: Verify no remaining references to deleted files**

Run: `grep -r "RuntimeCameraMath" src/`
Expected: No matches

- [ ] **Step 4: Verify no runtime references to CameraComponent (only editor/test may remain)**

Run: `grep -r "CameraComponent" src/game/ src/engine/`
Expected: No matches in `src/game/` or `src/engine/`. The component header files can remain for now but should have no consumers.

Run: `grep -r "PrimaryCameraTag" src/game/ src/engine/`
Expected: No matches in `src/game/` or `src/engine/`.

- [ ] **Step 5: If CameraComponent.h and PrimaryCameraTag.h have zero consumers, delete them**

Check editor references:
Run: `grep -r "CameraComponent\|PrimaryCameraTag" src/editor/`
Expected: No matches (all migrated in Task 11). If confirmed:

```bash
git rm src/game/components/CameraComponent.h src/game/components/PrimaryCameraTag.h
```

- [ ] **Step 6: Full build and test**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build && ctest --test-dir build -V`
Expected: All targets compile. All tests pass.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "Remove deprecated RuntimeCameraMath, CameraComponent, and PrimaryCameraTag"
```

---

### Task 13: Final Verification

**Files:** None (verification only)

- [ ] **Step 1: Run all camera-specific tests**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && ctest --test-dir build -R "camera|transition|shake|fov" -V`
Expected: All 5 tests PASS

- [ ] **Step 2: Run the full test suite**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && ctest --test-dir build -V`
Expected: All tests PASS

- [ ] **Step 3: Build all executables**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike && cmake --build build --target level-editor && cmake --build build --target procedural-model-viewer`
Expected: All three executables compile successfully

- [ ] **Step 4: Launch level editor and verify camera works**

Run: `cd /Users/ozan/Projects/gsd-3d-roguelike && ./build/level-editor &`
- Open a scene file
- Start runtime preview
- Verify: Mouse look works, WASD movement works, torch follows camera
- Verify: No visual regressions in rendering

- [ ] **Step 5: Commit verification note (if any fixes were needed)**

If any issues were found and fixed, commit them:
```bash
git add -A
git commit -m "Fix issues found during camera module verification"
```
