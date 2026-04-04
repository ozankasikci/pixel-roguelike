# Phase 16: Editor Trigger and Behavior Authoring with Save-Load Fidelity - Research

**Researched:** 2026-04-04
**Domain:** C++ level editor — ImGui inspector UI, ECS-backed editor data model, scene file serialization
**Confidence:** HIGH

## Summary

Phase 16 promotes triggers from a read-only side-channel (`readOnlyTriggers_`) into first-class `EditorSceneObject` entries with full editor support: creation via context menu, shape/position/size editing in the inspector, gizmo resize handles in the viewport, and round-trip save-load fidelity through `toLevelDef()` and `loadFromSceneFile()`.

The behavior and interactable authoring work is additive to the existing data model. `LevelMeshPlacement`, `LevelLightPlacement`, and `TriggerPlacement` already carry `std::vector<BehaviorDeclaration> behaviors` and (for meshes) `std::optional<InteractableDeclaration> interactable`. The scene file parser already serializes and deserializes all of these fields. The gap is entirely on the editor side: `toLevelDef()` drops trigger objects (no `Trigger` case in the switch), and there is no inspector UI for behavior lists or interactable fields on any entity kind.

All infrastructure (data model, serialization, behavior system, TriggerComponent, preview renderer wireframes) was built in Phase 13. This phase is purely an editor feature layer on top of that infrastructure.

**Primary recommendation:** Extend the `EditorSceneObjectKind` enum + variant + `addTrigger()` + `toLevelDef()` as the first task — everything else (outliner, inspector, gizmo, test) depends on that foundation being correct.

## Project Constraints (from CLAUDE.md)

- Engine: Custom C++20 + OpenGL 4.1 Core Profile. No Unity/Unreal/Godot.
- UI: Dear ImGui v1.92.6 via `ImGuiLayer` infrastructure. No web frameworks.
- Build: CMake with `FetchContent`; test registration via `pixel_roguelike_add_test()`.
- Tests: Standalone executables (no external test framework); exit code = pass/fail.
- Naming: Classes `PascalCase`, methods `camelCase`, private members trailing underscore, constants `k` prefix.
- Non-copyable by default (`= delete` copy ctor/assignment).
- RAII for OpenGL resources.
- Components are POD structs (no methods, no inheritance).
- Every mutation entry point on `EditorSceneDocument` requires capture-before/push-after to `EditorCommandStack`.
- `pruneSelection` must be called after every undo/redo to avoid inspector null-dereference on stale IDs.
- `duplicateObject()` copies nodeId verbatim — `ensureObjectNodeId()` must be called on duplicate.
- Commits: no Claude co-author, no conventional commit prefixes.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Trigger authoring**
- D-01: Triggers are created via right-click context menu on entities ("Add Trigger Zone"), positioned relative to the parent entity
- D-02: Trigger volumes render as semi-transparent wireframes with drag handles on corners/edges to resize halfExtents (box) or radius (sphere) — same gizmo style as colliders
- D-03: Both Box and Sphere trigger shapes supported from the start, with a shape selector dropdown in the inspector
- D-04: Triggers appear as standalone top-level entries in the outliner, same as meshes and lights — can exist independently or be parented

**Behavior editing UI**
- D-05: Behaviors shown as collapsible event sections in the inspector (onActivate, onEnter, onExit, onTimer), each listing its action entries with a "+ Add Action" button per section
- D-06: Action type selection uses a categorized dropdown: Door (Open/Close/Toggle), Audio (PlaySound), Lighting (SetLight, FlickerLight), Entity (Enable/Disable), Player (Lock/Unlock/Teleport), Message (ShowMessage), Timing (Delay), Events (EmitEvent)
- D-07: Target node ID uses a combo dropdown listing all entities with a nodeId in the scene, type-to-filter, "self" always first option
- D-08: Core subset of action types for this phase: door operations (Open/Close/Toggle), SetLight, PlaySound, Delay, EnableEntity, DisableEntity. Remaining types (FlickerLight, ShowMessage, EmitEvent, LockPlayer, UnlockPlayer, TeleportPlayer) deferred to follow-up

