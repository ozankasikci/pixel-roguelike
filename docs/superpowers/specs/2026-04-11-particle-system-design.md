# Particle System Design Spec

## Overview

A CPU-simulated, instanced-rendered particle system for the custom C++ OpenGL 4.1 engine. Designed for atmospheric and environmental effects — dust motes, sparks, steam, fog, interaction feedback — matching the Stanley Parable-inspired art direction.

The system follows a strategy-based architecture (SOLID) where the emitter owns the fixed lifecycle loop and delegates varying behavior to pluggable strategies for emission shape, forces, and property animation.

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Simulation | CPU | 1k-5k particles; macOS emulates transform feedback via Metal (poor perf on Apple Silicon); CPU trivially handles this scale |
| Rendering | Instanced billboards | One draw call per emitter; `glDrawArraysInstanced` with per-instance VBO |
| Blending | Both additive and alpha | Additive for sparks/embers (no sorting); alpha for smoke/dust/fog (per-emitter insertion sort) |
| Pipeline position | Before post-processing | Industry standard — particles render into HDR scene FBO, receive bloom/stylize/tone mapping naturally |
| Shader | Dedicated `particles.vert`/`particles.frag` | Separate from scene shader; handles billboarding, soft particles, HDR output |
| Definitions | Data-driven `.particle` files | Loaded through ContentRegistry with parent inheritance, consistent with materials |
| Architecture | Strategy-based emitter | Open/Closed for shapes, forces, animators; fixed lifecycle loop; no module-stack over-engineering |

## Architecture

### Layer Placement

```
Engine Layer (src/engine/particles/)
  ParticlePool          — SoA storage, swap-and-pop lifecycle
  ParticleEmitter       — core lifecycle loop, holds strategies
  ParticleRenderer      — instanced billboard rendering, VBO management
  EmitterShape          — interface + built-in: Point, Sphere, Cone
  ForceFunction         — interface + built-in: Gravity, Drag, Wind
  ValueAnimator<T>      — interface + built-in: Constant, Lerp, ColorGradient, FloatCurve

Game Layer (src/game/particles/)
  ParticleEmitterComponent  — ECS component, references definition by ID
  ParticleSystem            — System subclass, Gameplay phase
  ParticleRenderBridge      — called by RuntimeSceneRenderer during scene pass

Content
  ParticleEmitterDefinition — parsed from .particle files, registered in ContentRegistry
  assets/particles/*.particle

Shaders
  assets/shaders/game/particles.vert
  assets/shaders/game/particles.frag
```

Dependency flow: `ParticleSystem` (game) -> `ParticleEmitter` / `ParticlePool` / `ParticleRenderer` (engine). Engine layer has zero game dependencies.

### CMake Target Graph Extension

```
gameplay -> game_particles -> engine_particles -> engine_rendering -> engine_core
```

## Particle Data Layout

### SoA Pool Arrays

```
positions[]      — vec3, world-space (or local-space if simulationSpace == Local)
velocities[]     — vec3
colors[]         — vec4 (RGBA, animated over lifetime)
sizes[]          — float (animated over lifetime)
ages[]           — float (seconds since birth)
lifetimes[]      — float (max age per particle, randomized at spawn)
rotations[]      — float (billboard rotation angle in radians)
rotationSpeeds[] — float (angular velocity, randomized at spawn)
```

### Pool Mechanics

- Fixed capacity per emitter (set from definition: 256, 512, 1024, 2048)
- `aliveCount_` tracks contiguous active region `[0, aliveCount_)`
- Spawn: write at `[aliveCount_]`, increment
- Kill: swap dead particle with particle at `[aliveCount_ - 1]`, decrement (swap-and-pop)
- Fractional emission accumulator: `accumulator_ += rate * dt`, spawn `floor(accumulator_)`, keep remainder

### Instance Buffer (uploaded to GPU each frame)

Per instance, 28 bytes:
```
position  — vec3  (12 bytes)
color     — vec4 packed as RGBA8 (4 bytes)
size      — float (4 bytes)
rotation  — float (4 bytes)
age_norm  — float normalized t = age/lifetime (4 bytes, for atlas frame selection)
```

