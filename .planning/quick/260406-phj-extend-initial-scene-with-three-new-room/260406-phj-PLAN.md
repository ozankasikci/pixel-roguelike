---
phase: quick
plan: 260406-phj
type: execute
wave: 1
depends_on: []
files_modified:
  - assets/scenes/initial_scene.scene
  - src/game/scenes/InitialSceneScripted.cpp
autonomous: false

must_haves:
  truths:
    - "All three doors in initial_scene open when player presses E"
    - "Each door leads to a distinct room the player can walk into"
    - "Rooms have floor, walls, ceiling, colliders, and lighting"
    - "Room themes feel distinct: storage, office, utility corridor"
  artifacts:
    - path: "assets/scenes/initial_scene.scene"
      provides: "Extended scene with three new rooms and no static door/frame meshes"
    - path: "src/game/scenes/InitialSceneScripted.cpp"
      provides: "Scripted geometry spawning three interactive single doors"
  key_links:
    - from: "src/game/scenes/InitialSceneScripted.cpp"
      to: "src/game/prefabs/GameplayPrefabs.cpp"
      via: "spawnSingleDoor()"
      pattern: "spawnSingleDoor"
    - from: "src/game/scenes/GenericFileScene.cpp"
      to: "src/game/scenes/InitialSceneScripted.cpp"
      via: "scriptedGeometryRegistry lookup by levelId"
      pattern: "registerScriptedGeometry"
---

<objective>
Extend the initial_scene with three new rooms (one behind each door) and make all three doors interactive/openable using the existing spawnSingleDoor prefab system.

Purpose: Transform the starting room from a sealed box with decorative doors into a proper multi-room level with explorable spaces behind each door, giving the player actual spaces to discover.

Output: Updated initial_scene.scene with room geometry for three new rooms, plus a new InitialSceneScripted.cpp that registers scripted geometry to spawn interactive doors via spawnSingleDoor().
</objective>

<execution_context>
@/Users/ozan/.claude/get-shit-done/workflows/execute-plan.md
@/Users/ozan/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@assets/scenes/initial_scene.scene
@src/game/scenes/GenericFileScene.cpp
@src/game/prefabs/GameplayPrefabs.cpp
@src/game/prefabs/GameplayPrefabData.h
@src/game/level/LevelDef.h
@src/game/level/LevelBuilder.cpp
@src/game/level/LevelLoader.cpp
@src/game/behavior/DoorAnimationSystem.cpp

<interfaces>
<!-- Key contracts the executor needs -->

From src/game/prefabs/GameplayPrefabData.h:
```cpp
struct SingleDoorSpawnSpec {
    std::string doorMeshName = "SM_DoorA";
    std::string frameMeshName = "SM_FrameA";
    std::string doorMaterialId = "qdp_door_a";
    std::string frameMaterialId = "stone_default";
    glm::vec3 rootPosition{0.0f};      // world position of door frame center (floor level)
    float doorYawDegrees = 0.0f;       // rotation of entire assembly around Y
    float openAngle = 90.0f;           // how far the door swings open (degrees)
    float openDuration = 1.2f;         // seconds for open animation
    float interactDistance = 2.5f;
    float interactDotThreshold = 0.55f;
    bool locked = false;
    std::string lockedPrompt = "E  This door is locked";
    glm::vec3 doorTint{1.0f};
    glm::vec3 frameTint{1.0f};
};
```

From src/game/prefabs/GameplayPrefabs.h:
```cpp
entt::entity spawnSingleDoor(LevelBuilder& builder, const SingleDoorSpawnSpec& spec);
```

From src/game/scenes/GenericFileScene.h:
```cpp
static void registerScriptedGeometry(const std::string& levelId,
                                      std::function<void(LevelBuilder&)> callback);
```

Scene file format (from LevelDef.cpp parser):
```
mesh <meshId> <px> <py> <pz> <sx> <sy> <sz> <rx> <ry> <rz> material <materialId> tint <r> <g> <b> node <nodeId>
light point <px> <py> <pz> <r> <g> <b> <radius> <intensity> node <nodeId>
collider box solid <px> <py> <pz> <hx> <hy> <hz> node <nodeId>
```

Available procedural meshes: prison_floor, prison_ceiling, prison_wall, prison_wall_panel, prison_wall_door, prison_wall_window, prison_wall_large_window, prison_baseboard, prison_desk, prison_chair, warden_chair, prison_cabinet, prison_shelf, ceiling_light_panel, office_door, inst_door_knob, inst_hvac_vent, inst_smoke_detector

