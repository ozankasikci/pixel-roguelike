# Phase 11: Add Mesh Discoverability - Research

**Researched:** 2026-04-01
**Domain:** ImGui popup UI, editor toolbar patterns, existing placement system wiring
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- **D-01:** "Add Mesh" button lives in the top toolbar, alongside existing transform tool buttons — always visible, one click away
- **D-02:** Clicking the button opens a popup mesh picker (not a dropdown, not immediate placement mode)
- **D-03:** Simple scrollable name list of all available meshes — no thumbnails, no grid, no 3D previews
- **D-04:** Text filter input at the top of the picker popup — filters the mesh list as user types
- **D-05:** After selecting a mesh from the picker, enter click-to-place mode using the existing `beginPlacement(placementState, EditorPlacementKind::Mesh, ...)` system
- **D-06:** Use the currently selected material (`ui.selectedMaterialId`) for the placed mesh — matches existing Place Mesh behavior

### Claude's Discretion
- Exact ImGui window/popup style for the picker (modal vs non-modal popup)
- Button label text and icon (if any)
- How mesh names are derived from asset paths (filename, meshId, or display name)
- Whether the picker popup auto-closes on selection or stays open for multi-place

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DISC-01 | Add Mesh picker button to add meshes to the scene | Button placement in viewport toolbar, ImGui popup picker, `meshIds` vector from `sortedMeshNames()`, `beginPlacement()` call pattern — all verified in codebase |
</phase_requirements>

## Summary

Phase 11 is a focused UI addition to the level editor: a discoverable "Add Mesh" button in the viewport toolbar that opens an ImGui popup containing a text-filtered, scrollable list of all loaded mesh IDs. When the user selects a mesh name, the picker calls `beginPlacement()` to enter click-to-place mode (the same path as the existing "Place Mesh" menu item). No new rendering infrastructure, no new C++ types, and no new data files are needed.

The codebase already has everything required. `meshIds` (a `std::vector<std::string>` populated by `sortedMeshNames(previewWorld.meshLibrary())`) is already in scope in `main.cpp`. `beginPlacement()` is already declared in `LevelEditorUi.h` and called at line 489. The viewport toolbar pattern (ImGui::Button + ImGui::OpenPopup + ImGui::BeginPopup) is already used for "Add", "Snap Settings", and "Helpers" buttons at lines 1168–1217. This phase wires these existing pieces together with a new popup and a filter buffer.

The one gap in the existing system is auto-selection of the placed mesh. The success criteria require "selects it immediately," but the current `commitPlacement` path discards the returned node ID. The plan must address this: the cleanest approach is to have `commitPlacement` return `std::optional<std::uint64_t>` so callers can select the placed object.

**Primary recommendation:** Add the "Add Mesh" button immediately before the "W/E/R" transform tool buttons in the viewport toolbar (lines 1181–1186 of `main.cpp`). Use `ImGui::BeginPopup` (non-modal) with a `char filterBuffer[128]` local to main.cpp, populated via `ImGui::InputText`. Auto-focus the filter on popup open via `ImGui::SetKeyboardFocusHere()`. Close popup on selection. Update `selectedIds` after placement by extending `commitPlacement` to return the new object's ID.

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Dear ImGui | v1.92.6 (project standard) | All editor UI — popups, buttons, input text, scrolling lists | Already integrated via GLFW+OpenGL backend; all editor panels use it |
| C++20 STL | C++20 | `std::string`, `std::vector`, case-insensitive filtering | Project standard per CLAUDE.md |

No new libraries are needed. This phase is pure ImGui wiring in existing C++ files.

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| spdlog | v1.x | Debug logging if picker fails to find meshes | Optional — only if diagnosing edge cases |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `ImGui::BeginPopup` (non-modal) | `ImGui::BeginPopupModal` | Modal blocks all other input — better for destructive confirmations, not for a picker that the user may want to cancel by clicking elsewhere. Non-modal matches the existing "Add" popup and "Snap Settings" popup patterns. |
| `meshIds` from `sortedMeshNames()` | `buildProjectAssetBrowserTree()` filtered by `Mesh` kind | `meshIds` is already scoped in main.cpp and stays in sync with the mesh library. Asset browser tree is a heavier call (filesystem scan) and returns raw node trees needing flattening. Use `meshIds` — it is the authoritative list for the placement system. |

## Architecture Patterns

### Recommended Project Structure

No new files needed. All changes go into two existing files:
- `apps/level_editor/main.cpp` — button + popup UI, filter buffer, placement call, auto-select
- `src/editor/viewport/EditorViewportInteraction.h/.cpp` — extend `commitPlacement` return type

