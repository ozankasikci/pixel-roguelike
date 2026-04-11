# Particle System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement a CPU-simulated, instanced-rendered particle system with data-driven definitions, strategy-based architecture, and full pipeline integration.

**Architecture:** Engine layer (`src/engine/particles/`) provides simulation (SoA pool, emitter lifecycle) and rendering (instanced billboards). Game layer integrates through ContentRegistry (`.particle` definitions), ECS (component + system), and a module following the existing door/checkpoint module pattern. Particles render between the scene pass and post-processing in the existing `SceneRenderPipeline`.

**Tech Stack:** C++20, OpenGL 4.1, GLSL 410, EnTT ECS, GLM math

---

## File Map

### New Files — Engine Layer (`src/engine/particles/`)

| File | Responsibility |
|------|---------------|
| `ParticleTypes.h` | Enums (`BlendMode`, `SimulationSpace`), `ParticleRenderBatch` POD struct |
| `ValueAnimator.h` | Template interface + `Constant<T>`, `Lerp<T>`, `ColorGradient`, `FloatCurve` (header-only) |
| `EmitterShape.h` | Interface + `PointShape`, `SphereShape`, `ConeShape` (header-only) |
| `ForceFunction.h` | Interface + `GravityForce`, `DragForce` (header-only) |
| `ParticlePool.h/.cpp` | SoA storage arrays, swap-and-pop lifecycle, instance buffer packing |
| `ParticleEmitter.h/.cpp` | Lifecycle loop (spawn/update/kill/sort/pack), holds strategies |
| `ParticleRenderer.h/.cpp` | VAO/VBO setup, instanced draw calls, shader binding, GL state |

### New Files — Game Layer

| File | Responsibility |
|------|---------------|
| `src/game/particles/ParticleEmitterDefinition.h/.cpp` | Definition struct, `.particle` file parser, parent resolution |
| `src/game/components/ParticleEmitterComponent.h` | ECS component (emitterId + enabled flag) |
| `src/game/modules/particles/ParticleModule.h/.cpp` | Module registration, keyword registration |
| `src/game/modules/particles/ParticleSpawner.h/.cpp` | Entity creation from LevelDef |
| `src/game/modules/particles/ParticleSerializer.h/.cpp` | Scene file serialization |
| `src/game/modules/particles/ParticleSystem.h/.cpp` | Per-frame emitter update, render batch collection |
| `src/game/modules/particles/CMakeLists.txt` | Module build target |

### New Files — Shaders

| File | Responsibility |
|------|---------------|
| `assets/shaders/game/particles.vert` | Billboard expansion, per-instance attributes |
| `assets/shaders/game/particles.frag` | Texture sampling, soft particles, HDR output |

### New Files — Content

| File | Responsibility |
|------|---------------|
| `assets/particles/dust_motes.particle` | Ambient floating dust |
| `assets/particles/torch_sparks.particle` | Directional spark emitter |

### New Files — Tests

| File | Responsibility |
|------|---------------|
| `tests/engine/test_value_animator.cpp` | Constant, Lerp, ColorGradient, FloatCurve |
| `tests/engine/test_emitter_shape.cpp` | Point, Sphere, Cone position/direction sampling |
| `tests/engine/test_force_function.cpp` | Gravity, Drag |
| `tests/engine/test_particle_pool.cpp` | Spawn, kill, swap-and-pop, capacity |
| `tests/engine/test_particle_emitter.cpp` | Full lifecycle: spawn, update, kill, sort |
| `tests/game/test_particle_definition.cpp` | Parse, parent inheritance, roundtrip |

### Modified Files

| File | Change |
|------|--------|
| `src/engine/CMakeLists.txt` | Add `engine_particles` library |
| `src/engine/rendering/SceneRenderPipeline.h` | Add `renderParticlePass()`, add `particleBatches` to `SceneRenderInput` |
| `src/engine/rendering/SceneRenderPipeline.cpp` | Call `renderParticlePass()` between scene and post-process |
| `src/engine/rendering/TextureUnits.h` | Add `kParticleTexture = 17` |
| `src/game/CMakeLists.txt` | Add definition files to `game_content`, add `modules/particles` subdirectory |
| `src/game/content/ContentRegistry.h` | Add particle emitter map + find/load methods |
| `src/game/content/ContentRegistry.cpp` | Load `.particle` files in `loadDefaults()` |
| `src/game/rendering/RuntimeSceneRenderer.cpp` | Collect particle batches into `SceneRenderInput` |
| `apps/runtime/CMakeLists.txt` | Link `game_module_particles` |
| `apps/level_editor/CMakeLists.txt` | Link `game_module_particles` |
| `src/editor/CMakeLists.txt` | Link `game_module_particles` |
| `tests/engine/CMakeLists.txt` | Add particle test targets |
| `tests/game/CMakeLists.txt` | Add definition test target |

---

### Task 1: Engine Particles CMake Target + Type Foundations

**Files:**
- Create: `src/engine/particles/ParticleTypes.h`
- Create: `src/engine/particles/ValueAnimator.h`
- Modify: `src/engine/CMakeLists.txt`

This task creates the `engine_particles` library target and the foundational types that everything else depends on: enums, batch struct, and the ValueAnimator template with four implementations.

- [ ] **Step 1: Create ParticleTypes.h**

```cpp
// src/engine/particles/ParticleTypes.h
#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace particles {

enum class BlendMode : uint8_t { Additive, AlphaBlend };
enum class SimulationSpace : uint8_t { World, Local };
enum class EmitterState : uint8_t { Playing, Stopping, Stopped };

// Per-instance data uploaded to GPU each frame. Tightly packed for glBufferSubData.
struct alignas(4) ParticleInstance {
    glm::vec3 position;   // 12 bytes
    uint32_t colorPacked; // 4 bytes (RGBA8)
    float size;           // 4 bytes
    float rotation;       // 4 bytes
    float normalizedAge;  // 4 bytes
};                        // 28 bytes total
static_assert(sizeof(ParticleInstance) == 28, "ParticleInstance must be 28 bytes");

// Engine-layer render data passed from game system to SceneRenderPipeline.
struct ParticleRenderBatch {
    const ParticleInstance* instances = nullptr;
    int count = 0;
    BlendMode blendMode = BlendMode::Additive;
    GLuint texture = 0;
    float emissiveStrength = 1.0f;
    float softParticleFade = 0.5f;
};

inline uint32_t packColorRGBA8(const glm::vec4& c) {
    auto toByte = [](float f) -> uint8_t {
        return static_cast<uint8_t>(glm::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f);
    };
    return (uint32_t(toByte(c.r)))
         | (uint32_t(toByte(c.g)) << 8)
         | (uint32_t(toByte(c.b)) << 16)
         | (uint32_t(toByte(c.a)) << 24);
}

} // namespace particles
```

- [ ] **Step 2: Create ValueAnimator.h**

```cpp
// src/engine/particles/ValueAnimator.h
#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace particles {

template<typename T>
class ValueAnimator {
public:
    virtual ~ValueAnimator() = default;
    virtual T evaluate(float t) const = 0;
};

// --- Constant<T> ---
template<typename T>
class Constant : public ValueAnimator<T> {
public:
    explicit Constant(T value) : value_(value) {}
    T evaluate(float /*t*/) const override { return value_; }
private:
    T value_;
};

// --- Lerp<T> ---
template<typename T>
class Lerp : public ValueAnimator<T> {
public:
    Lerp(T start, T end) : start_(start), end_(end) {}
    T evaluate(float t) const override {
        const float clamped = glm::clamp(t, 0.0f, 1.0f);
        return start_ + (end_ - start_) * clamped;
    }
private:
    T start_;
    T end_;
};

// --- ColorGradient ---
// Specialization for vec4 with multiple color stops.
class ColorGradient : public ValueAnimator<glm::vec4> {
public:
    using Stop = std::pair<float, glm::vec4>;

    explicit ColorGradient(std::vector<Stop> stops) : stops_(std::move(stops)) {
        std::sort(stops_.begin(), stops_.end(),
                  [](const Stop& a, const Stop& b) { return a.first < b.first; });
    }

    glm::vec4 evaluate(float t) const override {
        if (stops_.empty()) return glm::vec4(1.0f);
        if (stops_.size() == 1 || t <= stops_.front().first) return stops_.front().second;
        if (t >= stops_.back().first) return stops_.back().second;

        for (std::size_t i = 0; i + 1 < stops_.size(); ++i) {
            if (t >= stops_[i].first && t <= stops_[i + 1].first) {
                const float range = stops_[i + 1].first - stops_[i].first;
                const float local = (range > 0.0f) ? (t - stops_[i].first) / range : 0.0f;
                return glm::mix(stops_[i].second, stops_[i + 1].second, local);
            }
        }
        return stops_.back().second;
    }

private:
    std::vector<Stop> stops_;
};

// --- FloatCurve ---
// Piecewise-linear keyframe animation for float values.
class FloatCurve : public ValueAnimator<float> {
public:
    using Keyframe = std::pair<float, float>;

    explicit FloatCurve(std::vector<Keyframe> keyframes) : keyframes_(std::move(keyframes)) {
        std::sort(keyframes_.begin(), keyframes_.end(),
                  [](const Keyframe& a, const Keyframe& b) { return a.first < b.first; });
    }

    float evaluate(float t) const override {
        if (keyframes_.empty()) return 0.0f;
        if (keyframes_.size() == 1 || t <= keyframes_.front().first) return keyframes_.front().second;
        if (t >= keyframes_.back().first) return keyframes_.back().second;

        for (std::size_t i = 0; i + 1 < keyframes_.size(); ++i) {
            if (t >= keyframes_[i].first && t <= keyframes_[i + 1].first) {
                const float range = keyframes_[i + 1].first - keyframes_[i].first;
                const float local = (range > 0.0f) ? (t - keyframes_[i].first) / range : 0.0f;
                return glm::mix(keyframes_[i].second, keyframes_[i + 1].second, local);
            }
        }
        return keyframes_.back().second;
    }

private:
    std::vector<Keyframe> keyframes_;
};

} // namespace particles
```

- [ ] **Step 3: Add engine_particles to CMakeLists.txt**

In `src/engine/CMakeLists.txt`, append after the `engine_camera` block:

```cmake
add_library(engine_particles STATIC
    particles/ParticlePool.cpp
    particles/ParticleEmitter.cpp
    particles/ParticleRenderer.cpp
)
target_include_directories(engine_particles PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_particles PUBLIC engine_rendering glm::glm)
```

Note: `ParticlePool.cpp`, `ParticleEmitter.cpp`, and `ParticleRenderer.cpp` don't exist yet — create empty placeholder files so CMake configures:

```cpp
// src/engine/particles/ParticlePool.cpp
#include "engine/particles/ParticlePool.h"

// src/engine/particles/ParticleEmitter.cpp
#include "engine/particles/ParticleEmitter.h"

// src/engine/particles/ParticleRenderer.cpp
#include "engine/particles/ParticleRenderer.h"
```

And minimal placeholder headers:

```cpp
// src/engine/particles/ParticlePool.h
#pragma once

// src/engine/particles/ParticleEmitter.h
#pragma once

// src/engine/particles/ParticleRenderer.h
#pragma once
```

- [ ] **Step 4: Verify CMake configures**

Run: `cd build && cmake ..`
Expected: Configures without errors, `engine_particles` target created.

- [ ] **Step 5: Commit**

```bash
git add src/engine/particles/ src/engine/CMakeLists.txt
git commit -m "Add engine_particles library with ParticleTypes and ValueAnimator"
```

---

### Task 2: EmitterShape and ForceFunction Strategy Interfaces

**Files:**
- Create: `src/engine/particles/EmitterShape.h`
- Create: `src/engine/particles/ForceFunction.h`

- [ ] **Step 1: Create EmitterShape.h**

