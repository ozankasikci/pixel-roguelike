# Phase 16: Editor Trigger and Behavior Authoring with Save-Load Fidelity - Context

**Gathered:** 2026-04-04
**Status:** Ready for planning

<domain>
## Phase Boundary

Make triggers first-class editor objects and add UI for authoring behaviors and interactables on scene entities, with full round-trip save-load fidelity. Currently triggers are read-only in the editor (`readOnlyTriggers_`), behaviors/interactables can only be hand-authored in `.scene` files, and `toLevelDef()` drops trigger data on save.

</domain>

<decisions>
## Implementation Decisions

### Trigger authoring
- **D-01:** Triggers are created via right-click context menu on entities ("Add Trigger Zone"), positioned relative to the parent entity
- **D-02:** Trigger volumes render as semi-transparent wireframes with drag handles on corners/edges to resize halfExtents (box) or radius (sphere) — same gizmo style as colliders
- **D-03:** Both Box and Sphere trigger shapes supported from the start, with a shape selector dropdown in the inspector
- **D-04:** Triggers appear as standalone top-level entries in the outliner, same as meshes and lights — can exist independently or be parented

### Behavior editing UI
- **D-05:** Behaviors shown as collapsible event sections in the inspector (onActivate, onEnter, onExit, onTimer), each listing its action entries with a "+ Add Action" button per section
- **D-06:** Action type selection uses a categorized dropdown: Door (Open/Close/Toggle), Audio (PlaySound), Lighting (SetLight, FlickerLight), Entity (Enable/Disable), Player (Lock/Unlock/Teleport), Message (ShowMessage), Timing (Delay), Events (EmitEvent)
- **D-07:** Target node ID uses a combo dropdown listing all entities with a nodeId in the scene, type-to-filter, "self" always first option
- **D-08:** Core subset of action types for this phase: door operations (Open/Close/Toggle), SetLight, PlaySound, Delay, EnableEntity, DisableEntity. Remaining types (FlickerLight, ShowMessage, EmitEvent, LockPlayer, UnlockPlayer, TeleportPlayer) deferred to follow-up

### Interactable editing
- **D-09:** "Make Interactable" checkbox on mesh entities in the inspector. When checked, shows prompt text, interaction distance, and dot threshold fields inline
- **D-10:** Interaction distance visualized as a wireframe ring/sphere around the mesh in the viewport. No visualization for dot threshold

### Save-load fidelity
- **D-11:** Triggers become full `EditorSceneObject` entries (new `EditorSceneObjectKind::Trigger` variant) and survive save/load round-trips via `toLevelDef()` and `loadFromSceneFile()`
- **D-12:** Behavior lists and interactable data on mesh/light/trigger payloads preserved through all editor edit operations
- **D-13:** New dedicated test file for behavior/trigger round-trip fidelity — loads a .scene with triggers, behaviors, and interactables, saves it, reloads, verifies everything matches

### Claude's Discretion
- Exact wireframe colors and transparency for trigger volumes
- ImGui layout details for behavior/interactable inspector sections
- Which action parameter fields need special widgets vs simple float/string inputs
- Internal ordering of the categorized action type dropdown

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Behavior system (Phase 13)
- `src/game/behavior/ActionTypes.h` — All 14 action types, typed ActionParams variant, ActionEntry struct
- `src/game/behavior/BehaviorComponent.h` — BehaviorComponent with onActivate/onEnter/onExit/onTimer action lists
- `src/game/behavior/TriggerComponent.h` — TriggerComponent with Box/Sphere shapes, halfExtents, radius, runtime state flags
- `src/game/behavior/BehaviorSystem.h` — BehaviorSystem dispatcher that processes action lists
- `src/game/behavior/TriggerSystem.h` — TriggerSystem overlap detection

### Level data model
- `src/game/level/LevelDef.h` — TriggerPlacement, BehaviorDeclaration, InteractableDeclaration structs; LevelDef aggregate
- `src/game/level/LevelDef.cpp` — Scene file parser including trigger and behavior sub-line parsing
- `src/game/level/LevelBuilder.h` — `addTrigger()` method that creates trigger entities from placements

### Editor scene document
- `src/editor/scene/EditorSceneDocument.h` — EditorSceneObject, EditorSceneObjectKind, EditorSceneObjectPayload variant; `readOnlyTriggers_` (line 73, 139)
- `src/editor/scene/EditorSceneDocument.cpp` — `loadFromSceneFile()` (loads triggers as read-only), `toLevelDef()` (currently drops triggers), `save()`
- `src/editor/scene/EditorSceneSerializer.h` — EditorSceneSerializer namespace wrapping LevelDef load/save

### Editor UI patterns
- `src/editor/ui/EditorInspectorPanel.cpp` — Existing inspector panel for assets (materials, environments, prefabs)
- `src/editor/ui/EditorOutlinerPanel.cpp` — Outliner panel listing scene objects
- `src/editor/render/EditorScenePreviewRenderer.cpp` — Preview renderer (already includes TriggerComponent)

### Existing round-trip test
- `tests/game/test_level_roundtrip.cpp` — Existing level round-trip test (pattern reference for the new dedicated test)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `EditorSceneObject` + `EditorSceneObjectKind` enum + `EditorSceneObjectPayload` variant: extend with a `Trigger` kind and `TriggerPlacement` variant member
- `EditorOutlinerPanel`: already renders all `EditorSceneObjectKind` types — adding Trigger follows the same switch pattern
- Existing collider wireframe rendering: similar gizmo handle approach can be reused for trigger volume visualization
- `test_level_roundtrip.cpp`: pattern for the new dedicated round-trip test

### Established Patterns
- Every new editor object kind follows: add to `EditorSceneObjectKind` enum, add payload to variant, add `addXxx()` method on `EditorSceneDocument`, add case in `toLevelDef()`, add outliner/inspector rendering
- Inspector panels use ImGui collapsible headers (`ImGui::CollapsingHeader`) for grouping
- Context menus use `ImGui::BeginPopupContextItem` in the outliner

### Integration Points
- `EditorSceneDocument::loadFromSceneFile()` — must promote `readOnlyTriggers_` to full `EditorSceneObject` entries
- `EditorSceneDocument::toLevelDef()` — must serialize Trigger objects back to `LevelDef::triggers`
- `EditorOutlinerPanel` — must render Trigger entries with appropriate icon
- Inspector panel — must add behavior/interactable editing sections for mesh, light, and trigger entities
- Viewport renderer — must draw trigger wireframes and interaction distance rings

</code_context>

<specifics>
## Specific Ideas

- Trigger gizmo handles should follow the same style as existing collider gizmos for consistency
- The categorized action dropdown should group related actions logically (Door ops together, lighting together, etc.)
- "self" should always be the default and first option in the target node dropdown
- The "Make Interactable" checkbox should be a simple toggle that reveals/hides the fields — not a separate panel

</specifics>

<deferred>
## Deferred Ideas

- Full support for remaining 8 action types (FlickerLight, ShowMessage, EmitEvent, LockPlayer, UnlockPlayer, TeleportPlayer) — follow-up phase
- Dot threshold viewport visualization (cone/arc showing facing requirement)
- Behavior copy/paste between entities
- Behavior templates/presets for common patterns (door trigger, light switch, etc.)

</deferred>

---

*Phase: 16-editor-trigger-and-behavior-authoring-with-save-load-fidelity*
*Context gathered: 2026-04-04*