QDP door meshes (all at 0.21 uniform scale): SM_DoorA (wooden), SM_FrameA, SM_DoorC (white), SM_FrameC, SM_DoorD (heavy metal), SM_FrameD

Materials: qdp_wall, qdp_floor, qdp_door_a/c/d, qdp_wall_door, concrete_wall, inst_beige_wall, inst_glossy_floor, floor_default, ceiling_default, wood_default, metal_default, stone_default, brick_default
</interfaces>

Current initial_scene layout:
```
                   NORTH (Z=6) - Door B (SM_DoorD, metal, 180deg)
         +---+---+---D---+---+
         |                     |
         |                     |
  Door A |                     | Door C (locked w/ chain)
 (SM_DoorA, 90deg)        (SM_DoorC, -90deg)
  X=-5   D                     D   X=5
         |                     |
         |   * player spawn    |
         |   (0, 1, -0.5)     |
         +---+---+---+---+---+
                   SOUTH (Z=-2)
```

Existing door positions (extracted from scene mesh lines):
- Door A: rootPos=(-4.84, 0, 0.66), yaw=90, group node_123 scale (1.0, 1.456, 1.425)
- Door B: rootPos=(0.08, 0, 5.85), yaw=180 (frame/door at Z=5.85)
- Door C: rootPos=(4.85, 0, 2.93), yaw=-90 (frame/door at X=4.85)

Wall openings (prison_wall_door segments):
- West wall door: at (-5.0, 0, 1.0) facing 90deg (node n_ww2)
- North wall door: at (0.0, 0, 6.0) facing 180deg (node n_wn3) -- note: wall_door at Z=6, but door mesh at Z=5.85
- East wall door: at (5.0, 0, 3.0) facing -90deg (node n_we3)
</context>

<tasks>

<task type="auto">
  <name>Task 1: Update scene file and create scripted geometry for interactive doors and three new rooms</name>
  <files>assets/scenes/initial_scene.scene, src/game/scenes/InitialSceneScripted.cpp, CMakeLists.txt</files>
  <action>
**Part A: Remove static door meshes from scene file**