```cpp
// src/engine/particles/EmitterShape.h
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include <random>

namespace particles {

class EmitterShape {
public:
    virtual ~EmitterShape() = default;
    virtual glm::vec3 samplePosition(std::mt19937& rng) const = 0;
    virtual glm::vec3 sampleDirection(std::mt19937& rng) const = 0;
};

// Uniform random point on unit sphere (Marsaglia method).
inline glm::vec3 randomOnUnitSphere(std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    glm::vec3 v;
    float lengthSq;
    do {
        v = glm::vec3(dist(rng), dist(rng), dist(rng));
        lengthSq = glm::dot(v, v);
    } while (lengthSq > 1.0f || lengthSq < 1e-6f);
    return v / std::sqrt(lengthSq);
}

// --- PointShape ---
class PointShape : public EmitterShape {
public:
    glm::vec3 samplePosition(std::mt19937& /*rng*/) const override {
        return glm::vec3(0.0f);
    }
    glm::vec3 sampleDirection(std::mt19937& rng) const override {
        return randomOnUnitSphere(rng);
    }
};

// --- SphereShape ---
class SphereShape : public EmitterShape {
public:
    explicit SphereShape(float radius, bool surfaceOnly = false)
        : radius_(radius), surfaceOnly_(surfaceOnly) {}

    glm::vec3 samplePosition(std::mt19937& rng) const override {
        glm::vec3 dir = randomOnUnitSphere(rng);
        if (surfaceOnly_) {
            return dir * radius_;
        }
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        // Cube root for uniform volume distribution
        float r = radius_ * std::cbrt(dist(rng));
        return dir * r;
    }

    glm::vec3 sampleDirection(std::mt19937& rng) const override {
        // Direction = outward from center (same as position direction)
        glm::vec3 pos = samplePosition(rng);
        float len = glm::length(pos);
        return (len > 1e-6f) ? pos / len : randomOnUnitSphere(rng);
    }

private:
    float radius_;
    bool surfaceOnly_;
};

// --- ConeShape ---
// Spawns at the base disc, direction within the cone angle.
class ConeShape : public EmitterShape {
public:
    ConeShape(float angleDegrees, float baseRadius)
        : cosAngle_(std::cos(glm::radians(angleDegrees))), baseRadius_(baseRadius) {}

    glm::vec3 samplePosition(std::mt19937& rng) const override {
        std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
        std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
        float angle = angleDist(rng);
        float r = baseRadius_ * std::sqrt(radiusDist(rng)); // sqrt for uniform disc
        return glm::vec3(r * std::cos(angle), 0.0f, r * std::sin(angle));
    }

    glm::vec3 sampleDirection(std::mt19937& rng) const override {
        // Random direction within the cone around +Y axis
        std::uniform_real_distribution<float> cosThetaDist(cosAngle_, 1.0f);
        std::uniform_real_distribution<float> phiDist(0.0f, glm::two_pi<float>());
        float cosTheta = cosThetaDist(rng);
        float sinTheta = std::sqrt(1.0f - cosTheta * cosTheta);
        float phi = phiDist(rng);
        return glm::normalize(glm::vec3(sinTheta * std::cos(phi),
                                         cosTheta,
                                         sinTheta * std::sin(phi)));
    }

private:
    float cosAngle_;
    float baseRadius_;
};

} // namespace particles
```

- [ ] **Step 2: Create ForceFunction.h**

```cpp
// src/engine/particles/ForceFunction.h
#pragma once

#include <glm/glm.hpp>

#include <algorithm>

namespace particles {

class ForceFunction {
public:
    virtual ~ForceFunction() = default;
    virtual glm::vec3 apply(const glm::vec3& position,
                            const glm::vec3& velocity,
                            float dt) const = 0;
};

// --- GravityForce ---
class GravityForce : public ForceFunction {
public:
    explicit GravityForce(glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f))
        : gravity_(gravity) {}

    glm::vec3 apply(const glm::vec3& /*position*/,
                    const glm::vec3& velocity,
                    float dt) const override {
        return velocity + gravity_ * dt;
    }

private:
    glm::vec3 gravity_;
};

// --- DragForce ---
class DragForce : public ForceFunction {
public:
    explicit DragForce(float coefficient) : coefficient_(coefficient) {}

    glm::vec3 apply(const glm::vec3& /*position*/,
                    const glm::vec3& velocity,
                    float dt) const override {
        float factor = std::max(0.0f, 1.0f - coefficient_ * dt);
        return velocity * factor;
    }

private:
    float coefficient_;
};

} // namespace particles
```

- [ ] **Step 3: Commit**

```bash
git add src/engine/particles/EmitterShape.h src/engine/particles/ForceFunction.h
git commit -m "Add EmitterShape and ForceFunction strategy interfaces with built-in implementations"
```

---

### Task 3: Strategy Unit Tests

**Files:**
- Create: `tests/engine/test_value_animator.cpp`
- Create: `tests/engine/test_emitter_shape.cpp`
- Create: `tests/engine/test_force_function.cpp`
- Modify: `tests/engine/CMakeLists.txt`

- [ ] **Step 1: Write test_value_animator.cpp**

```cpp
// tests/engine/test_value_animator.cpp
#include "engine/particles/ValueAnimator.h"

#include <cassert>
#include <cmath>

static bool nearEq(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

static bool nearEqVec4(const glm::vec4& a, const glm::vec4& b, float eps = 1e-4f) {
    return nearEq(a.x, b.x, eps) && nearEq(a.y, b.y, eps) &&
           nearEq(a.z, b.z, eps) && nearEq(a.w, b.w, eps);
}

int main() {
    using namespace particles;

    // Constant<float>
    {
        Constant<float> c(5.0f);
        assert(nearEq(c.evaluate(0.0f), 5.0f));
        assert(nearEq(c.evaluate(0.5f), 5.0f));
        assert(nearEq(c.evaluate(1.0f), 5.0f));
    }

    // Lerp<float>
    {
        Lerp<float> l(0.0f, 10.0f);
        assert(nearEq(l.evaluate(0.0f), 0.0f));
        assert(nearEq(l.evaluate(0.5f), 5.0f));
        assert(nearEq(l.evaluate(1.0f), 10.0f));
        // Clamps beyond [0,1]
        assert(nearEq(l.evaluate(-0.5f), 0.0f));
        assert(nearEq(l.evaluate(1.5f), 10.0f));
    }

    // Lerp<vec4>
    {
        Lerp<glm::vec4> l(glm::vec4(1, 0, 0, 1), glm::vec4(0, 0, 1, 0));
        auto mid = l.evaluate(0.5f);
        assert(nearEqVec4(mid, glm::vec4(0.5f, 0.0f, 0.5f, 0.5f)));
    }

    // ColorGradient
    {
        ColorGradient g({
            {0.0f, glm::vec4(1, 1, 0, 1)},
            {0.5f, glm::vec4(1, 0, 0, 1)},
            {1.0f, glm::vec4(0, 0, 0, 0)},
        });
        assert(nearEqVec4(g.evaluate(0.0f), glm::vec4(1, 1, 0, 1)));
        assert(nearEqVec4(g.evaluate(0.25f), glm::vec4(1, 0.5f, 0, 1)));
        assert(nearEqVec4(g.evaluate(0.5f), glm::vec4(1, 0, 0, 1)));
        assert(nearEqVec4(g.evaluate(1.0f), glm::vec4(0, 0, 0, 0)));
    }

    // ColorGradient with unsorted stops (should auto-sort)
    {
        ColorGradient g({
            {1.0f, glm::vec4(0, 0, 0, 0)},
            {0.0f, glm::vec4(1, 1, 1, 1)},
        });
        assert(nearEqVec4(g.evaluate(0.5f), glm::vec4(0.5f, 0.5f, 0.5f, 0.5f)));
    }

    // ColorGradient edge cases
    {
        // Empty gradient
        ColorGradient empty({});
        assert(nearEqVec4(empty.evaluate(0.5f), glm::vec4(1.0f)));

        // Single stop
        ColorGradient single({{0.5f, glm::vec4(0.3f, 0.6f, 0.9f, 1.0f)}});
        assert(nearEqVec4(single.evaluate(0.0f), glm::vec4(0.3f, 0.6f, 0.9f, 1.0f)));
        assert(nearEqVec4(single.evaluate(1.0f), glm::vec4(0.3f, 0.6f, 0.9f, 1.0f)));
    }

    // FloatCurve
    {
        FloatCurve curve({
            {0.0f, 0.1f},
            {0.5f, 1.0f},
            {1.0f, 0.0f},
        });
        assert(nearEq(curve.evaluate(0.0f), 0.1f));
        assert(nearEq(curve.evaluate(0.25f), 0.55f));
        assert(nearEq(curve.evaluate(0.5f), 1.0f));
        assert(nearEq(curve.evaluate(0.75f), 0.5f));
        assert(nearEq(curve.evaluate(1.0f), 0.0f));
    }

    // FloatCurve edge cases
    {
        FloatCurve empty({});
        assert(nearEq(empty.evaluate(0.5f), 0.0f));

        FloatCurve single({{0.5f, 3.0f}});
        assert(nearEq(single.evaluate(0.0f), 3.0f));
        assert(nearEq(single.evaluate(1.0f), 3.0f));
    }

    return 0;
}
```

- [ ] **Step 2: Write test_emitter_shape.cpp**

```cpp
// tests/engine/test_emitter_shape.cpp
#include "engine/particles/EmitterShape.h"

#include <cassert>
#include <cmath>

int main() {
    using namespace particles;
    std::mt19937 rng(42);

    // PointShape: position is always origin
    {
        PointShape shape;
        for (int i = 0; i < 100; ++i) {
            auto pos = shape.samplePosition(rng);
            assert(pos.x == 0.0f && pos.y == 0.0f && pos.z == 0.0f);
        }
        // Direction should be unit length
        for (int i = 0; i < 100; ++i) {
            auto dir = shape.sampleDirection(rng);
            float len = glm::length(dir);
            assert(std::fabs(len - 1.0f) < 0.01f);
        }
    }

    // SphereShape: position within radius
    {
        SphereShape shape(2.0f, false);
        for (int i = 0; i < 200; ++i) {
            auto pos = shape.samplePosition(rng);
            assert(glm::length(pos) <= 2.0f + 0.01f);
        }
    }

    // SphereShape surface only: position on radius
    {
        SphereShape shape(3.0f, true);
        for (int i = 0; i < 200; ++i) {
            auto pos = shape.samplePosition(rng);
            assert(std::fabs(glm::length(pos) - 3.0f) < 0.01f);
        }
    }

    // ConeShape: position on base disc within radius
    {
        ConeShape shape(30.0f, 0.5f);
        for (int i = 0; i < 200; ++i) {
            auto pos = shape.samplePosition(rng);
            assert(pos.y == 0.0f); // base disc is on XZ plane
            float r = std::sqrt(pos.x * pos.x + pos.z * pos.z);
            assert(r <= 0.5f + 0.01f);
        }
    }

    // ConeShape: direction within cone angle from +Y
    {
        ConeShape shape(45.0f, 0.1f);
        float cosLimit = std::cos(glm::radians(45.0f));
        for (int i = 0; i < 200; ++i) {
            auto dir = shape.sampleDirection(rng);
            assert(std::fabs(glm::length(dir) - 1.0f) < 0.01f);
            float cosAngle = glm::dot(dir, glm::vec3(0.0f, 1.0f, 0.0f));
            assert(cosAngle >= cosLimit - 0.01f);
        }
    }

    return 0;
}
```

- [ ] **Step 3: Write test_force_function.cpp**

```cpp
// tests/engine/test_force_function.cpp
#include "engine/particles/ForceFunction.h"

#include <cassert>
#include <cmath>

static bool nearEq(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

int main() {
    using namespace particles;

    // GravityForce: adds gravity * dt to velocity
    {
        GravityForce gravity(glm::vec3(0.0f, -10.0f, 0.0f));
        auto result = gravity.apply(glm::vec3(0.0f), glm::vec3(1.0f, 0.0f, 0.0f), 0.1f);
        assert(nearEq(result.x, 1.0f));
        assert(nearEq(result.y, -1.0f));
        assert(nearEq(result.z, 0.0f));
    }

    // DragForce: scales velocity by (1 - coefficient * dt)
    {
        DragForce drag(2.0f);
        auto result = drag.apply(glm::vec3(0.0f), glm::vec3(10.0f, 0.0f, 0.0f), 0.1f);
        // factor = 1 - 2.0 * 0.1 = 0.8
        assert(nearEq(result.x, 8.0f));
    }

    // DragForce: clamped so factor never goes negative
    {
        DragForce drag(100.0f);
        auto result = drag.apply(glm::vec3(0.0f), glm::vec3(5.0f, 5.0f, 5.0f), 0.1f);
        // factor = max(0, 1 - 100*0.1) = 0
        assert(nearEq(result.x, 0.0f));
        assert(nearEq(result.y, 0.0f));
        assert(nearEq(result.z, 0.0f));
    }

    return 0;
}
```

