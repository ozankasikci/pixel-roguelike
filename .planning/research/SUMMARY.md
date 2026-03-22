# Project Research Summary

**Project:** Custom C++ 3D Roguelike with 1-bit Dithered Rendering
**Domain:** First-person 3D dungeon crawler, custom C++ engine
**Researched:** 2026-03-23
**Confidence:** MEDIUM-HIGH

## Executive Summary

This is a custom C++ first-person dungeon crawler distinguished by its 1-bit ordered dithering aesthetic — every frame rendered as pure black and white through a Bayer matrix post-process shader. Research confirms the project is technically well-scoped: OpenGL 4.6 + GLFW + EnTT ECS + Jolt Physics is the right stack, the rendering pipeline pattern is proven (Return of the Obra Dinn as primary reference), and the feature set is achievable for a solo or small team. The recommended approach is a strict bottom-up build order — platform layer, then rendering primitives, then the dithering post-process validated visually, then ECS and camera, then gameplay systems — with every sprint producing a playable build. Skipping ahead or building systems "in anticipation" of later needs is the single most common failure mode for custom engine projects.

The central technical challenge is the dithering pipeline itself. Three specific hazards dominate: (1) temporal instability — screen-space dithering produces a "swimming" crawl on camera movement that causes motion sickness, requiring world-space pattern anchoring from day one; (2) color space corruption — the dither threshold comparison must occur in linear space before any gamma correction, or torch falloff and tonal gradients will look wrong; (3) internal render resolution — applying a Bayer dither at full 1080p produces invisible fine noise rather than the chunky legible pattern that defines the aesthetic, requiring downscaling to 480-720p before the dither pass. All three must be solved in Phase 1 before any other system is built on top of the renderer.

The feature set is focused by the project's scope constraints: no procedural generation, no multiplayer, no inventory management, no story system. This is the right call — handcrafted gothic cathedral levels, deliberate melee-and-ranged combat, and 3-5 enemy types with a boss is a complete, shippable game. The differentiating visual identity (1-bit dithered point lighting responding to torch proximity) is technically achievable as a direct outcome of the rendering architecture. The risk is scope creep into "standard" game features that fight the aesthetic — stamina systems, color-coded HUDs, animated dither noise — each of which is documented in research as an anti-feature for this specific game.

## Key Findings

### Recommended Stack

The stack is built around OpenGL 4.6 (Core Profile) as the graphics API, deliberately chosen over Vulkan. The 1-bit output means GPU throughput is never the bottleneck — what matters is rapid iteration on the dithering shader, which OpenGL enables with ~100 lines to draw a triangle vs Vulkan's 1,000+. GLFW 3.4 handles windowing and input with a focused API that maps cleanly to an event bus architecture. EnTT v3.16.0 provides the ECS, requiring C++20 and delivering cache-friendly component iteration used in production games (Minecraft). Jolt Physics v5.4.0 replaces the now-stagnant Bullet3 and includes a character controller — critical for first-person capsule collision.

The dithering post-process is a fullscreen quad fragment shader that reads from an offscreen HDR framebuffer, computes luminance, and applies a Bayer matrix threshold to output pure black/white. This is the "Obra Dinn pattern." stb_image handles texture loading, stb_truetype handles HUD text rendering in 1-bit, OpenAL Soft provides 3D positional audio (torch crackle as the player approaches), and Dear ImGui provides a runtime debug overlay for tuning dither thresholds and inspecting ECS state.

**Core technologies:**
- C++20: Required by EnTT v3.16; `std::span`, concepts, and designated initializers reduce boilerplate across engine code
- CMake 3.28+: De-facto standard; FetchContent manages GLM, EnTT, Jolt, ImGui, spdlog without vcpkg/Conan overhead
- OpenGL 4.6 Core Profile: Right choice for this project — the 1-bit output is the fidelity ceiling, not GPU throughput
- GLFW 3.4: Minimal, focused windowing/input; no SDL audio/networking bloat
- GLAD 2: Modern OpenGL loader; generates only the extension subset needed; replaces stagnant GLEW
- GLM 1.0.3: Header-only math library with GLSL-identical syntax; the only maintained option with full spec coverage
- EnTT v3.16.0: Header-only ECS; sparse-set component storage; forces clean game-state/render separation
- Jolt Physics v5.4.0: Modern C++20 physics with included character controller; replaces Bullet3
- OpenAL Soft v1.25.1: 3D positional audio; the standard for spatial audio in custom C++ engines
- Dear ImGui v1.92.6: Runtime debug overlay for dither parameter tuning and ECS inspection

