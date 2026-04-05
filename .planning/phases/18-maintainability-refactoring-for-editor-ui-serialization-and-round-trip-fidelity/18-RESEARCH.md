# Phase 18: Maintainability Refactoring for Editor UI, Serialization, and Round-Trip Fidelity - Research

**Researched:** 2026-04-05
**Domain:** C++ refactoring — parser consolidation, visitor pattern, inspector decomposition, format migration
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Extract `parseNodeMetadata()` and `parseShapeTokens()` helper functions from LevelDef.cpp. Each object parser calls helpers instead of inline loops. Eliminates 8+ duplicate metadata parsing loops (~200 lines saved).
- **D-02:** Extract per-action parser functions (`parseDoorActionParams()`, `parseSoundActionParams()`, etc.) for all 14 action types. Variant type guaranteed by prior switch on ActionType. Type-safe and independently testable.
- **D-03:** Create a `PlacementBase` struct with common fields (position, rotation, nodeId, parentNodeId). All placement types embed it. Helpers and the parser work against PlacementBase for shared fields.
- **D-04:** Delete all legacy format parsing code entirely (~460 lines): `collider_box`, `collider_cylinder`, `trigger_box`, `trigger_sphere`, `trigger_cylinder`, `trigger_capsule`. Scene files were already migrated in Phase 17. Clean break.
- **D-05:** Unify light keywords: `light`, `spot_light`, `dir_light` → single `light` keyword with `type=point/spot/directional` token. Consistent with how `collider` now uses shape/mode tokens.
- **D-06:** Light format migration happens in this phase alongside the parser refactor. Parser only handles new format. All .scene files migrated in the same commit.
- **D-07:** Split EditorInspectorPanel.cpp into per-type inspector classes: `MeshInspector`, `LightInspector`, `ColliderInspector`, `ReflectionProbeInspector`, `PlayerSpawnInspector`, `ArchetypeInspector`, `GroupInspector`. Each is a separate .cpp file with a `drawInspector()` method. Main panel dispatches to the right one.
- **D-08:** Also extract asset inspectors: `MaterialInspector`, `EnvironmentInspector`, `PrefabInspector`. Full decomposition of the monolithic panel.
- **D-09:** Create a shared `drawTransformSection(PlacementBase&)` function that all inspectors call. Single source of truth for position/rotation/scale editing UI.
- **D-10:** Implement visitor pattern for EditorSceneDocument operations. Create a `SceneObjectVisitor` interface with `visitMesh()`, `visitLight()`, etc. Replace 7 switch statements (duplicateObject, toLevelDef, localTransformMatrix, objectKindName, objectLabel, objectAnchor) with visitor implementations.
- **D-11:** Adding a new object kind becomes adding one new visitor case, not updating 7 switch statements across 6 functions.

### Claude's Discretion

- Whether to keep `std::variant<>` for EditorSceneObjectPayload or replace with polymorphic base class — evaluate which approach works better with the visitor pattern and PlacementBase refactor
- Exact file organization for per-type inspectors (subdirectory vs flat in `src/editor/ui/`)
- Whether PlacementBase should use composition (embedded struct) or inheritance
- Which of the 7 switch statements benefit most from visitor pattern vs. simple `std::visit` lambdas

### Deferred Ideas (OUT OF SCOPE)

- Declarative format schema (describing .scene format as data rather than parser code)
- Extending test coverage for environment serialization and group hierarchy
- EditorOutlinerPanel switch statement cleanup
- Node ID type safety improvements (string keys → typed handles)
</user_constraints>

---

## Summary

Phase 18 is a pure refactoring phase across three primary files: `LevelDef.cpp` (1,490 lines), `EditorSceneDocument.cpp` (924 lines), and `EditorInspectorPanel.cpp` (1,516 lines). No new features ship. The goal is to reduce the cost of future changes in Phases 19+.