- [ ] **Step 4: Add tests to CMakeLists.txt**

Append to `tests/engine/CMakeLists.txt`:

```cmake
pixel_roguelike_add_test(test_value_animator
    SOURCES test_value_animator.cpp
    LIBRARIES glm::glm
    LABELS engine particles
)

pixel_roguelike_add_test(test_emitter_shape
    SOURCES test_emitter_shape.cpp
    LIBRARIES glm::glm
    LABELS engine particles
)

pixel_roguelike_add_test(test_force_function
    SOURCES test_force_function.cpp
    LIBRARIES glm::glm
    LABELS engine particles
)
```

- [ ] **Step 5: Build and run tests**

Run: `cd build && cmake .. && cmake --build . --target test_value_animator test_emitter_shape test_force_function && ctest -R "test_(value_animator|emitter_shape|force_function)" -V`
Expected: All 3 tests PASS.

- [ ] **Step 6: Commit**

```bash
git add tests/engine/test_value_animator.cpp tests/engine/test_emitter_shape.cpp tests/engine/test_force_function.cpp tests/engine/CMakeLists.txt
git commit -m "Add strategy tests for ValueAnimator, EmitterShape, ForceFunction"
```

---

### Task 4: ParticlePool

**Files:**
- Create: `src/engine/particles/ParticlePool.h` (replace placeholder)
- Create: `src/engine/particles/ParticlePool.cpp` (replace placeholder)
- Create: `tests/engine/test_particle_pool.cpp`
- Modify: `tests/engine/CMakeLists.txt`

- [ ] **Step 1: Write test_particle_pool.cpp**

```cpp
// tests/engine/test_particle_pool.cpp
#include "engine/particles/ParticlePool.h"

#include <cassert>
#include <cmath>

int main() {
    using namespace particles;

    // Basic spawn and count
    {
        ParticlePool pool(64);
        assert(pool.aliveCount() == 0);
        assert(pool.capacity() == 64);

        pool.spawn(glm::vec3(1, 2, 3), glm::vec3(0, 1, 0), glm::vec4(1), 0.5f, 2.0f, 0.0f);
        assert(pool.aliveCount() == 1);
        assert(pool.positions()[0] == glm::vec3(1, 2, 3));
        assert(pool.velocities()[0] == glm::vec3(0, 1, 0));
        assert(std::fabs(pool.lifetimes()[0] - 2.0f) < 1e-5f);
    }

    // Kill via swap-and-pop
    {
        ParticlePool pool(64);
        pool.spawn(glm::vec3(1, 0, 0), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f);
        pool.spawn(glm::vec3(2, 0, 0), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f);
        pool.spawn(glm::vec3(3, 0, 0), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f);
        assert(pool.aliveCount() == 3);

        pool.kill(0); // Swaps with last (index 2)
        assert(pool.aliveCount() == 2);
        // Particle at index 0 should now be what was at index 2
        assert(pool.positions()[0] == glm::vec3(3, 0, 0));
    }

    // Capacity enforcement
    {
        ParticlePool pool(2);
        pool.spawn(glm::vec3(0), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f);
        pool.spawn(glm::vec3(0), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f);
        assert(pool.aliveCount() == 2);

        // Should not crash or add beyond capacity
        pool.spawn(glm::vec3(0), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f);
        assert(pool.aliveCount() == 2); // unchanged
    }

    // Instance buffer packing (direct order for additive)
    {
        ParticlePool pool(64);
        pool.spawn(glm::vec3(1, 0, 0), glm::vec3(0), glm::vec4(1, 0, 0, 1), 0.5f, 5.0f, 0.0f);
        pool.ages()[0] = 2.5f; // half of lifetime=5

        std::vector<ParticleInstance> instances(1);
        pool.packInstances(instances.data(), pool.aliveCount());
        assert(instances[0].position == glm::vec3(1, 0, 0));
        assert(std::fabs(instances[0].size - 0.5f) < 1e-5f);
        assert(std::fabs(instances[0].normalizedAge - 0.5f) < 1e-5f);
    }

    // Sorted packing for alpha blend
    {
        ParticlePool pool(64);
        pool.spawn(glm::vec3(0, 0, -5), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f); // far
        pool.spawn(glm::vec3(0, 0, -1), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f); // near
        pool.spawn(glm::vec3(0, 0, -3), glm::vec3(0), glm::vec4(1), 0.1f, 1.0f, 0.0f); // mid

        std::vector<uint32_t> indices(3);
        pool.sortByDistance(glm::vec3(0, 0, 0), indices.data());
        // Back-to-front: farthest first
        assert(pool.positions()[indices[0]].z == -5.0f);
        assert(pool.positions()[indices[1]].z == -3.0f);
        assert(pool.positions()[indices[2]].z == -1.0f);

        std::vector<ParticleInstance> instances(3);
        pool.packInstancesSorted(instances.data(), indices.data(), 3);
        assert(instances[0].position.z == -5.0f);
        assert(instances[1].position.z == -3.0f);
        assert(instances[2].position.z == -1.0f);
    }

    return 0;
}
```

- [ ] **Step 2: Implement ParticlePool.h**

```cpp
// src/engine/particles/ParticlePool.h
#pragma once

#include "engine/particles/ParticleTypes.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace particles {

class ParticlePool {
public:
    explicit ParticlePool(int capacity);

    int capacity() const { return capacity_; }
    int aliveCount() const { return aliveCount_; }

    // Spawn a new particle. Returns false if pool is full.
    bool spawn(const glm::vec3& position,
               const glm::vec3& velocity,
               const glm::vec4& color,
               float size,
               float lifetime,
               float rotationSpeed);

    // Kill particle at index (swap-and-pop with last alive).
    void kill(int index);

    // Pack alive particles into instance buffer (direct order).
    void packInstances(ParticleInstance* out, int count) const;

    // Sort alive particles back-to-front by squared distance to camera.
    // Writes sorted indices into `out` (must be at least aliveCount_ elements).
    void sortByDistance(const glm::vec3& cameraPos, uint32_t* out) const;

    // Pack alive particles into instance buffer in sorted order.
    void packInstancesSorted(ParticleInstance* out, const uint32_t* sortedIndices, int count) const;

    // SoA array accessors
    glm::vec3* positions() { return positions_.data(); }
    glm::vec3* velocities() { return velocities_.data(); }
    glm::vec4* colors() { return colors_.data(); }
    float* sizes() { return sizes_.data(); }
    float* ages() { return ages_.data(); }
    float* lifetimes() { return lifetimes_.data(); }
    float* rotations() { return rotations_.data(); }
    float* rotationSpeeds() { return rotationSpeeds_.data(); }

    const glm::vec3* positions() const { return positions_.data(); }
    const float* ages() const { return ages_.data(); }
    const float* lifetimes() const { return lifetimes_.data(); }

private:
    void packOne(ParticleInstance& out, int index) const;

    int capacity_;
    int aliveCount_ = 0;

    std::vector<glm::vec3> positions_;
    std::vector<glm::vec3> velocities_;
    std::vector<glm::vec4> colors_;
    std::vector<float> sizes_;
    std::vector<float> ages_;
    std::vector<float> lifetimes_;
    std::vector<float> rotations_;
    std::vector<float> rotationSpeeds_;
};

} // namespace particles
```

- [ ] **Step 3: Implement ParticlePool.cpp**

```cpp
// src/engine/particles/ParticlePool.cpp
#include "engine/particles/ParticlePool.h"

#include <algorithm>
#include <numeric>

namespace particles {

ParticlePool::ParticlePool(int capacity)
    : capacity_(capacity)
    , positions_(capacity)
    , velocities_(capacity)
    , colors_(capacity)
    , sizes_(capacity)
    , ages_(capacity)
    , lifetimes_(capacity)
    , rotations_(capacity)
    , rotationSpeeds_(capacity) {
}

bool ParticlePool::spawn(const glm::vec3& position,
                         const glm::vec3& velocity,
                         const glm::vec4& color,
                         float size,
                         float lifetime,
                         float rotationSpeed) {
    if (aliveCount_ >= capacity_) return false;

    const int i = aliveCount_;
    positions_[i] = position;
    velocities_[i] = velocity;
    colors_[i] = color;
    sizes_[i] = size;
    ages_[i] = 0.0f;
    lifetimes_[i] = lifetime;
    rotations_[i] = 0.0f;
    rotationSpeeds_[i] = rotationSpeed;
    ++aliveCount_;
    return true;
}

void ParticlePool::kill(int index) {
    if (index < 0 || index >= aliveCount_) return;
    const int last = aliveCount_ - 1;
    if (index != last) {
        positions_[index] = positions_[last];
        velocities_[index] = velocities_[last];
        colors_[index] = colors_[last];
        sizes_[index] = sizes_[last];
        ages_[index] = ages_[last];
        lifetimes_[index] = lifetimes_[last];
        rotations_[index] = rotations_[last];
        rotationSpeeds_[index] = rotationSpeeds_[last];
    }
    --aliveCount_;
}

void ParticlePool::packOne(ParticleInstance& out, int index) const {
    out.position = positions_[index];
    out.colorPacked = packColorRGBA8(colors_[index]);
    out.size = sizes_[index];
    out.rotation = rotations_[index];
    out.normalizedAge = (lifetimes_[index] > 0.0f)
                            ? ages_[index] / lifetimes_[index]
                            : 1.0f;
}

void ParticlePool::packInstances(ParticleInstance* out, int count) const {
    const int n = std::min(count, aliveCount_);
    for (int i = 0; i < n; ++i) {
        packOne(out[i], i);
    }
}

void ParticlePool::sortByDistance(const glm::vec3& cameraPos, uint32_t* out) const {
    std::iota(out, out + aliveCount_, 0u);

    // Insertion sort: nearly O(n) for mostly-sorted data (particles rarely reorder).
    for (int i = 1; i < aliveCount_; ++i) {
        uint32_t key = out[i];
        float keyDist = glm::dot(positions_[key] - cameraPos, positions_[key] - cameraPos);
        int j = i - 1;
        while (j >= 0) {
            float jDist = glm::dot(positions_[out[j]] - cameraPos, positions_[out[j]] - cameraPos);
            if (jDist >= keyDist) break; // Back-to-front: farthest first
            out[j + 1] = out[j];
            --j;
        }
        out[j + 1] = key;
    }
}

void ParticlePool::packInstancesSorted(ParticleInstance* out,
                                       const uint32_t* sortedIndices,
                                       int count) const {
    for (int i = 0; i < count; ++i) {
        packOne(out[i], static_cast<int>(sortedIndices[i]));
    }
}

} // namespace particles
```

- [ ] **Step 4: Add pool test to CMakeLists.txt**

Append to `tests/engine/CMakeLists.txt`:

```cmake
pixel_roguelike_add_test(test_particle_pool
    SOURCES test_particle_pool.cpp
    LIBRARIES engine_particles
    LABELS engine particles
)
```

- [ ] **Step 5: Build and run test**

Run: `cd build && cmake .. && cmake --build . --target test_particle_pool && ctest -R test_particle_pool -V`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/engine/particles/ParticlePool.h src/engine/particles/ParticlePool.cpp tests/engine/test_particle_pool.cpp tests/engine/CMakeLists.txt
git commit -m "Add ParticlePool with SoA layout, swap-and-pop, and insertion sort"
```

---

### Task 5: ParticleEmitter

**Files:**
- Create: `src/engine/particles/ParticleEmitter.h` (replace placeholder)
- Create: `src/engine/particles/ParticleEmitter.cpp` (replace placeholder)
- Create: `tests/engine/test_particle_emitter.cpp`
- Modify: `tests/engine/CMakeLists.txt`

- [ ] **Step 1: Write test_particle_emitter.cpp**

```cpp
// tests/engine/test_particle_emitter.cpp
#include "engine/particles/ParticleEmitter.h"
#include "engine/particles/EmitterShape.h"
#include "engine/particles/ForceFunction.h"
#include "engine/particles/ValueAnimator.h"

#include <cassert>
#include <cmath>

