---
type: quick
id: 260329-uom
title: Fix warden office room being too small
completed: "2026-03-29"
duration: ~5 min
tasks_completed: 2
files_modified: 2
commits:
  - fe2be3a
  - 35c3345
tags: [scene, level-design, prison, geometry]
---

# Quick Task 260329-uom: Fix Warden Office Room Being Too Small

**One-liner:** Raised warden office ceiling from 2.5m to 3.5m by updating all three wall mesh generators in PrisonAssets.cpp and repositioning ceiling tiles, lights, wall colliders, and adding a ceiling collider in warden_office.scene.

## Tasks Completed

### Task 1: Raise wall meshes from 2.5m to 3.5m in PrisonAssets.cpp

**Commit:** `fe2be3a`
**File:** `src/game/levels/prison/PrisonAssets.cpp`

Changes to three wall creation functions:

- `createPrisonWall()`: main slab center Y 1.25->1.75, half-height 1.25->1.75; top border strip Y 2.46->3.46; left/right vertical border strips center Y 1.25->1.75, half-height 1.21->1.71
- `createPrisonWallWindow()`: top section center Y 2.25->2.75, half-height 0.25->0.75 (now fills from 2.0m to 3.5m); bottom section and window opening unchanged
- `createPrisonWallDoor()`: top section center Y 2.3->2.8, half-height 0.2->0.7 (now fills from 2.1m to 3.5m); door opening and side sections unchanged

Build verified: `cmake --build build --target pixel-roguelike` passed.

### Task 2: Update warden_office.scene -- ceiling, lights, colliders

**Commit:** `35c3345`
**File:** `assets/scenes/warden_office.scene`

- All 12 `prison_ceiling` tile Y positions raised from 2.5 to 3.5
- Spot light Y raised from 2.4 to 3.4
- Ambient lights raised proportionally: 1.8->2.5 (back), 2.0->2.8 (mid), 2.0->2.8 (front)
- Wall colliders updated from (center Y=1.25, half-height=1.25) to (center Y=1.75, half-height=1.75) on back/left/right walls
- Door top collider updated from (2.3, 0.2) to (2.8, 0.7)
- Added ceiling collider: `collider_box 0.0 3.5 4.0 3.0 0.05 4.0 node node_ceil` to prevent player clip-through

## Deviations from Plan

None - plan executed exactly as written.

## Known Stubs

None.

## Self-Check: PASSED

- `fe2be3a` exists in git log
- `35c3345` exists in git log
- `src/game/levels/prison/PrisonAssets.cpp` modified: confirmed
- `assets/scenes/warden_office.scene` modified: confirmed