**Avoid:** GLEW (stagnant), Vulkan (boilerplate dominates schedule), Assimp (use tinygltf instead), Boost (C++20 STL covers it), OpenGL compatibility profile.

### Expected Features

Research confirms a focused MVP that validates the core concept — does 1-bit first-person dungeon combat feel good — without scope bloat.

**Must have (table stakes):**
- First-person camera with smooth mouse look, FOV slider (60-120°), and separate sensitivity controls — absence generates immediate motion sickness complaints; Obra Dinn's oversight is the cautionary note
- Player health system with 1-bit-readable HUD (shape-based, not color-coded — color coding is unavailable in 1-bit)
- Melee combat with hit detection, audio feedback, and enemy reaction — the core loop
- Ranged combat with projectile travel and hit detection — differentiates from pure melee
- 3+ enemy types with patrol/detect/attack AI state machines — minimum for combat variety
- One boss encounter — confirms the game has a shape and a climax
- Point light sources (torches) that affect dither density — the signature visual differentiator
- 2-3 handcrafted gothic cathedral levels — enough to feel like a game, not a prototype
- Save at level boundaries — unshippable without it
- Basic audio: footsteps, weapon impacts, ambient dungeon sounds
- Options menu: FOV, sensitivity, audio volume, fullscreen toggle
- Clear death/game over state

**Should have (competitive differentiators):**
- World-space anchored dithering (not screen-space) — prevents motion sickness and is the defining quality split vs naive implementations
- Gothic cathedral architecture specifically designed for 1-bit silhouette readability — arched ceilings, stone pillars, torch light in pure black/white
- Dithering-responsive point lighting — the technical differentiator with direct visual payoff
- Weapon progression (find/upgrade) — player attachment without inventory complexity
- Enemy silhouette-first design — skeletal figures and armored knights read clearly in 1-bit

**Defer (v2+):**
- Full controller support — WASD + mouse is genre standard; adds input mapping complexity
- Procedural level generation — explicitly out of scope; destroys handcrafted gothic atmosphere
- Inventory management UI — fights the 1-bit visual identity
- Stamina/dodge roll system — adds third combat resource; unnecessary complexity given design philosophy
- Steam achievements/leaderboards — platform integration non-trivial in custom C++; zero game quality impact
- Multiplayer — no netcode infrastructure in scope

**Anti-features to avoid:** Animated dithering noise (destroys video stream readability), character class/RPG stats (implies leveling progression out of scope), procedural audio generation (marginal payoff, high complexity).

### Architecture Approach

The engine follows a strict five-layer dependency model (Platform → Core → Rendering → Function → Game) where upper layers call downward only. The rendering layer is explicitly separated into Scene Renderer (geometry + lighting → offscreen FBO) and DitherPass (fullscreen quad Bayer shader → screen), keeping the two concerns isolated. The ECS uses components as plain data structs with no methods — logic lives in systems in the gameplay layer, keeping cache locality intact. All game state (Main Menu / Playing / Paused / Dead / Level Complete) and all enemy AI (Patrol / Alert / Attack) are finite state machines, preventing flag soup.