The work falls into four independent tracks. First, the parser consolidation track: extract `parseNodeMetadata()`, `parseShapeTokens()`, and per-action-type parser helpers from LevelDef.cpp, and introduce `PlacementBase` to give those helpers a shared target. Second, the legacy format cleanup track: delete the ~460-line block of `collider_box`, `collider_cylinder`, `trigger_box`, etc. parsers and migrate `light`/`spot_light`/`dir_light` to a unified `light type=point/spot/directional` format across all six .scene files. Third, the inspector decomposition track: split the 1,516-line monolith into 10+ single-type inspector .cpp files, each with a `drawInspector()` method, with a shared `drawTransformSection()` helper. Fourth, the visitor pattern track: replace the 7 switch-on-kind statements in EditorSceneDocument.cpp with a `SceneObjectVisitor` interface that guarantees all object kinds are handled.

The key design tension is between `std::variant` + `std::visit` (already used in `nodeIdPtr()` and `parentNodeIdPtr()`) and a virtual dispatch visitor. Given that the codebase already has `EditorSceneObjectPayload` as a `std::variant<>`, the planner should evaluate extending `std::visit` for the remaining switch statements before introducing a full virtual visitor hierarchy — but the decision is discretionary.

**Primary recommendation:** Sequence the work as (1) PlacementBase + parser helpers, (2) legacy deletion + light format migration, (3) inspector split, (4) visitor pattern. Each track is independently testable and merge-safe.

---

## Project Constraints (from CLAUDE.md)

| Directive | Implication for This Phase |
|-----------|---------------------------|
| C++20 standard | Use designated initializers, `std::visit` with generic lambdas, `if constexpr` |
| OpenGL 4.1 Core Profile only | Not relevant to this refactoring phase |
| Custom C++ engine — no Unity/Unreal | Not relevant |
| Components are POD structs (no methods, no inheritance) | `PlacementBase` MUST be a POD struct embedded by composition, not a base class with virtual methods |
| `.clang-format` LLVM style: 4-space indent, 100-char column, attached braces | All new files must comply |
| Files: `PascalCase.h` / `PascalCase.cpp` matching primary class name | Inspector files: `MeshInspector.cpp`, `LightInspector.cpp`, etc. |
| `#pragma once` (no include guards) | All new headers use `#pragma once` |
| Include order: stdlib → third-party → project headers | Maintain in all new files |
| Non-copyable by default | Inspector classes should delete copy ctor/assignment if they hold state |
| Tests: standalone executables with exit code = pass/fail | Round-trip tests validate the new format |
| Build: CMake with `FetchContent` | New .cpp files need to be added to CMakeLists.txt target |
| Three executables: `pixel-roguelike`, `level-editor`, `procedural-model-viewer` | Inspector files belong to the `editor` CMake target |
| Commit without co-author, no prefix | Per global CLAUDE.md |

---

## Standard Stack

This phase is pure C++ refactoring — no new library dependencies.

### Relevant Existing Patterns Already in Codebase

| Pattern | Location | How It Applies |
|---------|----------|---------------|
| `std::variant` + `std::visit` | `EditorSceneDocument.cpp` — `nodeIdPtr()`, `parentNodeIdPtr()` | Extend for remaining 7 switch statements (visitor track D-10) |
| `std::visit` with `if constexpr` | `LevelDef.cpp` — `serializeActionEntry()` | Model for per-type action param serializers (D-02) |
| `ImGui::CollapsingHeader` + `beginInspectorPropertyTable` | `EditorInspectorPanel.cpp` | Pattern for per-type inspector section structure |
| `editVec3()` helper | `EditorInspectorPanel.cpp` | Foundation for `drawTransformSection()` (D-09) |
| `appendNodeMetadata()` | `LevelDef.cpp` line 86-95 | Pattern for the extraction of `parseNodeMetadata()` |
| Unified `collider` keyword with shape/mode tokens | `LevelDef.cpp` lines 727-882 (new parser), 1381-1424 (serializer) | Exact template for unified `light` keyword (D-05) |
| `collectRemainingTokens()` | `LevelDef.cpp` | Shared utility already extracted; keep as-is |

---

## Architecture Patterns

### Track 1: PlacementBase + Parser Helpers

**What the code currently looks like:**

Every placement struct (`LevelMeshPlacement`, `LevelLightPlacement`, etc.) independently declares:
```cpp
std::string nodeId;
std::string parentNodeId;
```

