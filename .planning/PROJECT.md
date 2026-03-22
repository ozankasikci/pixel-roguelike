# 3D Roguelike

## What This Is

A first-person 3D roguelike with a distinctive 1-bit dithered black-and-white visual style, built on a custom C++ engine. The player explores gothic cathedral-like environments, fighting enemies with melee and ranged weapons, progressing through handcrafted levels with escalating difficulty and boss encounters.

## Core Value

The 1-bit dithered 3D rendering — a visually striking, modern take on retro aesthetics that makes the game instantly recognizable. If the look doesn't land, nothing else matters.

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] Custom C++ rendering engine with 3D scene rendering
- [ ] Post-process 1-bit dithering shader (black and white only)
- [ ] First-person camera with smooth WASD + mouse look controls
- [ ] Gothic architectural environments (arches, pillars, torches)
- [ ] Melee combat (swords, axes — swing, hit detection)
- [ ] Ranged combat (bows, spells — projectiles)
- [ ] Basic enemy AI (patrol, detect, attack)
- [ ] Weapon progression (find/upgrade weapons)
- [ ] Multiple enemy types with different behaviors
- [ ] Boss encounters
- [ ] Handcrafted level design
- [ ] Point light sources (torches, glowing effects) that work with the dithering aesthetic
- [ ] Player health and damage system

### Out of Scope

- Procedural level generation — handcrafted levels only
- Multiplayer — single-player experience
- Mobile/web platforms — desktop only
- Story/narrative system — gameplay-driven, not story-driven
- Inventory management UI — keep it minimal for v1

## Context

- Visual reference: 1-bit dithered aesthetic similar to Return of the Obra Dinn — full 3D geometry rendered then reduced to pure black and white via ordered dithering
- The dithering post-process is the defining visual feature; it needs to handle lighting, depth, and atmospheric effects gracefully
- Gothic cathedral architecture is the primary environmental theme (arched ceilings, stone pillars, wall-mounted torches)
- Custom engine means building from scratch: windowing, input, rendering pipeline, game loop, physics/collision

## Constraints

- **Engine**: Custom C++ — no Unity/Unreal/Godot
- **Graphics API**: To be determined by research (OpenGL vs Vulkan tradeoff)
- **Visual style**: Strictly 1-bit (black and white) — no grayscale, no color
- **Platform**: Desktop (Windows/macOS/Linux)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Custom C++ engine | Full control over rendering pipeline for the dithering effect | — Pending |
| Post-process dithering | Render full 3D first, then dither — simpler pipeline, easier to tune | — Pending |
| First-person perspective | Immersive, matches the gothic dungeon crawling feel | — Pending |
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

---
*Last updated: 2026-03-23 after initialization*