**Major components:**
1. Platform Layer (GLFW): OS window, OpenGL context, raw input events — nothing above this knows about GLFW
2. Rendering Layer: ShaderManager, TextureManager, MeshManager, Framebuffer (FBO), Renderer (scene draw calls + lighting), DitherPass (fullscreen Bayer post-process)
3. Core Layer: Math (GLM), EventBus (publish/subscribe decoupling), ResourceManager (GPU resource cache)
4. ECS World (EnTT): Entity IDs, component storage (Transform, Health, AIState, Renderable, Light), system iteration
5. Gameplay Layer: Camera, Physics/Collision (Jolt), CombatSystem (melee sweep + projectile lifecycle), EnemyAI (FSM), LevelManager, GameState (FSM)
6. Audio System (OpenAL Soft): Positional SFX, ambient; subscribes to EventBus; no direct combat/AI coupling

**Key architectural decisions:**
- DitherPass is isolated in `rendering/DitherPass.h` — never mixed with scene rendering logic; easy to toggle, tune, or replace
- No `IRenderer` abstraction — write directly to OpenGL; only extract abstraction when there are three concrete cases
- Fixed physics timestep (60Hz accumulator pattern) — physics deterministic at any framerate
- ECS components are plain structs, never classes with virtual methods — cache-friendly iteration is mandatory for 200+ enemy entities

### Critical Pitfalls

1. **The Engine Trap** — Building infrastructure without gameplay. Every week must produce something playable. Build first-person movement + collision before any abstraction. If no playable build exists after one month, the project is off-track. Warning sign: any class name containing "generic" or "abstract."

2. **Swimming Dither (temporal instability)** — Screen-space UV coordinates for the Bayer pattern lookup produce a "crawling" effect on camera movement that is actively nauseating in first-person. Must be fixed at architecture time by anchoring the pattern to world-space position (reconstructed from depth buffer in the post-process pass). Cannot be retrofitted easily. Non-negotiable from day one.

3. **Gamma/Color Space Mismatch** — The dither threshold comparison must occur in linear color space before gamma correction. If GL_FRAMEBUFFER_SRGB is enabled globally (not only on the final output pass), torch falloff gradients will look harsh and tonal range will be crushed. Establish the framebuffer pipeline and color space strategy before writing the first shader.

4. **Dither Applied at Full Resolution** — At 1920x1080 a 4x4 Bayer matrix produces sub-pixel noise that loses the chunky ordered-dither character. The scene must be rendered at reduced internal resolution (480p or 720p) and upscaled with nearest-neighbor to the display. Obra Dinn used a double-resolution-then-collapse trick for the same reason.

5. **Player Stuck on Corners (AABB collision)** — Simple AABB overlap detection fails at wall edges, corners, and steps. Use capsule sweep testing with Quake-style clip-and-slide collision response from the start. An AABB discrete collision implementation requires a full rewrite when level design begins; capsule sweep does not.

## Implications for Roadmap

Based on the combined research, the architecture's build order maps directly to roadmap phases. The dependency chain is non-negotiable: the dithering post-process must be validated before any lighting is tuned; lighting must be stable before levels are authored; levels must exist before combat can be meaningfully tested; combat must work before enemy AI is useful.

### Phase 1: Engine Foundation and Dithering Pipeline

**Rationale:** The 1-bit dithering post-process is the entire visual identity of the game. It must be validated first because every subsequent system (lighting, level design, UI, enemy readability) depends on how things look through the dither. Building gameplay before the renderer is validated means rebuilding everything when the dither changes something fundamental. Three critical pitfalls (swimming dither, color space mismatch, full-resolution pattern) must be resolved here or they become expensive retrofits.
**Delivers:** OpenGL 4.6 context via GLFW, fullscreen quad dithering post-process with world-space pattern anchoring, correct linear color pipeline, internal rendering at 480-720p with nearest-neighbor upscale, Dear ImGui debug overlay for real-time dither parameter tuning.
**Addresses:** FOV slider (options menu scaffolding), first-person camera locomotion
**Avoids:** Swimming dither (Pitfall 2), color space mismatch (Pitfall 3), full-resolution dither (anti-pattern 5), Vulkan choice (Pitfall 4), premature abstraction (Pitfall 6), shader compilation stutter (Pitfall 8)
**Research flag:** SKIP — pattern is well-documented; OpenGL FBO + fullscreen quad + Bayer matrix is a solved problem with reference implementations

