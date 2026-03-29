# Modular Prison Asset Kit & Warden's Office Level Design

**Status:** Approved
**Date:** 2026-03-29

## Overview

A modular prison asset kit for the Warden game's first playable level — the Warden's Office. The kit uses a hybrid approach: procedural C++ generation for architectural and furniture pieces, with `.glb` imports reserved for detailed props later. All assets target the existing 1-bit dithered rendering pipeline.

## Grid System

- **Module width:** 2m
- **Module height (ceiling):** 2.5m
- **Wall thickness:** 0.2m
- **Floor/ceiling thickness:** 0.1m

The grid is deliberately smaller than the cathedral's 3m system. Low ceilings and narrow walls sell "prison" immediately; the contrast will make later areas feel different.

## Asset Kit — Architectural (Procedural)

All architectural pieces are procedurally generated from merged primitives (cubes, cylinders, planes) using `mergeMeshes()`, following the same pattern as `createRomanesqueDoorFrameMesh()`.

### Wall Panels (2m wide × 2.5m tall × 0.2m thick)

**`prison_wall`** — Solid concrete slab. A single cube with a subtle inset border created by a thinner overlay cube, giving the illusion of a poured-concrete panel joint.

**`prison_wall_window`** — Same base slab with a rectangular cutout (0.6m × 0.4m, positioned at 1.8m height). Three vertical bars (cylinders, r=0.015m) span the opening. Constructed as four cube sections around the void plus bar cylinders.

**`prison_wall_door`** — Slab with a 0.9m × 2.1m door-frame cutout. Frame edges are inset steel angle pieces (thin cubes). The door itself is a separate mesh.

### Floor & Ceiling (2m × 2m × 0.1m)

**`prison_floor`** — Flat slab with a recessed border line (thin cube inset 0.05m from edges).

**`prison_ceiling`** — Identical geometry, placed inverted at 2.5m height.

### Baseboard (2m × 0.15m × 0.05m)

**`prison_baseboard`** — Thin strip running along wall bases, hiding the wall-floor seam.

## Asset Kit — Furniture & Door (Procedural)

### Prison Door (`prison_door`, 0.9m × 2.1m × 0.05m)

Heavy steel slab (main cube). A small barred observation window (0.25m × 0.3m) at eye height (~1.5m) with two vertical bars. A rectangular handle (thin cube, right side). Three hinge plates on the left edge (thin cubes). Locking mechanism plate below the handle. All boxy, industrial — no curves.

### Desk (`prison_desk`, 1.2m × 0.75m × 0.6m)

Flat top slab (cube). Four angle-iron legs (thin vertical cubes). A modesty panel on the back (thin cube spanning between rear legs). One drawer block on the right side (cube with a thin handle strip).

### Chair (`prison_chair`, 0.45m × 0.45m seat, 0.85m total height)

Seat slab (cube). Four legs (thin cubes). Backrest (vertical cube, no padding). Utilitarian metal stool.

### Filing Cabinet (`prison_cabinet`, 0.45m × 1.3m × 0.6m)

Tall rectangular box. Three horizontal seam lines (thin cubes recessed into the front face) dividing it into drawers. Small handle rectangles on each drawer front.

### Wall Shelf (`prison_shelf`, 0.8m × 0.03m × 0.25m)

Single flat slab with two L-bracket supports underneath (small cube pairs).

## Materials

- **Walls, floor, ceiling:** `Stone` material kind (concrete)
- **Furniture, door, bars, baseboards:** `Metal` material kind
- The existing dither shader handles both without changes

## Warden's Office Level Layout

Room: 6m × 8m, ceiling at 2.5m. Assembled from kit pieces on the 2m grid.

```
     0m    2m    4m    6m
8m   ┌─────┬─────┬─────┐
     │  W  │ W+w │  W  │  ← back wall (center has barred window)
     │shelf│     │ cab │
6m   ├─    │     │    ─┤
     │  W  │     │  W  │  ← side walls
     │     │     │     │
4m   ├─    │     │    ─┤
     │  W  │desk │  W  │  ← desk centered, chair south of it
     │     │chair│     │
2m   ├─    │     │    ─┤
     │  W  │W+d  │  W  │  ← front wall (center has door frame + door)
0m   └─────┴─────┴─────┘
```

### Piece Count

- 12 floor tiles (3×4 grid)
- 12 ceiling tiles
- 14 wall panels: 3 back + 4 per side + 3 front
  - `prison_wall_window` at back-center
  - `prison_wall_door` at front-center
  - All others are solid `prison_wall`
- 14 baseboards along all walls
- 1 desk (centered at ~3.0, 0.0, 4.5)
- 1 chair (south of desk)
- 1 filing cabinet (back-right corner)
- 1 wall shelf (back-left wall)
- 1 prison door (in door frame, front-center)

### Lighting

- One spot light above the desk — harsh, downward cone
- One dim point light near the barred window — faint exterior glow
- Ambient kept very low for oppressive mood

### Colliders

- Box colliders on all walls
- Box colliders on desk, cabinet
- Player spawn at desk position (3.0, 0.0, 4.5), facing south toward the door

### Player Spawn

Player spawns at the desk, facing the door. The story beat: "Player wakes at the warden's desk."

## Code Structure

### New Files

| File | Purpose |
|------|---------|
| `src/game/levels/prison/PrisonAssets.h` | Declaration of `registerPrisonAssets(MeshLibrary&)` |
| `src/game/levels/prison/PrisonAssets.cpp` | Procedural generation of all 11 prison meshes |
| `src/game/scenes/WardenOfficeScene.h` | Scene setup class declaration |
| `src/game/scenes/WardenOfficeScene.cpp` | Creates `LevelLoadRequest` pointing to `.scene` file |
| `assets/scenes/warden_office.scene` | Level data — mesh placements, lights, colliders, spawn |

### Modified Files

| File | Change |
|------|--------|
| `apps/model_viewer/main.cpp` | Add `registerPrisonAssets()` call + viewer presets for each prison mesh |
| `CMakeLists.txt` | Add new source files to build target |

### No Changes To

Engine code, shaders, renderer, ECS systems, physics. The prison kit is purely new content using existing infrastructure.

## Mesh Registration

`registerPrisonAssets(MeshLibrary&)` follows the same pattern as `registerCathedralAssets()`:

```
prison_wall
prison_wall_window
prison_wall_door
prison_floor
prison_ceiling
prison_baseboard
prison_door
prison_desk
prison_chair
prison_cabinet
prison_shelf
```

## Verification

1. Build → run `procedural-model-viewer --list` to confirm all prison meshes register
2. Cycle through each mesh with `[`/`]` to visually verify geometry
3. Run main game loading `warden_office.scene` to verify the assembled room
