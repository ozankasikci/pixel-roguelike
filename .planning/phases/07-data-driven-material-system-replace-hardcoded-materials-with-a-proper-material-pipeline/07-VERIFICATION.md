---
phase: 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline
verified: 2026-03-30T21:00:00Z
status: passed
score: 19/19 must-haves verified
re_verification: null
gaps: []
human_verification:
  - test: "Open the level editor and confirm Materials appears as a category in the asset browser with all 13 materials listed"
    expected: "Materials folder node visible with stone_default, wood_default, brick_default, etc. listed as children"
    why_human: "Asset browser tree is filesystem-driven; listing requires UI exercise to confirm"
  - test: "Right-click a material in the asset browser and use New Material, Rename, and Delete, then verify changes appear in assets/materials/ on disk and in-memory list updates"
    expected: "New .material file created; rename moves file; delete removes file; list refreshes without restart"
    why_human: "Popup modal flow and file I/O side-effects require manual exercise"
  - test: "Select a material in the browser and confirm the inspector shows a preview sphere plus sliders (Roughness Bias, Specular Level, Emissive Strength) and Feature Flags checkboxes (Animated, Subsurface, Detail Brick/Wood/Stone/Floor)"
    expected: "Preview sphere renders the material. Checkboxes toggle correctly. Save writes the file."
    why_human: "ImGui rendering and sphere preview require visual confirmation"
  - test: "Edit a .material file externally (e.g. change roughness_bias in stone_default.material) while the editor is running, wait 1 second, and confirm the change is reflected in-renderer without restart"
    expected: "Hot-reload fires within 500ms; updated roughness visible on scene geometry"
    why_human: "Hot-reload is timing-dependent and requires live renderer observation"
---

# Phase 07: Data-Driven Material System Verification Report

**Phase Goal:** Replace hardcoded MaterialKind enum with data-driven material pipeline — auto-scanning, property-driven shaders, hot-reload, editor CRUD, validation
**Verified:** 2026-03-30T21:00:00Z
**Status:** passed
**Re-verification:** No — initial verification

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | MaterialKind.h is deleted — enum no longer exists | VERIFIED | `ls src/game/rendering/MaterialKind.h` returns "no such file"; zero grep matches for `MaterialKind` in src/, apps/, tests/ |
| 2 | MaterialDefinition has feature flag fields (animated, subsurface, detailBrick, detailWood, detailStone, detailFloor) and specularLevel | VERIFIED | MaterialDefinition.h lines 42–48: all optional fields present; ResolvedMaterialDefinition.h lines 69–75: all resolved fields with defaults |
| 3 | ContentRegistry auto-scans assets/materials/ recursively instead of hardcoded list | VERIFIED | ContentRegistry.cpp line 570: `fs::recursive_directory_iterator(directory,...)` confirmed |
| 4 | Duplicate material IDs are detected and logged as errors | VERIFIED | ContentRegistry.cpp line 581: `spdlog::error("Duplicate material id '{}' in '{}' — skipped...")` |
| 5 | All .material files contain roughness_bias and specular_level baked from shader defaults | VERIFIED | All 12 non-parent .material files contain `specular_level`; wood_default has `roughness_bias 0.74`; brick_wall_old inherits via parent brick_default |
| 6 | All .scene files use explicit material <id> syntax | VERIFIED | cathedral.scene: 75 mesh lines all use `material <id>`; silos_cloister.scene and warden_office.scene verified same |
| 7 | scene.frag uses feature flag uniforms (uMaterialAnimated, uMaterialBrickDetail, etc.) instead of uMaterialKind | VERIFIED | scene.frag lines 55–60: 6 uniform int declarations; zero occurrences of uMaterialKind |
| 8 | scene.vert uses uMaterialAnimated instead of uMaterialKind for flame animation | VERIFIED | scene.vert line 12: `uniform int uMaterialAnimated`; uMaterialKind absent |
| 9 | Renderer.h no longer includes game/rendering/MaterialKind.h (engine/game layering fix) | VERIFIED | Zero matches for `MaterialKind\|shadingModel` in Renderer.h |
| 10 | RenderMaterialData carries feature flags and specularLevel instead of shadingModel | VERIFIED | Renderer.h lines 15–21: specularLevel, animated, subsurface, detailBrick, detailWood, detailStone, detailFloor all present |
| 11 | Renderer.cpp binds all feature flag uniforms to shader | VERIFIED | Renderer.cpp lines 109–114: setInt calls for all 6 feature flag uniforms plus setFloat for specularLevel |
| 12 | MaterialTextureLibrary::resolve() takes only materialId, no MaterialKind parameter | VERIFIED | MaterialTextureLibrary.cpp line 184: `resolve(std::string_view materialId) const` |
| 13 | Missing materials return a magenta fallback | VERIFIED | MaterialTextureLibrary.cpp lines 175–188: makeMagentaFallback() returns baseColor (1,0,1); `spdlog::warn` on unknown materialId |
| 14 | File watcher polls assets/materials/ for changes every 500ms and reloads modified files | VERIFIED | ContentRegistry.h line 127: `kMaterialPollIntervalMs = 500`; ContentRegistry.cpp lines 636–661: polling with last_write_time comparison, calls `texLibrary.reloadMaterial()` |
| 15 | Hot-reload is wired into the editor main loop | VERIFIED | apps/level_editor/main.cpp line 413: `content.pollMaterialHotReload(materialTextures)` called per frame |
| 16 | Validation catches invalid roughness/metalness ranges, missing parents, and empty IDs | VERIFIED | ContentRegistry.cpp lines 621, 628: range checks `[0, 1]` for roughness_bias and metalness; line 614: missing parent error |
| 17 | Asset browser has a Materials category listing all discovered materials | VERIFIED | EditorAssetBrowser.cpp line 50: `.material` extension classified as `EditorAssetBrowserKind::Material`; sortedMaterialIds iterates content.materials() map; tree nodes rendered per file |
| 18 | Material inspector shows sliders for all float properties, checkboxes for feature flags, and preview sphere | VERIFIED | EditorInspectorPanel.cpp lines 246, 270–284, 536: Specular Level slider, Feature Flags collapsing header with 6 checkboxes, drawMaterialPreview call |
| 19 | Changes in inspector write back to .material files with validation; CRUD ops call addMaterial/removeMaterial | VERIFIED | EditorInspectorPanel.cpp lines 499–506: validateMaterialDefinition → saveMaterialDefinitionAsset → content.addMaterial(); EditorAssetBrowserPanel.cpp lines 668–760: three modal popups for New/Rename/Delete wired through AssetBrowserActionResult |

