# Architecture Research

**Domain:** Custom C++ first-person 3D game engine with 1-bit dithered rendering
**Researched:** 2026-03-23
**Confidence:** MEDIUM — Core patterns are well-established (HIGH for engine layers, ECS, rendering pipeline). Dithering pipeline specifics and roguelike game-state patterns partially from single sources (MEDIUM). Build order recommendations synthesized from multiple projects.

---

## Standard Architecture

### System Overview

A custom C++ game engine for this project follows a strict five-layer dependency model where upper layers call downward only. No layer calls upward.

```
┌────────────────────────────────────────────────────────────────┐
│                        GAME LAYER                              │
│  ┌───────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │  Game     │  │  Combat  │  │  Enemy   │  │  Level      │  │
│  │  State    │  │  System  │  │  AI      │  │  Manager    │  │
│  └─────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬──────┘  │
└────────┼─────────────┼─────────────┼────────────────┼─────────┘
         │             │             │                │
┌────────┴─────────────┴─────────────┴────────────────┴─────────┐
│                        FUNCTION LAYER                          │
│  ┌───────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │  ECS      │  │  Physics │  │  Camera  │  │  Audio      │  │
│  │  World    │  │  /Collis.│  │  System  │  │  System     │  │
│  └─────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬──────┘  │
└────────┼─────────────┼─────────────┼────────────────┼─────────┘
         │             │             │                │
┌────────┴─────────────┴─────────────┴────────────────┴─────────┐
│                        RENDERING LAYER                         │
│  ┌────────────────────────────────────────────────────────┐   │
│  │  Scene Renderer → Framebuffer → Dither Post-Process    │   │
│  │  (3D geometry + lighting)   (FBO)   (Bayer shader)     │   │
│  └────────────────────────────────────────────────────────┘   │
│  ┌───────────┐  ┌──────────┐  ┌──────────────────────────┐   │
│  │  Shader   │  │  Texture │  │  Mesh / VAO / VBO        │   │
│  │  Manager  │  │  Manager │  │  Manager                 │   │
│  └───────────┘  └──────────┘  └──────────────────────────┘   │
└────────────────────────────────────────────────────────────────┘
         │
┌────────┴───────────────────────────────────────────────────────┐
│                        CORE LAYER                              │
│  ┌───────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │  Math     │  │  Memory  │  │  Event   │  │  Resource   │  │
│  │  (GLM)    │  │  Mgmt    │  │  Bus     │  │  Manager    │  │
│  └───────────┘  └──────────┘  └──────────┘  └─────────────┘  │
└────────────────────────────────────────────────────────────────┘
         │
┌────────┴───────────────────────────────────────────────────────┐
│                        PLATFORM LAYER                          │
│  ┌───────────┐  ┌──────────┐  ┌──────────┐  ┌─────────────┐  │
│  │  Window   │  │  Input   │  │  OpenGL  │  │  File       │  │
│  │  (GLFW)   │  │  (GLFW)  │  │  Context │  │  System     │  │
│  └───────────┘  └──────────┘  └──────────┘  └─────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

---

### Component Responsibilities

| Component | Responsibility | Communicates With |
|-----------|----------------|-------------------|
| **Platform / Window** | OS window creation, OpenGL context init, raw input events | Input system (delivers events upward) |
| **Input System** | Translate raw GLFW events to game actions (WASD, mouse delta, attack button) | Camera system, Game State |
| **Event Bus** | Decouple senders from receivers via publish/subscribe | All layers (used as loose coupling mechanism) |
| **Math (GLM)** | vec3/mat4/quaternion operations, frustum math | Used by all layers above |
| **Resource Manager** | Load, cache, and reference-count textures, meshes, shaders; no duplicate GPU uploads | Texture Manager, Mesh Manager, Shader Manager |
| **Shader Manager** | Compile GLSL, link programs, cache by path, set uniforms | Renderer, Dither Post-Process |
| **Texture Manager** | Load images (stb_image), upload to GPU, cache handles | Renderer, Dither Post-Process |
| **Mesh Manager** | Parse OBJ/custom format, create VAO/VBO/EBO, cache by path | Renderer |
| **Scene Renderer** | Submit draw calls for visible entities; deferred or forward shading; point lights | Mesh Manager, Shader Manager, Framebuffer |
| **Framebuffer (FBO)** | Offscreen render target at scene resolution; color + depth attachments | Scene Renderer (writes to it), Dither Post-Process (reads from it) |
| **Dither Post-Process** | Fullscreen quad pass that reads FBO color, applies luminance + Bayer matrix threshold, outputs pure black/white | Framebuffer (source), Default framebuffer (output) |
| **Camera System** | First-person view/projection matrix from position + pitch/yaw; mouse-look smoothing | Input System (consumes mouse delta), Renderer (provides matrices) |
| **Physics / Collision** | AABB and capsule collision vs level geometry; gravity; sweep tests for melee | ECS World, Level Manager |
| **ECS World** | Entity IDs, component storage (position, health, AI state, renderable, etc.), system iteration | All game-layer systems |
| **Audio System** | Positional sound effects and ambient; wraps miniaudio or OpenAL | ECS World (entity positions), Event Bus (trigger sounds on events) |
| **Enemy AI** | Patrol / detect / attack state machine per enemy entity | ECS World, Physics/Collision, Combat System |
| **Combat System** | Melee hit detection (swept sphere vs AABB), ranged projectile lifecycle, damage application | ECS World (health components), Physics |
| **Game State** | FSM: MainMenu / Playing / Paused / Dead / LevelComplete; orchestrates transitions | All game systems |
| **Level Manager** | Load handcrafted level data (geometry, entity spawn points, light positions), stream into ECS and Renderer | ECS World, Renderer, Resource Manager |

---

## Recommended Project Structure

```
src/
├── platform/            # OS and graphics API surface
│   ├── Window.h/.cpp    # GLFW window + OpenGL context
│   └── Input.h/.cpp     # Raw GLFW callback -> InputEvent structs
├── core/                # Cross-cutting low-level utilities
│   ├── math/            # Convenience wrappers on GLM
│   ├── memory/          # Pool allocator, arena (optional for v1)
│   ├── events/          # EventBus — simple observer/publish-subscribe
│   └── resources/       # ResourceManager — handle-based cache
├── rendering/           # All GPU concerns
│   ├── ShaderManager.h/.cpp
│   ├── TextureManager.h/.cpp
│   ├── MeshManager.h/.cpp
│   ├── Framebuffer.h/.cpp   # FBO wrapper
│   ├── Renderer.h/.cpp      # Scene submission, draw calls, lighting
│   └── DitherPass.h/.cpp    # Fullscreen Bayer dither post-process
├── ecs/                 # Entity-Component-System
│   ├── World.h/.cpp     # Registry (wraps EnTT or custom)
│   └── components/      # Plain structs: Transform, Health, AIState, Renderable, Light…
├── gameplay/            # Game-specific systems
│   ├── Camera.h/.cpp
│   ├── Physics.h/.cpp       # AABB collision, sweep tests
│   ├── CombatSystem.h/.cpp  # Melee + projectile logic
│   ├── EnemyAI.h/.cpp       # Patrol/detect/attack FSM
│   ├── LevelManager.h/.cpp  # Level load, spawn entities
│   └── GameState.h/.cpp     # Top-level FSM
├── audio/               # Sound effects and ambient
│   └── AudioSystem.h/.cpp   # miniaudio or OpenAL wrapper
├── assets/              # Raw source assets (OBJ, PNG, GLSL, level data)
│   ├── shaders/
│   ├── meshes/
│   ├── textures/
│   └── levels/
└── main.cpp             # Engine init, game loop
```

### Structure Rationale

- **platform/ vs core/ vs rendering/:** Strict separation ensures platform code never leaks into game logic. The rendering layer knows about OpenGL; nothing above it does.
- **ecs/components/ as plain structs:** No methods on components — systems in gameplay/ own the logic. This prevents data/logic coupling that kills cache performance.
- **rendering/DitherPass.h separate from Renderer.h:** The dither is a distinct post-process pass. Keeping it isolated makes it easy to toggle, tune parameters, or replace the dithering algorithm without touching scene rendering.
- **assets/ in source tree for v1:** For a solo/small project, embedding asset paths at build time is fine. A content pipeline (asset cooking) is unnecessary complexity for v1.

---

## Architectural Patterns

### Pattern 1: Offscreen Framebuffer + Post-Process Pass (1-bit Dithering)

**What:** Render the entire 3D scene to a high-precision offscreen FBO. Then run a single fullscreen quad pass through the dither shader, outputting to the default framebuffer. The scene renderer never knows about the dithering.

**When to use:** Any visual effect that needs the full composited scene as input — dithering, bloom, CRT scanlines, SSAO. Always the correct architecture for post-process effects.

**Trade-offs:** One extra FBO and texture copy per frame. Negligible cost at 1080p with modern hardware. Huge DX win: scene renderer stays clean, dither parameters are hot-reloadable at runtime.

**Example:**
```cpp
// Per-frame render loop
sceneFBO.bind();
renderer.drawScene(camera, ecs);   // all 3D geometry + lighting
sceneFBO.unbind();