**Interactable editing**
- D-09: "Make Interactable" checkbox on mesh entities in the inspector. When checked, shows prompt text, interaction distance, and dot threshold fields inline
- D-10: Interaction distance visualized as a wireframe ring/sphere around the mesh in the viewport. No visualization for dot threshold

**Save-load fidelity**
- D-11: Triggers become full `EditorSceneObject` entries (new `EditorSceneObjectKind::Trigger` variant) and survive save/load round-trips via `toLevelDef()` and `loadFromSceneFile()`
- D-12: Behavior lists and interactable data on mesh/light/trigger payloads preserved through all editor edit operations
- D-13: New dedicated test file for behavior/trigger round-trip fidelity — loads a .scene with triggers, behaviors, and interactables, saves it, reloads, verifies everything matches

### Claude's Discretion
- Exact wireframe colors and transparency for trigger volumes
- ImGui layout details for behavior/interactable inspector sections
- Which action parameter fields need special widgets vs simple float/string inputs
- Internal ordering of the categorized action type dropdown

### Deferred Ideas (OUT OF SCOPE)
- Full support for remaining 8 action types (FlickerLight, ShowMessage, EmitEvent, LockPlayer, UnlockPlayer, TeleportPlayer) — follow-up phase
- Dot threshold viewport visualization (cone/arc showing facing requirement)
- Behavior copy/paste between entities
- Behavior templates/presets for common patterns (door trigger, light switch, etc.)
</user_constraints>

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Dear ImGui | v1.92.6 | Inspector UI, outliner rows, combo dropdowns, collapsing headers | Already integrated; all editor UI uses it |
| EnTT | v3.16.0 | ECS registry backing EditorPreviewWorld | Already in use; `EditorSceneDocument` is separate from ECS |
| GLM | 1.0.3 | Position/scale math for trigger gizmo handles | All editor transforms use GLM |
| OpenGL 4.1 Core | 4.1 | Viewport wireframe rendering via RenderObject | Project ceiling; all shaders target GLSL 4.10 |

No new libraries needed. This phase is entirely in existing infrastructure.

**Installation:** None required.

## Architecture Patterns

### Adding a New EditorSceneObjectKind — The Established Pattern

Every new editor object kind follows this exact checklist (verified from codebase):

1. Add to `enum class EditorSceneObjectKind` in `EditorSceneDocument.h`
2. Add payload struct to `using EditorSceneObjectPayload = std::variant<...>` — use `TriggerPlacement` from `LevelDef.h`
3. Add `addTrigger()` method to `EditorSceneDocument`
4. Add `case EditorSceneObjectKind::Trigger:` in `toLevelDef()` — push to `level.triggers`
5. Add `case EditorSceneObjectKind::Trigger:` in `applyWorldTransform()` — update position/halfExtents/radius
6. Add `case EditorSceneObjectKind::Trigger:` in `localTransformMatrix()` — return position matrix
7. Update `nodeIdPtr()` and `parentNodeIdPtr()` visitors — `TriggerPlacement` already has `nodeId` and `parentNodeId`
8. Add outliner rendering in `EditorOutlinerPanel.cpp` — follows the switch pattern in `buildOutlinerVisibleRows`
9. Add inspector rendering in the scene inspector section
10. Add viewport wireframe rendering for selected triggers (gizmo handles) in `EditorScenePreviewRenderer.cpp`
11. Update `editorSceneObjectLabel()` and `editorSceneObjectKindName()` free functions at bottom of `EditorSceneDocument.h`

Critical: `loadFromSceneFile()` must replace `readOnlyTriggers_ = level.triggers` with `for (const auto& t : level.triggers) addTrigger(t)`. The `readOnlyTriggers_` field and `triggers()` accessor can then be removed.

### EditorSceneDocumentState — Capture/Restore for Undo

