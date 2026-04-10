# Camera Module Design

## Overview

A comprehensive camera module for the engine layer that decouples camera management from the player entity, provides a composable effect pipeline, and publishes a single authoritative `CameraState` that all consumers read.

**Motivation:** The current camera is a raw `CameraComponent` bolted onto the player entity via `PrimaryCameraTag`. There's no interpolation, no effects, no transitions -- and five separate systems reach into the component directly. This makes it impossible to add camera shake, scripted transitions, or FOV manipulation without scattering camera logic across the codebase.

**Approach:** CameraManager + lightweight effect layer (Approach 2 from brainstorming). Game code feeds a base state each frame. Effects modify it in a composable pipeline. All consumers read the final output.

## v1 Scope

**Included:**
- Camera as its own engine module, decoupled from the player entity
- Smooth transitions (`transitionTo(position, yaw, pitch, duration, easing)`)
- Camera shake (trauma-based with Perlin noise)
- FOV manipulation (punch on impacts, gradual shifts)
- Single `CameraState` output consumed by all systems

**Deferred:**
- Head bob tied to movement
- Scripted camera sequences (cutscene-style paths)
- Spline/path following
- Editor camera adoption (editor keeps its own `EditorCamera` for now)

## Architecture

### Layer Placement

Engine layer: `src/engine/camera/` as a new CMake target `engine_camera` depending on `engine_core` (for GLM). The module has zero game-specific knowledge. Game code depends on it.

### Data Flow

```
Game code (player input, scripts, etc.)
    │
    ▼
CameraManager::setBaseState(position, yaw, pitch)    ← input
    │
    ▼
CameraManager::update(deltaTime)
    │  base state
    │  → effect[0].apply()   (e.g., TransitionEffect - may override)
    │  → effect[1].apply()   (e.g., ShakeEffect - additive offset)
    │  → effect[2].apply()   (e.g., FOVEffect - additive FOV)
    │  → recompute view/projection matrices
    │
    ▼
CameraManager::getState() → const CameraState&       ← output
    │
    ├──→ RuntimeSceneRenderer (view/proj matrices, position, direction)
    ├──→ Audio listener (position, forward, up)
    ├──→ Torch lighting (position, forward, right)
    ├──→ Interaction raycast (position, forward)
    └──→ Viewmodel rendering (inverse view matrix)
```

### File Layout

```
src/engine/camera/
├── CameraState.h              // CameraState struct, EasingType enum
├── CameraEffect.h             // Base class (~15 lines)
├── CameraManager.h / .cpp     // Owns base state, effect list, final output
├── CameraMath.h / .cpp        // Easing functions, matrix builders (from RuntimeCameraMath)
└── effects/
    ├── TransitionEffect.h / .cpp
    ├── ShakeEffect.h / .cpp
    └── FOVEffect.h / .cpp
```

### Ownership

`CameraManager` is registered as a service on `Application` via `emplaceService<CameraManager>()`. `RuntimeGameSession` uses it during gameplay.

## Core Types

### CameraState

The single authoritative output that all consumers read. Replaces `RuntimeCameraState`.

```cpp
struct CameraState {
    glm::vec3 position{0.0f};
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    glm::vec3 right{1.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    glm::mat4 viewMatrix{1.0f};
    glm::mat4 projectionMatrix{1.0f};
    float yaw = -90.0f;   // degrees
    float pitch = 0.0f;   // degrees
    float fov = 70.0f;    // degrees
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};
```

Vectors and matrices are recomputed from yaw/pitch/position after effects are applied, so effects can work in angle space and the manager reconciles.

### EasingType

```cpp
enum class EasingType {
    Linear,
    EaseOutQuad,
    EaseOutCubic,      // what the editor already uses
    EaseInOutCubic,
    EaseInOutQuad
};
```

`CameraMath` provides `float evaluateEasing(EasingType, float t)`. More curves added trivially -- it's a switch statement.

### CameraEffect Base

