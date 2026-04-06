# Quick Task 260406-qr6: Remove Scripted Geometry — Context

**Gathered:** 2026-04-06
**Status:** Ready for planning

<domain>
## Task Boundary

Remove all scripted geometry and define all doors, walls, and room entities directly in the initial_scene.scene file. Verify placement with the editor debug harness.

Currently InitialSceneScripted.cpp spawns three doors via code. The .scene format doesn't natively support DoorComponent — doors are only possible via archetypes or code. The user wants to extend the scene format to support doors natively.

</domain>

<decisions>
## Implementation Decisions

### Door Definition Method
- **Extend the .scene format** to support native DoorComponent serialization
- Add door component support to LevelDef, EditorSceneSerializer, and LevelBuilder
- Doors should be first-class entities in the scene file, not archetypes or code-spawned

### Scripted Geometry Cleanup
- **Remove everything**: Delete InitialSceneScripted.cpp/.h, remove lookupScriptedGeometry() from GenericFileScene, remove registerInitialSceneScripted() calls from both runtime and editor main.cpp files
- Clean removal — no dead code left behind

### Verification
- Use the editor debug harness to inspect entity positions after changes
- Ensure all three doors appear correctly, walls are properly positioned, rooms are accessible

</decisions>

<specifics>
## Specific Ideas

Files to extend for door serialization:
- `src/game/level/LevelDef.h` — add door fields to LevelMeshPlacement or create LevelDoorPlacement
- `src/game/level/LevelDef.cpp` — parse/serialize door entries
- `src/game/level/LevelBuilder.cpp` — build door entities from LevelDef
- `src/editor/scene/EditorSceneSerializer.cpp` — read/write door data in .scene files
- `assets/scenes/initial_scene.scene` — define three doors with positions, rotations, mesh refs, materials

Files to delete/clean:
- `src/game/scenes/InitialSceneScripted.cpp` — delete
- `src/game/scenes/InitialSceneScripted.h` — delete
- `src/game/scenes/GenericFileScene.h/.cpp` — remove lookupScriptedGeometry() and registry
- `apps/runtime/main.cpp` — remove registerInitialSceneScripted() call
- `apps/level_editor/main.cpp` — remove registerInitialSceneScripted() call

</specifics>
