# 3D Roguelike

## What This Is

A first-person 3D roguelike with a distinctive 1-bit dithered black-and-white visual style, built on a custom C++ engine. The player explores gothic cathedral-like environments, fighting enemies with melee and ranged weapons, progressing through handcrafted levels with escalating difficulty and boss encounters.

## Core Value

The 1-bit dithered 3D rendering — a visually striking, modern take on retro aesthetics that makes the game instantly recognizable. If the look doesn't land, nothing else matters.

## Requirements

### Validated

- ✓ Custom C++ rendering engine with 3D scene rendering — v1.0
- ✓ Post-process 1-bit dithering shader (black and white only) — v1.0
- ✓ Point light sources (torches, glowing effects) that work with the dithering aesthetic — v1.0
- ✓ Modular ECS architecture with clean engine/game boundary — v1.0
- ✓ Equipment/inventory system (Dark Souls-style weapon management) — v1.0
- ✓ Editor build system (build-and-run from editor) — v1.0

### Active

- [ ] First-person camera with smooth WASD + mouse look controls
- [ ] Gothic architectural environments (arches, pillars, torches)
- [ ] Player collision with walls, floors, objects
- [ ] Melee combat (swords, axes — swing, hit detection)
- [ ] Basic enemy AI (patrol, detect, attack)
- [ ] Player health and damage system
- [ ] Health indicator visible on HUD (shape-based, no color)
- [ ] Player death and game over
- [ ] Enemy hit reaction (knockback/flinch)
- [ ] Enemy health and death
- [ ] Game state saves at level boundaries
- [ ] Options menu (FOV, sensitivity, fullscreen)

### Out of Scope

- Procedural level generation — handcrafted levels only
- Multiplayer — single-player experience
- Mobile/web platforms — desktop only
- Story/narrative system — gameplay-driven, not story-driven
- Animated/flickering dither — destroys video stream readability
- Controller support — WASD + mouse is genre standard; defer to later
- Color in any form — strictly 1-bit black and white

## Context

Shipped v1.0 Engine Foundation with ~23,000 LOC C++ across 145 commits (2026-03-23 to 2026-03-29).

**Tech stack:** OpenGL 4.1 Core Profile, GLFW 3.4, GLM, EnTT 3.16, Jolt Physics, Dear ImGui, spdlog, GLAD 2, Assimp, stb_image/stb_truetype.

**Engine architecture:** Modular CMake build (engine_core, engine_rendering, engine_input, engine_physics, engine_scene, engine_ui, gameplay, editor). ECS with free-function systems. Clean engine/game boundary — engine compiles with zero game-layer imports. ActionMap for input binding. TypeScript scripting via QuickJS.

**Three executables:** `pixel-roguelike` (game), `level-editor` (scene editor with build system), `procedural-model-viewer` (asset preview).

**Known tech debt:** RuntimeInputState dead code, duplicated clamp helpers, test_content_registry path resolution failure.

## Constraints

- **Engine**: Custom C++ — no Unity/Unreal/Godot
- **Graphics API**: OpenGL 4.1 Core Profile (macOS ceiling)
- **Visual style**: Strictly 1-bit (black and white) — no grayscale, no color
- **Platform**: Desktop (Windows/macOS/Linux)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Custom C++ engine | Full control over rendering pipeline for the dithering effect | ✓ Good — clean modular architecture achieved |
| Post-process dithering | Render full 3D first, then dither — simpler pipeline, easier to tune | ✓ Good — Bayer 8x8 with world-space anchoring works well |
| OpenGL 4.1 Core Profile | macOS caps at 4.1; sufficient for fullscreen quad dither pass | ✓ Good — no need for Vulkan complexity |
| EnTT ECS | Header-only, cache-friendly, well-documented | ✓ Good — clean component/system separation |
| Jolt Physics | Modern C++20, better API than Bullet3, character controller included | ✓ Good — physics system working behind pimpl |
| Engine/game boundary | Engine layer compiles with zero game imports for reusability | ✓ Good — achieved in v1.0 Phase 4 |
| Free-function systems | Game systems as free functions called from RuntimeGameSession::tick() | ✓ Good — simpler than System base class for game logic |
| Equipment before combat | Build inventory data layer before combat system needs it | ✓ Good — clean foundation for weapon mechanics |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd:transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-03-29 after v1.0 Engine Foundation milestone*