Every parser block independently runs:
```cpp
if (tokens[index] == "node") { placement.nodeId = tokens[index+1]; index += 2; continue; }
if (tokens[index] == "parent") { placement.parentNodeId = tokens[index+1]; index += 2; continue; }
throwParseError(path, lineNumber, "invalid X metadata");
```
This pattern repeats 8+ times across the 1,490-line file.

**Recommended approach for PlacementBase:**

Use embedded composition (not inheritance) — mandatory per CLAUDE.md POD rule:

```cpp
// LevelDef.h
struct PlacementBase {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    std::string nodeId;
    std::string parentNodeId;
};

struct LevelMeshPlacement {
    PlacementBase base;
    std::string meshId;
    glm::vec3 scale{1.0f};
    // ...
};
```

Or alternatively, add the fields directly to the struct and have helpers accept `std::string& outNodeId, std::string& outParentNodeId`. This avoids restructuring all downstream code that accesses `placement.nodeId` directly (e.g., `LevelBuilder.cpp`, round-trip tests).

**Recommendation (discretion area):** Use OUT-PARAMETER helpers rather than embedding PlacementBase, to minimize churn at call sites. All existing code accessing `placement.nodeId` continues to work. The helper signature:

```cpp
// In anonymous namespace of LevelDef.cpp
void parseNodeMetadata(const std::string& path, int lineNumber,
                       const std::vector<std::string>& tokens,
                       std::string& outNodeId, std::string& outParentNodeId);
```

This avoids breaking `LevelBuilder.cpp`, all test assertions like `loaded.meshes.front().nodeId`, and `editorSceneObjectAnchor()` which accesses placement fields directly.

**Per-action parser helpers (D-02):**

The current `parseActionEntry()` is a single 150-line function with manual field dispatch using `std::get_if`. The extraction pattern is:

```cpp
void parseDoorActionParams(DoorActionParams& params,
                           const std::string& path, int lineNumber,
                           const std::vector<std::string>& tokens, std::size_t i);
```

The switch on `ActionType` in `parseActionEntry()` calls the right helper. Each helper handles only its own params. The `target`, `delay`, `fire_once` tokens remain in the main dispatcher.

### Track 2: Legacy Format Deletion + Light Migration

**What to delete:** Lines 727-882 of LevelDef.cpp contain parsers for:
- `collider_box` (~40 lines)
- `collider_cylinder` (~40 lines)
- `trigger_box`, `trigger_sphere`, `trigger_cylinder`, `trigger_capsule` (~320 lines total)

These are dead code since Phase 17 migrated all .scene files to the unified `collider` keyword.

**New light format (D-05):**

Existing formats and their serializer (lines 1330-1378):
```
light <x> <y> <z> <r> <g> <b> <radius> <intensity> [node X] [parent X]
spot_light <x> <y> <z> <dx> <dy> <dz> <r> <g> <b> <radius> <intensity> <inner> <outer> <shadows> [node X]
dir_light <dx> <dy> <dz> <r> <g> <b> <intensity> [node X]
```

Proposed unified format (mirroring the collider unification from Phase 17):
```
light point <x> <y> <z> <r> <g> <b> <radius> <intensity> [node X] [parent X]
light spot <x> <y> <z> <dx> <dy> <dz> <r> <g> <b> <radius> <intensity> <inner> <outer> <shadows> [node X]
light directional <dx> <dy> <dz> <r> <g> <b> <intensity> [node X]
```

**Scene files to migrate (light keyword occurrences):**

| File | Light occurrences | Types |
|------|-------------------|-------|
| `assets/scenes/initial_scene.scene` | 7 | point only |
| `assets/scenes/country_house.scene` | 11 | point only |
| `assets/scenes/cathedral.scene` | 13 | point + spot |
| `assets/scenes/silos_cloister.scene` | 3 | point + directional |
| `assets/scenes/institutional_room.scene` | 6 | point only |
| `assets/scenes/warden_office.scene` | 8 | point only |