```cpp
class CameraEffect {
public:
    virtual ~CameraEffect() = default;
    virtual void update(float deltaTime) = 0;
    virtual CameraState apply(const CameraState& input) const = 0;
    virtual bool isFinished() const = 0;
};
```

Three methods, no state in the base. Effects are either **override** (TransitionEffect replaces position/rotation) or **additive** (ShakeEffect adds offsets, FOVEffect adds to FOV). This distinction lives in each effect's `apply()` implementation.

## CameraManager API

```cpp
class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    // --- Input (called by game code each frame) ---
    void setBaseState(const glm::vec3& position, float yaw, float pitch);
    void setProjection(float fov, float aspectRatio, float nearPlane, float farPlane);

    // --- Effect management ---
    template<typename T, typename... Args>
    T& addEffect(Args&&... args);

    void removeEffect(CameraEffect* effect);
    void clearEffects();

    // --- Convenience methods ---
    void transitionTo(const glm::vec3& position, float yaw, float pitch,
                      float duration, EasingType easing = EasingType::EaseOutCubic);
    void shake(float trauma);
    void punchFOV(float deltaFov, float duration,
                  EasingType easing = EasingType::EaseOutCubic);

    // --- Tick & output ---
    void update(float deltaTime);
    const CameraState& getState() const;

    // --- Query ---
    bool isTransitioning() const;
    bool hasActiveEffects() const;

private:
    CameraState baseState_;
    CameraState finalState_;
    float aspectRatio_ = 16.0f / 9.0f;
    std::vector<std::unique_ptr<CameraEffect>> effects_;

    void rebuildMatrices(CameraState& state) const;
    void pruneFinishedEffects();
};
```

### Design Decisions

- **Two-tier API.** Convenience methods (`transitionTo`, `shake`, `punchFOV`) cover 90% of use cases. `addEffect<T>()` is the escape hatch for custom effects.
- **`shake()` is accumulative.** If a ShakeEffect already exists, adds trauma to it. Multiple trauma sources naturally stack. If none exists, one is created and persists (trauma decays to zero).
- **`update()` ordering.** Tick all effects → apply in insertion order over base state → rebuild matrices → prune finished effects.
- **`setBaseState()` called every frame** even during transitions. Base state keeps tracking player position so return transitions have a valid target.
- **`transitionTo()` replaces active transition.** If called while a transition is already in progress, the current transition is removed and a new one is created from the current final state to the new target. No queuing -- game code sequences transitions explicitly.

## Built-in Effects

### TransitionEffect

Smoothly moves the camera from a captured start state to a target. While active, **overrides** position/yaw/pitch.

```cpp
class TransitionEffect : public CameraEffect {
public:
    TransitionEffect(const CameraState& from, const glm::vec3& targetPos,
                     float targetYaw, float targetPitch,
                     float duration, EasingType easing);

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;

private:
    glm::vec3 fromPosition_, targetPosition_;
    float fromYaw_, targetYaw_;
    float fromPitch_, targetPitch_;
    float duration_, progress_ = 0.0f;
    EasingType easing_;
};
```

- Interpolates each field independently using eased `t`
- Yaw interpolation handles wraparound (shortest path around 360)
- When finished, gets pruned -- base state takes over next frame
- Usage: `cameraManager.transitionTo(doorPos, doorYaw, doorPitch, 1.5f)`

### ShakeEffect

Persistent effect using trauma + Perlin noise. Not auto-removed.

```cpp
class ShakeEffect : public CameraEffect {
public:
    float maxTranslationOffset = 0.05f;   // meters
    float maxYawOffset = 1.5f;            // degrees
    float maxPitchOffset = 1.0f;          // degrees
    float noiseFrequency = 8.0f;
    float traumaDecayRate = 1.5f;         // trauma per second

    void addTrauma(float amount);
    float getTrauma() const;

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;      // always false (persistent)

private:
    float trauma_ = 0.0f;
    float noiseTime_ = 0.0f;
};
```