ditherPass.apply(sceneFBO.colorTexture()); // reads scene, writes to default FBO
```

### Pattern 2: Entity-Component-System (Data-Oriented)

**What:** Entities are integer IDs. Components are plain data structs stored contiguously in typed arrays. Systems iterate arrays and transform data. No virtual dispatch, no inheritance hierarchies on game objects.

**When to use:** Always for game objects in a 3D game. Especially valuable when iterating 100s of enemies each frame — cache locality matters.

**Trade-offs:** Initial conceptual overhead vs OOP. Pays off immediately with more than ~50 entities. EnTT is the recommended library — mature, header-only, battle-tested in production games.

**Example:**
```cpp
// Define components as plain structs
struct Transform { glm::vec3 pos; float yaw, pitch; };
struct Health    { int current, max; };
struct AIState   { enum class Mode { Patrol, Alert, Attack } mode; float alertTimer; };

// System iterates only entities with the components it cares about
auto view = world.view<Transform, AIState>();
for (auto [entity, xform, ai] : view.each()) {
    enemyAI.update(entity, xform, ai, dt);
}
```

### Pattern 3: Game Loop with Fixed Physics Timestep

**What:** The main loop separates physics/logic updates (fixed timestep, e.g. 60Hz) from rendering (variable, uncapped or vsync). Input is polled once per frame before the update tick.

**When to use:** Any game with physics or deterministic simulation. Prevents physics behaving differently at different framerates.

**Trade-offs:** Slightly more complex loop. The "accumulator" pattern handles partial timesteps. This is the industry standard — there is no good reason not to use it.

**Example:**
```cpp
const float FIXED_DT = 1.0f / 60.0f;
float accumulator = 0.0f;