### Pattern 1: Toolbar Button + Non-Modal Popup
**What:** Existing pattern used for "Add", "Snap Settings", and "Helpers" in the viewport toolbar.
**When to use:** Any toolbar action that opens a transient picker or settings panel.
**Example (from `main.cpp` lines 1168–1174):**
```cpp
// Source: apps/level_editor/main.cpp line 1168
if (ImGui::Button("Add")) {
    ImGui::OpenPopup("ViewportCreateMenu");
}
if (ImGui::BeginPopup("ViewportCreateMenu")) {
    renderCreateCommands();
    ImGui::EndPopup();
}
```

The "Add Mesh" picker follows exactly this structure with an additional filter input and scrollable list body.

### Pattern 2: Auto-Focus Filter on Popup Open
**What:** `ImGui::IsWindowAppearing()` + `ImGui::SetKeyboardFocusHere()` to focus the filter input the first frame the popup appears. Used in the "New Scene" and "Save Layout As" modals.
**Example (from `main.cpp` line 1055):**
```cpp
// Source: apps/level_editor/main.cpp line 1055
if (ImGui::BeginPopupModal("NewScenePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::InputText("Name", newSceneNameBuffer, sizeof(newSceneNameBuffer), ...);
```

For a non-modal `BeginPopup`, the same `IsWindowAppearing()` guard works.

### Pattern 3: beginPlacement Call
**What:** Sets `placementState` to Mesh kind with a specific meshId and materialId, entering click-to-place mode.
**Example (from `main.cpp` line 488):**
```cpp
// Source: apps/level_editor/main.cpp line 488
if (ImGui::MenuItem("Place Mesh")) {
    beginPlacement(placementState, EditorPlacementKind::Mesh, ui.selectedMeshId, ui.selectedMaterialId);
}
```

The "Add Mesh" picker uses the same call, replacing `ui.selectedMeshId` with the user's picker selection.