`EditorSceneDocumentState` holds `std::vector<EditorSceneObject> objects`. Since `TriggerPlacement` becomes a variant member, trigger state is automatically captured/restored by `captureState()` / `restoreState()` — no changes needed to these functions.

However: every mutation (add trigger, resize handles, edit behavior, toggle interactable) must go through the `EditorCommandStack` pattern:
```cpp
// Source: v1.1 research note in STATE.md
const auto before = document.captureState();
// ... mutate document ...
commandStack.push(std::make_unique<EditorDocumentStateCommand>(
    "Add Trigger Zone", before, document.captureState()));
```

### Behavior Inspector Pattern

Behavior data lives on the payload structs (`LevelMeshPlacement::behaviors`, `LevelLightPlacement::behaviors`, `TriggerPlacement::behaviors`). The inspector must reach into the payload via `std::get<>` to read and mutate the behavior lists.

The ImGui pattern for event sections (from UI-SPEC):
```cpp
// Source: UI-SPEC.md, EditorInspectorPanel.cpp pattern
if (ImGui::CollapsingHeader("On Activate")) {
    for (auto& action : behaviors) {
        renderActionEntryRow(action, document, ...);
    }
    if (ImGui::Button("+ Add Action")) {
        // capture-before, push default ActionEntry, push-after
    }
}
```

Action type categorized dropdown — the two-combo (Category left, ActionType right) pattern matches existing editor dropdown conventions.

### Trigger Gizmo Handle Pattern

Existing collider gizmo in `EditorScenePreviewRenderer.cpp` (lines ~138–168) renders scale handles for box/cylinder colliders using a 6-face approach. The trigger gizmo follows the same pattern — render a small quad/point at each face center, hit-test in screenspace, drag updates the corresponding `halfExtents` axis or `radius`.

The key difference from ImGuizmo (which handles full translate/rotate/scale gizmos) is that trigger resize handles are custom screenspace hit-tests — not ImGuizmo operations. Existing collider handle code in `applyGizmoToSelectedObject()` shows the pattern.

### Interactable Distance Ring

From `EditorScenePreviewRenderer.cpp` (lines ~203–229), existing trigger wireframes use `RenderObject` with `wireframe=true, ignoreDepth=true, unlit=true`. The interaction distance ring uses the same technique: push a `RenderObject` with the cube mesh scaled to `interactable->distance * 2.0f` radius as a sphere approximation, color `glm::vec3(0.80f, 0.60f, 0.20f)`.

This must be rendered for all mesh entities with `interactable.has_value() && interactable->distance > 0`, not just selected ones (per UI-SPEC D-10 interaction contract).

### Scene File Format (HIGH confidence — read from LevelDef.cpp directly)

Trigger serialization in `serializeLevelDef()` (line 1391):
```
trigger_box <px> <py> <pz> <hx> <hy> <hz> [fire_once] [node <id>] [parent <parentId>]
  on_activate <action_type> [target <nodeId>] [delay <float>] [fire_once] [params...]
  on_enter <action_type> ...
  on_exit <action_type> ...
  on_timer <action_type> ...

trigger_sphere <px> <py> <pz> <radius> [fire_once] [node <id>] [parent <parentId>]
  on_activate ...
```

Behavior sub-lines are indented (2 spaces). The parser's `attachSubLine` lambda dispatches on first token (`on_activate`, `on_enter`, `on_exit`, `on_timer`, `behavior`, `interactable`).

**This serialization is already complete.** The gap is `toLevelDef()` dropping triggers — once triggers are `EditorSceneObject` entries and `toLevelDef()` has the Trigger case, serialization works for free.

### Test Pattern (from test_level_roundtrip.cpp)

New test `tests/game/test_behavior_trigger_roundtrip.cpp` follows the same structure:
1. Build a `LevelDef` with trigger placements carrying `BehaviorDeclaration` entries and mesh placements with `InteractableDeclaration`
2. Call `serializeLevelDef()` → verify key tokens in string
3. `saveLevelDef()` to temp path → `loadLevelDef()` → `fs::remove()`
4. Assert all behavior lists and interactable fields survived
5. Register in `tests/game/CMakeLists.txt` with `pixel_roguelike_add_test(test_behavior_trigger_roundtrip SOURCES ... LIBRARIES game_content LABELS game)`