### Phase 2: First-Person Player Movement and Collision

**Rationale:** Movement feel is the foundation of all gameplay. Enemy AI, combat hit detection, level design, and boss encounters are all meaningless if the player doesn't move well. Collision must be capsule-sweep from the start or level design will expose fundamental problems that require a full movement rewrite. This is also the phase most likely to fall into the Engine Trap — scope it narrowly to "player can walk through a dithered gothic room."
**Delivers:** WASD + mouse first-person controller, capsule sweep + clip-and-slide collision vs level geometry, gravity and step height, ECS World setup (EnTT), Transform and Health components, player entity spawning
**Addresses:** First-person camera with smooth mouse look, mouse sensitivity settings, FOV slider
**Avoids:** Engine Trap (Pitfall 1), player stuck on corners (Pitfall 5), variable timestep physics (Pitfall 4 analogue)
**Research flag:** SKIP — capsule collision + clip-and-slide is well-documented; Jolt Physics character controller is the implementation vehicle

### Phase 3: Level Loading and Lighting

**Rationale:** Lighting must be tuned through the dithering pass, not before it. Building this phase after the dithering pipeline (Phase 1) ensures torch attenuation curves can be tuned for 1-bit output directly. Handcrafted level geometry must exist before combat can be tested in meaningful spatial contexts. Level loading is required before level design work can begin.
**Delivers:** LevelManager (load geometry + spawn points + light positions), point light sources (torches) feeding the scene shader, Jolt Physics static mesh collision for level geometry, 1-2 prototype gothic cathedral rooms with working torch lighting dithered correctly
**Addresses:** Point light sources (torches) that affect dither density, gothic cathedral architecture, level geometry loading
**Avoids:** Lighting that doesn't survive the dithering pass (Pitfall 7); hardcoded geometry as permanent shortcut (technical debt)
**Research flag:** NEEDS RESEARCH — level file format choice (OBJ vs glTF via tinygltf vs custom binary) has downstream impact on the asset pipeline; and torch attenuation tuning for 1-bit may need iteration to find the right falloff curve

### Phase 4: Combat System

**Rationale:** Once a player can move through a lit level, the core loop can be validated. Melee and ranged combat are both required for the MVP — ranged specifically because it differentiates from pure melee dungeon crawlers and exercises projectile physics. Both must work before enemy AI is meaningful. Hit feedback (audio + enemy reaction) is part of the definition of "working" combat — not polish.
**Delivers:** CombatSystem with melee swept-sphere hit detection and projectile lifecycle, Health components and damage application, hit feedback (screen shake, sound effect, enemy stagger), player death state, weapon pickup and swap mechanic
**Addresses:** Melee combat with hit detection and feedback, ranged combat (projectile weapon), player health system with 1-bit HUD indicator, clear death/game over state
**Avoids:** Hit detection using only center-screen ray (UX pitfall); weapon viewmodel obscuring the screen during attack (UX pitfall)
**Research flag:** SKIP — combat patterns are well-established; swept sphere for melee, projectile entity lifecycle for ranged, EventBus for decoupled hit events are all standard

### Phase 5: Enemy AI and Types

**Rationale:** Enemy AI requires a working combat system to be meaningful — an enemy that attacks must be able to do damage, and an enemy that reacts to hits must have a health component. Multiple enemy types are required for the MVP (3+ minimum) but they are ECS component combinations, not new class hierarchies, making them cheap to add once the AI FSM is working. Boss encounter is gated on all combat and AI systems being solid.
**Delivers:** EnemyAI with patrol/detect/attack FSM, 3 enemy types as distinct ECS component configurations (grunt, ranged, heavy), enemy hit reactions and death, one boss encounter with distinct behavior and visual silhouette
**Addresses:** Enemy AI (patrol, detect, engage), multiple enemy types with distinct behaviors, boss encounter, enemy silhouette-first design for 1-bit readability
**Avoids:** Inheritance hierarchies for game objects (anti-pattern 3); enemies invisible at range (UX pitfall — minimum ambient term prevents full-black dithering)
**Research flag:** SKIP — FSM enemy AI is well-documented for this complexity level; no behavior trees needed