while (running) {
    float frameTime = timer.delta();
    accumulator += frameTime;

    input.poll();

    while (accumulator >= FIXED_DT) {
        world.update(FIXED_DT);     // physics, AI, combat
        accumulator -= FIXED_DT;
    }

    float alpha = accumulator / FIXED_DT; // for interpolated rendering (optional v1)
    renderer.drawScene(camera, world);
    ditherPass.apply(sceneFBO.colorTexture());
    window.swapBuffers();
}
```

### Pattern 4: Finite State Machine for Game State and Enemy AI

**What:** A clearly enumerated set of states with explicit transition conditions. Used at two levels: top-level game state (Menu/Playing/Paused/Dead) and per-enemy AI (Patrol/Alert/Attack/Dead).

**When to use:** Any entity with discrete behavioral modes. Prevents "flag soup" — booleans and conditionals scattered across update functions.

**Trade-offs:** Slight boilerplate. Far simpler than behavior trees for the AI complexity this game requires.

---

## Data Flow

### Frame Render Flow

```
[Player Mouse/Keyboard]
        |
        v
[Input::poll()]  -->  [InputEvent queue]
        |
        v
[Camera::update(mouseDeltas)]  -->  [View/Projection matrices]
        |
        v
[World::fixedUpdate(dt)]  -->  [Physics, AI, Combat, Damage]
        |
        v
[LevelManager]  -->  [ECS: Renderable + Transform components]
        |
        v
[Renderer::drawScene(camera, ecs)]  -->  [Writes to sceneFBO]
        |
        v
[DitherPass::apply(sceneFBO.color)]  -->  [Default framebuffer]
        |
        v
[Window::swapBuffers()]  -->  [Screen]
```

### Combat Hit Detection Flow

```
[Player input: swing/fire]
        |
        v
[CombatSystem::triggerAttack(playerEntity)]
        |
        +--> Melee: sweep sphere from hand position along attack arc
        |         |
        |         v
        |    Physics::sweepTest()  -->  [hits EnemyEntity]
        |         |
        |         v
        |    ECS: Health component decremented, HitEvent dispatched
        |
        +--> Ranged: spawn ProjectileEntity at weapon muzzle
                  |
                  v
             Physics::update() moves projectile each tick
                  |
                  v
             CollisionResponse: ProjectileEntity + EnemyEntity
                  |
                  v
             ECS: Health decremented, ProjectileEntity destroyed
```

### Game State Transitions

```
[GameState::MainMenu]
        |
        v  (player starts game)
[GameState::Playing]  <-->  [GameState::Paused]
        |
        +-- (player dies) --> [GameState::Dead]
        |                         |
        |                         v (restart)
        +-- (level cleared) --> [GameState::LevelComplete]
                                  |
                                  v (next level)
                             [GameState::Playing]