### Anti-Patterns to Avoid

- **Modifying `EditorSceneDocumentState` struct.** It captures the entire `objects_` vector — trigger payloads are already covered once `TriggerPlacement` is in the variant. No struct changes needed.
- **Bypassing the command stack.** Every inspector edit (add action, remove action, change parameter, toggle interactable, resize trigger) must go through `EditorDocumentStateCommand`. The type system does not enforce this — it must be done manually.
- **Reading `readOnlyTriggers_` after migration.** Once `loadFromSceneFile()` promotes triggers to `EditorSceneObject` entries, `readOnlyTriggers_` should be cleared/removed to avoid stale state.
- **Using ImGuizmo for trigger resize.** Trigger resize is custom 6-face handle gizmo, not ImGuizmo translate/rotate/scale. ImGuizmo handles translation of the trigger's origin; resize uses custom hit-testing.
- **Forgetting `ensureObjectNodeId()` on addTrigger.** The `addObject()` private method already calls `ensureObjectNodeId()`, so `addTrigger()` gets it for free — but if triggers are created elsewhere this must not be bypassed.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Behavior data model | Custom behavior struct | Existing `BehaviorDeclaration`, `ActionEntry`, `ActionParams` from Phase 13 | Already complete; all 14 action types, typed variant params |
| Trigger serialization | Custom trigger serializer | Existing `serializeLevelDef()` / `loadLevelDef()` | Already handles `trigger_box`, `trigger_sphere`, behaviors as sub-lines |
| Undo/redo for behavior edits | Custom command type | `EditorDocumentStateCommand` (captures full document state) | Already handles all mutations; behavior vector changes are covered |
| Collider wireframe rendering | Custom shader | `RenderObject` with `wireframe=true, ignoreDepth=true, unlit=true` | Same technique used for existing trigger and collider wireframes |
| Node ID lookup for target combo | Custom lookup | `findObjectIdByNodeId()` or iterate `document.objects()` checking `nodeIdPtr()` | Already available on `EditorSceneDocument` |
| Two-column inspector layout | Custom layout | `renderInspectorPropertyRow()` from `EditorPanelUtils.cpp` | Established convention — 108px label column |

## Common Pitfalls

### Pitfall 1: `std::visit` overloads missing Trigger variant
**What goes wrong:** `nodeIdPtr()`, `parentNodeIdPtr()`, `localTransformMatrix()`, `applyWorldTransform()`, `duplicateObject()`, and `editorSceneObjectLabel()` all use `std::visit` or `switch (object.kind)`. Adding `EditorSceneObjectKind::Trigger` to the enum without updating all these call sites causes compile errors or UB.
**Why it happens:** The variant visitor and kind switch are separate from the enum definition — the compiler doesn't enforce that they're exhaustive unless `default:` is removed.
**How to avoid:** Search all files for `EditorSceneObjectKind::` and `std::get<LevelGroupNode>` (last case) to find every switch/visitor that needs a Trigger case.
**Warning signs:** Compiler warnings about unhandled enum values (if `-Wswitch` is active).

### Pitfall 2: `readOnlyTriggers_` left populated after migration
**What goes wrong:** `appendHelperObjects()` in `EditorScenePreviewRenderer.cpp` (line 203) renders from `document.triggers()` which returns `readOnlyTriggers_`. After migration, if `readOnlyTriggers_` is not cleared and triggers are now also in `objects_`, trigger wireframes render twice.
**Why it happens:** The old path is still live after adding the new path.
**How to avoid:** In `loadFromSceneFile()`, replace `readOnlyTriggers_ = level.triggers` with the `addTrigger()` loop, and clear `readOnlyTriggers_`. Then remove `appendHelperObjects`'s loop over `document.triggers()` and replace it with a loop over objects of kind Trigger.
**Warning signs:** Doubled wireframe overlays in the viewport.

