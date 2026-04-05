# Phase 18: Maintainability Refactoring for Editor UI, Serialization, and Round-Trip Fidelity - Context

**Gathered:** 2026-04-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Structural refactoring to reduce code duplication, improve type safety, and increase maintainability across the level serializer (LevelDef.cpp), editor scene document (EditorSceneDocument.cpp), and inspector panels (EditorInspectorPanel.cpp). No new features — this phase makes the existing code cheaper to extend in future phases.

</domain>

<decisions>
## Implementation Decisions

### Parser consolidation
- **D-01:** Extract `parseNodeMetadata()` and `parseShapeTokens()` helper functions from LevelDef.cpp. Each object parser calls helpers instead of inline loops. Eliminates 8+ duplicate metadata parsing loops (~200 lines saved).
- **D-02:** Extract per-action parser functions (`parseDoorActionParams()`, `parseSoundActionParams()`, etc.) for all 14 action types. Variant type guaranteed by prior switch on ActionType. Type-safe and independently testable.
- **D-03:** Create a `PlacementBase` struct with common fields (position, rotation, nodeId, parentNodeId). All placement types embed it. Helpers and the parser work against PlacementBase for shared fields.

### Legacy format cleanup
- **D-04:** Delete all legacy format parsing code entirely (~460 lines): `collider_box`, `collider_cylinder`, `trigger_box`, `trigger_sphere`, `trigger_cylinder`, `trigger_capsule`. Scene files were already migrated in Phase 17. Clean break.
- **D-05:** Unify light keywords: `light`, `spot_light`, `dir_light` → single `light` keyword with `type=point/spot/directional` token. Consistent with how `collider` now uses shape/mode tokens.
- **D-06:** Light format migration happens in this phase alongside the parser refactor. Parser only handles new format. All .scene files migrated in the same commit.

### Inspector decomposition
- **D-07:** Split EditorInspectorPanel.cpp into per-type inspector classes: `MeshInspector`, `LightInspector`, `ColliderInspector`, `ReflectionProbeInspector`, `PlayerSpawnInspector`, `ArchetypeInspector`, `GroupInspector`. Each is a separate .cpp file with a `drawInspector()` method. Main panel dispatches to the right one.
- **D-08:** Also extract asset inspectors: `MaterialInspector`, `EnvironmentInspector`, `PrefabInspector`. Full decomposition of the monolithic panel.
- **D-09:** Create a shared `drawTransformSection(PlacementBase&)` function that all inspectors call. Single source of truth for position/rotation/scale editing UI.

### Switch proliferation
- **D-10:** Implement visitor pattern for EditorSceneDocument operations. Create a `SceneObjectVisitor` interface with `visitMesh()`, `visitLight()`, etc. Replace 7 switch statements (duplicateObject, toLevelDef, localTransformMatrix, objectKindName, objectLabel, objectAnchor) with visitor implementations.
- **D-11:** Adding a new object kind becomes adding one new visitor case, not updating 7 switch statements across 6 functions.

### Claude's Discretion
- Whether to keep `std::variant<>` for EditorSceneObjectPayload or replace with polymorphic base class — evaluate which approach works better with the visitor pattern and PlacementBase refactor
- Exact file organization for per-type inspectors (subdirectory vs flat in `src/editor/ui/`)
- Whether PlacementBase should use composition (embedded struct) or inheritance
- Which of the 7 switch statements benefit most from visitor pattern vs. simple `std::visit` lambdas

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Level serialization
- `src/game/level/LevelDef.h` — All placement structs (LevelMeshPlacement, LevelLightPlacement, LevelColliderPlacement, etc.), LevelDef aggregate
- `src/game/level/LevelDef.cpp` — Scene file parser and serializer (1,490 lines, primary refactoring target)
- `src/game/level/LevelBuilder.h` — Entity spawning from placement data
- `src/game/level/LevelBuilder.cpp` — Placement struct → game component mapping

### Editor scene document
- `src/editor/scene/EditorSceneDocument.h` — EditorSceneObject, EditorSceneObjectKind enum, EditorSceneObjectPayload variant (7 types)
- `src/editor/scene/EditorSceneDocument.cpp` — 7 switch statements, 924 lines
- `src/editor/scene/EditorSceneSerializer.h` — Thin wrapper over LevelDef load/save

### Editor UI panels
- `src/editor/ui/EditorInspectorPanel.cpp` — Monolithic inspector, 1,516 lines (primary decomposition target)
- `src/editor/ui/EditorOutlinerPanel.cpp` — Outliner with object kind switches (561 lines)
- `src/editor/ui/EditorAssetBrowserPanel.cpp` — Asset browser (785 lines)
- `src/editor/ui/EditorEnvironmentPanel.cpp` — Environment panel (555 lines)

### Behavior/action system
- `src/game/behavior/ActionTypes.h` — All 14 action types, typed ActionParams variant
- `src/game/behavior/BehaviorComponent.h` — BehaviorComponent with action lists

### Existing tests
- `tests/game/test_level_roundtrip.cpp` — Level round-trip test (199 lines)
- `tests/game/test_behavior_trigger_roundtrip.cpp` — Behavior/trigger round-trip test (181 lines)

### Scene files to migrate (light format)
- `assets/scenes/initial_scene.scene` — Primary scene with lights
- `assets/scenes/country_house.scene` — Scene with lights

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `EditorSceneObjectKind` enum and `EditorSceneObjectPayload` variant — will be refactored but pattern is established
- Existing `editVec3()` helper in inspector — foundation for shared `drawTransformSection()`
- `std::visit` pattern already used in `nodeIdPtr()`/`parentNodeIdPtr()` — can extend for visitor pattern
- Phase 17's collider unification pattern — same approach applies to light keyword unification

### Established Patterns
- Components are POD structs (no methods, no inheritance) — PlacementBase must respect this
- Scene file line-based format with indented sub-lines for behaviors
- Parser uses token-based approach with `std::istringstream` and keyword dispatch
- ImGui inspector sections use `ImGui::CollapsingHeader` for grouping
- Tests use standalone executables with exit code = pass/fail

### Integration Points
- `LevelDef.h` PlacementBase → affects all placement struct definitions
- `LevelDef.cpp` parser → must handle new unified light format
- `EditorSceneDocument.cpp` → visitor pattern replaces all 7 switch statements
- `EditorInspectorPanel.cpp` → split into 10+ files, main panel becomes dispatcher
- `LevelBuilder.cpp` → may need updates to work with PlacementBase
- All `.scene` files → light keyword migration

</code_context>

<specifics>
## Specific Ideas

- Follow Phase 17's migration pattern for light keywords: parser handles new format only, all scene files migrated in one commit
- PlacementBase should make `parseNodeMetadata()` a single function that takes a `PlacementBase&`
- Per-type inspector classes should each have a consistent interface (`drawInspector()`) for easy dispatch
- The visitor pattern should make it trivial to add new object kinds in future phases

</specifics>

<deferred>
## Deferred Ideas

- Declarative format schema (describing .scene format as data rather than parser code) — more ambitious than this refactor
- Extending test coverage for environment serialization and group hierarchy — can be added alongside refactor or in follow-up
- EditorOutlinerPanel switch statement cleanup — less critical, smaller file
- Node ID type safety improvements (string keys → typed handles) — separate concern

</deferred>

---

*Phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity*
*Context gathered: 2026-04-05*