Upload via buffer orphaning: `glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STREAM_DRAW)` then `glBufferSubData()`.

### Sorting (alpha-blend emitters only)

Sort an index array by squared distance to camera (avoids sqrt). Insertion sort — nearly O(n) since particle order is mostly stable frame-to-frame. Pack instance buffer in sorted back-to-front order.

## Strategy Interfaces

### EmitterShape

```cpp
class EmitterShape {
public:
    virtual ~EmitterShape() = default;
    virtual glm::vec3 samplePosition(std::mt19937& rng) const = 0;
    virtual glm::vec3 sampleDirection(std::mt19937& rng) const = 0;
};
```

Built-in implementations:
- `PointShape` — spawn at origin, random direction on unit sphere
- `SphereShape(float radius, bool surfaceOnly)` — random point in/on sphere, direction = outward from center
- `ConeShape(float angle, float radius, float length)` — spawn at base disc, direction within cone angle

### ForceFunction

```cpp
class ForceFunction {
public:
    virtual ~ForceFunction() = default;
    virtual glm::vec3 apply(const glm::vec3& position,
                            const glm::vec3& velocity,
                            float dt) const = 0;
};
```

Built-in implementations:
- `GravityForce(glm::vec3 gravity)` — `velocity + gravity * dt`
- `DragForce(float coefficient)` — `velocity * (1 - coefficient * dt)`
- `WindForce(glm::vec3 direction, float strength, float turbulence)` — directional push with noise variation

Forces stored as `std::vector<std::unique_ptr<ForceFunction>>` on the emitter (typically 1-3).

### ValueAnimator\<T\>

```cpp
template<typename T>
class ValueAnimator {
public:
    virtual ~ValueAnimator() = default;
    virtual T evaluate(float t) const = 0;  // t in [0, 1]
};
```

Built-in implementations:
- `Constant<T>(T value)` — returns value regardless of t
- `Lerp<T>(T start, T end)` — linear interpolation
- `ColorGradient(std::vector<std::pair<float, glm::vec4>> stops)` — lerp between adjacent color stops
- `FloatCurve(std::vector<std::pair<float, float>> keyframes)` — piecewise-linear float keyframes

The emitter holds:
- `std::unique_ptr<EmitterShape>` shape
- `std::vector<std::unique_ptr<ForceFunction>>` forces
- `std::unique_ptr<ValueAnimator<glm::vec4>>` colorOverLifetime
- `std::unique_ptr<ValueAnimator<float>>` sizeOverLifetime
- `std::unique_ptr<ValueAnimator<float>>` opacityOverLifetime (multiplied into color alpha)

## Emitter Lifecycle

### States

- `Playing` — actively spawning and updating particles
- `Stopping` — no new spawns, existing particles live out their lifetimes
- `Stopped` — all particles dead, emitter inactive

### Emitter Properties (from definition)

```
maxParticles        — pool capacity (256, 512, 1024, 2048)
emissionRate        — particles per second
bursts[]            — array of {time, count} for one-shot spawns
looping             — whether emission restarts after duration
duration            — how long the emitter spawns (seconds), 0 = infinite
simulationSpace     — World or Local
blendMode           — Additive or AlphaBlend
textureId           — optional particle texture reference
initialSpeedRange   — [min, max] speed multiplied by shape direction
lifetimeRange       — [min, max] particle lifetime (randomized at spawn)
rotationSpeedRange  — [min, max] angular velocity (randomized at spawn)
warmUp              — bool, pre-simulate looping emitters at init
softParticleFade    — float, distance for soft particle depth fade (0 = disabled)
emissiveStrength    — float, HDR multiplier for bloom participation (1.0 = no bloom)
```

### Core Update Loop (per frame, per emitter)