```

---

## Build Order (Component Dependencies)

Build in this order. Each phase depends on the layers below it being functional.

```
Phase 1 — Foundation (no deps)
  Platform: Window, OpenGL context, input events

Phase 2 — Rendering Primitives (requires Phase 1)
  ShaderManager, TextureManager, MeshManager
  Framebuffer (FBO wrapper)
  Basic Renderer (draw a static mesh)

Phase 3 — 1-Bit Dither Pipeline (requires Phase 2)
  DitherPass — fullscreen quad + Bayer matrix shader
  Scene renders to FBO, dither outputs to screen
  *** Validate the visual style before building gameplay ***

Phase 4 — ECS and Camera (requires Phase 1, Phase 2)
  ECS World (EnTT), core components
  Camera system (first-person mouse-look + WASD)
  Player can move through a scene with dithered rendering

Phase 5 — Level and Collision (requires Phase 4)
  LevelManager: load geometry + light positions
  Physics: AABB collision, gravity, sweep tests
  Player walks around gothic level geometry

Phase 6 — Lighting Integration (requires Phase 3, Phase 5)
  Point lights (torches) fed to scene shader
  Lighting affects luminance, dithering responds correctly
  Atmospheric quality validated

Phase 7 — Combat (requires Phase 4, Phase 5)
  CombatSystem: melee swing detection, projectile lifecycle
  Health components, damage application
  Player can hit targets

Phase 8 — Enemy AI (requires Phase 7)
  EnemyAI: patrol/detect/attack FSM
  Multiple enemy types as ECS component variations
  Enemy hit detection, death

Phase 9 — Game State and Audio (requires Phase 7, Phase 8)
  GameState FSM, level transitions
  AudioSystem for combat and ambient sounds

Phase 10 — Content (requires all systems)
  Handcrafted levels, weapon variants, boss encounters