Remove lines 62-68 from `assets/scenes/initial_scene.scene` (the six SM_FrameA, SM_DoorA, SM_FrameD, SM_DoorD, SM_FrameC, SM_DoorC mesh entries and the inst_chain_padlock entry). Also remove line 89 (the group node_123 that Door A's group referenced). Keep ALL wall panels, wall_door openings, floors, ceilings, lights, colliders, and player_spawn intact.

Remove the existing wall colliders on the three sides where doors are (west n_col_west, north n_col_north, east n_col_east) since the rooms will extend through these walls. The south wall collider (n_col_south) stays.

**Part B: Add three new rooms to the scene file**

Append the following room geometry to the scene file. Each room connects to the main room through the existing wall openings.

**Room 1 -- Storage Room (through Door A, west wall, extends in -X direction)**
A narrow 6m x 4m room (X[-11,-5], Z[-1,3]) with 4m ceiling, concrete/stone aesthetic. Contains shelves, a cabinet.

Geometry to add:
- Floor: 3x2 grid of `prison_floor` at X=-10,-8,-6, Z=0,2 (material: floor_default, tint: 0.55 0.45 0.32)
- Ceiling: 3x2 grid of `prison_ceiling` at X=-10,-8,-6, Z=0,2 at Y=4.0 (material: ceiling_default, tint: 0.9 0.88 0.84)
- West wall (X=-11): 2 `prison_wall_panel` at Z=0,2 facing -90 deg rotation (0, -90, 0) -- the inside face points east into the room
- North wall (Z=3): 3 `prison_wall_panel` at X=-10,-8,-6 facing 180 deg (0, 180, 0)
- South wall (Z=-1): 3 `prison_wall_panel` at X=-10,-8,-6 facing 0 deg (0, 0, 0)
- East wall: This IS the main room's west wall, which already has panels. No new wall panels needed here.
- 2 `prison_shelf` on west wall at (-10.85, 1.6, 0.5) and (-10.85, 1.6, 2.0) facing -90deg
- 1 `prison_cabinet` at (-10.2, 0, 2.5) facing 180deg
- 2 `ceiling_light_panel` at (-8.0, 3.99, 1.0) and (-8.0, 3.99, 2.0)
- 2 point lights at (-8.0, 3.7, 1.0) and (-8.0, 3.7, 2.0), warm color (1.0, 0.95, 0.85), radius 5, intensity 3.5

Colliders:
- Floor: box solid (-8.0, -0.05, 1.0) halfExtents (3.0, 0.05, 2.0)
- Ceiling: box solid (-8.0, 4.0, 1.0) halfExtents (3.0, 0.05, 2.0)
- West wall: box solid (-11.0, 2.0, 1.0) halfExtents (0.1, 2.0, 2.0)
- North wall: box solid (-8.0, 2.0, 3.0) halfExtents (3.0, 2.0, 0.1)
- South wall: box solid (-8.0, 2.0, -1.0) halfExtents (3.0, 2.0, 0.1)

**Room 2 -- Control Room / Office (through Door B, north wall, extends in +Z direction)**
A 6m x 6m room (X[-3,3], Z[6,12]) with 4m ceiling, institutional aesthetic with glossy floor. Contains a desk, chair, and a large window on the far wall.

Geometry to add:
- Floor: 3x3 grid of `prison_floor` at X=-2,0,2, Z=7,9,11 (material: inst_glossy_floor, tint: 1.0 1.0 1.0)
- Ceiling: 3x3 grid of `prison_ceiling` at X=-2,0,2, Z=7,9,11 at Y=4.0 (material: ceiling_default, tint: 0.94 0.92 0.88)
- North wall (Z=12): 1 `prison_wall_panel` at X=-2 facing 180deg, 1 `prison_wall_large_window` at X=0 facing 180deg, 1 `prison_wall_panel` at X=2 facing 180deg
- East wall (X=3): 3 `prison_wall_panel` at Z=7,9,11 facing -90deg (0, -90, 0)
- West wall (X=-3): 3 `prison_wall_panel` at Z=7,9,11 facing 90deg (0, 90, 0)
- South wall: The main room's north wall (already has panels with door opening)
- 1 `prison_desk` at (0.0, 0, 10.0) facing 180deg
- 1 `warden_chair` at (0.0, 0, 9.2) facing 0deg
- 1 `prison_cabinet` at (2.2, 0, 11.5) facing 180deg
- 2 `prison_baseboard` along back wall at (-2.0, 0, 11.9) and (2.0, 0, 11.9) facing 180deg
- 3 `ceiling_light_panel` at (-1.0, 3.99, 8.0), (1.0, 3.99, 8.0), (0.0, 3.99, 10.5)
- 3 point lights at matching positions, warm color (1.0, 0.97, 0.9), radius 6, intensity 4
- 1 window light behind large window: point at (0.0, 2.0, 11.8), color (1.0, 0.98, 0.94), radius 8, intensity 5

Colliders:
- Floor: box solid (0.0, -0.05, 9.0) halfExtents (3.0, 0.05, 3.0)
- Ceiling: box solid (0.0, 4.0, 9.0) halfExtents (3.0, 0.05, 3.0)
- North wall: box solid (0.0, 2.0, 12.0) halfExtents (3.0, 2.0, 0.1)
- East wall: box solid (3.0, 2.0, 9.0) halfExtents (0.1, 2.0, 3.0)
- West wall: box solid (-3.0, 2.0, 9.0) halfExtents (0.1, 2.0, 3.0)
- Desk: box solid (0.0, 0.375, 10.0) halfExtents (0.6, 0.375, 0.3)
- Cabinet: box solid (2.2, 0.65, 11.5) halfExtents (0.225, 0.65, 0.3)

**Room 3 -- Utility Corridor (through Door C, east wall, extends in +X direction)**
A long narrow 8m x 3m corridor (X[5,13], Z[1.5,4.5]) with 4m ceiling, concrete/metal utilitarian look. Has HVAC vents and exposed pipes feel.

Geometry to add:
- Floor: 4x2 grid of `prison_floor` at X=6,8,10,12, Z=2,4 (material: floor_default, tint: 0.48 0.45 0.4)
- Ceiling: 4x2 grid of `prison_ceiling` at X=6,8,10,12, Z=2,4 at Y=4.0 (material: ceiling_default, tint: 0.88 0.86 0.82)
- North wall (Z=5): 4 `prison_wall_panel` at X=6,8,10,12 facing 180deg
- South wall (Z=1): 4 `prison_wall_panel` at X=6,8,10,12 facing 0deg
- East wall (X=13): 1 `prison_wall_panel` at Z=2 facing -90deg, 1 `prison_wall_panel` at Z=4 facing -90deg
- West wall: Main room's east wall (already has panels with door opening)
- 2 `inst_hvac_vent` at (7.0, 3.5, 4.9) and (11.0, 3.5, 4.9) facing 180deg
- 1 `inst_smoke_detector` at (9.0, 3.97, 3.0)
- 4 `ceiling_light_panel` at X=6.5,8.5,10.5,12.0 Y=3.99 Z=3.0
- 4 point lights at matching positions, slightly cool color (0.95, 0.95, 1.0), radius 4, intensity 3

Colliders:
- Floor: box solid (9.0, -0.05, 3.0) halfExtents (4.0, 0.05, 1.5)
- Ceiling: box solid (9.0, 4.0, 3.0) halfExtents (4.0, 0.05, 1.5)
- North wall: box solid (9.0, 2.0, 5.0) halfExtents (4.0, 2.0, 0.1)
- South wall: box solid (9.0, 2.0, 1.0) halfExtents (4.0, 2.0, 0.1)
- East wall: box solid (13.0, 2.0, 3.0) halfExtents (0.1, 2.0, 2.0)

**Also add partial wall colliders for the main room** (replacing the removed full-wall colliders) so the player can't walk through solid wall segments but CAN walk through door openings into the new rooms:
- West wall: Two collider segments flanking the door opening at Z=1.0
  - South of door: box solid (-5.0, 3.0, -0.5) halfExtents (0.1, 3.0, 1.5)
  - North of door: box solid (-5.0, 3.0, 4.5) halfExtents (0.1, 3.0, 1.5)
- North wall: Two collider segments flanking the door opening at X=0
  - West of door: box solid (-3.0, 3.0, 6.0) halfExtents (2.0, 3.0, 0.1)
  - East of door: box solid (3.0, 3.0, 6.0) halfExtents (2.0, 3.0, 0.1)
- East wall: Two collider segments flanking the door opening at Z=3.0
  - South of door: box solid (5.0, 3.0, 0.0) halfExtents (0.1, 3.0, 2.0)
  - North of door: box solid (5.0, 3.0, 5.0) halfExtents (0.1, 3.0, 1.0)

**Part C: Create InitialSceneScripted.cpp**

Create `src/game/scenes/InitialSceneScripted.cpp` that registers scripted geometry for levelId "initial_scene". This file uses a static initializer pattern to call `GenericFileScene::registerScriptedGeometry("initial_scene", callback)` where the callback spawns three single doors using `spawnSingleDoor()`.

```cpp
#include "game/scenes/GenericFileScene.h"
#include "game/level/LevelBuilder.h"
#include "game/prefabs/GameplayPrefabs.h"

namespace {

static const bool kRegistered = [] {
    GenericFileScene::registerScriptedGeometry("initial_scene", [](LevelBuilder& builder) {
        // Door A -- wooden door, west wall opening
        // Wall door opening is at (-5.0, 0, 1.0) facing 90deg
        // Root position slightly inset from wall plane
        {
            SingleDoorSpawnSpec spec;
            spec.doorMeshName = "SM_DoorA";
            spec.frameMeshName = "SM_FrameA";
            spec.doorMaterialId = "qdp_door_a";
            spec.frameMaterialId = "stone_default";
            spec.rootPosition = glm::vec3(-4.85f, 0.0f, 1.0f);
            spec.doorYawDegrees = 90.0f;
            spec.openAngle = 90.0f;
            spec.openDuration = 1.2f;
            spec.interactDistance = 2.5f;
            spec.interactDotThreshold = 0.55f;
            spec.locked = false;
            spec.doorTint = glm::vec3(1.0f);
            spec.frameTint = glm::vec3(1.0f);
            spawnSingleDoor(builder, spec);
        }

        // Door B -- heavy metal door, north wall opening
        // Wall door opening is at (0.0, 0, 6.0) facing 180deg
        {
            SingleDoorSpawnSpec spec;
            spec.doorMeshName = "SM_DoorD";
            spec.frameMeshName = "SM_FrameD";
            spec.doorMaterialId = "qdp_door_d";
            spec.frameMaterialId = "stone_default";
            spec.rootPosition = glm::vec3(0.0f, 0.0f, 5.85f);
            spec.doorYawDegrees = 180.0f;
            spec.openAngle = 90.0f;
            spec.openDuration = 1.8f;  // heavier door, slower open
            spec.interactDistance = 2.5f;
            spec.interactDotThreshold = 0.55f;
            spec.locked = false;
            spec.doorTint = glm::vec3(0.6f, 0.58f, 0.55f);
            spec.frameTint = glm::vec3(0.65f, 0.63f, 0.6f);
            spawnSingleDoor(builder, spec);
        }

        // Door C -- white door, east wall opening (was locked, now unlocked)
        // Wall door opening is at (5.0, 0, 3.0) facing -90deg
        {
            SingleDoorSpawnSpec spec;
            spec.doorMeshName = "SM_DoorC";
            spec.frameMeshName = "SM_FrameC";
            spec.doorMaterialId = "qdp_door_c";
            spec.frameMaterialId = "stone_default";
            spec.rootPosition = glm::vec3(4.85f, 0.0f, 3.0f);
            spec.doorYawDegrees = -90.0f;
            spec.openAngle = 90.0f;
            spec.openDuration = 1.0f;
            spec.interactDistance = 2.5f;
            spec.interactDotThreshold = 0.55f;
            spec.locked = false;
            spec.doorTint = glm::vec3(0.88f, 0.85f, 0.8f);
            spec.frameTint = glm::vec3(0.85f, 0.82f, 0.78f);
            spawnSingleDoor(builder, spec);
        }
    });
    return true;
}();

} // namespace
```

**Part D: Add InitialSceneScripted.cpp to CMakeLists.txt**

Find the CMakeLists.txt that lists `GenericFileScene.cpp` (likely in `src/game/scenes/` or the game-layer CMakeLists) and add `InitialSceneScripted.cpp` next to it. The static initializer will auto-register the callback at program startup.

IMPORTANT NOTES:
- Door root positions should be slightly inset from the wall plane (0.15m) so the frame mesh sits properly in the wall opening
- Use 0.21 uniform scale for QDP door/frame meshes (this is the standard QDP pack scale)
- The `spawnSingleDoor` function handles frame placement, door leaf creation, collider, DoorLeafComponent, DoorComponent, and InteractableComponent automatically
- Room wall panel placement: `prison_wall_panel` is a 2m wide x 4m tall module. Position is the center-bottom of the panel. Rotation Y determines which way the panel faces
- `prison_floor` and `prison_ceiling` are 2m x 2m tiles
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target pixel-roguelike 2>&1 | tail -20</automated>
  </verify>
  <done>
    - InitialSceneScripted.cpp compiles and links with the pixel-roguelike target
    - initial_scene.scene has no SM_DoorA/C/D or SM_FrameA/C/D static mesh entries
    - initial_scene.scene has three new rooms: storage (west), office (north), corridor (east)
    - Each room has floor, ceiling, walls, at least one light, and colliders
  </done>
</task>

<task type="checkpoint:human-verify" gate="blocking">
  <name>Task 2: Verify doors and rooms in-game</name>
  <action>
    Human verifies the three interactive doors and new rooms work correctly in-game.
    What was built: Three interactive doors in the initial_scene that open when the player presses E, each leading to a new room -- a storage room (west/Door A), a control office (north/Door B), and a utility corridor (east/Door C).
    How to verify:
    1. Launch the game: cd /Users/ozan/Projects/gsd-3d-roguelike and run ./build/apps/pixel-roguelike
    2. Load the initial_scene (should be the default or select it from the scene menu)
    3. Walk to Door A (west wall, wooden door) -- press E, verify it swings open and you can walk into the storage room with shelves and a cabinet
    4. Walk to Door B (north wall, heavy metal door) -- press E, verify it opens (slower, heavier feel), walk into the control office with a desk, chair, cabinet, and large window on the far wall
    5. Walk to Door C (east wall, white door) -- press E, verify it opens and leads to a long utility corridor with HVAC vents and ceiling lights
    6. Verify all rooms have proper lighting (no pitch-black areas), solid floors (no falling through), and walls that block movement
    7. Verify no visual gaps between the main room walls and the new room walls
  </action>
  <verify>Human visual and gameplay verification</verify>
  <done>All three doors open correctly and lead to distinct, properly lit, collision-bounded rooms. Type "approved" or describe issues.</done>
</task>

</tasks>

<verification>
- `cmake --build build --target pixel-roguelike` succeeds with no errors
- Scene file parses without error (no crash on load)
- Three doors respond to E key press with open animation
- Player can physically walk through opened doors into new rooms
- All rooms have floor colliders (player does not fall)
- All rooms have wall colliders (player cannot walk through walls)
- Each room has at least 2 lights for visibility
</verification>

<success_criteria>
- All three doors in initial_scene are interactive (press E to open)
- Three distinct rooms are explorable: storage (west), office (north), corridor (east)
- No visual gaps or collision holes between rooms
- Game compiles and runs without crashes
</success_criteria>

<output>
After completion, create `.planning/quick/260406-phj-extend-initial-scene-with-three-new-room/260406-phj-SUMMARY.md`
</output>