**Score:** 19/19 truths verified

### Required Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/game/rendering/MaterialDefinition.h` | Feature flag fields on MaterialDefinition and ResolvedMaterialDefinition; contains `std::optional<bool> animated` | VERIFIED | Lines 42–48 optional fields; lines 69–75 resolved defaults |
| `src/game/content/ContentRegistry.cpp` | Recursive directory scanner for materials; contains `recursive_directory_iterator` | VERIFIED | Line 570 confirmed |
| `assets/materials/wood_default.material` | Roughness and specular baked from shader defaults; contains `roughness_bias 0.74` | VERIFIED | Line 8: `roughness_bias 0.74`; line 12: `specular_level 0.24` |
| `assets/scenes/cathedral.scene` | Migrated scene file with material <id> syntax; contains `material stone_default` | VERIFIED | Line 14: `material stone_default` |
| `assets/shaders/game/scene.frag` | Property-driven uber-shader without MaterialKind branches; contains `uMaterialAnimated` | VERIFIED | Line 55: `uniform int uMaterialAnimated`; no MATERIAL_* constants or uMaterialKind |
| `assets/shaders/game/scene.vert` | Vertex shader with animated flag; contains `uMaterialAnimated` | VERIFIED | Line 12 confirmed |
| `src/engine/rendering/geometry/Renderer.h` | RenderMaterialData without MaterialKind dependency; contains `bool animated` | VERIFIED | Lines 15–21 confirmed; no MaterialKind include |
| `src/engine/rendering/geometry/Renderer.cpp` | Feature flag uniform binding; contains `setInt.*uMaterial` | VERIFIED | Lines 109–114 confirmed |
| `src/game/content/ContentRegistry.h` | File watcher polling API and CRUD mutation methods; contains `pollMaterialHotReload` | VERIFIED | Lines 92, 95, 98–99 confirmed |
| `src/game/content/ContentRegistry.cpp` | Polling using last_write_time, addMaterial, removeMaterial | VERIFIED | Lines 636–661, 668, 682 confirmed |
| `src/game/rendering/MaterialTextureLibrary.h` | Single-material cache invalidation API; contains `reloadMaterial` | VERIFIED | Line 25 confirmed |
| `src/editor/ui/EditorAssetBrowserPanel.cpp` | Materials category in asset browser with CRUD context menu; contains `Materials` | VERIFIED | Lines 381–393, 543–583, 668–760 confirmed |
| `src/editor/ui/EditorInspectorPanel.cpp` | Material inspector with feature flag checkboxes and preview sphere; contains feature flag UI | VERIFIED | Lines 270–284, 536 confirmed |

### Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `ContentRegistry.cpp` | `assets/materials/` | `recursive_directory_iterator` | WIRED | Line 570: iterator confirmed; duplicate detection at line 581 |
| `MaterialDefinition.cpp` | `assets/materials/*.material` | parsing feature flags | WIRED | Lines 327–347: all 6 feature flags parsed; lines 457–468: serialized |
| `Renderer.cpp` | `assets/shaders/game/scene.frag` | setInt uniform calls for feature flags | WIRED | Lines 109–114: 6 setInt + 1 setFloat calls match shader uniform declarations |
| `MaterialTextureLibrary.cpp` | `Renderer.h` | populates RenderMaterialData with feature flags | WIRED | Lines 210–215: all 6 flags copied from resolved definition into RenderMaterialData |
| `LevelEditorCore.cpp` (main.cpp) | `ContentRegistry.cpp` | `pollMaterialHotReload` called in editor main loop | WIRED | main.cpp line 413 confirmed |
| `ContentRegistry.cpp` | `MaterialTextureLibrary.cpp` | `reloadMaterial` called on file change | WIRED | ContentRegistry.cpp line 661: `texLibrary.reloadMaterial(updatedId, materials_)` |
| `EditorAssetBrowserPanel.cpp` | `ContentRegistry.cpp` | `addMaterial()`/`removeMaterial()` for CRUD | WIRED | AssetBrowserActionResult delegated to main.cpp lines 951, 959, 969 |
| `EditorInspectorPanel.cpp` | `MaterialDefinition.cpp` | `saveMaterialDefinitionAsset` for write-back | WIRED | Line 505 confirmed |

### Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `Renderer.cpp` | `material.animated`, `material.detailBrick`, etc. | `MaterialTextureLibrary::resolve()` → `RenderMaterialData` | Yes — copied from `ResolvedMaterialDefinition` fields populated by file parsing | FLOWING |
| `scene.frag` | `uMaterialAnimated`, `uMaterialBrickDetail`, etc. | `Renderer.cpp` setInt uniform calls | Yes — bound from `RenderMaterialData` feature flags per draw call | FLOWING |
| `ContentRegistry.cpp` | `materials_` map | `recursive_directory_iterator` scan of `assets/materials/` | Yes — 13 real .material files on disk | FLOWING |
| `EditorInspectorPanel.cpp` | `draft` MaterialDefinition | `asset.absolutePath` load → `loadMaterialDefinitionAsset()` | Yes — loaded from disk, saved back via `saveMaterialDefinitionAsset` | FLOWING |

### Behavioral Spot-Checks

Step 7b: SKIPPED — tests require compiled executables; build verification was confirmed by executor self-checks (all 3 targets built successfully per summaries, commits exist in git log). No runnable headless entry point for material rendering checks.

### Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|-------------|-------------|--------|----------|
| MAT-DATA-MODEL | 07-01 | Feature flags and specularLevel on MaterialDefinition | SATISFIED | MaterialDefinition.h fields confirmed |
| MAT-AUTO-SCAN | 07-01 | Recursive auto-scanner replacing hardcoded file list | SATISFIED | ContentRegistry.cpp line 570 |
| MAT-SCENE-MIGRATE | 07-01 | All .scene files use explicit material <id> syntax | SATISFIED | All 3 scene files verified |
| MAT-REMOVE-ENUM | 07-02 | MaterialKind enum deleted from codebase | SATISFIED | File absent; zero grep matches |
| MAT-UBER-SHADER | 07-02 | scene.frag/scene.vert use feature flag uniforms only | SATISFIED | Shaders confirmed; no uMaterialKind |
| MAT-FEATURE-FLAGS | 07-02 | Feature flags wired through Renderer to shader | SATISFIED | Full data-flow trace confirmed |
| MAT-MAGENTA-FALLBACK | 07-02 | Missing materials return visible magenta fallback | SATISFIED | MaterialTextureLibrary.cpp lines 175–188 |
| MAT-HOT-RELOAD | 07-04 | File watcher polls assets/materials/ and reloads on change | SATISFIED | ContentRegistry.cpp lines 635–661; 500ms interval |
| MAT-VALIDATION | 07-04 | Validation at load time for roughness/metalness ranges and empty IDs | SATISFIED | ContentRegistry.cpp lines 607–628 |
| MAT-MAGENTA-FALLBACK-VISUAL | 07-04 | Magenta fallback wired end-to-end | SATISFIED | Same as MAT-MAGENTA-FALLBACK — wired via resolve() |
| MAT-EDITOR-BROWSER | 07-03 | Asset browser has Materials category | SATISFIED | EditorAssetBrowser.cpp classifies .material files; browser renders material nodes |
| MAT-EDITOR-INSPECTOR | 07-03 | Inspector panel shows material properties with feature flags and preview sphere | SATISFIED | EditorInspectorPanel.cpp confirmed |
| MAT-EDITOR-CREATE | 07-03 | New/Rename/Delete CRUD operations for materials in editor | SATISFIED | Three modal popups wired with file I/O and ContentRegistry mutations |

**No orphaned requirements** — all 13 requirement IDs from plan frontmatter are accounted for.

### Anti-Patterns Found

No blockers or warnings found in any phase 07 modified files. No TODO/FIXME/HACK/PLACEHOLDER comments. No empty handlers. No hardcoded empty arrays/objects flowing to rendering.

**Notable observation:** `brick_wall_old.material` does not have its own `roughness_bias` or `specular_level` — it inherits from `parent brick_default`. This is intentional and correct; the parent carries the brick defaults. No bug.

**Pre-existing issue (not introduced by this phase):** `test_content_registry` has a pre-existing failure unrelated to phase 07 — test expects environment files (`neutral.environment`, `cloister_daylight.environment`, `game_ready_neutral.environment`) that do not exist on disk. Documented in `deferred-items.md`. This predates all phase 07 changes.

### Human Verification Required

#### 1. Materials category visible in asset browser

**Test:** Open the level editor; look for a "materials" folder node in the asset browser panel containing all 13 .material files
**Expected:** Materials folder visible with all 13 materials listed as leaf nodes; clicking one selects it and opens in inspector
**Why human:** Filesystem-driven tree rendering requires live UI exercise

#### 2. CRUD operations: New, Rename, Delete materials

**Test:** Right-click the materials folder node — verify "+" button appears; use it to create a new material. Right-click an existing material — verify New Material, Rename, Delete menu items appear; exercise each
**Expected:** New creates a file in assets/materials/; Rename renames the file and updates the in-memory list; Delete removes the file and removes from list
**Why human:** Modal popup flow and filesystem side-effects require manual exercise

#### 3. Inspector feature flags and preview sphere

**Test:** Select any material in the asset browser and inspect it
**Expected:** Inspector shows: (a) preview sphere at the top rendered with the material, (b) sliders for Roughness Bias, Specular Level, Emissive Strength, (c) Feature Flags collapsing header with Animated/Subsurface/Detail-Brick/Wood/Stone/Floor checkboxes
**Why human:** ImGui rendering requires visual confirmation; preview sphere rendering requires GPU

#### 4. Hot-reload: external edit reflects in renderer within 1 second

**Test:** With the editor open and a scene loaded, edit `assets/materials/stone_default.material` in a text editor (change roughness_bias value), save, and wait
**Expected:** Without restarting the editor, the rendered roughness of stone surfaces updates visibly within ~500ms
**Why human:** Timing-dependent; requires live renderer observation

### Gaps Summary

No gaps. All 19 observable truths verified against the codebase. All 13 requirement IDs satisfied. All key links confirmed wired. Data flows from disk through content registry → resolved definitions → texture library → RenderMaterialData → shader uniforms. The MaterialKind enum is fully eliminated with zero remaining references in src/, apps/, or tests/.

---

_Verified: 2026-03-30T21:00:00Z_
_Verifier: Claude (gsd-verifier)_