### Phase 6: Audio, Game State, and Save System

**Rationale:** Audio absence breaks immersion immediately and is consistently cited as a "feels broken" failure. This phase completes the game loop — death screen, level transitions, and save at level boundaries are all required for any player to understand they're playing a complete game rather than a prototype. OpenAL Soft integration via EventBus keeps audio decoupled from combat and AI code.
**Delivers:** AudioSystem (OpenAL Soft) with footstep, weapon impact, and ambient dungeon sounds; spatial torch crackle audio; GameState FSM (Menu/Playing/Paused/Dead/LevelComplete) with transitions; save/checkpoint at level boundaries; options menu (FOV, sensitivity, volume, fullscreen)
**Addresses:** Basic audio (footsteps, combat, ambient), save/checkpoint system, options/settings menu, clear death/game over state, game state transitions
**Avoids:** Audio absence breaking immersion; losing all progress on exit
**Research flag:** SKIP — OpenAL Soft + EventBus integration is well-documented; game state FSM is a standard pattern

### Phase 7: Content — Handcrafted Levels and Weapon Variety

**Rationale:** With all engine and gameplay systems validated, the final phase is content delivery. Level design can now iterate quickly because the rendering pipeline, collision, combat, and enemy behavior are all locked. 2-3 gothic cathedral levels satisfy the MVP; weapon variants (melee archetype + ranged archetype) provide the minimal weapon variety the genre expects.
**Delivers:** 2-3 completed handcrafted gothic cathedral levels with enemy placements, light positions, and boss encounter; weapon variants (one melee archetype, one ranged archetype); run statistics / death screen
**Addresses:** 2-3 handcrafted levels in gothic cathedral setting, weapon variety, final boss encounter polish
**Avoids:** Level design invalidating collision system (levels authored against proven player movement); late rendering pipeline changes forcing level geometry rework
**Research flag:** NEEDS RESEARCH — gothic architectural 3D asset creation (OBJ/glTF authoring for 1-bit silhouette readability) may need dedicated exploration of asset workflow; enemy mesh design for high-contrast 1-bit silhouettes is a specialized design constraint

### Phase Ordering Rationale

- **Renderer before gameplay:** Every gameplay system's look is filtered through the dithering pass. Tuning lighting, HUD readability, enemy visibility, and weapon viewmodel placement all require the final visual output. Building gameplay in a non-dithered view means re-evaluating every decision later.
- **Movement before combat:** Combat feel is 80% movement. The player needs to be able to strafe, close distance, and back away fluently before melee swing timing and ranged weapon arc have meaning.
- **Level loading before full combat testing:** Combat in a 10x10 box is not representative. The gothic cathedral architecture — narrow corridors, arched rooms, torch-lit alcoves — is the designed context for the combat.
- **All systems before content:** Level authoring against a broken collision system, untuned dithering, or incomplete combat requires expensive rework. Systems first, then fill with content.

### Research Flags

Phases needing deeper research during planning:
- **Phase 3 (Level Loading and Lighting):** Level file format choice (OBJ vs glTF/tinygltf vs custom) has downstream impact on the asset pipeline and level iteration workflow; tinygltf is recommended in STACK.md but integration specifics need validation. Torch attenuation curve tuning for 1-bit output is empirical and may require research into Obra Dinn's specific approach.
- **Phase 7 (Content):** 3D asset creation workflow for 1-bit silhouette readability is a specialized constraint with limited documentation. Enemy mesh design guidelines for high-contrast black/white rendering need research before the asset authoring phase begins.