int main() {
    using namespace particles;

    // Basic emission at constant rate
    {
        ParticleEmitterConfig config;
        config.maxParticles = 128;
        config.emissionRate = 100.0f;
        config.lifetimeMin = 1.0f;
        config.lifetimeMax = 1.0f;
        config.initialSpeedMin = 1.0f;
        config.initialSpeedMax = 1.0f;

        ParticleEmitter emitter(config);
        emitter.setShape(std::make_unique<PointShape>());
        emitter.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        emitter.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));

        // After 0.1s at 100/s, expect ~10 particles
        emitter.update(0.1f, glm::mat4(1.0f), glm::vec3(0.0f));
        assert(emitter.pool().aliveCount() >= 9 && emitter.pool().aliveCount() <= 11);
    }

    // Particles die after lifetime
    {
        ParticleEmitterConfig config;
        config.maxParticles = 64;
        config.emissionRate = 10.0f;
        config.lifetimeMin = 0.5f;
        config.lifetimeMax = 0.5f;
        config.initialSpeedMin = 0.0f;
        config.initialSpeedMax = 0.0f;

        ParticleEmitter emitter(config);
        emitter.setShape(std::make_unique<PointShape>());
        emitter.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        emitter.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));

        emitter.update(0.1f, glm::mat4(1.0f), glm::vec3(0.0f)); // spawn ~1
        int afterSpawn = emitter.pool().aliveCount();
        assert(afterSpawn > 0);

        // Advance past lifetime
        for (int i = 0; i < 10; ++i) {
            emitter.update(0.1f, glm::mat4(1.0f), glm::vec3(0.0f));
        }
        // Particles from the first frame should be dead now (0.5s lifetime, 1.0s elapsed)
        // But new particles keep spawning, so count should be stable
    }

    // Gravity affects velocity
    {
        ParticleEmitterConfig config;
        config.maxParticles = 4;
        config.emissionRate = 1000.0f; // burst spawn
        config.lifetimeMin = 10.0f;
        config.lifetimeMax = 10.0f;
        config.initialSpeedMin = 0.0f;
        config.initialSpeedMax = 0.0f;

        ParticleEmitter emitter(config);
        emitter.setShape(std::make_unique<PointShape>());
        emitter.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        emitter.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));
        emitter.addForce(std::make_unique<GravityForce>(glm::vec3(0.0f, -10.0f, 0.0f)));

        emitter.update(1.0f, glm::mat4(1.0f), glm::vec3(0.0f));
        // After 1s of -10 gravity with 0 initial speed:
        // velocity = 0 + (-10)*1 = -10, position = 0 + (-10)*1 = -10
        // (Euler integration is approximate)
        for (int i = 0; i < emitter.pool().aliveCount(); ++i) {
            assert(emitter.pool().positions()[i].y < -1.0f);
        }
    }

    // Emitter state transitions
    {
        ParticleEmitterConfig config;
        config.maxParticles = 32;
        config.emissionRate = 100.0f;
        config.lifetimeMin = 0.1f;
        config.lifetimeMax = 0.1f;

        ParticleEmitter emitter(config);
        emitter.setShape(std::make_unique<PointShape>());
        emitter.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        emitter.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));

        assert(emitter.state() == EmitterState::Playing);

        emitter.stop(); // transition to Stopping
        assert(emitter.state() == EmitterState::Stopping);

        // Advance until all particles die
        for (int i = 0; i < 20; ++i) {
            emitter.update(0.05f, glm::mat4(1.0f), glm::vec3(0.0f));
        }
        assert(emitter.state() == EmitterState::Stopped);
        assert(emitter.pool().aliveCount() == 0);
    }

    // Capacity limit: pool never exceeds maxParticles
    {
        ParticleEmitterConfig config;
        config.maxParticles = 8;
        config.emissionRate = 10000.0f;
        config.lifetimeMin = 100.0f;
        config.lifetimeMax = 100.0f;

        ParticleEmitter emitter(config);
        emitter.setShape(std::make_unique<PointShape>());
        emitter.setColorAnimator(std::make_unique<Constant<glm::vec4>>(glm::vec4(1.0f)));
        emitter.setSizeAnimator(std::make_unique<Constant<float>>(0.1f));

        emitter.update(1.0f, glm::mat4(1.0f), glm::vec3(0.0f));
        assert(emitter.pool().aliveCount() == 8);
    }

    return 0;
}
```

- [ ] **Step 2: Implement ParticleEmitter.h**

```cpp
// src/engine/particles/ParticleEmitter.h
#pragma once

#include "engine/particles/EmitterShape.h"
#include "engine/particles/ForceFunction.h"
#include "engine/particles/ParticlePool.h"
#include "engine/particles/ParticleTypes.h"
#include "engine/particles/ValueAnimator.h"

#include <glm/glm.hpp>

#include <memory>
#include <random>
#include <vector>

namespace particles {

struct BurstEvent {
    float time;
    int count;
};

struct ParticleEmitterConfig {
    int maxParticles = 256;
    float emissionRate = 10.0f;
    std::vector<BurstEvent> bursts;
    bool looping = true;
    float duration = 0.0f; // 0 = infinite
    SimulationSpace simulationSpace = SimulationSpace::World;
    BlendMode blendMode = BlendMode::Additive;
    float initialSpeedMin = 1.0f;
    float initialSpeedMax = 3.0f;
    float lifetimeMin = 0.5f;
    float lifetimeMax = 2.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;
    bool warmUp = false;
    float softParticleFade = 0.5f;
    float emissiveStrength = 1.0f;
    GLuint texture = 0;
};

class ParticleEmitter {
public:
    explicit ParticleEmitter(const ParticleEmitterConfig& config);

    void setShape(std::unique_ptr<EmitterShape> shape);
    void addForce(std::unique_ptr<ForceFunction> force);
    void setColorAnimator(std::unique_ptr<ValueAnimator<glm::vec4>> animator);
    void setSizeAnimator(std::unique_ptr<ValueAnimator<float>> animator);
    void setOpacityAnimator(std::unique_ptr<ValueAnimator<float>> animator);

    void update(float dt, const glm::mat4& emitterTransform, const glm::vec3& cameraPos);
    void stop();
    void warmUp(float duration, float fixedDt = 1.0f / 60.0f);

    EmitterState state() const { return state_; }
    const ParticlePool& pool() const { return pool_; }
    const ParticleEmitterConfig& config() const { return config_; }

    // Build render batch for this emitter's current state.
    // Returns batch with pointer into instanceBuffer_.
    ParticleRenderBatch buildRenderBatch();

private:
    void spawnParticles(float dt, const glm::mat4& emitterTransform);
    void updateParticles(float dt);
    void killDeadParticles();

    ParticleEmitterConfig config_;
    ParticlePool pool_;
    EmitterState state_ = EmitterState::Playing;
    float emitterAge_ = 0.0f;
    float emissionAccumulator_ = 0.0f;
    std::mt19937 rng_{42};
    glm::vec3 lastCameraPos_{0.0f};

    std::unique_ptr<EmitterShape> shape_;
    std::vector<std::unique_ptr<ForceFunction>> forces_;
    std::unique_ptr<ValueAnimator<glm::vec4>> colorAnimator_;
    std::unique_ptr<ValueAnimator<float>> sizeAnimator_;
    std::unique_ptr<ValueAnimator<float>> opacityAnimator_;

    // Reused per-frame buffers
    std::vector<ParticleInstance> instanceBuffer_;
    std::vector<uint32_t> sortIndices_;
};

} // namespace particles
```

- [ ] **Step 3: Implement ParticleEmitter.cpp**

```cpp
// src/engine/particles/ParticleEmitter.cpp
#include "engine/particles/ParticleEmitter.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace particles {

ParticleEmitter::ParticleEmitter(const ParticleEmitterConfig& config)
    : config_(config)
    , pool_(config.maxParticles)
    , instanceBuffer_(config.maxParticles)
    , sortIndices_(config.maxParticles) {
}

void ParticleEmitter::setShape(std::unique_ptr<EmitterShape> shape) {
    shape_ = std::move(shape);
}

void ParticleEmitter::addForce(std::unique_ptr<ForceFunction> force) {
    forces_.push_back(std::move(force));
}

void ParticleEmitter::setColorAnimator(std::unique_ptr<ValueAnimator<glm::vec4>> animator) {
    colorAnimator_ = std::move(animator);
}

void ParticleEmitter::setSizeAnimator(std::unique_ptr<ValueAnimator<float>> animator) {
    sizeAnimator_ = std::move(animator);
}

void ParticleEmitter::setOpacityAnimator(std::unique_ptr<ValueAnimator<float>> animator) {
    opacityAnimator_ = std::move(animator);
}

void ParticleEmitter::stop() {
    if (state_ == EmitterState::Playing) {
        state_ = EmitterState::Stopping;
    }
}

void ParticleEmitter::warmUp(float duration, float fixedDt) {
    const int steps = static_cast<int>(duration / fixedDt);
    for (int i = 0; i < steps; ++i) {
        update(fixedDt, glm::mat4(1.0f), glm::vec3(0.0f));
    }
}

void ParticleEmitter::update(float dt, const glm::mat4& emitterTransform,
                              const glm::vec3& cameraPos) {
    lastCameraPos_ = cameraPos;

    if (state_ == EmitterState::Stopped) return;

    // Spawn new particles (only in Playing state)
    if (state_ == EmitterState::Playing) {
        spawnParticles(dt, emitterTransform);
        emitterAge_ += dt;

        // Check if emitter duration has elapsed
        if (config_.duration > 0.0f && emitterAge_ >= config_.duration) {
            if (config_.looping) {
                emitterAge_ = 0.0f;
            } else {
                state_ = EmitterState::Stopping;
            }
        }
    }

    // Update all alive particles
    updateParticles(dt);

    // Kill dead particles
    killDeadParticles();

    // Transition from Stopping -> Stopped when all dead
    if (state_ == EmitterState::Stopping && pool_.aliveCount() == 0) {
        state_ = EmitterState::Stopped;
    }
}

void ParticleEmitter::spawnParticles(float dt, const glm::mat4& emitterTransform) {
    if (!shape_) return;

    emissionAccumulator_ += config_.emissionRate * dt;

    // Check burst events
    int burstCount = 0;
    for (const auto& burst : config_.bursts) {
        if (emitterAge_ <= burst.time && emitterAge_ + dt > burst.time) {
            burstCount += burst.count;
        }
    }

    int toSpawn = static_cast<int>(emissionAccumulator_) + burstCount;
    emissionAccumulator_ -= std::floor(emissionAccumulator_);

    std::uniform_real_distribution<float> speedDist(config_.initialSpeedMin, config_.initialSpeedMax);
    std::uniform_real_distribution<float> lifeDist(config_.lifetimeMin, config_.lifetimeMax);
    std::uniform_real_distribution<float> rotSpeedDist(config_.rotationSpeedMin, config_.rotationSpeedMax);

    for (int i = 0; i < toSpawn; ++i) {
        glm::vec3 localPos = shape_->samplePosition(rng_);
        glm::vec3 localDir = shape_->sampleDirection(rng_);

        glm::vec3 worldPos;
        glm::vec3 worldVel;

        if (config_.simulationSpace == SimulationSpace::World) {
            worldPos = glm::vec3(emitterTransform * glm::vec4(localPos, 1.0f));
            worldVel = glm::vec3(emitterTransform * glm::vec4(localDir, 0.0f)) * speedDist(rng_);
        } else {
            worldPos = localPos;
            worldVel = localDir * speedDist(rng_);
        }

        glm::vec4 color = colorAnimator_ ? colorAnimator_->evaluate(0.0f) : glm::vec4(1.0f);
        float size = sizeAnimator_ ? sizeAnimator_->evaluate(0.0f) : 0.1f;
        float lifetime = lifeDist(rng_);
        float rotSpeed = rotSpeedDist(rng_);

        pool_.spawn(worldPos, worldVel, color, size, lifetime, rotSpeed);
    }
}