Total: 48 light lines across 6 files. Note: `cathedral.scene` has 3 `spot_light` entries; `silos_cloister.scene` has 1 `dir_light`. These are the tricky ones requiring careful positional argument mapping.

**Critical:** The parser's `CurrentEntityKind` tracker and `attachSubLine` lambda use `CurrentEntityKind::Light` to route behavior sub-lines to the right placement. The new parser must set `currentKind = CurrentEntityKind::Light` for all three types.

**Round-trip test update required:** `tests/game/test_level_roundtrip.cpp` currently asserts `spot_light` handling. After migration, the test must assert the new unified format and verify the old keywords are NOT present (mirroring the collider format assertions at lines 107-115).

### Track 3: Inspector Decomposition

**Current structure:** `EditorInspectorPanel.cpp` (1,516 lines) is a single translation unit containing:
- Asset inspector session management (~60 lines)
- Behavior authoring helpers (action category defs, per-row rendering) (~300 lines)
- `renderMaterialDraftFields()` (~120 lines) — material asset inspector
- `renderEnvironmentDraftFields()` (~180 lines) — environment asset inspector
- `renderPrefabDraftFields()` (~60 lines) — prefab asset inspector
- `renderMaterialAssetInspector()`, `renderEnvironmentAssetInspector()`, `renderPrefabAssetInspector()` (~180 lines)
- `renderSceneSelectionInspector()` (~400 lines) containing the monolithic switch on `EditorSceneObjectKind`
- The `renderInspector()` entry point (~50 lines)

**Target file structure (discretion: flat in `src/editor/ui/`):**

```
src/editor/ui/
├── EditorInspectorPanel.cpp       (entry point + dispatch only, ~80 lines)
├── EditorInspectorPanel.h         (unchanged)
├── EditorPanels.h                 (unchanged)
├── inspectors/
│   ├── MeshInspector.h / .cpp     (drawMeshInspector)
│   ├── LightInspector.h / .cpp    (drawLightInspector)
│   ├── ColliderInspector.h / .cpp (drawColliderInspector)
│   ├── ReflectionProbeInspector.h / .cpp
│   ├── PlayerSpawnInspector.h / .cpp
│   ├── ArchetypeInspector.h / .cpp
│   ├── GroupInspector.h / .cpp
│   ├── MaterialInspector.h / .cpp
│   ├── EnvironmentInspector.h / .cpp
│   └── PrefabInspector.h / .cpp
└── InspectorUtils.h / .cpp        (drawTransformSection, editVec3, editColor, renderInspectorPropertyRow, etc.)
```

**Or flat alternative (no subdirectory):**

```
src/editor/ui/
├── EditorInspectorPanel.cpp       (dispatch)
├── InspectorMesh.cpp
├── InspectorLight.cpp
├── ...etc
└── InspectorUtils.cpp
```

**Recommendation:** Use a `src/editor/ui/` subdirectory named `inspectors/` to keep the directory scannable. Update `CMakeLists.txt` to glob or enumerate the new files.

**The `drawTransformSection()` signature (D-09):**

Since PlacementBase uses out-parameter helpers (not embedded struct), the transform section helper takes separate refs:

```cpp
// InspectorUtils.h
bool drawTransformSection(glm::vec3& position,
                          glm::vec3& rotation,
                          glm::vec3& scale,
                          const EditorSceneDocument& document,
                          EditorCommandStack& commandStack,
                          const EditorSceneDocumentState& beforeState);
```

For types without scale (lights, colliders with their own extent fields), pass a dummy or provide an overload.

**Alternatively**, the per-type inspector functions each call `editVec3` directly (as they do now) but share the `renderInspectorPropertyRow` and `trackSceneItem` utilities from `InspectorUtils`. The transform deduplication is then only partial — position/rotation are shared but scale is per-type. This is the lower-risk interpretation of D-09.

**Inspector interface pattern:**

Each inspector is a free function (not a class), consistent with the existing codebase style:

