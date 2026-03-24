# Player Movement Design

## Summary

Replace the free-flying camera with proper ground-based first-person movement using Jolt Physics `CharacterVirtual`. The player walks on surfaces, collides with walls and pillars, climbs steps, and jumps with variable height. Movement uses smooth/inertial acceleration with reduced air control.

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Physics approach | Jolt Physics CharacterVirtual | Already in tech stack spec; handles ground detection, steps, slopes out of the box; needed for enemies/projectiles later |
| Movement feel | Smooth/inertial | Gradual acceleration and deceleration for weighty, satisfying feel |
| Jumping | Variable-height (hold to jump higher) | Expressive control; pairs well with inertial movement |
| Slopes & stairs | Auto-step + slope limit (Jolt built-in) | Cathedral already has steps; 0.3m step height, 45 degree slope limit |
| Air control | Reduced (~30-40% of ground speed) | Allows mid-jump corrections without feeling floaty |
| Player entity | Camera entity with physics capsule | First-person game, no visible body needed; simplest viable approach |

## Architecture

### New Files

| File | Purpose |
|------|---------|
| `src/engine/physics/PhysicsSystem.h/.cpp` | Jolt world init, fixed-timestep stepping, static body creation helpers |
| `src/game/systems/PlayerMovementSystem.h/.cpp` | Input-driven velocity, jump logic, feeds CharacterVirtual |
| `src/game/components/CharacterControllerComponent.h` | Holds `JPH::CharacterVirtual` ref, eye height, capsule dimensions |
| `src/game/components/PlayerMovementComponent.h` | Velocity, movement tuning params, jump state |
| `src/game/components/PhysicsBodyComponent.h` | Stores Jolt `BodyID` for static collision entities |

### Modified Files

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add Jolt Physics v5.4.0 via FetchContent |
| `src/main.cpp` | Register PhysicsSystem and PlayerMovementSystem in pipeline |
| `src/game/scenes/CathedralScene.cpp` | Create collision shapes for floor/walls/pillars/steps; adjust player spawn |
| `src/game/systems/CameraSystem.cpp` | Remove WASD movement; sync position from CharacterVirtual + eye offset |

### System Pipeline (per frame)

```
InputSystem          → poll keys, mouse, swap state arrays
    ↓
PhysicsSystem        → step Jolt world (fixed timestep with accumulator)
    ↓
PlayerMovementSystem → read input + ground state, compute velocity
                       → CharacterVirtual::ExtendedUpdate() resolves collisions
    ↓
CameraSystem         → sync position from CharacterVirtual + eye height
                       → mouse look, compute view/projection matrices
    ↓
RenderSystem         → render scene, dither post-pass, ImGui
```

## Player Physics Capsule

- **Type**: `CharacterVirtual` (not `Character` — gives explicit control over movement without fighting the physics solver)
- **Shape**: `CapsuleShape` — 1.8m total height (0.7m half-height cylinder + 0.2m radius caps)
- **Eye offset**: Camera at +1.6m from capsule base
- **Max slope angle**: 45 degrees
- **Max step height**: 0.3m
- **Mass**: 80kg

### CharacterControllerComponent

```cpp
struct CharacterControllerComponent {
    JPH::Ref<JPH::CharacterVirtual> character;
    float eyeHeight = 1.6f;
    float capsuleRadius = 0.2f;
    float capsuleHalfHeight = 0.7f;
};
```

## Movement Model

Velocity-based with acceleration/deceleration. No direct position manipulation.

```
desiredDir = (forward * inputW + right * inputD - ...).normalized()
desiredVelocity = desiredDir * maxGroundSpeed

if (grounded):
    velocity.xz = moveToward(velocity.xz, desiredVelocity.xz, acceleration * dt)
else:
    velocity.xz = moveToward(velocity.xz, desiredVelocity.xz, airAcceleration * dt)

velocity.y += gravity * dt
```

Forward/right vectors are projected onto the horizontal plane (Y=0) so looking up/down doesn't affect movement speed.

### PlayerMovementComponent

```cpp
struct PlayerMovementComponent {
    glm::vec3 velocity{0.0f};
    float maxGroundSpeed = 5.0f;
    float acceleration = 25.0f;       // reach full speed in ~0.2s
    float deceleration = 20.0f;       // stop in ~0.25s
    float airAcceleration = 8.0f;     // ~30-40% of ground
    float gravity = -15.0f;
    bool grounded = false;
};
```

## Variable-Height Jumping

Two-phase jump:

1. **Rising (space held)**: Initial impulse of `jumpImpulse`. While space is held and player is rising, gravity is reduced by `jumpHoldGravityScale`. Capped by `maxJumpHoldTime`.
2. **Falling (space released or apex)**: Full gravity resumes.

Short tap produces ~0.5m hop, full hold produces ~1.5m jump.

### Jump Parameters (on PlayerMovementComponent)

```cpp
float jumpImpulse = 6.0f;
float jumpHoldGravityScale = 0.4f;
float maxJumpHoldTime = 0.3f;
float jumpHoldTimer = 0.0f;
bool jumpHeld = false;
```

### Jump Logic Per Frame

1. If `grounded` and space just pressed: set `velocity.y = jumpImpulse`, `jumpHeld = true`, `jumpHoldTimer = 0`
2. If `jumpHeld` and space still held and `velocity.y > 0` and `jumpHoldTimer < maxJumpHoldTime`: use `gravity * jumpHoldGravityScale`, increment timer
3. Otherwise: `jumpHeld = false`, full gravity

## Collision Shapes

All static bodies using simple primitives (no mesh colliders).

| Visual Element | Collision Shape | Half-Extents / Dimensions |
|---|---|---|
| Floor | Box | (4, 0.05, 15) at Y=-0.05 |
| Ceiling | Box | (4, 0.05, 15) at Y=6.05 |
| Left wall | Box | (0.15, 3, 15) at X=-4 |
| Right wall | Box | (0.15, 3, 15) at X=4 |
| Back wall | Box | (4, 3, 0.15) at Z=-30 |
| Pillars | Cylinder | radius=0.15, halfHeight=3 |
| Steps | Box | matching step dimensions |
| Arches | None | decorative only |

## Player Spawn

Position changed from `(0, 2, 5)` to `(0, 0.9, 4)` — capsule base on the floor near the entrance, eye height at 1.6m above.

## ImGui Debug Panel

All tunable values exposed at runtime:
- `maxGroundSpeed`, `acceleration`, `deceleration`
- `airAcceleration`
- `gravity`
- `jumpImpulse`, `jumpHoldGravityScale`, `maxJumpHoldTime`
- `eyeHeight`
- Ground state display (grounded/air/steep)
- Current velocity magnitude

## Not Included (future work)

- Coyote time / jump buffering
- Slope speed penalties
- Crouching / sliding
- Head bobbing
- Footstep sounds
- Fall damage
