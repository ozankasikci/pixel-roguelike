---
type: quick
tasks: 2
files_modified:
  - src/game/levels/prison/PrisonAssets.cpp
  - assets/scenes/warden_office.scene
autonomous: true
---

<objective>
Raise warden office ceiling from 2.5m to 3.5m for a spacious, Stanley Parable-style institutional feel.

Purpose: The 2.5m ceiling feels cramped with the 1.8m player. A 3.5m ceiling creates the slightly surreal, liminal office space that matches the art direction.
Output: Taller walls, repositioned ceiling/lights/colliders, added ceiling collider.
</objective>

<context>
@src/game/levels/prison/PrisonAssets.cpp
@assets/scenes/warden_office.scene
</context>

<tasks>

<task type="auto">
  <name>Task 1: Raise wall meshes from 2.5m to 3.5m in PrisonAssets.cpp</name>
  <files>src/game/levels/prison/PrisonAssets.cpp</files>
  <action>
Modify three wall creation functions. All use `addBox` with `generateCube(2.0f)` so scale values are half-extents.

**createPrisonWall():**
- Line 40 main slab: change center Y from 1.25 to 1.75, half-height from 1.25 to 1.75
  `addBox(glm::vec3(0.0f, 1.75f, 0.0f), glm::vec3(1.0f, 1.75f, 0.1f));`
- Line 44 top border strip: change Y from 2.46 to 3.46
  `addBox(glm::vec3(0.0f, 3.46f, 0.105f), glm::vec3(0.96f, 0.02f, 0.005f));`
- Lines 48-50 left/right vertical border strips: change half-height from 1.21 to 1.71, center Y from 1.25 to 1.75
  `addBox(glm::vec3(-0.96f, 1.75f, 0.105f), glm::vec3(0.02f, 1.71f, 0.005f));`
  `addBox(glm::vec3(0.96f, 1.75f, 0.105f), glm::vec3(0.02f, 1.71f, 0.005f));`
- Update the comment on line 38: "Main slab: 2m wide, 3.5m tall, 0.2m thick"

**createPrisonWallWindow():**
- Line 76 bottom section: stays the same (floor to 1.6m)
- Line 78 top section: change to fill from 2.0m up to 3.5m. Center Y = 2.75, half-height = 0.75
  `addBox(glm::vec3(0.0f, 2.75f, 0.0f), glm::vec3(1.0f, 0.75f, 0.1f));`
- Lines 80-82 left/right beside window: stay the same (these are at window height 1.6-2.0m)
- Update the comment on line 77: "Top section: 2.0m to 3.5m"

**createPrisonWallDoor():**
- Line 109 top section: change to fill from 2.1m up to 3.5m. Center Y = 2.8, half-height = 0.7
  `addBox(glm::vec3(0.0f, 2.8f, 0.0f), glm::vec3(1.0f, 0.7f, 0.1f));`
- Lines 111-113 left/right beside door: stay the same (floor to 2.1m)
- Update the comment on line 108: "Top section: 2.1m to 3.5m (full width)"
  </action>
  <verify>Build compiles: cd build && cmake --build . --target pixel-roguelike 2>&1 | tail -5</verify>
  <done>All three prison wall meshes produce 3.5m tall geometry. Bottom sections and door/window openings remain unchanged.</done>
</task>

<task type="auto">
  <name>Task 2: Update warden_office.scene -- ceiling, lights, colliders</name>
  <files>assets/scenes/warden_office.scene</files>
  <action>
**Ceiling tiles (lines 15-26):** Change all 12 ceiling tile Y positions from 2.5 to 3.5. These are the `prison_ceiling` mesh lines. Only the second value (Y position) changes.

**Spot light (line 59):** Change Y from 2.4 to 3.4
  `spot_light 0.0 3.4 4.5 0.0 -1.0 0.0 1.0 0.95 0.85 8.0 4.0 40.0 55.0 false node node_57`

**Ambient lights (lines 60-62):** Raise proportionally into the taller space:
- Line 60: Y from 1.8 to 2.5
  `light 0.0 2.5 7.8 0.8 0.85 0.95 6.0 1.2 node node_58`
- Line 61: Y from 2.0 to 2.8
  `light 0.0 2.8 4.0 1.0 0.95 0.9 10.0 1.0 node node_59`
- Line 62: Y from 2.0 to 2.8
  `light 0.0 2.8 1.0 0.9 0.9 0.95 8.0 0.6 node node_60`

**Wall colliders (lines 64-67):** Update Y center and half-height from (1.25, 1.25) to (1.75, 1.75):
- Line 64 (back wall): `collider_box 0.0 1.75 8.0 3.0 1.75 0.1 node node_62`
- Line 65 (front wall left): `collider_box -2.0 1.75 0.0 1.0 1.75 0.1 node node_63`
- Line 66 (front wall right): `collider_box 2.0 1.75 0.0 1.0 1.75 0.1 node node_64`
- Line 67 (front wall top above door): `collider_box 0.0 2.8 0.0 1.0 0.7 0.1 node node_65`
- Line 68 (left wall): `collider_box -3.0 1.75 4.0 0.1 1.75 4.0 node node_66`
- Line 69 (right wall): `collider_box 3.0 1.75 4.0 0.1 1.75 4.0 node node_67`

**Add ceiling collider:** Insert a new line after the right wall collider (after current line 69). This prevents the player from jumping through the ceiling:
  `collider_box 0.0 3.5 4.0 3.0 0.05 4.0 node node_ceil`
  (Centered over room at X=0 Z=4, Y=3.5 ceiling height, half-extents 3.0/0.05/4.0 to cover 6m x 8m room)
  </action>
  <verify>Run the game and load warden office scene. Ceiling should appear at 3.5m height with correct lighting. Player should not clip through ceiling.</verify>
  <done>Ceiling tiles at Y=3.5, lights raised proportionally, all wall colliders match 3.5m height, ceiling collider prevents clip-through.</done>
</task>

</tasks>

<verification>
Build and run: `cd build && cmake --build . --target pixel-roguelike && ./pixel-roguelike`
Load the warden office scene. Confirm:
1. Ceiling is visibly higher -- room feels spacious
2. Walls extend fully from floor to ceiling with no gaps
3. Door and window openings remain at their original heights
4. Lighting fills the taller room evenly
5. Player cannot jump through ceiling
</verification>

<success_criteria>
- Warden office ceiling at 3.5m (was 2.5m)
- Walls, colliders, lights all consistent with new height
- No visual gaps between walls and ceiling
- Ceiling collider present and functional
- No changes to floor, furniture, door opening height, or window opening height
</success_criteria>