```cpp
// MeshInspector.h
#pragma once
#include "editor/scene/EditorSceneDocument.h"
// ...

void drawMeshInspector(LevelMeshPlacement& mesh,
                       EditorSceneDocument& document,
                       const std::vector<std::string>& meshIds,
                       const std::vector<std::string>& materialIds,
                       const ContentRegistry& content,
                       EditorCommandStack& commandStack,
                       EditorPendingCommand& pendingCommand);
```

The main panel's switch dispatches to the right function.

### Track 4: Visitor Pattern for EditorSceneDocument

**The 7 switch statements to replace:**

| Function | Location in EditorSceneDocument.cpp | Complexity |
|----------|--------------------------------------|-----------|
| `localTransformMatrix()` | ~line 785 | Medium — different matrix per kind |
| `applyWorldTransform()` | ~line 540 | High — decomposes and writes back to variant |
| `editorSceneObjectKindName()` | ~line 843 | Low — string lookup |
| `editorSceneObjectLabel()` | ~line 863 | Medium — formatted string |
| `editorSceneObjectAnchor()` | ~line 906 | Low — position field access |
| `toLevelDef()` | ~line 651 | Medium — populate LevelDef vectors |
| `duplicateObject()` | ~line 615 | Trivial — already delegates to `addObject` |