void ParticleEmitter::updateParticles(float dt) {
    const int count = pool_.aliveCount();
    auto* pos = pool_.positions();
    auto* vel = pool_.velocities();
    auto* colors = pool_.colors();
    auto* sizes = pool_.sizes();
    auto* ages = pool_.ages();
    auto* lifetimes = pool_.lifetimes();
    auto* rotations = pool_.rotations();
    auto* rotSpeeds = pool_.rotationSpeeds();

    for (int i = 0; i < count; ++i) {
        ages[i] += dt;

        // Apply forces
        for (const auto& force : forces_) {
            vel[i] = force->apply(pos[i], vel[i], dt);
        }

        // Integrate position
        pos[i] += vel[i] * dt;

        // Integrate rotation
        rotations[i] += rotSpeeds[i] * dt;

        // Animate properties over normalized lifetime
        float t = (lifetimes[i] > 0.0f) ? ages[i] / lifetimes[i] : 1.0f;
        t = glm::clamp(t, 0.0f, 1.0f);

        if (colorAnimator_) colors[i] = colorAnimator_->evaluate(t);
        if (sizeAnimator_) sizes[i] = sizeAnimator_->evaluate(t);
        if (opacityAnimator_) colors[i].a *= opacityAnimator_->evaluate(t);
    }
}

void ParticleEmitter::killDeadParticles() {
    const auto* ages = pool_.ages();
    const auto* lifetimes = pool_.lifetimes();

    // Iterate backward for safe swap-and-pop
    for (int i = pool_.aliveCount() - 1; i >= 0; --i) {
        if (ages[i] >= lifetimes[i]) {
            pool_.kill(i);
        }
    }
}

ParticleRenderBatch ParticleEmitter::buildRenderBatch() {
    const int count = pool_.aliveCount();
    if (count == 0) return {};

    if (config_.blendMode == BlendMode::AlphaBlend) {
        pool_.sortByDistance(lastCameraPos_, sortIndices_.data());
        pool_.packInstancesSorted(instanceBuffer_.data(), sortIndices_.data(), count);
    } else {
        pool_.packInstances(instanceBuffer_.data(), count);
    }

    ParticleRenderBatch batch;
    batch.instances = instanceBuffer_.data();
    batch.count = count;
    batch.blendMode = config_.blendMode;
    batch.texture = config_.texture;
    batch.emissiveStrength = config_.emissiveStrength;
    batch.softParticleFade = config_.softParticleFade;
    return batch;
}

} // namespace particles
```

- [ ] **Step 4: Add emitter test to CMakeLists.txt**

Append to `tests/engine/CMakeLists.txt`:

```cmake
pixel_roguelike_add_test(test_particle_emitter
    SOURCES test_particle_emitter.cpp
    LIBRARIES engine_particles
    LABELS engine particles
)
```

- [ ] **Step 5: Build and run tests**

Run: `cd build && cmake .. && cmake --build . --target test_particle_emitter && ctest -R test_particle_emitter -V`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add src/engine/particles/ParticleEmitter.h src/engine/particles/ParticleEmitter.cpp tests/engine/test_particle_emitter.cpp tests/engine/CMakeLists.txt
git commit -m "Add ParticleEmitter with lifecycle loop, strategy integration, and render batch building"
```

---

### Task 6: Particle Shaders

**Files:**
- Create: `assets/shaders/game/particles.vert`
- Create: `assets/shaders/game/particles.frag`
- Modify: `src/engine/rendering/TextureUnits.h`

- [ ] **Step 1: Add particle texture unit**

In `src/engine/rendering/TextureUnits.h`, add after `kCsmShadowMap`:

```cpp
constexpr int kParticleTexture = 17;
constexpr int kParticleDepth = 18;
```

- [ ] **Step 2: Create particles.vert**

```glsl
// assets/shaders/game/particles.vert
#version 410 core

// Per-vertex (shared quad)
layout(location = 0) in vec2 aQuadPos; // (-0.5,-0.5) to (0.5,0.5)

// Per-instance (one per particle)
layout(location = 1) in vec3 aWorldPos;
layout(location = 2) in vec4 aColor;     // RGBA8 unpacked by GL
layout(location = 3) in float aSize;
layout(location = 4) in float aRotation;
layout(location = 5) in float aNormalizedAge;

uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;
out float vLinearDepth;

void main() {
    // Extract camera right/up from view matrix columns
    vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 cameraUp    = vec3(uView[0][1], uView[1][1], uView[2][1]);

    // Apply billboard rotation around view direction
    float cosR = cos(aRotation);
    float sinR = sin(aRotation);
    vec2 rotated = vec2(
        aQuadPos.x * cosR - aQuadPos.y * sinR,
        aQuadPos.x * sinR + aQuadPos.y * cosR
    );

    // Billboard world position
    vec3 worldPos = aWorldPos
                  + cameraRight * rotated.x * aSize
                  + cameraUp    * rotated.y * aSize;

    vec4 viewPos = uView * vec4(worldPos, 1.0);
    gl_Position = uProjection * viewPos;

    vUV = aQuadPos + 0.5; // Map [-0.5,0.5] -> [0,1]
    vColor = aColor;
    vLinearDepth = -viewPos.z; // Positive distance from camera
}
```

- [ ] **Step 3: Create particles.frag**

```glsl
// assets/shaders/game/particles.frag
#version 410 core

in vec2 vUV;
in vec4 vColor;
in float vLinearDepth;

uniform sampler2D uParticleTexture;
uniform sampler2D uSceneDepth;
uniform int uHasTexture;
uniform float uEmissiveStrength;
uniform float uSoftFadeRange;
uniform float uNearPlane;
uniform float uFarPlane;
uniform vec2 uViewportSize;

layout(location = 0) out vec4 FragColor;

float linearizeDepth(float d) {
    return (2.0 * uNearPlane * uFarPlane) / (uFarPlane + uNearPlane - (2.0 * d - 1.0) * (uFarPlane - uNearPlane));
}

void main() {
    vec4 texColor;
    if (uHasTexture != 0) {
        texColor = texture(uParticleTexture, vUV);
    } else {
        // Default radial falloff (soft circle)
        float dist = length(vUV - vec2(0.5));
        float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
        texColor = vec4(1.0, 1.0, 1.0, alpha);
    }

    vec4 color = texColor * vColor;

    // Soft particle depth fade
    if (uSoftFadeRange > 0.0) {
        vec2 screenUV = gl_FragCoord.xy / uViewportSize;
        float sceneDepthRaw = texture(uSceneDepth, screenUV).r;
        float sceneDepthLinear = linearizeDepth(sceneDepthRaw);
        float fade = clamp((sceneDepthLinear - vLinearDepth) / uSoftFadeRange, 0.0, 1.0);
        color.a *= fade;
    }

    // Emissive strength for bloom participation
    color.rgb *= uEmissiveStrength;

    FragColor = color;
}
```

- [ ] **Step 4: Commit**

```bash
git add assets/shaders/game/particles.vert assets/shaders/game/particles.frag src/engine/rendering/TextureUnits.h
git commit -m "Add particle billboard shaders with soft particles and HDR emissive output"
```

---

### Task 7: ParticleRenderer + Pipeline Integration

**Files:**
- Create: `src/engine/particles/ParticleRenderer.h` (replace placeholder)
- Create: `src/engine/particles/ParticleRenderer.cpp` (replace placeholder)
- Modify: `src/engine/rendering/SceneRenderPipeline.h`
- Modify: `src/engine/rendering/SceneRenderPipeline.cpp`

- [ ] **Step 1: Implement ParticleRenderer.h**

```cpp
// src/engine/particles/ParticleRenderer.h
#pragma once

#include "engine/particles/ParticleTypes.h"
#include "engine/rendering/core/Shader.h"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace particles {

class ParticleRenderer {
public:
    ParticleRenderer() = default;
    ~ParticleRenderer();

    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    void init();
    void shutdown();

    // Render all particle batches. Called between scene pass and post-process.
    void render(const std::vector<ParticleRenderBatch>& batches,
                GLuint sceneDepthTexture,
                const glm::mat4& view,
                const glm::mat4& projection,
                float nearPlane,
                float farPlane,
                int viewportWidth,
                int viewportHeight);

private:
    void setupQuadVAO();
    void drawBatch(const ParticleRenderBatch& batch);

    std::unique_ptr<Shader> shader_;
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
    GLuint instanceVBO_ = 0;
};

} // namespace particles
```

- [ ] **Step 2: Implement ParticleRenderer.cpp**

```cpp
// src/engine/particles/ParticleRenderer.cpp
#include "engine/particles/ParticleRenderer.h"

#include "engine/rendering/TextureUnits.h"

#include <glad/gl.h>

namespace particles {

ParticleRenderer::~ParticleRenderer() {
    shutdown();
}

void ParticleRenderer::init() {
    shader_ = std::make_unique<Shader>("assets/shaders/game/particles.vert",
                                        "assets/shaders/game/particles.frag");
    setupQuadVAO();
}

void ParticleRenderer::shutdown() {
    if (quadVAO_) { glDeleteVertexArrays(1, &quadVAO_); quadVAO_ = 0; }
    if (quadVBO_) { glDeleteBuffers(1, &quadVBO_); quadVBO_ = 0; }
    if (instanceVBO_) { glDeleteBuffers(1, &instanceVBO_); instanceVBO_ = 0; }
    shader_.reset();
}

void ParticleRenderer::setupQuadVAO() {
    // Triangle strip quad: 4 vertices
    float quadVertices[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f,
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);
    glGenBuffers(1, &instanceVBO_);

    glBindVertexArray(quadVAO_);

    // Quad vertex positions (attribute 0)
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);

    // Instance buffer (attributes 1-5)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    // Pre-allocate a reasonable size; will be orphaned each frame
    glBufferData(GL_ARRAY_BUFFER, 2048 * sizeof(ParticleInstance), nullptr, GL_STREAM_DRAW);

    const GLsizei stride = sizeof(ParticleInstance);

    // Attribute 1: vec3 position
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, position)));
    glVertexAttribDivisor(1, 1);

    // Attribute 2: vec4 color (packed RGBA8, read as normalized unsigned bytes)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, colorPacked)));
    glVertexAttribDivisor(2, 1);

    // Attribute 3: float size
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, size)));
    glVertexAttribDivisor(3, 1);

    // Attribute 4: float rotation
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, rotation)));
    glVertexAttribDivisor(4, 1);

    // Attribute 5: float normalizedAge
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(ParticleInstance, normalizedAge)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ParticleRenderer::render(const std::vector<ParticleRenderBatch>& batches,
                               GLuint sceneDepthTexture,
                               const glm::mat4& view,
                               const glm::mat4& projection,
                               float nearPlane,
                               float farPlane,
                               int viewportWidth,
                               int viewportHeight) {
    if (batches.empty() || !shader_) return;

    shader_->use();
    shader_->setMat4("uView", view);
    shader_->setMat4("uProjection", projection);
    shader_->setFloat("uNearPlane", nearPlane);
    shader_->setFloat("uFarPlane", farPlane);
    shader_->setVec2("uViewportSize", glm::vec2(viewportWidth, viewportHeight));

    // Bind scene depth for soft particles
    glActiveTexture(GL_TEXTURE0 + TextureUnits::kParticleDepth);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTexture);
    shader_->setInt("uSceneDepth", TextureUnits::kParticleDepth);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);  // Particles don't write depth
    glEnable(GL_BLEND);

    glBindVertexArray(quadVAO_);

    // Pass 1: Additive emitters (order doesn't matter)
    glBlendFunc(GL_ONE, GL_ONE);
    for (const auto& batch : batches) {
        if (batch.blendMode == BlendMode::Additive && batch.count > 0) {
            drawBatch(batch);
        }
    }

    // Pass 2: Alpha-blend emitters (already per-particle sorted back-to-front)
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const auto& batch : batches) {
        if (batch.blendMode == BlendMode::AlphaBlend && batch.count > 0) {
            drawBatch(batch);
        }
    }

    // Restore GL state
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void ParticleRenderer::drawBatch(const ParticleRenderBatch& batch) {
    // Upload instance data (buffer orphaning)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO_);
    const GLsizeiptr dataSize = batch.count * static_cast<GLsizeiptr>(sizeof(ParticleInstance));
    glBufferData(GL_ARRAY_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, dataSize, batch.instances);

    // Per-emitter uniforms
    shader_->setFloat("uEmissiveStrength", batch.emissiveStrength);
    shader_->setFloat("uSoftFadeRange", batch.softParticleFade);

    if (batch.texture != 0) {
        glActiveTexture(GL_TEXTURE0 + TextureUnits::kParticleTexture);
        glBindTexture(GL_TEXTURE_2D, batch.texture);
        shader_->setInt("uParticleTexture", TextureUnits::kParticleTexture);
        shader_->setInt("uHasTexture", 1);
    } else {
        shader_->setInt("uHasTexture", 0);
    }

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, batch.count);
}

} // namespace particles
```

