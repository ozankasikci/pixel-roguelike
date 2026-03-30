# Phase 7: Data-Driven Material System - Research

**Researched:** 2026-03-30
**Domain:** C++ material pipeline refactor, GLSL uber-shader, file system scanning, ImGui editor tooling
**Confidence:** HIGH (entire codebase inspected; all relevant source files read)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**D-01:** Auto-scan `assets/materials/**/*.material` recursively at startup. No manifest file — drop a file in, it's available.
**D-02:** Subdirectories supported for organization (e.g. `materials/stone/`, `materials/wood/`).
**D-03:** Two-pass loading: first pass scans all files into memory, second pass resolves inheritance. Load order doesn't matter.
**D-04:** Duplicate material IDs (same ID in two files) are an error at load time. First-loaded wins, duplicate is skipped with error log.
**D-05:** Missing material fallback: bright magenta flat color (no textures, no lighting response) + warning log. Classic engine convention.
**D-06:** Migrate ALL legacy MaterialKind-based references in .scene files to explicit `material <id>` syntax. Clean break, no legacy code path. One-time migration script.
**D-07:** Remove the MaterialKind enum entirely (both C++ enum and GLSL `const int MATERIAL_*` constants). Material behavior is fully defined by properties (roughness, metalness, etc.). One PBR uber-shader, no type branching.
**D-08:** Special behaviors (wax flame flicker, moss subsurface, etc.) implemented via boolean feature flags in .material files (e.g. `subsurface true`, `animated true`, `emissive_flicker true`). Shader checks flags, not material type.
**D-09:** Procedural textures remain as `procedural_source` property in .material files. Generator name is a string looked up in a registry (existing pattern, now canonical).
**D-10:** Full material editor inspector panel with sliders, color pickers, and all material properties editable. Changes write back to .material files.
**D-11:** Preview sphere rendered inline in the material inspector panel using existing EditorAssetPreviewRenderer.
**D-12:** Create new materials from template/parent: right-click in asset browser > New Material, pick a parent (or blank), creates .material file with inherited defaults.
**D-13:** Asset browser gets a "Materials" category listing all discovered materials. Click to inspect, right-click for create/rename/delete. Follows the pattern Phase 6 establishes for scenes.
**D-14:** File watcher monitors `assets/materials/` for changes. When a file changes, re-parse and update the material in-memory. Procedural textures regenerate if parameters changed.
**D-15:** Validation at both load time AND on save from editor. Editor shows validation errors inline.
**D-16:** Broken inheritance chains (parent doesn't exist): material is skipped entirely, error logged with file path and missing parent name. Meshes using it get the magenta fallback.

### Claude's Discretion

- .material file format evolution (keep current key-value text format or switch to JSON/TOML)
- File watcher implementation details (polling interval, platform API choice)
- Exact validation error presentation in editor UI
- Material inspector layout and organization of property groups
- Migration script implementation for .scene files
- Procedural texture generator registry implementation details

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

---

## Summary

Phase 7 is a refactor-plus-feature phase centered on three distinct problems: (1) replacing a hardcoded material list with a filesystem scan, (2) removing the `MaterialKind` enum in favor of property-driven PBR shading with GLSL feature flags, and (3) building editor tooling (browser, inspector, file watcher) that mirrors what Phase 6 establishes for scenes.

The existing codebase is well-structured for this work. `MaterialDefinition` already handles inheritance resolution via a two-pass recursive algorithm. `loadMaterialDefinitionAsset()` already parses the text key-value format. `MaterialTextureLibrary::resolve()` already assembles `RenderMaterialData` from resolved definitions. The biggest surgical work is: (a) removing the `shading_model` field from `MaterialDefinition` and replacing it with feature-flag booleans, (b) gutting the `applyMaterialDetail` / `materialRoughness` / `materialSpecularLevel` / `materialLightTintResponse` type-switch branches in `scene.frag` and replacing them with uniform flags, and (c) threading those flags through `RenderMaterialData` → `Renderer::drawScene()` → GLSL.

The scene file migration is mechanical: three `.scene` files exist, all currently using legacy `stone`/`wood`/`floor` tokens on most mesh lines. A migration script scans for those tokens and replaces them with `material <default-id>` equivalents using the same mapping already in `defaultMaterialIdForKind()`.

**Primary recommendation:** Work in phases — material data model and scan first, then shader refactor (most impactful, most risk), then editor tooling. Do NOT attempt both the shader overhaul and the editor in a single plan.

---

## Standard Stack

### Core (already in project — no new dependencies)

| Component | Location | Purpose | Notes |
|-----------|----------|---------|-------|
| `std::filesystem` (C++20) | Already used in `ContentRegistry.cpp` (`sortedDefinitionFiles`) | Directory scanning | `fs::recursive_directory_iterator` for subdirectory support |
| spdlog | Already linked | Duplicate/missing material warnings | `spdlog::warn(...)` with file path and ID |
| Dear ImGui | Already linked | Material inspector, browser category | All existing editor panels use it |
| `EditorAssetPreviewRenderer` | `src/editor/render/` | Preview sphere in inspector | Already has `drawMaterialPreview(RenderMaterialData, bgColor, suffix)` |
| OpenGL 4.1 / GLSL 410 | Engine constraint | Shader uniforms for feature flags | No new GLSL features needed |

### New Additions (minimal)

| Component | Purpose | Notes |
|-----------|---------|-------|
| File watcher (polling-based) | Hot-reload on `assets/materials/` change | `std::filesystem::last_write_time` polled every N ms — no platform API needed; simple and portable |

**No new library dependencies.** Everything needed is already in the project.

---

## Architecture Patterns

### Recommended Project Structure (no structural changes needed)

The existing layout already works. Materials stay in `assets/materials/` (subdirs now allowed). No new source directories needed.

```
assets/materials/
├── masonry_base.material      # root material (no parent)
├── stone/
│   ├── stone_default.material
│   └── cloister_stone.material
├── brick/
│   ├── brick_default.material
│   └── brick_wall_old.material
... (or keep flat — either is valid)
src/game/rendering/
├── MaterialDefinition.h       # MODIFIED: remove MaterialKind field, add feature flags
├── MaterialDefinition.cpp     # MODIFIED: parse/serialize feature flags
├── MaterialTextureLibrary.h   # MODIFIED: remove MaterialKind param from resolve()
├── MaterialTextureLibrary.cpp # MODIFIED: resolve() without legacyKind
src/game/content/
├── ContentRegistry.h          # MODIFIED: scanner replaces hardcoded list
├── ContentRegistry.cpp        # MODIFIED: recursive scan, duplicate detection, magenta fallback
src/engine/rendering/geometry/
├── Renderer.h                 # MODIFIED: RenderMaterialData removes MaterialKind field
├── Renderer.cpp               # MODIFIED: transmit feature flags instead of uMaterialKind
assets/shaders/game/
├── scene.frag                 # MAJOR REFACTOR: uber-shader, feature flags, no kind branches
├── scene.vert                 # MINOR: remove uMaterialKind uniform, add feature flag uniforms
src/editor/ui/
├── EditorAssetBrowserPanel.cpp  # ADD: Materials category (mirrors Scenes category)
├── EditorInspectorPanel.cpp     # REFACTOR: remove shading model combo, add feature flag checkboxes
```

### Pattern 1: Recursive Directory Scanner (replaces hardcoded list)

The existing `sortedDefinitionFiles()` in `ContentRegistry.cpp` already does a flat scan using `fs::directory_iterator`. Replace it with `fs::recursive_directory_iterator` for subdirectory support.

```cpp
// Source: std::filesystem (C++20), already used in ContentRegistry.cpp
void ContentRegistry::loadMaterialsFromDirectory(const std::string& relativeDirectory) {
    namespace fs = std::filesystem;
    const fs::path directory = resolveProjectPath(relativeDirectory);
    if (!fs::exists(directory)) return;

    // First pass: scan all files
    for (const auto& entry : fs::recursive_directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".material") continue;

        try {
            auto material = loadMaterialDefinitionAsset(entry.path().string());
            if (materials_.count(material.id)) {
                spdlog::warn("Duplicate material id '{}' in '{}' — skipped",
                             material.id, entry.path().string());
                continue;
            }
            materials_.emplace(material.id, std::move(material));
        } catch (const std::exception& e) {
            spdlog::error("Failed to load material '{}': {}", entry.path().string(), e.what());
        }
    }
    // Second pass: validate inheritance (parents exist)
    for (auto& [id, def] : materials_) {
        if (def.parent.has_value() && !materials_.count(*def.parent)) {
            spdlog::error("Material '{}' has missing parent '{}' — will use magenta fallback",
                          id, *def.parent);
            // Flag for fallback; don't erase — keep the definition for error display
        }
    }
}
```

**Confidence:** HIGH — `std::filesystem::recursive_directory_iterator` is standard C++17/20, confirmed working in this codebase.

### Pattern 2: Feature Flags Replace MaterialKind Enum

**Old approach:** `ResolvedMaterialDefinition::shadingModel = MaterialKind::Brick` → shader branches on `uMaterialKind == MATERIAL_BRICK`.

**New approach:** Explicit boolean properties on `MaterialDefinition` and `ResolvedMaterialDefinition`, passed as integer uniforms to the shader.

```cpp
// In MaterialDefinition.h — replace optional<MaterialKind> shadingModel with:
struct MaterialDefinition {
    // ... existing fields ...
    std::optional<bool> animated;        // wax flame, emissive flicker
    std::optional<bool> subsurface;      // moss SSS (future)
    std::optional<bool> useBrickDetail;  // brick procedural geometry
    std::optional<bool> useWoodDetail;   // plank seam/grain overlay
    std::optional<bool> useStoneDetail;  // block course overlay
    std::optional<bool> useFloorDetail;  // slab seam overlay
    // NOTE: shading_model field removed entirely
};

struct ResolvedMaterialDefinition {
    // ... existing fields ...
    bool animated = false;
    bool subsurface = false;
    bool useBrickDetail = false;
    bool useWoodDetail = false;
    bool useStoneDetail = false;
    bool useFloorDetail = false;
};
```

In `RenderMaterialData` (Renderer.h):
```cpp
struct RenderMaterialData {
    std::string id;
    // MaterialKind shadingModel REMOVED
    glm::vec3 baseColor{1.0f};
    bool useMaterialMaps = false;
    bool useProceduralDetail = false;
    bool animated = false;
    bool subsurface = false;
    bool useBrickDetail = false;
    bool useWoodDetail = false;
    bool useStoneDetail = false;
    bool useFloorDetail = false;
    // ... rest unchanged ...
};
```

In `Renderer.cpp` drawScene loop:
```cpp
shader_->setInt("uMaterialAnimated",    material.animated ? 1 : 0);
shader_->setInt("uMaterialSubsurface",  material.subsurface ? 1 : 0);
shader_->setInt("uMaterialBrickDetail", material.useBrickDetail ? 1 : 0);
shader_->setInt("uMaterialWoodDetail",  material.useWoodDetail ? 1 : 0);
shader_->setInt("uMaterialStoneDetail", material.useStoneDetail ? 1 : 0);
shader_->setInt("uMaterialFloorDetail", material.useFloorDetail ? 1 : 0);
// Remove: shader_->setInt("uMaterialKind", ...)
```

### Pattern 3: Uber-Shader Refactor (scene.frag / scene.vert)

The shader currently has these `uMaterialKind`-dependent blocks to convert:

| Location in scene.frag | What it does | Replacement |
|-----------------------|--------------|-------------|
| `applyMaterialDetail()` (lines 546-569) | Routes to per-kind detail function | Check individual feature flags |
| `materialRoughness()` (lines 571-594) | Returns kind-specific roughness | Remove — roughness now fully from `uMaterialRoughnessScale` + `uMaterialRoughnessBias` + texture |
| `materialSpecularLevel()` (lines 596-626) | Returns kind-specific specular | Remove — specular level encoded as `lightTintResponse` property |
| `materialLightTintResponse()` (lines 628-651) | Returns kind-specific tint | Remove — already a uniform `uMaterialLightTintResponse` |
| `flameMask()` (lines 252-253) | Checks `uMaterialKind == MATERIAL_WAX` | Check `uMaterialAnimated` flag instead |
| `main()` lines 987-993 | `uMaterialKind == MATERIAL_BRICK` normals | Check `uMaterialBrickDetail` flag |
| `main()` lines 1007, 1024 | `uMaterialKind != MATERIAL_BRICK` conditions | Check `!uMaterialBrickDetail` |

In `scene.vert`:
- Remove `uniform int uMaterialKind;` and `const int MATERIAL_WAX = 3;`
- Add `uniform int uMaterialAnimated;`
- Replace `uMaterialKind == MATERIAL_WAX` check with `uMaterialAnimated != 0`

**The roughness/specular consolidation:** Currently `materialRoughness()` ignores `uMaterialRoughnessScale`/`uMaterialRoughnessBias` in a way that could produce inconsistent results. Post-refactor: roughness is computed purely from material map (or fallback), multiplied by scale and offset by bias. The existing default values in each `.material` file should encode what was previously hardcoded in the shader. Migration: read the old `materialRoughness()` return values and bake them into `.material` files as `roughness_bias` values (e.g., wood=0.74 → `roughness_bias 0.74` on the root material when no texture is present).

### Pattern 4: Magenta Fallback Material

Create a single statically-defined fallback `RenderMaterialData` in `MaterialTextureLibrary` (or `ContentRegistry`) returned whenever a material ID is not found:

```cpp
static RenderMaterialData makeMagentaFallback() {
    RenderMaterialData m;
    m.id = "__missing__";
    m.baseColor = {1.0f, 0.0f, 1.0f};  // bright magenta
    m.useMaterialMaps = false;
    m.useProceduralDetail = false;
    // all feature flags false — flat unlit-ish render
    return m;
}
```

The `definitionFor()` method in `MaterialTextureLibrary` currently throws when the stone fallback is missing. Replace with magenta return.

### Pattern 5: File Watcher (Polling)

Use `std::filesystem::last_write_time` polled in the editor main loop or on a timer. This avoids platform-specific APIs (FSEvents/inotify/ReadDirectoryChangesW) while being perfectly adequate for a development-time hot-reload.

```cpp
// Stored per-material at load time
std::unordered_map<std::string, fs::file_time_type> materialFileTimes_;

// Checked each editor frame (or every 1000ms)
void ContentRegistry::pollMaterialHotReload() {
    for (auto& [path, knownTime] : materialFileTimes_) {
        auto currentTime = fs::last_write_time(path);
        if (currentTime != knownTime) {
            knownTime = currentTime;
            reloadMaterialFile(path);
        }
    }
}
```

**Note:** Hot-reload invalidates `MaterialTextureLibrary` cache entries for the changed material (clear its entry in `resolvedDefinitions_` and `materials_`, trigger procedural texture regeneration). The file path → material ID mapping requires storing it at scan time.

### Pattern 6: Asset Browser Materials Category

The Phase 6 scenes category is the exact structural template. The `EditorAssetBrowserNode` and `buildProjectAssetBrowserTree()` in `EditorAssetBrowser.cpp` already handle the display logic. Add a node kind (`AssetKind::Material`) and a scan of `assets/materials/` (recursive) to populate material nodes.

Right-click context menu actions: **New Material**, **Rename**, **Delete** — mirrors scenes pattern precisely.

### Anti-Patterns to Avoid

- **Don't keep MaterialKind as a compatibility shim:** Decision D-07 is a clean break. Leaving the enum as a "deprecated" alias creates ambiguity for future code. Delete the header.
- **Don't infer feature flags from material ID prefixes:** (`brick_` → useBrickDetail). Always read from the `.material` file explicitly. IDs are user-visible strings, not type encodings.
- **Don't use the existing `shading_model` file key as a migration path:** Remove parsing for `shading_model` token entirely once migration is done. The token has no meaning in the new system.
- **Don't regenerate procedural textures on every hot-reload:** Only invalidate textures whose `proceduralSource` or related parameters actually changed. Hash the parameters the same way `textureKeyFor()` already does.
- **Don't store roughness defaults only in the shader:** The `materialRoughness()` switch encodes per-kind defaults (wood=0.74, metal=0.34, etc.) that are currently NOT in .material files. These values must be migrated into the material files (as `roughness_bias` or `roughness_scale`) before the shader switch is removed.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Recursive directory scan | Custom `walk()` function | `std::filesystem::recursive_directory_iterator` | Already used in project, handles symlinks/errors, platform-portable |
| Text file watching | Platform-specific API (FSEvents, inotify) | `std::filesystem::last_write_time` polling | Adequate for dev-time hot-reload, zero dependencies, already available |
| Inheritance cycle detection | Custom visited-set algorithm | Existing `resolveMaterialDefinitionRecursive()` already has `visiting` set | Wheel already invented in `MaterialDefinition.cpp` |
| Material preview sphere | Custom mesh/FBO setup | `EditorAssetPreviewRenderer::drawMaterialPreview()` | Already exists, already used for mesh previews |
| Material serialization | Custom writer | `serializeMaterialDefinitionAsset()` already exists | Handles all current fields; extend it for new flags |

---

## Runtime State Inventory

This phase involves renaming/removing the `MaterialKind` enum but NOT renaming material IDs or file names. All material IDs remain the same. This is a code/shader refactor, not a rename operation.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | `.scene` files embed legacy material tokens (`stone`, `wood`, `floor`, `brick`, `metal`, `wax`, `moss`). Three files affected: `cathedral.scene`, `silos_cloister.scene`, `warden_office.scene` | One-time migration script: replace legacy tokens with `material <default-id>` syntax |
| Live service config | None — no external services store material data | None |
| OS-registered state | None | None |
| Secrets/env vars | None — no material-related env vars | None |
| Build artifacts | None — no generated binaries depend on MaterialKind integer values at rest | None |

**Scene file migration detail:** `cathedral.scene` uses `stone`, `wood`, `floor`, and `brick` tokens on approximately 100+ mesh lines. `silos_cloister.scene` and `warden_office.scene` similarly. The mapping from legacy token to material ID is already in `defaultMaterialIdForKind()`:
- `stone` → `stone_default`
- `wood` → `wood_default`
- `metal` → `metal_default`
- `wax` → `wax_default`
- `moss` → `moss_default`
- `floor` → `floor_default`
- `brick` → `brick_default`

The migration script is a straightforward text substitution: read each `.scene` line, check if after the 10 standard mesh fields there's a bare token matching a MaterialKind name, replace with `material <id>`.

---

## Common Pitfalls

### Pitfall 1: Roughness Defaults Only Live in the Shader
**What goes wrong:** After removing `materialRoughness()` from scene.frag, all materials render with roughness = `roughnessScale * roughnessBias + 0` = 0 or 1 (wrong), because the per-kind base roughness values (wood=0.74, metal=0.34, etc.) were never written into .material files.
**Why it happens:** The current design splits roughness behavior across two places: .material file (scale and bias) and the shader (base value). After removing the shader half, the base is gone.
**How to avoid:** Before removing `materialRoughness()`, audit each MaterialKind's roughness and write `roughness_bias <value>` into the corresponding default .material files. Verify visually after the shader change.
**Warning signs:** All surfaces appear uniformly matte (roughness ~1.0) or mirror-like after the shader change.

### Pitfall 2: Wax Flame Logic Depends on Color Heuristic
**What goes wrong:** The flame flicker in `scene.vert` and `scene.frag` uses `saturationOf(uBaseColor) > 0.28 && uBaseColor.r > 0.88` to detect "this is a flame" — a heuristic on the base color, not an explicit flag.
**Why it happens:** The MaterialKind=Wax detection was combined with a color threshold for flames (candles have a warm yellow-orange tint while wax pillars are white).
**How to avoid:** The `animated` feature flag (D-08) replaces this. Materials that should flicker set `animated true` in their .material file. The `flameMask()` function can be simplified to just check `uMaterialAnimated != 0`. The `saturationOf()` color check can be dropped.
**Warning signs:** Flame doesn't flicker after shader refactor, or wax wall segments flicker unexpectedly.

### Pitfall 3: Brick Normal Map Has Special Code Path
**What goes wrong:** `applyMaterialMapNormal()` and the `main()` function both have special cases for `uMaterialKind == MATERIAL_BRICK` that invoke `brickMacroMasks()` and `detailBrickNormal()`. These paths do NOT go through the standard normal map sampling.
**Why it happens:** Brick uses procedurally-generated normals with parallax-style offset on top of the texture, rather than the standard `normalMapNoTile` path.
**How to avoid:** The `useBrickDetail` flag replaces `uMaterialKind == MATERIAL_BRICK` in all these checks. Confirm every `MATERIAL_BRICK` conditional is replaced.
**Warning signs:** Brick walls lose their 3D mortar groove effect after the refactor.

### Pitfall 4: Renderer.h Lives in engine_rendering, MaterialKind in game
**What goes wrong:** `RenderMaterialData` in `Renderer.h` (engine layer) currently `#include`s `game/rendering/MaterialKind.h` (game layer), violating the engine→game dependency direction. This works now because it's tolerated, but removing `MaterialKind` from `Renderer.h` is actually an architectural improvement.
**Why it happens:** MaterialKind was bolted onto the engine struct when the game rendering layer was being built.
**How to avoid:** When removing `MaterialKind` from `RenderMaterialData`, also remove the `#include "game/rendering/MaterialKind.h"` from Renderer.h. The feature flag booleans don't need any game-layer includes. This is a clean layering improvement as a side effect of D-07.
**Warning signs:** Compiler error in Renderer.h after removing the field — check that no other engine headers also pull in MaterialKind.

### Pitfall 5: `resolveHelperMaterial()` in EditorScenePreviewRenderer and EditorViewportInteraction
**What goes wrong:** Multiple call sites in the editor use `MaterialKind::Wax`, `MaterialKind::Metal`, `MaterialKind::Moss` etc. directly to resolve "helper" materials for gizmos, checkpoints, and door geometry.
**Why it happens:** These calls predate the string-first material system.
**How to avoid:** Replace each `resolveHelperMaterial(materials, MaterialKind::X, "x_default")` with just `materials.resolve("x_default")` — the ID is already hardcoded alongside the enum. After removing MaterialKind, the enum param disappears entirely.
**Warning signs:** Compiler errors in `EditorScenePreviewRenderer.cpp`, `EditorViewportInteraction.cpp`, `EditorPreviewWorld.cpp`, and `EditorRuntimePreviewSession.cpp`.

### Pitfall 6: `MaterialTextureLibrary::resolve()` Signature Has `legacyKind` Param
**What goes wrong:** Every call site of `MaterialTextureLibrary::resolve()` passes both a `materialId` and a `MaterialKind legacyKind`. After removing the enum, the second parameter is gone. All call sites need updating.
**Why it happens:** The legacy kind was used as a fallback when `materialId` was empty.
**How to avoid:** The fallback is now "magenta" not "stone_default_for_this_kind". The new signature is `const RenderMaterialData& resolve(std::string_view materialId) const;`. When materialId is missing or unknown, return the magenta fallback.
**Warning signs:** Large number of compile errors at `resolve()` call sites.

### Pitfall 7: .material File Format — Keep Text, Not JSON/TOML
**Context:** This is a Claude's Discretion item. Recommendation: Keep the existing line-based key-value format.
**Rationale:** The existing parser in `loadMaterialDefinitionAsset()` is already battle-tested, the format is consistent with `.scene` and `.weapon` files project-wide, and switching to JSON/TOML would require a new dependency (nlohmann-json or toml++) or a new parser — cost with no benefit. Feature flags are just additional `key value` lines (e.g., `animated true`).
**New keys to add to the format:**
```
animated true           # wax flame flicker, animated behavior
subsurface false        # moss SSS (placeholder for future)
detail_brick true       # brick procedural geometry and mormals
detail_wood false       # plank seam/grain overlay
detail_stone false      # block course overlay
detail_floor false      # slab seam overlay
```

---

## Code Examples

### Verified Pattern: Recursive Directory Scan

```cpp
// Source: std::filesystem::recursive_directory_iterator (C++17 standard, confirmed in project)
namespace fs = std::filesystem;
const fs::path dir = resolveProjectPath("assets/materials");
for (const auto& entry : fs::recursive_directory_iterator(dir,
        fs::directory_options::skip_permission_denied)) {
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() != ".material") continue;
    // process file
}
```

### Verified Pattern: `fs::last_write_time` Polling for Hot-Reload

```cpp
// Source: std::filesystem (C++17, same header as existing includes in ContentRegistry.cpp)
auto mtime = std::filesystem::last_write_time(path);
if (mtime != storedMtime_) {
    storedMtime_ = mtime;
    onFileChanged(path);
}
```

### Verified Pattern: Feature Flags in GLSL (no type branches)

```glsl
// scene.frag — before (type branching):
if (uMaterialKind == MATERIAL_WAX) { ... flameMask logic ... }

// scene.frag — after (feature flag):
uniform int uMaterialAnimated;    // 0 or 1
float flameMask = (uMaterialAnimated != 0 && ...) ? 1.0 : 0.0;
```

```glsl
// scene.vert — before:
uniform int uMaterialKind;
const int MATERIAL_WAX = 3;
float flameMask = (uMaterialKind == MATERIAL_WAX && ...) ? 1.0 : 0.0;

// scene.vert — after:
uniform int uMaterialAnimated;
float flameMask = (uMaterialAnimated != 0 && ...) ? 1.0 : 0.0;
```

### Verified Pattern: Magenta Fallback (standard engine convention)

```cpp
// RenderMaterialData with magenta base color, no textures, no flags
RenderMaterialData MaterialTextureLibrary::makeMagentaFallback() {
    RenderMaterialData m;
    m.id = "__missing__";
    m.baseColor = glm::vec3(1.0f, 0.0f, 1.0f);
    m.useMaterialMaps = false;
    m.useProceduralDetail = false;
    // all feature flags false
    return m;
}
```

### Verified Pattern: Scene File Migration Script

The `.scene` parser in `LevelDef.cpp` already handles both formats simultaneously (lines 224-238). The migration script only needs to:
1. Read each `.scene` file
2. For each `mesh` line, find a bare material-kind token (`stone`, `wood`, etc.) as first optional token
3. Replace it with `material stone_default` (or corresponding ID)
4. Write the file back

The script can be a standalone C++ translation unit or a simple Python/bash script since this is a one-time operation.

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Per-kind shader branches | Single uber-shader with feature flags | This phase | All materials work with one shader; adding new behavior = add a flag |
| Hardcoded material list in ContentRegistry | Filesystem scan of `assets/materials/` | This phase | New material = drop file in folder |
| MaterialKind enum in engine layer | No enum; string IDs + feature flags | This phase | Engine layer no longer depends on game-layer type definitions |
| `sortedDefinitionFiles()` flat scan | `recursive_directory_iterator` | This phase | Subdirectory organization supported |

**Deprecated/outdated after this phase:**
- `MaterialKind.h` — deleted
- `shading_model` .material file key — parser removed after migration
- `legacyKind` parameter on `MaterialTextureLibrary::resolve()` — removed
- `defaultMaterialIdForKind()` — deleted after migration script uses it
- `tryParseMaterialKindToken()` — deleted
- `MATERIAL_STONE` / `MATERIAL_WOOD` etc. GLSL constants — deleted
- `uMaterialKind` uniform in scene.frag / scene.vert — deleted
- `LevelMeshPlacement::material` (optional MaterialKind field) — deleted after .scene migration

---

## Open Questions

1. **Where does `RenderMaterialData::useProceduralDetail` end up?**
   - What we know: Currently `useProceduralDetail = true` if `proceduralSource == GeneratedBrick || GeneratedStone`. Controls `texture()` vs `textureNoTile()` in the shader.
   - What's unclear: After D-07, should this become a feature flag like `detail_stone` / `detail_brick`, or remain as a computed flag in the texture library?
   - Recommendation: Keep as a computed `bool` in `RenderMaterialData` derived from `proceduralSource != None` — it's an implementation detail of texture sampling, not a user-authored property. The `detail_brick` flag controls the procedural geometry (normal/roughness overrides); `useProceduralDetail` controls texture tiling behavior.

2. **What is `specularLevel` after MaterialKind removal?**
   - What we know: `materialSpecularLevel()` returns per-kind values (wood=0.24, metal=1.0, etc.) used as `dielectricF0 = vec3(0.02 + specularLevel * 0.10)`.
   - What's unclear: Is this a new material property, or is it already covered by `metalness`?
   - Recommendation: `metalness` already handles the metallic path. For dielectrics, `specularLevel` maps to IOR — bake its effect into per-material roughness/metalness values. Can be dropped from the shader as a separate function; the values were per-kind heuristics, not physically-based properties. Alternatively, expose as `specular_level` float property in .material files.

3. **Hot-reload: does MaterialTextureLibrary need a separate invalidation API?**
   - What we know: `MaterialTextureLibrary::init()` re-resolves everything from scratch. For hot-reload, only one material changes.
   - Recommendation: Add `MaterialTextureLibrary::reloadMaterial(const std::string& id, const MaterialDefinition& updated)` that clears only that material's entries from `resolvedDefinitions_`, `textureSets_`, and `materials_`.

---

## Environment Availability

Step 2.6: SKIPPED — no new external dependencies. All required tools are already available in the project (C++ stdlib, existing linked libraries). The migration script operates on files already present on disk.

---

## Project Constraints (from CLAUDE.md)

- Graphics API: OpenGL 4.1 Core Profile. GLSL version `#version 410 core`. All shader changes target GLSL 4.10.
- No new system-wide package installs. All changes are within-project source modifications.
- Tests are standalone executables (no framework). Exit code = pass/fail.
- Three executables: `pixel-roguelike`, `level-editor`, `procedural-model-viewer`. All three link against `engine_rendering` / `game_rendering` and will need recompiling after `RenderMaterialData` changes.
- `#pragma once` throughout; no include guards.
- Engine layer must not depend on game layer headers. Removing `#include "game/rendering/MaterialKind.h"` from `Renderer.h` is required by this constraint and achieved by this phase.
- PascalCase classes, camelCase methods, snake_case members with trailing underscore for private.

---

## Sources

### Primary (HIGH confidence)

- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/rendering/MaterialDefinition.h` — Full struct definition, inheritance resolution API
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/rendering/MaterialDefinition.cpp` — Parser, resolver, serializer implementations
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/rendering/MaterialKind.h` — Enum to be removed (8 values confirmed)
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/rendering/MaterialTextureLibrary.h` — Library interface, `resolve()` signature
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/rendering/MaterialTextureLibrary.cpp` — Full implementation including disk cache, procedural generators
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/content/ContentRegistry.cpp` — Hardcoded material list (lines 510-528); existing `sortedDefinitionFiles()` pattern
- `/Users/ozan/Projects/gsd-3d-roguelike/src/engine/rendering/geometry/Renderer.h` — `RenderMaterialData` with `MaterialKind shadingModel`
- `/Users/ozan/Projects/gsd-3d-roguelike/src/engine/rendering/geometry/Renderer.cpp` — `drawScene()` material uniform binding, line 108 `setInt("uMaterialKind", ...)`
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/shaders/game/scene.frag` — Full shader (1100+ lines), all MaterialKind branches inspected
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/shaders/game/scene.vert` — Vertex shader, `uMaterialKind` used for wax flame deformation
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/level/LevelDef.h` — `LevelMeshPlacement` with `optional<MaterialKind> material`
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/level/LevelDef.cpp` — Legacy material token parsing (lines 224-238), `resolvedMaterialId()` fallback
- `/Users/ozan/Projects/gsd-3d-roguelike/src/editor/ui/EditorInspectorPanel.cpp` — Existing shading model combo, draft preview logic
- `/Users/ozan/Projects/gsd-3d-roguelike/src/editor/ui/EditorAssetBrowserPanel.cpp` — Asset browser structure (Phase 6 scenes pattern to follow)
- `/Users/ozan/Projects/gsd-3d-roguelike/src/editor/render/EditorAssetPreviewRenderer.h` — `drawMaterialPreview()` already exists
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/materials/*.material` — All 13 material files (flat, no subdirs currently)
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/scenes/cathedral.scene` — Scene file with legacy material tokens confirmed
- `.planning/phases/07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline/07-CONTEXT.md` — All locked decisions

### Secondary (MEDIUM confidence)

- `src/editor/ui/EditorPanelUtils.cpp` — `resolvePlacementMaterialKind()`, call sites using MaterialKind in editor UI
- `src/editor/viewport/EditorViewportInteraction.cpp` — `resolveHelperMaterial()` call sites
- `src/editor/render/EditorScenePreviewRenderer.cpp` — More `resolveHelperMaterial()` calls
- `src/editor/scene/EditorPreviewWorld.cpp` — MaterialKind usage in preview world

---

## Metadata

**Confidence breakdown:**
- Material data model changes: HIGH — full source inspection, all types and call sites confirmed
- Shader refactor scope: HIGH — full scene.frag read, all MaterialKind branches catalogued
- Scene file migration: HIGH — legacy token parsing code confirmed in LevelDef.cpp, three scene files confirmed
- Editor tooling: HIGH — EditorAssetBrowserPanel pattern confirmed via Phase 6, EditorAssetPreviewRenderer API confirmed
- File watcher: HIGH — `std::filesystem::last_write_time` is C++17 standard, already used in project
- Feature flag design: MEDIUM — specific flag names are Claude's discretion; the pattern is validated by industry practice (Unreal material graph flags, Unity material properties)

**Research date:** 2026-03-30
**Valid until:** 2026-04-30 (stable codebase; valid until Phase 7 execution begins changing source files)