```
1. SPAWN
   - accumulator += emissionRate * dt
   - check bursts[] for any whose time has passed
   - while spawnsQueued > 0 && aliveCount < maxParticles:
       position = shape.samplePosition(rng)
       direction = shape.sampleDirection(rng)
       velocity = direction * random(minSpeed, maxSpeed)
       lifetime = random(minLifetime, maxLifetime)
       rotation = 0, rotationSpeed = random(minRotSpeed, maxRotSpeed)
       color = colorAnimator.evaluate(0.0)
       size = sizeAnimator.evaluate(0.0)
       write into pool at [aliveCount], increment

2. UPDATE (iterate [0, aliveCount))
   - ages[i] += dt
   - for each force: velocities[i] = force.apply(positions[i], velocities[i], dt)
   - positions[i] += velocities[i] * dt
   - rotations[i] += rotationSpeeds[i] * dt
   - t = ages[i] / lifetimes[i]
   - colors[i] = colorAnimator.evaluate(t)
   - sizes[i] = sizeAnimator.evaluate(t)
   - colors[i].a *= opacityAnimator.evaluate(t)

3. KILL (iterate backward from aliveCount - 1 to 0)
   - if ages[i] >= lifetimes[i]: swap with last alive, decrement aliveCount

4. SORT (only if blendMode == AlphaBlend)
   - insertion sort index array by squared distance to camera, back-to-front

5. PACK INSTANCE BUFFER
   - write position/color/size/rotation/normalizedAge into instance data
   - sorted order for alpha, direct order for additive
```

### Simulation Space

- **World**: spawn position transformed to world space at birth using entity transform, then independent. Used for sparks, impact dust, explosions.
- **Local**: positions stored in emitter-local space, multiplied by entity transform at render time. Used for attached effects (steam vents, auras).

### Warm-Up

For looping emitters with `warmUp: true`, pre-simulate N steps at init (`update(fixedDt)` in a tight loop for `duration / fixedDt` iterations) so the effect starts visually full.

## Rendering Pipeline Integration

### Render Order

Within `SceneRenderPipeline::render()`:

```
1. Shadow pass (existing)
2. Scene pass — opaque geometry (existing)
3. >>> Particle pass (NEW) <<<
4. Post-processing: bloom, SSAO, stylize, composite (existing)
```

### Particle Render Pass

```
1. Bind particle shader
2. Set shared uniforms: uView, uProjection, uCameraPos, uNearPlane, uFarPlane
3. Bind scene depth texture (from opaque pass) for soft particles

4. Render additive emitters (any order):
   - glBlendFunc(GL_ONE, GL_ONE)
   - glDepthMask(GL_FALSE)
   - For each emitter:
     - Upload instance buffer (buffer orphaning)
     - Bind particle texture
     - Set per-emitter uniforms: emissiveStrength, softParticleFade, hasTexture
     - glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, aliveCount)

5. Render alpha-blend emitters (rough front-to-back by emitter distance):
   - glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
   - glDepthMask(GL_FALSE)
   - For each emitter:
     - Upload instance buffer (already back-to-front per-particle sorted)
     - Bind particle texture
     - Set per-emitter uniforms
     - glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, aliveCount)

6. Restore GL state: re-enable depth writes, reset blend func
```

### Particle Shader

**Vertex shader** (`particles.vert`):
- Input: quad vertex position (attribute 0), per-instance data (attributes 1-5 with divisor 1)
- Extract camera right/up from view matrix columns
- Apply billboard rotation around view direction
- Compute world position: `particleCenter + right * localX * size + up * localY * size`
- Pass through: UV, color, linear depth

**Fragment shader** (`particles.frag`):
- Sample particle texture (or default radial falloff if no texture)
- Multiply texture RGBA by instance color
- Soft particle fade: `alpha *= saturate((sceneDepth - fragDepth) / uSoftFadeRange)`
- Apply emissive strength: `color.rgb *= uEmissiveStrength` (values > 1.0 trigger bloom)
- Output: single color attachment (into scene FBO)

### Vertex Buffer Layout

Shared quad VBO (4 vertices, never changes):
```
Attribute 0: vec2 position — (-0.5,-0.5), (0.5,-0.5), (-0.5,0.5), (0.5,0.5)
```