- [ ] **Step 3: Add particle batches to SceneRenderInput**

In `src/engine/rendering/SceneRenderPipeline.h`, add the include and field:

After the existing includes, add:
```cpp
#include "engine/particles/ParticleTypes.h"
```

In the `SceneRenderInput` struct, add after `bool shadowsEnabled`:
```cpp
const std::vector<particles::ParticleRenderBatch>* particleBatches = nullptr;
```

- [ ] **Step 4: Add ParticleRenderer to SceneRenderPipeline**

In `src/engine/rendering/SceneRenderPipeline.h`, add include:
```cpp
#include "engine/particles/ParticleRenderer.h"
```

In the `SceneRenderPipeline` class, add private member:
```cpp
particles::ParticleRenderer particleRenderer_;
```

Add private method declaration:
```cpp
void renderParticlePass(const SceneRenderInput& input);
```

- [ ] **Step 5: Wire particle pass into render()**

In `src/engine/rendering/SceneRenderPipeline.cpp`:

In `init()`, add after `ensureFramebuffers(1280, 720);`:
```cpp
particleRenderer_.init();
```

In `shutdown()`, add at the top:
```cpp
particleRenderer_.shutdown();
```

In `render()`, add after `renderScenePass(...)` (line 94) and before `renderPostProcess(...)` (line 97):
```cpp
    renderParticlePass(culledInput);
```

Add the implementation at the end of the file (before `ensureFramebuffers`):

```cpp
void SceneRenderPipeline::renderParticlePass(const SceneRenderInput& input) {
    if (!input.particleBatches || input.particleBatches->empty()) return;

    // Particle pass renders into sceneFBO_ (same as scene pass).
    // sceneFBO_ is still bound from renderScenePass.
    sceneFBO_.bind();
    glViewport(0, 0, sceneFBO_.width(), sceneFBO_.height());

    particleRenderer_.render(*input.particleBatches,
                              sceneFBO_.depthTexture(),
                              input.viewMatrix,
                              input.projectionMatrix,
                              input.nearPlane,
                              input.farPlane,
                              sceneFBO_.width(),
                              sceneFBO_.height());

    sceneFBO_.unbind();
    glDisable(GL_DEPTH_TEST);
}
```

- [ ] **Step 6: Update engine_particles link dependency**

The `SceneRenderPipeline` (in `engine_rendering`) now includes `ParticleRenderer.h` (in `engine_particles`). Since `engine_particles` already depends on `engine_rendering`, this creates a cycle. To fix: move the include of ParticleRenderer out of the header and forward-declare, or restructure.

Better approach: `SceneRenderPipeline` should NOT own the `ParticleRenderer`. Instead, the render method accepts fully-formed batches, and the `ParticleRenderer` lives in the game layer. Adjust:

Remove `ParticleRenderer` member from `SceneRenderPipeline`. Instead, make `renderParticlePass()` a standalone call that the game layer invokes directly. The `SceneRenderPipeline::render()` method is modified to call a function pointer or the game layer calls the particle renderer between scene and post-process.

Actually, the cleanest fix: make `engine_rendering` depend on `engine_particles` instead of the reverse. `engine_particles` only needs `Shader` and `TextureUnits` from `engine_rendering`, and `engine_rendering` needs `ParticleRenderer` and `ParticleRenderBatch`. This is a peer dependency.

In `src/engine/CMakeLists.txt`, change `engine_particles` to:
```cmake
add_library(engine_particles STATIC
    particles/ParticlePool.cpp
    particles/ParticleEmitter.cpp
    particles/ParticleRenderer.cpp
)
target_include_directories(engine_particles PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_particles PUBLIC engine_core glm::glm)
target_link_libraries(engine_particles PRIVATE glad_gl)
```

And add `engine_particles` as a dependency of `engine_rendering`:
```cmake
target_link_libraries(engine_rendering PUBLIC engine_core engine_particles glm::glm)
```

This way `engine_particles` has no rendering dependency (just GLM + GLAD for GL types), and `engine_rendering` pulls in particles.

Move `Shader` includes out of `ParticleRenderer.h` — instead, forward-declare and include in `.cpp`. `ParticleRenderer.h` only needs `<glad/gl.h>` and `<glm/glm.hpp>` for the interface.

Update `ParticleRenderer.h`:
```cpp
#pragma once

#include "engine/particles/ParticleTypes.h"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <memory>
#include <vector>

class Shader;

namespace particles {
// ... (same interface, just forward-declare Shader)
```

And in `ParticleRenderer.cpp`:
```cpp
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/TextureUnits.h"
```

- [ ] **Step 7: Build and verify**

Run: `cd build && cmake .. && cmake --build .`
Expected: Full build succeeds, no circular dependencies.

- [ ] **Step 8: Commit**

```bash
git add src/engine/particles/ParticleRenderer.h src/engine/particles/ParticleRenderer.cpp src/engine/rendering/SceneRenderPipeline.h src/engine/rendering/SceneRenderPipeline.cpp src/engine/rendering/TextureUnits.h src/engine/CMakeLists.txt
git commit -m "Add ParticleRenderer with instanced billboards and integrate particle pass into SceneRenderPipeline"
```

---

### Task 8: ParticleEmitterDefinition + Parser

**Files:**
- Create: `src/game/particles/ParticleEmitterDefinition.h`
- Create: `src/game/particles/ParticleEmitterDefinition.cpp`
- Create: `tests/game/test_particle_definition.cpp`
- Modify: `src/game/CMakeLists.txt`
- Modify: `tests/game/CMakeLists.txt`

- [ ] **Step 1: Write test_particle_definition.cpp**

```cpp
// tests/game/test_particle_definition.cpp
#include "game/particles/ParticleEmitterDefinition.h"
#include "common/TestSupport.h"

#include <cassert>
#include <fstream>

int main() {
    // Parse a basic definition file
    {
        const auto def = loadParticleEmitterDefinition(PARTICLE_DEF_FILE);
        assert(def.id == "torch_sparks");
        assert(def.maxParticles.has_value() && *def.maxParticles == 256);
        assert(def.emissionRate.has_value() && test_support::nearlyEqual(*def.emissionRate, 30.0f));
        assert(def.looping.has_value() && *def.looping == true);
        assert(def.blendMode.has_value() && *def.blendMode == "additive");
        assert(def.lifetimeMin.has_value() && test_support::nearlyEqual(*def.lifetimeMin, 0.3f));
        assert(def.lifetimeMax.has_value() && test_support::nearlyEqual(*def.lifetimeMax, 0.8f));
        assert(def.shapeType.has_value() && *def.shapeType == "cone");
        assert(def.forces.size() == 2);
        assert(def.colorStops.size() == 3);
        assert(def.sizeKeyframes.size() == 2);
    }

    // Parse with parent inheritance
    {
        const auto parent = loadParticleEmitterDefinition(PARTICLE_PARENT_FILE);
        const auto child = loadParticleEmitterDefinition(PARTICLE_CHILD_FILE);
        assert(child.parent.has_value() && *child.parent == parent.id);

        std::unordered_map<std::string, ParticleEmitterDefinition> defs;
        defs.emplace(parent.id, parent);
        defs.emplace(child.id, child);

        const auto resolved = resolveParticleEmitterDefinition(child.id, defs);
        // Child overrides emission_rate but inherits parent's maxParticles
        assert(resolved.maxParticles == 512);
        assert(test_support::nearlyEqual(resolved.emissionRate, 50.0f));
    }

    // Roundtrip: write then read
    {
        ParticleEmitterDefinition def;
        def.id = "roundtrip_test";
        def.maxParticles = 128;
        def.emissionRate = 20.0f;
        def.blendMode = "alpha";
        def.lifetimeMin = 1.0f;
        def.lifetimeMax = 3.0f;
        def.shapeType = "sphere";
        def.shapeParam0 = 2.0f;

        const auto path = test_support::tempPath("roundtrip_test.particle");
        saveParticleEmitterDefinition(path.string(), def);
        const auto loaded = loadParticleEmitterDefinition(path.string());
        std::filesystem::remove(path);

        assert(loaded.id == def.id);
        assert(*loaded.maxParticles == 128);
        assert(test_support::nearlyEqual(*loaded.emissionRate, 20.0f));
        assert(*loaded.blendMode == "alpha");
        assert(*loaded.shapeType == "sphere");
    }

    return 0;
}
```

- [ ] **Step 2: Implement ParticleEmitterDefinition.h**

```cpp
// src/game/particles/ParticleEmitterDefinition.h
#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

struct ParticleForceDeclaration {
    std::string type;         // "gravity" or "drag"
    glm::vec3 value{0.0f};   // gravity vector or unused
    float coefficient = 0.0f; // drag coefficient
};

struct ParticleEmitterDefinition {
    std::string id;
    std::optional<std::string> parent;
    std::optional<int> maxParticles;
    std::optional<float> emissionRate;
    std::optional<bool> looping;
    std::optional<float> duration;
    std::optional<std::string> simulationSpace; // "world" or "local"
    std::optional<std::string> blendMode;       // "additive" or "alpha"
    std::optional<std::string> texture;
    std::optional<bool> warmUp;
    std::optional<float> softParticleFade;
    std::optional<float> emissiveStrength;
    std::optional<float> lifetimeMin;
    std::optional<float> lifetimeMax;
    std::optional<float> initialSpeedMin;
    std::optional<float> initialSpeedMax;
    std::optional<float> rotationSpeedMin;
    std::optional<float> rotationSpeedMax;
    std::optional<std::string> shapeType;       // "point", "sphere", "cone"
    std::optional<float> shapeParam0;           // shape-specific (radius or angle)
    std::optional<float> shapeParam1;           // shape-specific (baseRadius or surfaceOnly)

    std::vector<ParticleForceDeclaration> forces;
    std::vector<std::pair<float, glm::vec4>> colorStops;
    std::vector<std::pair<float, float>> sizeKeyframes;
};

// Resolved definition with all values concrete (defaults applied, parent merged).
struct ResolvedParticleEmitterDefinition {
    std::string id;
    int maxParticles = 256;
    float emissionRate = 10.0f;
    bool looping = true;
    float duration = 0.0f;
    std::string simulationSpace = "world";
    std::string blendMode = "additive";
    std::string texture;
    bool warmUp = false;
    float softParticleFade = 0.5f;
    float emissiveStrength = 1.0f;
    float lifetimeMin = 0.5f;
    float lifetimeMax = 2.0f;
    float initialSpeedMin = 1.0f;
    float initialSpeedMax = 3.0f;
    float rotationSpeedMin = 0.0f;
    float rotationSpeedMax = 0.0f;
    std::string shapeType = "point";
    float shapeParam0 = 0.0f;
    float shapeParam1 = 0.0f;
    std::vector<ParticleForceDeclaration> forces;
    std::vector<std::pair<float, glm::vec4>> colorStops;
    std::vector<std::pair<float, float>> sizeKeyframes;
};

ParticleEmitterDefinition loadParticleEmitterDefinition(const std::string& path);
void saveParticleEmitterDefinition(const std::string& path, const ParticleEmitterDefinition& def);
ResolvedParticleEmitterDefinition resolveParticleEmitterDefinition(
    const std::string& id,
    const std::unordered_map<std::string, ParticleEmitterDefinition>& definitions);
```

- [ ] **Step 3: Implement ParticleEmitterDefinition.cpp**

Implement the parser following the `loadMaterialDefinitionAsset()` pattern: line-by-line, tokenize, match key, parse values. Implement parent resolution following `resolveMaterialDefinition()` pattern. Implement serializer following `saveMaterialDefinitionAsset()` pattern.

The implementation uses `tokenizeRecord()`, `isCommentOrEmpty()`, and `throwParseError()` from `game/content/ParseUtils.h`. Full parsing for all keys: `id`, `parent`, `max_particles`, `emission_rate`, `looping`, `duration`, `simulation_space`, `blend_mode`, `texture`, `warm_up`, `soft_particle_fade`, `emissive_strength`, `lifetime` (2 floats), `initial_speed` (2 floats), `rotation_speed` (2 floats), `shape` (type + params), `force` (type + params), `color_over_lifetime` (time + rgba), `size_over_lifetime` (time + value).