### Pitfall 3: Behavior mutation not captured for undo
**What goes wrong:** The inspector directly mutates `std::get<LevelMeshPlacement>(object.payload).behaviors` without a command stack capture. Ctrl+Z does nothing for behavior changes.
**Why it happens:** Inspector mutation is easy to do inline. The capture-before pattern requires remembering to wrap every edit.
**How to avoid:** Use `EditorPendingCommand` state (present in `LevelEditorUi.h`) for begin-drag/end-drag commit, or capture-before/push-after on every button click.
**Warning signs:** Ctrl+Z reverting scene-level changes but not behavior list changes.

### Pitfall 4: Target node combo populated from stale snapshot
**What goes wrong:** The combo listing all entity nodeIds is built from `document.objects()` at render time. If a referenced `targetNodeId` was renamed or deleted, the inspector shows a validation error but the stale string is still in the `ActionEntry`. On save, the stale nodeId is serialized — the game's BehaviorSystem silently skips actions with unresolvable targets.
**Why it happens:** NodeId references are string-based with no referential integrity.
**How to avoid:** Show `ImGui::TextColored` error when `targetNodeId` is not found in `document.objects()`. This is per UI-SPEC: "Target node not found in scene". Do not remove/fix stale references automatically — let the user fix them.
**Warning signs:** Behavior fires at wrong target or silently does nothing in runtime.

### Pitfall 5: `applyWorldTransform` not handling Trigger kind
**What goes wrong:** Dragging a Trigger in the viewport via ImGuizmo translation calls `applyWorldTransform()`. If the Trigger case is missing, the function returns false and the drag is silently rejected.
**Why it happens:** `applyWorldTransform` has a switch over `object.kind` — adding Trigger to the enum without adding its case causes a no-op.
**How to avoid:** Trigger's `applyWorldTransform` case should extract position from the world matrix (translation only — triggers don't rotate) and write to `TriggerPlacement::position`.

### Pitfall 6: `EditorSceneDocumentState` excludes readOnlyTriggers
**What goes wrong:** The current `captureState()` captures `objects_` but not `readOnlyTriggers_`. After migration this is fine — triggers live in `objects_`. But if any code path still touches `readOnlyTriggers_`, undo will not restore it correctly.
**Why it happens:** Post-migration `readOnlyTriggers_` should be empty and unused.
**How to avoid:** Audit all usages of `readOnlyTriggers_` after migration. Remove `readOnlyTriggers_` field and `triggers()` accessor from the header once migration is complete, so the compiler catches any remaining uses.

## Code Examples

### Adding EditorSceneObjectKind::Trigger
```cpp
// Source: src/editor/scene/EditorSceneDocument.h — extend existing enum

enum class EditorSceneObjectKind {
    Mesh,
    Light,
    BoxCollider,
    CylinderCollider,
    ReflectionProbe,
    PlayerSpawn,
    Archetype,
    Group,
    Trigger,   // NEW
};

using EditorSceneObjectPayload = std::variant<
    LevelMeshPlacement,
    LevelLightPlacement,
    LevelBoxColliderPlacement,
    LevelCylinderColliderPlacement,
    LevelReflectionProbePlacement,
    LevelPlayerSpawn,
    LevelArchetypePlacement,
    LevelGroupNode,
    TriggerPlacement>;   // NEW
```

### addTrigger in EditorSceneDocument.cpp
```cpp
// Source: established pattern from addMesh(), addLight(), etc.
std::uint64_t EditorSceneDocument::addTrigger(const TriggerPlacement& placement) {
    return addObject(EditorSceneObjectKind::Trigger, placement);
}
```

### Trigger case in toLevelDef
```cpp
// Source: src/editor/scene/EditorSceneDocument.cpp — extend toLevelDef switch
case EditorSceneObjectKind::Trigger:
    level.triggers.push_back(std::get<TriggerPlacement>(object.payload));
    break;
```