```

---

## Anti-Patterns

### Anti-Pattern 1: Rendering Dithering In-Shader Per-Object

**What people do:** Apply the Bayer dither threshold inside the fragment shader of every mesh, at draw time.

**Why it's wrong:** Each mesh shader must carry the dithering logic and Bayer matrix. You cannot composite transparent objects or particles correctly — each is dithered in isolation, not as part of the final image. The effect does not work correctly with lighting that spans multiple objects. Tuning requires recompiling all shaders.

**Do this instead:** Render the scene normally to an FBO. Apply dither as a single post-process pass that sees the entire composited luminance image. This matches how Return of the Obra Dinn and similar games achieve the correct look.

### Anti-Pattern 2: God Object "Engine" Class

**What people do:** Create a single `Engine` class that owns the window, renderer, input, physics, audio, ECS, and game state. Everything calls `engine.getRenderer()`, `engine.getInput()`, etc.

**Why it's wrong:** Every system becomes coupled to everything else through the Engine object. Adding a new system requires modifying Engine. Unit testing is impossible. This is the most common mistake in first-time engine projects and consistently causes full rewrites.

**Do this instead:** Use an EventBus or explicit dependency injection. Each system receives only the specific interfaces it needs at construction time. `CombatSystem` gets a reference to the `ECSWorld` and `Physics` — nothing else.

### Anti-Pattern 3: Inheritance Hierarchies for Game Objects

**What people do:** `class Enemy : public Character : public Entity { ... }` with virtual `update()`, `render()`, `takeDamage()`.

**Why it's wrong:** Deep inheritance prevents sharing components selectively. A flying enemy that shoots also needs the Patrol component — impossible without multiple inheritance or interface explosion. Virtual dispatch on 200 enemies per frame is measurable overhead. Adding a new enemy type requires a new class file.

**Do this instead:** ECS composition. A "flying shooting enemy" is an entity with `[Transform, Health, AIState, PatrolPath, ProjectileShooter, FlyMovement]` components. New enemy types are new component combinations, not new classes.

### Anti-Pattern 4: Variable-Timestep Physics

**What people do:** Pass raw frame delta time directly to physics and collision — `physics.update(frameTime)`.

**Why it's wrong:** At 30fps the player clips through walls. At 144fps the game runs too fast or physics is unstable. Frame drops cause non-deterministic collision results.

**Do this instead:** Fixed-timestep physics accumulator (see Pattern 3 above). Physics always runs at 60Hz regardless of display framerate.

### Anti-Pattern 5: Dithering at Full Resolution Without Considering Pixel Size

**What people do:** Render at 1920x1080, apply 4x4 Bayer matrix dither — the pattern becomes invisibly fine.

**Why it's wrong:** The 1-bit aesthetic requires visible dithering pixels. At full HD the pattern is sub-pixel and the result looks like random noise, losing the ordered Bayer character entirely.

**Do this instead:** Render the scene at a lower internal resolution (e.g., 480p or 720p) and upscale with nearest-neighbor to the display. This gives the dither pattern the chunky, legible character that defines the aesthetic. The Obra Dinn pipeline uses a double-resolution-then-collapse-to-half trick to the same end.

---

## Integration Points

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Platform → Function | GLFW callbacks → InputEvent structs pushed to queue | Platform layer never knows about ECS or gameplay |
| Renderer → ECS | Renderer queries ECS for `[Transform, Renderable]` components | Read-only access — renderer does not write game state |
| Physics → ECS | Physics reads `Transform`, writes corrected `Transform` on collision response | Only system that mutates Transform externally |
| CombatSystem → ECS | Reads `Transform`, `Health`; writes `Health`; dispatches `HitEvent` via EventBus | EventBus decouples UI damage numbers, sound triggers from combat logic |
| DitherPass → Renderer | Reads `sceneFBO` color texture only | Zero knowledge of scene contents — pure image processing |
| LevelManager → ECS + Renderer | Spawns entities into ECS world; registers static geometry with Renderer | LevelManager owns the level-data schema |
| AudioSystem → EventBus | Subscribes to game events (hit, death, ambient triggers) | No direct coupling to combat or AI code |

### External Dependencies

| Library | Purpose | Integration Point |
|---------|---------|-------------------|
| GLFW | Window, OpenGL context, input | Platform layer only |
| OpenGL (via glad) | GPU rendering | Rendering layer only — nothing above includes GL headers |
| GLM | Math (vec3, mat4, quaternions) | Core/math — used everywhere above Platform |
| EnTT | ECS registry | ecs/World.h wraps it — game code uses World interface, not EnTT directly |
| stb_image | PNG/JPG texture loading | TextureManager only |
| Assimp (optional) | OBJ mesh loading | MeshManager only — can be replaced with custom loader later |
| miniaudio or OpenAL | Audio playback | AudioSystem only |

---

## Scaling Considerations

This is a single-player desktop game, not a networked service. "Scaling" means handling more entities and larger levels without framerate drops.

| Concern | At Level Scale (100 entities) | At Complex Boss Level (500 entities) | At Theoretical Limit |
|---------|-------------------------------|--------------------------------------|----------------------|
| ECS iteration | No problem, cache-friendly | Still fine with EnTT | Profile if >2000 entities |
| Draw calls | Batch static geometry | Instanced rendering for repeated meshes | Frustum culling + spatial partitioning |
| Collision | Brute-force AABB fine | Spatial grid (cells by level chunk) | BVH tree |
| AI updates | Update all enemies each tick | Throttle distant enemies to 10Hz | Distance-based LOD for AI |
| Dither pass | O(pixels), not O(entities) — always fast | Same | Not a bottleneck |

### First Bottleneck

Draw calls from static level geometry, not enemy count. The gothic cathedral mesh is large. Fix: merge static geometry into a single batched VBO at level load time. This is the first optimization to make if framerate suffers.

---

## Sources

- Isetta Engine — Engine architecture blog: https://isetta.io/blogs/engine-architecture/ (TLS error on fetch, cited from WebSearch findings)
- GAMES104 Layered Architecture: https://alalba221.github.io/blog/engine/LayeredArchitectureOfGameEngine
- Preshing — How to Write Your Own C++ Game Engine: https://preshing.com/20171218/how-to-write-your-own-cpp-game-engine/
- Whiskers Engine (modern C++ engine example): https://www.rhelmer.org/blog/building-whiskers-engine-cpp-game-engine/
- Return-of-the-One-Bit (OpenGL 1-bit dithering implementation): https://github.com/yunjay/Return-of-the-One-Bit
- Ultra Effects Part 9 — Obra Dithering (Unity but pipeline logic applies): https://danielilett.com/2020-02-26-tut3-9-obra-dithering/
- LearnOpenGL Framebuffers (FBO architecture): https://learnopengl.com/Advanced-OpenGL/Framebuffers
- EnTT ECS library: https://github.com/skypjack/entt
- GLEngine (OpenGL + SDL2 + ECS reference project): https://github.com/kaminskyalexander/GLEngine
- Pikuma — Tools & Libraries for C++ Game Engine: https://pikuma.com/blog/how-to-make-your-own-cpp-game-engine
- GameDev.net — Entity-Component-System pattern: https://www.gamedeveloper.com/design/the-entity-component-system---an-awesome-game-design-pattern-in-c-part-1-

---

*Architecture research for: Custom C++ first-person 3D roguelike with 1-bit dithered rendering*
*Researched: 2026-03-23*