- [ ] **Step 4: Add to game_content in CMakeLists.txt**

In `src/game/CMakeLists.txt`, add to the `game_content` source list:
```cmake
particles/ParticleEmitterDefinition.cpp
```

- [ ] **Step 5: Create test fixture files**

Create `tests/data/torch_sparks.particle`:
```
id torch_sparks
max_particles 256
emission_rate 30
looping true
blend_mode additive
lifetime 0.3 0.8
initial_speed 1.5 3.0
shape cone 25 0.05
force gravity 0 -4.0 0
force drag 0.5
color_over_lifetime 0.0 1.0 0.9 0.3 1.0
color_over_lifetime 0.5 1.0 0.5 0.1 0.8
color_over_lifetime 1.0 0.8 0.2 0.0 0.0
size_over_lifetime 0.0 0.04
size_over_lifetime 1.0 0.01
```

Create `tests/data/base_emitter.particle`:
```
id base_emitter
max_particles 512
emission_rate 10
looping true
blend_mode additive
lifetime 1.0 2.0
initial_speed 1.0 2.0
shape point
```

Create `tests/data/child_emitter.particle`:
```
id child_emitter
parent base_emitter
emission_rate 50
```

- [ ] **Step 6: Add test to CMakeLists.txt**

Append to `tests/game/CMakeLists.txt`:

```cmake
set(PARTICLE_DEF_FILE_DEFS
    PARTICLE_DEF_FILE="${CMAKE_SOURCE_DIR}/tests/data/torch_sparks.particle"
    PARTICLE_PARENT_FILE="${CMAKE_SOURCE_DIR}/tests/data/base_emitter.particle"
    PARTICLE_CHILD_FILE="${CMAKE_SOURCE_DIR}/tests/data/child_emitter.particle"
)

pixel_roguelike_add_test(test_particle_definition
    SOURCES test_particle_definition.cpp
    LIBRARIES game_content
    DEFINITIONS ${PARTICLE_DEF_FILE_DEFS}
    LABELS game particles
)
```

- [ ] **Step 7: Build and run test**

Run: `cd build && cmake .. && cmake --build . --target test_particle_definition && ctest -R test_particle_definition -V`
Expected: PASS

- [ ] **Step 8: Commit**

```bash
git add src/game/particles/ tests/data/*.particle tests/game/test_particle_definition.cpp tests/game/CMakeLists.txt src/game/CMakeLists.txt
git commit -m "Add ParticleEmitterDefinition with line-based parser and parent inheritance"
```

---

### Task 9: ContentRegistry Integration

**Files:**
- Modify: `src/game/content/ContentRegistry.h`
- Modify: `src/game/content/ContentRegistry.cpp`
- Create: `assets/particles/dust_motes.particle`
- Create: `assets/particles/torch_sparks.particle`

- [ ] **Step 1: Add particle map to ContentRegistry.h**

Add include:
```cpp
#include "game/particles/ParticleEmitterDefinition.h"
```

Add to the class:
```cpp
// Particle emitter definitions
const ParticleEmitterDefinition* findParticleEmitter(const std::string& id) const;
const std::unordered_map<std::string, ParticleEmitterDefinition>& particleEmitters() const { return particleEmitters_; }
void loadParticleEmittersFromDirectory(const std::string& relativeDirectory);
```

Add private member:
```cpp
std::unordered_map<std::string, ParticleEmitterDefinition> particleEmitters_;
```

- [ ] **Step 2: Implement in ContentRegistry.cpp**

Add `findParticleEmitter`:
```cpp
const ParticleEmitterDefinition* ContentRegistry::findParticleEmitter(const std::string& id) const {
    auto it = particleEmitters_.find(id);
    return it != particleEmitters_.end() ? &it->second : nullptr;
}
```

Add `loadParticleEmittersFromDirectory`:
```cpp
void ContentRegistry::loadParticleEmittersFromDirectory(const std::string& relativeDirectory) {
    const auto absDir = resolveProjectPath(relativeDirectory);
    for (const auto& path : sortedDefinitionFiles(relativeDirectory, ".particle")) {
        auto def = loadParticleEmitterDefinition(path);
        particleEmitters_.emplace(def.id, std::move(def));
    }
}
```

In `loadDefaults()`, add after the material loading block:
```cpp
loadParticleEmittersFromDirectory("assets/particles");
```

In `loadDefaults()` clear block, add:
```cpp
particleEmitters_.clear();
```

- [ ] **Step 3: Create sample particle definitions**

`assets/particles/dust_motes.particle`:
```
id dust_motes
max_particles 128
emission_rate 8
looping true
simulation_space world
blend_mode alpha
soft_particle_fade 0.3
emissive_strength 1.0
lifetime 4.0 8.0
initial_speed 0.05 0.15
rotation_speed -0.5 0.5
shape sphere 3.0
force gravity 0 0.02 0
force drag 0.3
color_over_lifetime 0.0 0.9 0.85 0.8 0.0
color_over_lifetime 0.2 0.9 0.85 0.8 0.4
color_over_lifetime 0.8 0.9 0.85 0.8 0.4
color_over_lifetime 1.0 0.9 0.85 0.8 0.0
size_over_lifetime 0.0 0.01
size_over_lifetime 0.5 0.02
size_over_lifetime 1.0 0.01
```

`assets/particles/torch_sparks.particle`:
```
id torch_sparks
max_particles 256
emission_rate 30
looping true
simulation_space world
blend_mode additive
soft_particle_fade 0.5
emissive_strength 2.0
lifetime 0.3 0.8
initial_speed 1.5 3.0
rotation_speed -1.0 1.0
shape cone 25 0.05
force gravity 0 -4.0 0
force drag 0.5
color_over_lifetime 0.0 1.0 0.9 0.3 1.0
color_over_lifetime 0.5 1.0 0.5 0.1 0.8
color_over_lifetime 1.0 0.8 0.2 0.0 0.0
size_over_lifetime 0.0 0.04
size_over_lifetime 1.0 0.01
```

- [ ] **Step 4: Build and verify**

Run: `cd build && cmake .. && cmake --build .`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/game/content/ContentRegistry.h src/game/content/ContentRegistry.cpp assets/particles/
git commit -m "Integrate particle definitions into ContentRegistry with directory loading"
```

---

### Task 10: Game Module — ParticleModule, Spawner, Serializer

**Files:**
- Create: `src/game/components/ParticleEmitterComponent.h`
- Create: `src/game/modules/particles/ParticleModule.h`
- Create: `src/game/modules/particles/ParticleModule.cpp`
- Create: `src/game/modules/particles/ParticleSpawner.h`
- Create: `src/game/modules/particles/ParticleSpawner.cpp`
- Create: `src/game/modules/particles/ParticleSerializer.h`
- Create: `src/game/modules/particles/ParticleSerializer.cpp`
- Create: `src/game/modules/particles/CMakeLists.txt`
- Modify: `src/game/CMakeLists.txt`

- [ ] **Step 1: Create ParticleEmitterComponent.h**

```cpp
// src/game/components/ParticleEmitterComponent.h
#pragma once

#include <string>

struct ParticleEmitterComponent {
    std::string emitterId;
    bool enabled = true;
};
```

- [ ] **Step 2: Create ParticleSpawner**

`src/game/modules/particles/ParticleSpawner.h`:
```cpp
#pragma once

#include <glm/glm.hpp>

class LevelBuilder;

namespace entt { enum class entity : unsigned int; using id_type = unsigned int; }

entt::entity spawnParticleEmitter(LevelBuilder& builder,
                                   const std::string& emitterId,
                                   const glm::vec3& position,
                                   const std::string& nodeId = {},
                                   const std::string& parentNodeId = {});
```

`src/game/modules/particles/ParticleSpawner.cpp`:
```cpp
#include "game/modules/particles/ParticleSpawner.h"

#include "game/components/ParticleEmitterComponent.h"
#include "game/level/LevelBuilder.h"

entt::entity spawnParticleEmitter(LevelBuilder& builder,
                                   const std::string& emitterId,
                                   const glm::vec3& position,
                                   const std::string& nodeId,
                                   const std::string& parentNodeId) {
    auto entity = builder.createTransformEntity(position);
    builder.registry().emplace<ParticleEmitterComponent>(entity,
        ParticleEmitterComponent{emitterId, true});

    if (!nodeId.empty()) {
        builder.attachNodeId(entity, nodeId);
    }

    builder.track(entity);
    return entity;
}
```

- [ ] **Step 3: Create ParticleSerializer**

`src/game/modules/particles/ParticleSerializer.h`:
```cpp
#pragma once

#include <string>
#include <vector>

struct LevelDef;

// Defined in LevelDef.h and registered via registerLevelDefKeyword
void registerParticleEmitterKeyword();
```

`src/game/modules/particles/ParticleSerializer.cpp`:
Implement the keyword registration for `particle_emitter` lines in `.scene` files. This uses the existing `registerLevelDefKeyword()` API from `LevelDef.h`. The parser reads `particle_emitter <emitterId> <px> <py> <pz>` and stores a placement struct. The serializer writes it back out. Since `LevelDef` doesn't have a particles vector yet, store particle placements in the module's own static storage accessed via the registry context, or add to LevelDef.

The simplest approach: add `std::vector<LevelParticleEmitterPlacement>` to `LevelDef`:

First, add to `src/game/level/LevelDef.h`:
```cpp
struct LevelParticleEmitterPlacement {
    std::string emitterId;
    glm::vec3 position{0.0f};
    std::string nodeId;
    std::string parentNodeId;
};
```

And in `LevelDef`:
```cpp
std::vector<LevelParticleEmitterPlacement> particleEmitters;
```

Then implement the serializer using `registerLevelDefKeyword()`.

- [ ] **Step 4: Create ParticleModule**

`src/game/modules/particles/ParticleModule.h`:
```cpp
#pragma once

void registerParticleModule();
```

`src/game/modules/particles/ParticleModule.cpp`:
```cpp
#include "game/modules/particles/ParticleModule.h"
#include "game/modules/particles/ParticleSerializer.h"

void registerParticleModule() {
    registerParticleEmitterKeyword();
}
```

- [ ] **Step 5: Create CMakeLists.txt**

`src/game/modules/particles/CMakeLists.txt`:
```cmake
add_library(game_module_particles STATIC
    ParticleModule.cpp
    ParticleSpawner.cpp
    ParticleSerializer.cpp
    ParticleSystem.cpp
)
target_include_directories(game_module_particles PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_particles PUBLIC
    engine_particles
    gameplay
)
```

Note: `ParticleSystem.cpp` doesn't exist yet — create a minimal placeholder:
```cpp
// src/game/modules/particles/ParticleSystem.cpp
#include "game/modules/particles/ParticleSystem.h"
```
```cpp
// src/game/modules/particles/ParticleSystem.h
#pragma once
```

- [ ] **Step 6: Wire into build system**

In `src/game/CMakeLists.txt`, add:
```cmake
add_subdirectory(modules/particles)
```

In `apps/runtime/CMakeLists.txt`, add `game_module_particles` to `target_link_libraries`.
In `apps/level_editor/CMakeLists.txt`, add `game_module_particles` to `target_link_libraries`.
In `src/editor/CMakeLists.txt`, add `game_module_particles` to `target_link_libraries`.

- [ ] **Step 7: Build and verify**

Run: `cd build && cmake .. && cmake --build .`
Expected: Build succeeds.

- [ ] **Step 8: Commit**

```bash
git add src/game/components/ParticleEmitterComponent.h src/game/modules/particles/ src/game/level/LevelDef.h src/game/CMakeLists.txt apps/runtime/CMakeLists.txt apps/level_editor/CMakeLists.txt src/editor/CMakeLists.txt
git commit -m "Add particle module with spawner, serializer, and keyword registration"
```

---

### Task 11: ParticleSystem + RuntimeSceneRenderer Integration

**Files:**
- Create: `src/game/modules/particles/ParticleSystem.h` (replace placeholder)
- Create: `src/game/modules/particles/ParticleSystem.cpp` (replace placeholder)
- Modify: `src/game/rendering/RuntimeSceneRenderer.h`
- Modify: `src/game/rendering/RuntimeSceneRenderer.cpp`

- [ ] **Step 1: Implement ParticleSystem**

`src/game/modules/particles/ParticleSystem.h`:
```cpp
#pragma once