### Pattern 4: Auto-Selection After Placement
**What:** `document.addMesh()` returns `std::uint64_t` (the new object's ID). Currently `commitPlacement` calls it and discards the return value. To auto-select the placed mesh, `commitPlacement` must be extended to return the ID.
**Current signature:**
```cpp
// Source: src/editor/viewport/EditorViewportInteraction.h line 66
void commitPlacement(EditorSceneDocument& document,
                     const EditorPlacementState& state,
                     const glm::vec3& position,
                     const ContentRegistry& content,
                     const EditorCamera& camera);
```
**Required change:** Return `std::optional<std::uint64_t>` — present when a Mesh was placed, empty for lights/colliders/etc. Callers that don't need the ID ignore it.

After placement in the click-to-place handler (main.cpp ~line 1620):
```cpp
// After commitPlacement:
const auto placedId = commitPlacement(document, placementState, *placementPoint, content, editCamera);
if (placedId.has_value()) {
    selectedIds = { *placedId };
    ui.inspectorContext = EditorInspectorContext::SceneSelection;
}
```

### Anti-Patterns to Avoid
- **Using `buildProjectAssetBrowserTree()` for the picker list:** It's a filesystem scan — slower than `meshIds`, requires recursive tree flattening, and introduces a second source of truth. Use `meshIds` (the MeshLibrary-backed sorted list).
- **Using BeginPopupModal for the picker:** Modal blocks all editor input. Non-modal lets the user click elsewhere to dismiss, matching the existing "Add" popup behavior.
- **Filtering on Enter only:** The CONTEXT.md specifies "filter on every keystroke." Use `ImGui::InputText` without `ImGuiInputTextFlags_EnterReturnsTrue` — it filters live on every character change.
- **Filter buffer in EditorUiState:** The filter string is transient (reset to empty each time the popup opens). Keep it as a `static char` or local lambda-captured buffer inside `main.cpp` — not persisted in `EditorUiState`.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Scrollable list with selection | Custom scroll widget | `ImGui::BeginChild` + `ImGui::Selectable` per item | ImGui handles scroll position, keyboard nav, hover highlight automatically |
| Case-insensitive substring filter | Custom string search | `std::string::find` after `std::tolower` transform on both strings | Simple, zero-dependency, good enough for < 100 mesh names |
| Mesh list data source | Filesystem scan at popup-open | `meshIds` vector already in scope in main.cpp | `sortedMeshNames(previewWorld.meshLibrary())` is updated when content reloads — already authoritative |
| Auto-close after selection | Manual popup state flag | `ImGui::CloseCurrentPopup()` | ImGui built-in; call it immediately after `beginPlacement()` in the Selectable click handler |

**Key insight:** ImGui popup + Selectable list with a filter input is a ~30-line pattern. All edge cases (scroll, keyboard nav, dismiss on click-outside) are handled by ImGui's popup stack.

## Common Pitfalls

### Pitfall 1: Popup ID Collision
**What goes wrong:** Using a generic name like `"MeshPicker"` that conflicts with another popup in the same ImGui context.
**Why it happens:** ImGui popup IDs are global within the context. Two `OpenPopup("MeshPicker")` calls in different code paths can conflict.
**How to avoid:** Use a scoped name like `"AddMeshPicker"` — unique within the editor.
**Warning signs:** Popup opens unexpectedly or fails to open after the first use.

### Pitfall 2: Filter Buffer Not Cleared on Open
**What goes wrong:** The filter text from a previous popup session persists, hiding meshes on re-open.
**Why it happens:** The char buffer is not cleared when `OpenPopup` is called.
**How to avoid:** Clear the filter buffer immediately before `ImGui::OpenPopup("AddMeshPicker")`:
```cpp
filterBuffer[0] = '\0';
ImGui::OpenPopup("AddMeshPicker");
```
**Warning signs:** Opening the picker a second time shows a pre-filtered list.

### Pitfall 3: Popup Not Opening Due to Call Ordering
**What goes wrong:** `ImGui::OpenPopup` is called but the popup never appears.
**Why it happens:** `OpenPopup` must be called in the same ImGui window as `BeginPopup`. If the button is rendered in one window but `BeginPopup` is called in another, the popup doesn't appear.
**How to avoid:** Keep `OpenPopup` and `BeginPopup` calls in the same window rendering block — as done by all existing toolbar popups in `main.cpp`.
**Warning signs:** `BeginPopup` always returns false.

### Pitfall 4: Auto-Select Breaks Undo
**What goes wrong:** Updating `selectedIds` after `commitPlacement` without respecting undo state — undo removes the mesh but `selectedIds` still holds its ID, causing a stale inspector.
**Why it happens:** `selectedIds` is not part of the command stack state; it's managed independently.
**How to avoid:** The existing `pruneSelection(document, selectedIds)` call (line 1761, 1773, 1832) already handles this — it clears stale IDs after undo/redo. No extra work needed for undo correctness. Just set `selectedIds = { placedId }` after placement; undo will prune it via the existing mechanism.
**Warning signs:** Inspector shows old object properties after undo.

### Pitfall 5: meshIds Is Stale After Asset Import
**What goes wrong:** User imports a new mesh via the Asset Browser, then opens the "Add Mesh" picker — new mesh not visible.
**Why it happens:** `meshIds` is rebuilt only on content reload (line 946: `meshIds = sortedMeshNames(...)`). The picker reads `meshIds` which may not include just-imported meshes.
**How to avoid:** This is pre-existing behavior: the same `meshIds` vector is used by the Inspector mesh dropdown. The fix (triggering a content reload after import) is out of scope for this phase. Document as known limitation.
**Warning signs:** Newly imported .glb not showing in picker until editor restart.

## Code Examples

Verified patterns from the existing codebase:

### Full "Add Mesh" Button + Picker Popup (recommended implementation)
```cpp
// In the viewport toolbar block (main.cpp, after the "Add" button at line 1168):
// Source: pattern from existing toolbar popups + BeginPopup pattern

static char addMeshFilter[128] = {};

if (ImGui::Button("Add Mesh")) {
    addMeshFilter[0] = '\0';  // clear filter on open
    ImGui::OpenPopup("AddMeshPicker");
}
ImGui::SetNextWindowSize(ImVec2(300.0f, 400.0f), ImGuiCond_Appearing);
if (ImGui::BeginPopup("AddMeshPicker")) {
    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##addmesh_filter", addMeshFilter, sizeof(addMeshFilter));
    ImGui::Separator();
    const std::string filterStr = addMeshFilter;
    if (ImGui::BeginChild("##addmesh_list", ImVec2(0.0f, 0.0f), false)) {
        for (const auto& id : meshIds) {
            if (!filterStr.empty()) {
                // Case-insensitive substring match
                std::string lower_id = id;
                std::string lower_filter = filterStr;
                std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
                std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);
                if (lower_id.find(lower_filter) == std::string::npos) {
                    continue;
                }
            }
            if (ImGui::Selectable(id.c_str())) {
                ui.selectedMeshId = id;
                beginPlacement(placementState, EditorPlacementKind::Mesh, id, ui.selectedMaterialId);
                ImGui::CloseCurrentPopup();
            }
        }
    }
    ImGui::EndChild();
    ImGui::EndPopup();
}
```

### commitPlacement Return Type Extension
```cpp
// src/editor/viewport/EditorViewportInteraction.h — change signature:
std::optional<std::uint64_t> commitPlacement(EditorSceneDocument& document,
                                              const EditorPlacementState& state,
                                              const glm::vec3& position,
                                              const ContentRegistry& content,
                                              const EditorCamera& camera);

// src/editor/viewport/EditorViewportInteraction.cpp — return the ID for Mesh kind:
case EditorPlacementKind::Mesh: {
    LevelMeshPlacement placement;
    placement.meshId = state.meshId;
    placement.position = position;
    placement.scale = glm::vec3(1.0f);
    placement.rotation = glm::vec3(0.0f);
    placement.materialId = state.materialId;
    return document.addMesh(placement);
}
// All other cases return std::nullopt
```

### Auto-Select After Placement (main.cpp click-to-place handler)
```cpp
// Replace the commitPlacement call at line 1620:
const auto placedId = commitPlacement(document, placementState, *placementPoint, content, editCamera);
commandStack.pushDocumentStateCommand("Place Object", beforeState, document.captureState(), document);
placementState.clear();
selectionPicker.clear();
if (placedId.has_value()) {
    selectedIds = { *placedId };
    ui.inspectorContext = EditorInspectorContext::SceneSelection;
}
previewDirty = true;
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| "Place Mesh" hidden in right-click "Create" menu | "Add Mesh" always-visible toolbar button | Phase 11 | Discoverable without any prior knowledge |
| Mesh picker via Inspector dropdown only | Dedicated picker popup with filter | Phase 11 | Single-step mesh selection vs. multi-step Inspector workflow |

**No deprecated patterns in this phase.** All APIs used are current and in production use in the codebase.

## Open Questions

1. **Single-place vs. multi-place after picker selection**
   - What we know: D-02/D-05 specify entering click-to-place mode; D-05 uses `beginPlacement()` which allows multiple placements until the user presses Esc.
   - What's unclear: Should the popup auto-close after one placement (user can re-open for each mesh), or stay open to place multiple different meshes in sequence?
   - Recommendation: Auto-close on selection (call `ImGui::CloseCurrentPopup()` immediately after `beginPlacement()`). This is simpler and aligns with the "picker as a gateway" model — the user clicks "Add Mesh" again if they want a different mesh. Left to Claude's Discretion per CONTEXT.md.

2. **"Add Mesh" button position in toolbar**
   - What we know: D-01 says "alongside existing transform tool buttons." The current toolbar has "Add" popup, then "Cancel Placement" (conditional), then "W / E / R" tool buttons.
   - What's unclear: Should "Add Mesh" replace the existing generic "Add" popup, or sit beside it?
   - Recommendation: Place "Add Mesh" as a separate button immediately before or after "Add" — do not remove the "Add" popup (it contains lights, colliders, etc.). This keeps both discoverable without losing any existing functionality.

## Environment Availability

Step 2.6: SKIPPED (no external dependencies — this phase is C++ code changes in the editor, all dependencies are already part of the build).

## Sources

### Primary (HIGH confidence)
- `apps/level_editor/main.cpp` — toolbar structure (lines 1153–1224), existing popup patterns (ViewportCreateMenu, ViewportSnapSettings), renderCreateCommands lambda (lines 487–514), meshIds initialization (line 335)
- `src/editor/viewport/EditorViewportInteraction.h/.cpp` — `commitPlacement` signature and body, `beginPlacement` declaration
- `src/editor/ui/LevelEditorUi.h` — `EditorUiState`, `EditorPlacementState`, `EditorPlacementKind`, `beginPlacement` declaration
- `src/editor/assets/EditorAssetBrowser.h/.cpp` — `buildProjectAssetBrowserTree`, `editorMeshIdForAssetPath`
- `src/editor/core/LevelEditorCore.cpp` — `sortedMeshNames` implementation (line 165)
- `src/editor/scene/EditorSceneDocument.h/.cpp` — `addMesh` return type `std::uint64_t` (line 70/187)

### Secondary (MEDIUM confidence)
- ImGui `BeginPopup` / `IsWindowAppearing` / `SetKeyboardFocusHere` patterns — verified against existing modal usages in main.cpp (lines 1054–1056, 802–806)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new libraries; all code paths verified in existing codebase
- Architecture: HIGH — exact insertion points identified with line numbers; patterns verified against existing toolbar code
- Pitfalls: HIGH — derived from reading actual code paths, not speculation

**Research date:** 2026-04-01
**Valid until:** Stable (no external dependency changes; valid until engine architecture changes)