- `magnitude = trauma_ * trauma_` (quadratic falloff)
- Samples `glm::perlin()` at three different seeds for X offset, Y offset, and rotation
- Offsets are **additive**
- Usage: `cameraManager.shake(0.3f)` on door slam, `cameraManager.shake(0.8f)` on explosion

### FOVEffect

Temporary punch to field of view. Auto-removed when finished.

```cpp
class FOVEffect : public CameraEffect {
public:
    FOVEffect(float deltaFov, float duration, EasingType easing);

    void update(float deltaTime) override;
    CameraState apply(const CameraState& input) const override;
    bool isFinished() const override;

private:
    float deltaFov_;
    float duration_, progress_ = 0.0f;
    EasingType easing_;
};
```

- Adds `deltaFov_ * (1.0 - evaluateEasing(easing_, progress_))` to input FOV
- Starts at full intensity, eases back to zero
- Multiple FOVEffects stack
- Usage: `cameraManager.punchFOV(8.0f, 0.4f)` on damage, `cameraManager.punchFOV(-5.0f, 2.0f)` for tight corridor

## Integration & Migration

### RuntimeGameSession Tick Order

```
// Current:
tickPlayerCamera(registry_, inputSystem_, aspect, deltaTime);
// ... render with capturePrimaryRuntimeCamera()

// New:
tickPlayerCamera(registry_, inputSystem_, cameraManager_, deltaTime);
//  └─ calls cameraManager.setBaseState() + setProjection()
cameraManager_.update(deltaTime);
// ... render with cameraManager_.getState()
```

### PlayerControlCamera Changes

- Still reads mouse input, computes yaw/pitch with sensitivity and clamping
- Calls `cameraManager.setBaseState(playerPosition, yaw, pitch)` and `cameraManager.setProjection(fov, aspect, near, far)`
- No longer writes to `CameraComponent` or calls `updateRuntimeCameraComponent()`
- Player entity keeps `TransformComponent` (for physics/collision) but `CameraComponent` on the player becomes unnecessary

### Consumer Migration

| Consumer | Currently reads from | After migration |
|----------|---------------------|-----------------|
| `RuntimeSceneRenderer` | `capturePrimaryRuntimeCamera()` via ECS query | `app.getService<CameraManager>().getState()` |
| Torch lighting | Camera forward/right/position from internal state | Same fields from `CameraManager::getState()` |
| `AudioListenerSystem` | ECS query for `PrimaryCameraTag + TransformComponent` | `app.getService<CameraManager>().getState().position/forward/up` |
| Interaction raycast | Camera forward/position from ECS | `CameraManager::getState().position/forward` |
| Viewmodel rendering | Inverse view matrix from internal state | `CameraManager::getState().viewMatrix` |
| Player movement direction | `CameraComponent.yaw` on player entity | Yaw available locally in `tickPlayerCamera` before feeding to manager |

### Removed/Deprecated

- `RuntimeCameraState` in `RuntimeCameraMath.h` -- replaced by `CameraState`
- `capturePrimaryRuntimeCamera()` -- no longer needed
- `updateRuntimeCameraComponent()` -- logic moves to `CameraManager::rebuildMatrices()`
- `CameraComponent` on the player entity -- removed entirely; camera state owned by the manager
- `PrimaryCameraTag` -- removed from runtime; no longer needed for camera selection
- Player spawner (`PlayerControlSpawner`) stops attaching `CameraComponent` and `PrimaryCameraTag` to the player entity

### Preserved

- `CameraDebugInfo` -- populated from `CameraManager::getState()` for ImGui overlay
- `EditorCamera` / `EditorViewportController` -- unchanged (out of scope)
- `buildForward()` math -- moves to `CameraMath.cpp` in the engine camera module

### CameraMath Consolidation

Existing `buildForward()` and matrix computation from `RuntimeCameraMath.cpp` move into `CameraMath.cpp`. Same math, new home. `evaluateEasing()` lives here too.