#include "engine/particles/ParticleEmitter.h"
#include "engine/particles/ParticleTypes.h"
#include "engine/ecs/GameRegistry.h"

#include <entt/entt.hpp>

#include <memory>
#include <unordered_map>
#include <vector>

class ContentRegistry;

class ParticleUpdateSystem {
public:
    void init(const ContentRegistry& content);
    void update(GameRegistry& registry, float deltaTime, const glm::vec3& cameraPos);
    const std::vector<particles::ParticleRenderBatch>& renderBatches() const { return renderBatches_; }

private:
    void ensureEmitter(entt::entity entity, const std::string& emitterId);

    const ContentRegistry* content_ = nullptr;
    std::unordered_map<entt::entity, std::unique_ptr<particles::ParticleEmitter>> emitters_;
    std::vector<particles::ParticleRenderBatch> renderBatches_;
};
```

`src/game/modules/particles/ParticleSystem.cpp`:
```cpp
#include "game/modules/particles/ParticleSystem.h"

#include "game/components/ParticleEmitterComponent.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/particles/ParticleEmitterDefinition.h"

#include "engine/particles/EmitterShape.h"
#include "engine/particles/ForceFunction.h"
#include "engine/particles/ValueAnimator.h"

void ParticleUpdateSystem::init(const ContentRegistry& content) {
    content_ = &content;
}

void ParticleUpdateSystem::ensureEmitter(entt::entity entity, const std::string& emitterId) {
    if (emitters_.count(entity)) return;
    if (!content_) return;

    const auto* def = content_->findParticleEmitter(emitterId);
    if (!def) return;

    auto resolved = resolveParticleEmitterDefinition(emitterId, content_->particleEmitters());

    particles::ParticleEmitterConfig config;
    config.maxParticles = resolved.maxParticles;
    config.emissionRate = resolved.emissionRate;
    config.looping = resolved.looping;
    config.duration = resolved.duration;
    config.simulationSpace = (resolved.simulationSpace == "local")
        ? particles::SimulationSpace::Local
        : particles::SimulationSpace::World;
    config.blendMode = (resolved.blendMode == "alpha")
        ? particles::BlendMode::AlphaBlend
        : particles::BlendMode::Additive;
    config.initialSpeedMin = resolved.initialSpeedMin;
    config.initialSpeedMax = resolved.initialSpeedMax;
    config.lifetimeMin = resolved.lifetimeMin;
    config.lifetimeMax = resolved.lifetimeMax;
    config.rotationSpeedMin = resolved.rotationSpeedMin;
    config.rotationSpeedMax = resolved.rotationSpeedMax;
    config.warmUp = resolved.warmUp;
    config.softParticleFade = resolved.softParticleFade;
    config.emissiveStrength = resolved.emissiveStrength;

    auto emitter = std::make_unique<particles::ParticleEmitter>(config);

    // Configure shape
    if (resolved.shapeType == "sphere") {
        emitter->setShape(std::make_unique<particles::SphereShape>(resolved.shapeParam0));
    } else if (resolved.shapeType == "cone") {
        emitter->setShape(std::make_unique<particles::ConeShape>(resolved.shapeParam0, resolved.shapeParam1));
    } else {
        emitter->setShape(std::make_unique<particles::PointShape>());
    }

    // Configure forces
    for (const auto& f : resolved.forces) {
        if (f.type == "gravity") {
            emitter->addForce(std::make_unique<particles::GravityForce>(f.value));
        } else if (f.type == "drag") {
            emitter->addForce(std::make_unique<particles::DragForce>(f.coefficient));
        }
    }

    // Configure animators
    if (!resolved.colorStops.empty()) {
        emitter->setColorAnimator(std::make_unique<particles::ColorGradient>(resolved.colorStops));
    } else {
        emitter->setColorAnimator(std::make_unique<particles::Constant<glm::vec4>>(glm::vec4(1.0f)));
    }

    if (!resolved.sizeKeyframes.empty()) {
        emitter->setSizeAnimator(std::make_unique<particles::FloatCurve>(resolved.sizeKeyframes));
    } else {
        emitter->setSizeAnimator(std::make_unique<particles::Constant<float>>(0.1f));
    }

    if (resolved.warmUp) {
        float warmDuration = (resolved.duration > 0.0f) ? resolved.duration : resolved.lifetimeMax;
        emitter->warmUp(warmDuration);
    }

    emitters_.emplace(entity, std::move(emitter));
}

void ParticleUpdateSystem::update(GameRegistry& registry, float deltaTime,
                                   const glm::vec3& cameraPos) {
    renderBatches_.clear();

    auto view = registry.view<ParticleEmitterComponent, TransformComponent>();
    for (auto [entity, particle, transform] : view.each()) {
        if (!particle.enabled) continue;

        ensureEmitter(entity, particle.emitterId);
        auto it = emitters_.find(entity);
        if (it == emitters_.end()) continue;

        it->second->update(deltaTime, transform.modelMatrix(), cameraPos);

        auto batch = it->second->buildRenderBatch();
        if (batch.count > 0) {
            renderBatches_.push_back(batch);
        }
    }

    // Clean up emitters for destroyed entities
    for (auto it = emitters_.begin(); it != emitters_.end();) {
        if (!registry.valid(it->first) || !registry.all_of<ParticleEmitterComponent>(it->first)) {
            it = emitters_.erase(it);
        } else {
            ++it;
        }
    }
}
```

- [ ] **Step 2: Integrate into RuntimeSceneRenderer**

In `src/game/rendering/RuntimeSceneRenderer.h`, add include:
```cpp
#include "engine/particles/ParticleTypes.h"
```

Add member:
```cpp
mutable std::vector<particles::ParticleRenderBatch> particle_batches_;
```

In `src/game/rendering/RuntimeSceneRenderer.cpp`, in the `render()` method, after collecting lights/reflections and before calling `pipeline_.render()`:

Read particle batches from registry context:
```cpp
auto* particleSystem = registry.ctx().find<ParticleUpdateSystem*>();
if (particleSystem) {
    particle_batches_ = (*particleSystem)->renderBatches();
} else {
    particle_batches_.clear();
}
```

And when building `SceneRenderInput`, add:
```cpp
input.particleBatches = &particle_batches_;
```

- [ ] **Step 3: Register ParticleUpdateSystem in RuntimeGameSession**

In `RuntimeGameSession::rebuild()`, after content loading and before level building:
```cpp
auto& particleSystem = registry_.ctx().emplace<ParticleUpdateSystem*>();
// Initialize with content registry
```

And in the per-frame update, call `particleSystem->update(registry_, deltaTime, cameraPos)`.

The exact integration depends on how `RuntimeGameSession` calls systems. Follow the existing pattern for `PhysicsSystem` — it's called explicitly in `RuntimeGameSession::update()`.

- [ ] **Step 4: Build and verify**

Run: `cd build && cmake .. && cmake --build .`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add src/game/modules/particles/ParticleSystem.h src/game/modules/particles/ParticleSystem.cpp src/game/rendering/RuntimeSceneRenderer.h src/game/rendering/RuntimeSceneRenderer.cpp
git commit -m "Add ParticleUpdateSystem with per-frame emitter management and RuntimeSceneRenderer integration"
```

---

### Task 12: End-to-End Visual Test

**Files:**
- Modify: `assets/scenes/cathedral.scene` (add a particle emitter for visual testing)

- [ ] **Step 1: Add a particle emitter to the cathedral scene**

Append to `assets/scenes/cathedral.scene`:
```
particle_emitter dust_motes 0 2 -6
particle_emitter torch_sparks 3.5 2.5 6
```

- [ ] **Step 2: Ensure registerParticleModule() is called**

In `apps/runtime/main.cpp`, add:
```cpp
#include "game/modules/particles/ParticleModule.h"
```

And call `registerParticleModule()` alongside the other module registrations.

Do the same in `apps/level_editor/main.cpp`.

- [ ] **Step 3: Build and run the game**

Run: `cd build && cmake .. && cmake --build . --target pixel-roguelike && ./pixel-roguelike`
Expected: Load the cathedral scene. Dust motes float in the air, torch sparks emit from the specified position. Particles receive bloom from the post-processing pipeline. Soft particles fade near geometry.

- [ ] **Step 4: Build and run the editor**

Run: `cd build && cmake --build . --target level-editor && ./level-editor`
Expected: Open the cathedral scene. Particles visible in the editor viewport during preview mode. ParticleEmitterComponent shows in the entity inspector when a particle emitter entity is selected.

- [ ] **Step 5: Commit**

```bash
git add assets/scenes/cathedral.scene apps/runtime/main.cpp apps/level_editor/main.cpp
git commit -m "Add particle emitters to cathedral scene for visual testing"
```

---

### Task 13: Editor Integration

**Files:**
- Modify: `src/editor/scene/EditorSceneDocument.h`
- Modify: `src/editor/scene/EditorSceneDocument.cpp`
- Create: `src/editor/ui/inspectors/ParticleEmitterInspector.h`
- Create: `src/editor/ui/inspectors/ParticleEmitterInspector.cpp`
- Modify: `src/editor/CMakeLists.txt`

- [ ] **Step 1: Add ParticleEmitter to EditorSceneObjectKind**

In `src/editor/scene/EditorSceneDocument.h`:

Add to enum `EditorSceneObjectKind`:
```cpp
ParticleEmitter,
```

Add to the `EditorSceneObjectPayload` variant:
```cpp
LevelParticleEmitterPlacement
```

Add method to the class:
```cpp
int addParticleEmitter(const LevelParticleEmitterPlacement& placement);
```

- [ ] **Step 2: Implement addParticleEmitter**

Follow the pattern of `addCheckpoint()` or `addReflectionProbe()` in `EditorSceneDocument.cpp`.

- [ ] **Step 3: Create ParticleEmitterInspector**

`src/editor/ui/inspectors/ParticleEmitterInspector.h`:
```cpp
#pragma once

struct LevelParticleEmitterPlacement;
class EditorSceneDocument;
class EditorCommandStack;
struct EditorPendingCommand;
struct EditorSceneDocumentState;

void drawParticleEmitterInspector(LevelParticleEmitterPlacement& placement,
                                   EditorSceneDocument& document,
                                   EditorCommandStack& commandStack,
                                   EditorPendingCommand& pendingCommand,
                                   const EditorSceneDocumentState& beforeState);
```

`src/editor/ui/inspectors/ParticleEmitterInspector.cpp`:
Implement using `renderInspectorPropertyRow()` for:
- Emitter ID (combo dropdown populated from ContentRegistry particle emitters)
- Position (vec3)
- Enabled toggle

Follow the `LightInspector.cpp` pattern for undo/redo support.

- [ ] **Step 4: Add to editor CMakeLists.txt**

Add `ui/inspectors/ParticleEmitterInspector.cpp` to the editor source list in `src/editor/CMakeLists.txt`.

- [ ] **Step 5: Wire inspector into the main inspector dispatch**

In the inspector dispatch code (wherever `EditorSceneObjectKind` is switched on), add a case for `ParticleEmitter` that calls `drawParticleEmitterInspector()`.

- [ ] **Step 6: Build and verify**

Run: `cd build && cmake .. && cmake --build . --target level-editor && ./level-editor`
Expected: Can add particle emitters from the editor, see them in the outliner, edit properties in the inspector, and see live preview.

- [ ] **Step 7: Commit**

```bash
git add src/editor/scene/EditorSceneDocument.h src/editor/scene/EditorSceneDocument.cpp src/editor/ui/inspectors/ParticleEmitterInspector.h src/editor/ui/inspectors/ParticleEmitterInspector.cpp src/editor/CMakeLists.txt
git commit -m "Add particle emitter support to level editor with inspector and scene document integration"
```

---

### Task 14: Run Full Test Suite

- [ ] **Step 1: Run all particle tests**

Run: `cd build && cmake .. && cmake --build . && ctest -L particles -V`
Expected: All 6 particle tests pass (value_animator, emitter_shape, force_function, particle_pool, particle_emitter, particle_definition).

- [ ] **Step 2: Run full test suite for regressions**

Run: `ctest -V`
Expected: All existing tests still pass. No regressions.

- [ ] **Step 3: Final commit if any fixups needed**

Only if tests revealed issues that required fixes.