### loadFromSceneFile trigger promotion
```cpp
// Source: src/editor/scene/EditorSceneDocument.cpp line ~88 — replace readOnlyTriggers_ assignment
// BEFORE:
readOnlyTriggers_ = level.triggers;

// AFTER:
for (const auto& trigger : level.triggers) {
    addTrigger(trigger);
}
readOnlyTriggers_.clear();  // remove stale path
```

### Trigger gizmo handles in EditorScenePreviewRenderer
```cpp
// Source: pattern from collider wireframe at lines ~138–168
// For selected trigger objects in objects():
if (object.kind == EditorSceneObjectKind::Trigger) {
    const auto& t = std::get<TriggerPlacement>(object.payload);
    const bool isSelected = isSelected(selectedIds, object.id);
    if (t.shape == TriggerShape::Box && cube != nullptr) {
        // Main wireframe
        const glm::vec3 tint = isSelected
            ? glm::vec3(0.40f, 1.00f, 0.40f)   // bright on select
            : glm::vec3(0.20f, 0.80f, 0.20f);   // #33CC33 from UI-SPEC
        objects.push_back(RenderObject{
            cube,
            makeModelMatrix(t.position, t.halfExtents * 2.0f),
            tint,
            materials.resolve("metal_default"),
            true, true, true, 1.5f
        });
        // Resize handles: 6 small cubes at face centers
        // ... one per axis-aligned face ...
    }
}
```

### Behavior inspector section
```cpp
// Source: UI-SPEC.md component spec; EditorInspectorPanel.cpp CollapsingHeader pattern
auto renderBehaviorSection = [&](const char* label, std::vector<ActionEntry>& actions) {
    const bool hasActions = !actions.empty();
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
    if (hasActions) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    if (ImGui::CollapsingHeader(label, flags)) {
        if (actions.empty()) {
            ImGui::TextDisabled("(none)");
        }
        for (int i = static_cast<int>(actions.size()) - 1; i >= 0; --i) {
            ImGui::PushID(i);
            if (ImGui::SmallButton("X")) {
                // capture before, erase actions[i], push command
                ImGui::SetItemTooltip("Remove action");
            }
            // ... category combo, action type combo, target combo, params ...
            ImGui::PopID();
        }
        if (ImGui::Button("+ Add Action", ImVec2(-1.0f, 0.0f))) {
            // capture before, push default ActionEntry, push command
        }
    }
};
```