Phases with standard patterns (skip research):
- **Phase 1 (Dithering Pipeline):** OpenGL FBO + fullscreen quad + Bayer matrix is fully documented; Obra Dinn reference implementation is publicly available
- **Phase 2 (Player Movement):** Capsule sweep + clip-and-slide is canonical; Jolt Physics character controller handles the implementation
- **Phase 4 (Combat System):** Swept-sphere melee, projectile entity lifecycle, EventBus-decoupled hit events are all standard patterns
- **Phase 5 (Enemy AI):** Three-state FSM (patrol/detect/attack) is well-documented for this complexity level
- **Phase 6 (Audio + Game State):** OpenAL Soft + EventBus integration and FSM game state management are both standard

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | All library versions verified from GitHub release pages (Dec 2025 or earlier); OpenGL vs Vulkan recommendation is well-argued and confirmed by multiple sources |
| Features | MEDIUM-HIGH | Table stakes and anti-features are well-supported; differentiator list is synthesized from Obra Dinn dev logs and genre analysis; competitor analysis covers relevant games |
| Architecture | MEDIUM-HIGH | Five-layer model and ECS patterns are well-established; dithering pipeline specifics sourced from working reference implementations; build order is synthesized recommendation, not canonical |
| Pitfalls | HIGH | Swimming dither verified from Obra Dinn developer logs; engine trap from multiple solo developer post-mortems; collision pitfall from multiple custom engine references; gamma/color space from LearnOpenGL documentation |

**Overall confidence:** MEDIUM-HIGH

### Gaps to Address

- **tinygltf vs custom level format:** Research recommends tinygltf for mesh loading but does not detail the full level data schema (light positions, spawn points, trigger zones). During Phase 3 planning, define the level file format before authoring begins.
- **Torch attenuation curves for 1-bit:** The research notes that 1/r² physically-correct falloff often produces visible edge rings in dithered output and recommends softer falloff, but specific curves are empirical. Plan for a tuning session in Phase 3 with the dithering pass active.
- **1-bit enemy mesh guidelines:** Enemy silhouette-first design is cited as a differentiator but no concrete mesh complexity or contrast guidelines exist in the research. Before Phase 7 asset authoring, establish minimum contrast ratio and silhouette complexity rules for enemy models.
- **macOS support:** OpenGL is deprecated on macOS at 4.1; the project targets OpenGL 4.6. If macOS is a deployment target, this is a blocking constraint requiring MoltenVK or metal-cpp. Research flags it but does not resolve the platform priority decision.

## Sources

### Primary (HIGH confidence)
- GLM GitHub releases — version 1.0.3 confirmed Dec 2025
- GLFW release notes — 3.4 features confirmed
- GLAD GitHub releases — v2.0.8 confirmed Sep 2025
- Jolt Physics GitHub releases — v5.4.0 confirmed Sep 2025
- Dear ImGui GitHub releases — v1.92.6 confirmed Feb 2025
- OpenAL Soft GitHub releases — v1.25.1 confirmed Jan 2025
- EnTT GitHub releases — v3.16.0 confirmed Nov 2025
- Lucas Pope / Return of the Obra Dinn developer logs (TIGSource) — dithering technique, temporal stability, light source count
- "Fix Your Timestep!" — Gaffer on Games — fixed timestep game loop
- LearnOpenGL Framebuffers — FBO architecture

### Secondary (MEDIUM confidence)
- Daniel Ilett dither tutorial — Bayer matrix GLSL approach
- Obra Dinn 1-bit shader discussions (Hacker News) — world-space anchoring technique
- "Surface-Stable Fractal Dither" by Aras Pranckevičius 2025 — pattern stability
- GAMES104 Layered Architecture — five-layer engine model
- EnTT ECS library documentation — component storage patterns
- Capsule collision detection (Wicked Engine) — clip-and-slide implementation
- "The Best First-Person Roguelikes" (The Gamer, Rogueliker) — genre feature expectations
- Solodevs and the engine trap (Karl Zylinski) — Engine Trap pitfall
- OpenGL vs Vulkan comparison — API selection rationale

### Tertiary (LOW confidence)
- Isetta Engine architecture blog — layered model (TLS error prevented direct fetch; cited from search findings)
- Whiskers Engine blog — modern C++ engine structure reference
- GLEngine GitHub (OpenGL + SDL2 + ECS reference) — structural reference

---
*Research completed: 2026-03-23*
*Ready for roadmap: yes*