**Design decision (Claude's discretion): `std::visit` vs virtual visitor**

The codebase already uses `std::visit` with generic lambdas for `nodeIdPtr()` and `parentNodeIdPtr()`:

```cpp
return std::visit([](auto& placement) -> std::string* { return &placement.nodeId; }, object.payload);
```

This pattern works because ALL payload types share the `nodeId` field. For functions like `localTransformMatrix()` where each type needs a DIFFERENT implementation, `std::visit` with an overloaded lambda or `if constexpr` is cleaner than virtual dispatch and avoids introducing a new base class/interface.

**Recommendation:** Use `std::visit` with type-dispatching overloads rather than a virtual `SceneObjectVisitor` interface. This:
1. Keeps the `std::variant<>` storage (no pointer indirection, no heap allocation)
2. Is consistent with `nodeIdPtr()` / `parentNodeIdPtr()` already in the file
3. Produces compile errors when a new type is added and the visit lambda doesn't handle it
4. Avoids the verbosity of a full visitor class hierarchy for what are essentially one-liners

**Example (editorSceneObjectAnchor):**

```cpp
glm::vec3 editorSceneObjectAnchor(const EditorSceneObject& object) {
    return std::visit([](const auto& p) -> glm::vec3 { return p.position; }, object.payload);
}
```

This works because all 7 payload types have `position`. Currently this function uses a switch with 7 cases — the `std::visit` version is 3 lines.

**Example (editorSceneObjectKindName):**

```cpp
const char* editorSceneObjectKindName(EditorSceneObjectKind kind) {
    // This one is not a payload visit — it's an enum switch
    // Keep as static lookup table or constexpr array
}
```

`kindName` dispatches on `EditorSceneObjectKind`, not on the payload variant — it stays as a switch or constexpr lookup.

**Example (localTransformMatrix — requires type-specific logic):**

```cpp
glm::mat4 EditorSceneDocument::localTransformMatrix(const EditorSceneObject& object) const {
    return std::visit([this](const auto& p) -> glm::mat4 {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, LevelMeshPlacement>) {
            return makeTransformMatrix(p.position, p.rotation, p.scale);
        } else if constexpr (std::is_same_v<T, LevelLightPlacement>) {
            return p.type == LightType::Directional
                ? glm::mat4(1.0f)
                : makeTransformMatrix(p.position, glm::vec3(0.0f), glm::vec3(1.0f));
        }
        // ... etc
    }, object.payload);
}
```

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Visitor dispatch | Custom vtable or dynamic_cast chain | `std::visit` with `if constexpr` | Already in codebase; variant is guaranteed exhaustive at compile time |
| Token parsing utilities | New string tokenizer | `std::istringstream` + `collectRemainingTokens()` (already exists) | Consistent with existing parser style |
| Transform decomposition | Custom matrix decompose | `glm::decompose` + `glm::extractEulerAngleXYZ` | Already used in `EditorSceneDocument.cpp` and `LevelDef.cpp` — `decomposeTransformMatrix()` is duplicated and can be shared |
| File I/O for scene migration | Custom migration tool | In-place sed-like text substitution in a Python or shell script, or direct source edits | 48 lines across 6 files — easiest to do by direct edit and verify with round-trip test |

**Key insight:** The `decomposeTransformMatrix()` helper is duplicated identically in both `LevelDef.cpp` (anonymous namespace) and `EditorSceneDocument.cpp` (anonymous namespace). Moving it to a shared utility (`engine/core/MathUtils.h` already exists) eliminates this duplication — but this is a low-priority bonus, not required by any decision.

---

## Common Pitfalls

### Pitfall 1: PlacementBase Breaks Access Patterns

**What goes wrong:** If `PlacementBase` is embedded as a named field (`placement.base.nodeId`), all downstream code in `LevelBuilder.cpp`, `EditorSceneDocument.cpp`, `editorSceneObjectAnchor()`, round-trip tests, and the inspector panel must be updated to use the new path. This is 50+ callsites.

**Why it happens:** The CONTEXT.md says "all placement types embed it" without specifying whether `nodeId` is accessed as `placement.nodeId` or `placement.base.nodeId`.

**How to avoid:** Either (a) use C++ inheritance so `placement.nodeId` still works via base class member access — but inheritance violates the POD rule — or (b) use out-parameter helpers that write to the existing struct's direct fields (no embedding), or (c) if embedding, do a comprehensive callsite audit BEFORE writing any code.

**Warning signs:** Compiler errors referencing `.base.nodeId` in unexpected files.

### Pitfall 2: Light Migration Token Count Mismatch

**What goes wrong:** The current `light` keyword expects `x y z r g b radius intensity` (8 floats). The current `spot_light` expects `x y z dx dy dz r g b radius intensity inner outer shadow_bool` (13 floats + 1 bool token). A new unified `light point` keyword shifts the positional arguments by one token (`point` is now token[1]).

**Why it happens:** Existing scene files have the old positional format baked in. The parser's `std::istringstream` extraction is positional — inserting a type token before the coordinates requires adjusting the extraction order.

**How to avoid:** Write the new parser first, then migrate ALL .scene files in the same commit. Run the round-trip test immediately. Verify `cathedral.scene` specifically (it has `spot_light`).

**Warning signs:** Round-trip test fails on light position/color fields being NaN or out of range.

### Pitfall 3: Inspector Split Creates CMakeLists.txt Drift

**What goes wrong:** New `MeshInspector.cpp`, `LightInspector.cpp`, etc. files are created but not added to the `editor` CMake target, causing linker errors (undefined references).

**Why it happens:** The CMakeLists.txt for the editor target lists .cpp files explicitly or uses `file(GLOB ...)` — if glob, it needs a CMake re-run; if explicit, all new files must be added.

**How to avoid:** Check the editor target's CMakeLists.txt before starting. If using explicit file listing, plan to add all 10+ new files in the same commit as creating them. If using glob, add a comment that re-running CMake is required.

**Warning signs:** `undefined reference to drawMeshInspector` at link time even though the file was created.

### Pitfall 4: `std::visit` Generic Lambda Ordering

**What goes wrong:** A `std::visit` with `if constexpr` chain silently falls through to a default branch that returns an incorrect value (e.g., `glm::mat4(1.0f)`) when a new object kind is added without updating the visitor.

**Why it happens:** Unlike a switch, `std::visit` with a generic lambda doesn't produce a compile warning for unhandled types if there's a fallback.

**How to avoid:** Use `static_assert(false, ...)` in the final `else` branch of each `if constexpr` chain, or use explicit overloads (e.g., an overloaded callable struct) which the compiler will reject if a type is unhandled. The `static_assert` approach works for `std::visit`:

```cpp
std::visit([](const auto& p) {
    using T = std::decay_t<decltype(p)>;
    if constexpr (std::is_same_v<T, LevelMeshPlacement>) { ... }
    else { static_assert(sizeof(T) == 0, "Unhandled payload type in visitor"); }
}, object.payload);
```

Note: `static_assert(false, ...)` in an `else` branch of a constexpr-if is a compile error even if never instantiated in some compilers (pre-C++23). Use `static_assert(sizeof(T) == 0, ...)` instead — it's the canonical workaround.

### Pitfall 5: Behavior Sub-Line Routing After Light Parser Change

**What goes wrong:** The parser uses a `CurrentEntityKind` enum and `attachSubLine` lambda to route indented behavior lines (e.g., `  on_enter play_sound...`) to the correct placement's `behaviors` vector. After unifying the light parser, `currentKind = CurrentEntityKind::Light` must still be set — and if the new unified light parser forgets this, behaviors silently drop.

**Why it happens:** The unified `light` branch in the parser must set `currentKind = CurrentEntityKind::Light` AND `currentIndex = data.lights.size()` before pushing the placement, matching what the old `light`/`spot_light`/`dir_light` branches each did.

**How to avoid:** Look at how the `collider` parser (the template for this unification) sets `currentKind` — triggers and solid+trigger set it, solid-only does not. For lights, always set it (lights can always have behaviors).

---

## Code Examples

### Verified Pattern: std::visit with if constexpr (from LevelDef.cpp serializeActionEntry)

```cpp
// Source: src/game/level/LevelDef.cpp ~line 425 (serializeActionEntry)
std::visit([&](const auto& params) {
    using T = std::decay_t<decltype(params)>;
    if constexpr (std::is_same_v<T, DoorActionParams>) {
        out << " duration " << formatFloat(params.duration);
    } else if constexpr (std::is_same_v<T, SoundActionParams>) {
        if (!params.soundId.empty()) out << " sound " << params.soundId;
        out << " volume " << formatFloat(params.volume);
    } // ...etc
}, entry.params);
```

This exact pattern works for per-action param serialization (D-02) and for the visitor replacement of switch statements (D-10).

### Verified Pattern: Generic visit for shared fields (from EditorSceneDocument.cpp)

```cpp
// Source: src/editor/scene/EditorSceneDocument.cpp ~line 759
std::string* EditorSceneDocument::nodeIdPtr(EditorSceneObject& object) {
    return std::visit([](auto& placement) -> std::string* { return &placement.nodeId; }, object.payload);
}
```

All 7 payload types have `nodeId` — generic lambda works. Same pattern applies to `position` for `editorSceneObjectAnchor()`.

### Verified Pattern: Unified collider format (template for light unification)

```cpp
// Source: src/game/level/LevelDef.cpp ~line 730 (new unified collider parser)
if (kind == "collider") {
    LevelColliderPlacement placement;
    std::string shapeToken, modeToken;
    if (!(stream >> shapeToken >> modeToken)) {
        throwParseError(path, lineNumber, "invalid collider record: missing shape/mode");
    }
    // ... parse shape and mode tokens, then parse positional args
    currentKind = CurrentEntityKind::Collider;
    currentIndex = data.colliders.size();
    data.colliders.push_back(std::move(placement));
    continue;
}
```

Apply the same pattern to unified `light`:
```cpp
if (kind == "light") {
    LevelLightPlacement placement;
    std::string typeToken;
    if (!(stream >> typeToken)) {
        throwParseError(path, lineNumber, "invalid light record: missing type");
    }
    // parse type token → placement.type
    // parse remaining positional args depending on type
    currentKind = CurrentEntityKind::Light;
    currentIndex = data.lights.size();
    data.lights.push_back(placement);
    continue;
}
```

### Verified Pattern: Inspector property row (existing shared utility)

```cpp
// Source: src/editor/ui/EditorInspectorPanel.cpp
bool renderInspectorPropertyRow(const char* label,
                                std::function<bool()> widget,
                                EditorInspectorFieldKind kind = EditorInspectorFieldKind::Default);
```

This is already a shared utility in `EditorPanelUtils.cpp`. All decomposed inspector functions call it.

---

## Runtime State Inventory

> This phase is a refactoring of source code and scene files — no runtime state, stored data, or external service configurations are affected.

| Category | Items Found | Action Required |
|----------|-------------|-----------------|
| Stored data | None — no databases or datastore keys reference inspector class names or parser keywords | None |
| Live service config | None — no external services involved | None |
| OS-registered state | None | None |
| Secrets/env vars | None | None |
| Build artifacts | CMakeLists.txt targets — new .cpp files must be added to the `editor` CMake target explicitly or via glob | Code edit required |

**Scene files are source-controlled assets, not runtime state.** The 48 light-keyword lines across 6 .scene files are migrated as code changes in git, not data migrations.

---

## Environment Availability

Step 2.6: SKIPPED — this phase is purely source code and scene file changes, no external tools or services beyond the project's own build system are required.

---

## Validation Architecture

> `nyquist_validation` is explicitly `false` in `.planning/config.json`. This section is SKIPPED.

---

## Open Questions

1. **PlacementBase embedding vs. out-parameter helpers**
   - What we know: D-03 says "All placement types embed it" — but the POD rule says no inheritance, and embedding as a named field breaks 50+ callsites.
   - What's unclear: Whether the intent is a named embedded struct (requiring `.base.` prefix) or inheritance (forbidden), or a different pattern entirely.
   - Recommendation: Use out-parameter helpers (`parseNodeMetadata(tokens, outNodeId, outParentNodeId)`) that write to the struct's direct fields. PlacementBase becomes a documentation concept, not an actual runtime type change. This satisfies D-03's deduplication intent without any callsite churn.

2. **CMakeLists.txt file enumeration method for editor target**
   - What we know: The editor target is built from `src/editor/`. The exact CMake source listing method is not confirmed from this research.
   - What's unclear: Whether new inspector .cpp files need manual addition or trigger a CMake re-run.
   - Recommendation: The planner should include a task to check and update CMakeLists.txt as the first step of the inspector decomposition plan.

3. **`editorSceneObjectKindName()` visitor vs. switch**
   - What we know: This function switches on `EditorSceneObjectKind` (an enum), not on the payload variant. `std::visit` is for the payload variant.
   - What's unclear: D-10 lists it as one of the 7 switch statements to replace, but it doesn't dispatch on the variant — it dispatches on an enum.
   - Recommendation: Keep it as a switch (or convert to a constexpr lookup table). It's not a variant visitor.

---

## Sources

### Primary (HIGH confidence)
- Direct inspection of `src/game/level/LevelDef.cpp` (1,490 lines) — confirmed structure, duplicate patterns, legacy blocks
- Direct inspection of `src/editor/scene/EditorSceneDocument.cpp` (924 lines) — confirmed 7 switch statements, `std::visit` usage
- Direct inspection of `src/editor/ui/EditorInspectorPanel.cpp` (1,516 lines) — confirmed monolith structure, section boundaries
- Direct inspection of `src/game/level/LevelDef.h` — confirmed placement struct fields
- Direct inspection of `src/editor/scene/EditorSceneDocument.h` — confirmed variant type, enum
- Direct inspection of `src/game/behavior/ActionTypes.h` — confirmed 14 action types and param variant
- Direct inspection of all 6 `.scene` files — confirmed light keyword occurrences (48 total)
- Direct inspection of `tests/game/test_level_roundtrip.cpp` — confirmed test patterns for round-trip validation
- Direct inspection of `.planning/config.json` — confirmed `nyquist_validation: false`
- Direct inspection of `CLAUDE.md` — confirmed POD rule, naming conventions, build conventions

### Secondary (MEDIUM confidence)
- C++17/20 `std::visit` with `if constexpr` — standard language feature, no external source needed
- `static_assert(sizeof(T) == 0, ...)` workaround for constexpr-if exhaustiveness — well-known C++ pattern documented in cppreference

---

## Metadata

**Confidence breakdown:**
- Parser consolidation plan: HIGH — based on direct code inspection, patterns are clear
- Light format migration: HIGH — 48 occurrences confirmed, template (collider unification) confirmed in same file
- Inspector decomposition: HIGH — section boundaries confirmed by reading the file
- Visitor pattern / std::visit recommendation: HIGH — existing usage confirmed in codebase

**Research date:** 2026-04-05
**Valid until:** 2026-05-05 (stable — no external dependencies, purely internal code)