### New test registration in CMakeLists.txt
```cmake
# Source: tests/game/CMakeLists.txt pattern
pixel_roguelike_add_test(test_behavior_trigger_roundtrip
    SOURCES test_behavior_trigger_roundtrip.cpp
    LIBRARIES game_content
    LABELS game
)
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Triggers read-only in editor (`readOnlyTriggers_`) | Triggers as full `EditorSceneObject::Trigger` entries | This phase | Full CRUD + undo/redo + save-load |
| Behaviors hand-authored in .scene files only | Behavior lists editable via inspector UI | This phase | Level designers can author behaviors without touching raw scene text |
| Interactable fields .scene-only | "Make Interactable" checkbox in inspector | This phase | No more manual `interactable` sub-lines |
| No inspector for behavior data | Per-event collapsible sections with action editor | This phase | Core behavior authoring workflow |

**Infrastructure already complete (Phase 13):**
- `ActionTypes.h`: 14 action types, typed `ActionParams` variant
- `BehaviorComponent.h`: onActivate/onEnter/onExit/onTimer action lists
- `TriggerComponent.h`: Box/Sphere shapes, halfExtents, radius, runtime state
- `LevelDef.h/cpp`: Full serialization/deserialization of triggers, behaviors, interactables
- Preview renderer: trigger wireframes already rendered from `readOnlyTriggers_`

## Environment Availability

Step 2.6: SKIPPED (no external dependencies — all changes are C++ source/editor code within the existing CMake project).

## Open Questions

1. **Trigger gizmo handle hit-testing approach**
   - What we know: Collider resize is handled via ImGuizmo's scale transform (which scales the entire object). Trigger resize needs to resize halfExtents per-axis without affecting position.
   - What's unclear: Whether to implement custom screenspace hit-testing for the 6 face handles, or use ImGuizmo with an intermediate "scale from center" decomposition.
   - Recommendation: Custom screenspace hit-testing matches the UI-SPEC description ("drag handles on corners/edges") and avoids ImGuizmo's scale-from-origin ambiguity. Each face handle is an 8x8px target at the face center in screen space. This is Claude's discretion per the context.

2. **`BehaviorDeclaration` vs `ActionEntry` in inspector**
   - What we know: `LevelDef.h` stores `std::vector<BehaviorDeclaration>` where each `BehaviorDeclaration` has `eventType` (string: "on_activate", etc.) and `action` (ActionEntry). The inspector needs to read/write these lists.
   - What's unclear: Whether to keep `BehaviorDeclaration` as the storage format in the inspector or convert to a `std::vector<ActionEntry>` per-event list (like `BehaviorComponent` does).
   - Recommendation: Keep `BehaviorDeclaration` since that is what `LevelMeshPlacement::behaviors` stores. The inspector must iterate all declarations and group them by `eventType` for rendering. On "Add Action", push a `BehaviorDeclaration{.eventType = "on_activate", .action = ActionEntry{}}`. This avoids data model changes.

3. **`showTriggers` UI toggle**
   - What we know: `EditorUiState` has `showColliders`, `showLightHelpers`, `showSpawnMarker` toggles in `LevelEditorUi.h`. Trigger volumes are always shown in the current read-only implementation.
   - What's unclear: Whether triggers should have their own visibility toggle or always be visible.
   - Recommendation: Add `showTriggers = true` to `EditorUiState`, following the existing pattern. Wire it to the viewport toolbar alongside `showColliders`. This is Claude's discretion.

## Sources

### Primary (HIGH confidence)
- `src/editor/scene/EditorSceneDocument.h` / `.cpp` — full document model, toLevelDef, loadFromSceneFile, addObject pattern
- `src/game/level/LevelDef.h` / `.cpp` — TriggerPlacement, BehaviorDeclaration, InteractableDeclaration, serializeLevelDef, loadLevelDef
- `src/game/behavior/ActionTypes.h` — 14 action types, ActionParams variant, ActionEntry struct
- `src/game/behavior/BehaviorComponent.h` — BehaviorComponent with per-event action lists
- `src/game/behavior/TriggerComponent.h` — TriggerShape enum, TriggerComponent fields
- `src/editor/render/EditorScenePreviewRenderer.cpp` — existing trigger wireframe rendering pattern (lines 203–229)
- `src/editor/ui/EditorOutlinerPanel.cpp` / `.h` — outliner row rendering pattern
- `src/editor/ui/LevelEditorUi.h` — EditorUiState, EditorPendingCommand, EditorPlacementKind
- `src/editor/core/EditorCommand.h` — EditorDocumentStateCommand, EditorCommandStack
- `tests/game/test_level_roundtrip.cpp` — test pattern reference
- `tests/game/CMakeLists.txt` — `pixel_roguelike_add_test()` registration pattern
- `.planning/phases/16-editor-trigger-and-behavior-authoring-with-save-load-fidelity/16-UI-SPEC.md` — ImGui widget specs, wireframe colors, interaction contracts

### Secondary (MEDIUM confidence)
- `.planning/STATE.md` accumulated context — v1.1 research notes on EditorCommandStack pattern, pruneSelection, duplicateObject/ensureObjectNodeId
- `src/editor/viewport/EditorViewportInteraction.h` — gizmo interaction API

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new libraries; all existing infrastructure
- Architecture: HIGH — direct read of all relevant source files; patterns verified
- Pitfalls: HIGH — based on direct code reading, not inference
- Serialization: HIGH — LevelDef.cpp read in full; trigger serialization already complete
- Gizmo handles: MEDIUM — approach is clear but exact screenspace hit-test implementation is Claude's discretion

**Research date:** 2026-04-04
**Valid until:** 2026-05-04 (stable codebase — no third-party API volatility)