Instance VBO (updated each frame per emitter):
```
Attribute 1: vec3 worldPosition   (divisor 1)
Attribute 2: vec4 color RGBA8     (divisor 1)
Attribute 3: float size           (divisor 1)
Attribute 4: float rotation       (divisor 1)
Attribute 5: float normalizedAge  (divisor 1)
```

## Data-Driven Definitions

### File Format (`.particle`)

Uses the same line-based `key value` format as `.material` and `.scene` files:

```
id torch_sparks
parent base_spark

max_particles 256
emission_rate 30
looping true
duration 0.0
simulation_space world
blend_mode additive
texture spark_radial
warm_up false
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

Line format conventions:
- `#` lines are comments
- `key value [value...]` — space-separated
- Range values as two numbers: `lifetime 0.3 0.8` means min=0.3, max=0.8
- Repeated keys accumulate: multiple `force` lines, multiple `color_over_lifetime` stops
- `shape <type> [params...]` — type-specific parameters inline
- `parent <id>` — inherits all values from parent, overrides only specified fields

### Content Pipeline

- `ParticleEmitterDefinition` — POD struct parsed from `.particle` files
- Registered in `ContentRegistry` alongside materials, environments, weapons
- **Parent inheritance**: definition can set `parent base_spark` and override only specific fields
- `ContentRegistry::resolveParticleEmitter(id)` returns the fully merged definition
- At emitter creation, the definition is resolved once and used to construct the `ParticleEmitter` with appropriate strategies

### ECS Component

```cpp
struct ParticleEmitterComponent {
    std::string emitterId;  // references a ParticleEmitterDefinition
    bool enabled = true;
};
```

Placed on entities in `.scene` files using the existing line-based format:
```
particle_emitter torch_sparks 5.0 3.2 -1.0
```

Format: `particle_emitter <emitter_id> <px> <py> <pz>`
Position is world-space. The emitter definition controls all other parameters.

### Particle Textures

- Small textures in `assets/textures/particles/` (radial gradient, soft circle, smoke puff, spark)
- Referenced by `texture` field in definition
- Loaded and cached by `ParticleRenderer`

## Editor Integration

- `ParticleEmitterComponent` available in entity inspector with `emitterId` dropdown from ContentRegistry
- Live particle preview in editor viewport (runs through existing RuntimeGameSession preview)
- Emitter shape gizmo: wireframe overlay (cone outline, sphere wireframe) at entity position
- Enable/disable toggle in inspector
- Looping emitters auto-warm-up in editor so they appear full immediately
- No particle simulation in layout-only mode (no active preview session)
- Scene file round-trip: only `emitterId` and `enabled` stored in `.scene` — all parameters in `.particle` definition

## MVP Scope

### Included in MVP

- ParticlePool with SoA layout and swap-and-pop
- ParticleEmitter with full lifecycle (spawn/update/kill/sort/pack)
- Instanced billboard rendering with dedicated shader
- Soft particles via depth buffer read
- Additive and alpha blending with per-emitter sorting
- EmitterShape: Point, Sphere, Cone
- ForceFunction: Gravity, Drag
- ValueAnimator: Constant, Lerp, ColorGradient, FloatCurve
- World and Local simulation space
- Data-driven `.particle` definitions with parent inheritance via ContentRegistry
- ParticleEmitterComponent for ECS integration
- Editor viewport preview and inspector support
- Warm-up for looping emitters
- HDR emissive output for bloom participation

### Deferred (post-MVP)

- Additional shapes: Box, Mesh Surface, Edge
- Additional forces: Wind, Vortex, Attractor, Curl Noise Turbulence
- Texture atlas / flipbook animation
- Sub-emitters (spawn child emitters on particle death/collision)
- Particle collision with scene geometry
- Stretched billboard (velocity-aligned) rendering mode
- Ribbon / trail rendering mode
- GPU simulation via transform feedback (optimization for 50k+ particles)
- Half-resolution particle rendering (overdraw optimization)
- Per-particle mesh rendering (debris, rocks)
- Particle LOD (reduce count/quality at distance)
