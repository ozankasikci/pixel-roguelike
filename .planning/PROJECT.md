# 3D Roguelike

## What This Is

A first-person 3D psychological horror game built on a custom C++ engine. The player explores a mechanical prison environment, investigating recordings, scratch marks, and environmental clues while manipulating doors, cameras, and control terminals to progress through interconnected cell blocks. Stanley Parable-inspired art style with clean, minimalist environments and warm soft lighting.

## Core Value

The Stanley Parable-inspired art style — clean, minimalist environments with warm soft lighting, muted color palette, and stylized realism. Geometric clarity over visual clutter. Institutional/corporate aesthetic applied to a prison setting creates an eerie, liminal atmosphere.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Custom C++ rendering engine with 3D scene rendering
- [ ] Post-process stylize pass with edge detection, bloom, and tone mapping
- [ ] First-person camera with smooth WASD + mouse look controls
- [ ] Prison/institutional environments (offices, cell blocks, corridors)
- [ ] Melee combat (swords, axes — swing, hit detection)
- [ ] Ranged combat (bows, spells — projectiles)
- [ ] Basic enemy AI (patrol, detect, attack)
- [ ] Weapon progression (find/upgrade weapons)
- [ ] Multiple enemy types with different behaviors
- [ ] Boss encounters
- [ ] Handcrafted level design
- [ ] Warm soft lighting with recessed ceiling panels and window daylight
- [ ] Player health and damage system

### Out of Scope

- Procedural level generation — handcrafted levels only
- Multiplayer — single-player experience
- Mobile/web platforms — desktop only
- Story/narrative system — gameplay-driven, not story-driven
- Inventory management UI — keep it minimal for v1

## Context

- Visual reference: The Stanley Parable / Ultra Deluxe — clean surfaces, warm lighting, slightly surreal atmosphere
- The stylize post-process provides edge detection, bloom, and tone mapping for a polished but not photorealistic look
- Prison/institutional architecture is the primary environmental theme (offices, cell blocks, corridors, control rooms)
- Custom engine means building from scratch: windowing, input, rendering pipeline, game loop, physics/collision

## Constraints

- **Engine**: Custom C++ — no Unity/Unreal/Godot
- **Graphics API**: OpenGL 4.1 Core Profile (macOS caps at 4.1)
- **Visual style**: Stanley Parable-inspired — clean, warm, minimalist with muted color palette
- **Platform**: Desktop (Windows/macOS/Linux)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Custom C++ engine | Full control over rendering pipeline for the stylized look | — Pending |
| Post-process stylize pass | Render full 3D first, then apply edge detection + tone mapping | — Pending |
| First-person perspective | Immersive, matches the eerie institutional exploration feel | — Pending |
| Handcrafted levels | More control over atmosphere and pacing than procedural | — Pending |

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

## Current Milestone: v1.1 Editor UX

**Goal:** Bring the level editor to professional quality — matching Unity/Unreal workflows for scene object manipulation.

**Target features:**
- Delete key removes selected scene objects
- Remove distracting selection overlay when another mesh is underneath
- Add meshes to scenes via asset browser/picker
- Move/translate scene objects with gizmos
- Duplicate scene objects
- Match Unity/Unreal-quality editor workflows

## Current State

Phase 10 complete — global keyboard shortcuts (Delete/F/Escape) with text-field safety guards, Ctrl+D duplicate with position offset, smooth animated multi-selection camera framing, and hover highlight (blue-white wireframe on unselected objects). Selection picker overlay removed in favor of hover highlight.

---
*Last updated: 2026-04-01 after Phase 10 completion*
