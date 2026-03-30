# Phase 7: Data-Driven Material System - Context

**Gathered:** 2026-03-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace the hardcoded material registration and shading model enums with a fully data-driven material pipeline. Auto-discover materials from the filesystem, remove the MaterialKind enum in favor of property-driven PBR shading, add editor tooling for creating/editing/previewing materials, and support hot-reload with validation. Migrate all legacy MaterialKind references in .scene files to explicit material IDs.

</domain>

<decisions>
## Implementation Decisions

### Material Discovery
- **D-01:** Auto-scan `assets/materials/**/*.material` recursively at startup. No manifest file — drop a file in, it's available.
- **D-02:** Subdirectories supported for organization (e.g. `materials/stone/`, `materials/wood/`).
- **D-03:** Two-pass loading: first pass scans all files into memory, second pass resolves inheritance. Load order doesn't matter.
- **D-04:** Duplicate material IDs (same ID in two files) are an error at load time. First-loaded wins, duplicate is skipped with error log.
- **D-05:** Missing material fallback: bright magenta flat color (no textures, no lighting response) + warning log. Classic engine convention.
- **D-06:** Migrate ALL legacy MaterialKind-based references in .scene files to explicit `material <id>` syntax. Clean break, no legacy code path. One-time migration script.

### Shading Model Extensibility
- **D-07:** Remove the MaterialKind enum entirely (both C++ enum and GLSL `const int MATERIAL_*` constants). Material behavior is fully defined by properties (roughness, metalness, etc.). One PBR uber-shader, no type branching.
- **D-08:** Special behaviors (wax flame flicker, moss subsurface, etc.) implemented via boolean feature flags in .material files (e.g. `subsurface true`, `animated true`, `emissive_flicker true`). Shader checks flags, not material type.
- **D-09:** Procedural textures remain as `procedural_source` property in .material files. Generator name is a string looked up in a registry (existing pattern, now canonical).

### Editor Material Tooling
- **D-10:** Full material editor inspector panel with sliders, color pickers, and all material properties editable. Changes write back to .material files.
- **D-11:** Preview sphere rendered inline in the material inspector panel using existing EditorAssetPreviewRenderer.
- **D-12:** Create new materials from template/parent: right-click in asset browser > New Material, pick a parent (or blank), creates .material file with inherited defaults.
- **D-13:** Asset browser gets a "Materials" category listing all discovered materials. Click to inspect, right-click for create/rename/delete. Follows the pattern Phase 6 establishes for scenes.

### Hot-Reload and Validation
- **D-14:** File watcher monitors `assets/materials/` for changes. When a file changes, re-parse and update the material in-memory. Procedural textures regenerate if parameters changed.
- **D-15:** Validation at both load time (catches issues in hand-edited files) AND on save from editor (prevents writing invalid materials). Editor shows validation errors inline.
- **D-16:** Broken inheritance chains (parent doesn't exist): material is skipped entirely, error logged with file path and missing parent name. Meshes using it get the magenta fallback.

### Claude's Discretion
- .material file format evolution (keep current key-value text format or switch to JSON/TOML — Claude decides based on what works best with the property-driven model)
- File watcher implementation details (polling interval, platform API choice)
- Exact validation error presentation in editor UI
- Material inspector layout and organization of property groups
- Migration script implementation for .scene files
- Procedural texture generator registry implementation details

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Material system (code being refactored)
- `src/game/rendering/MaterialDefinition.h` — MaterialDefinition and ResolvedMaterialDefinition structs, inheritance resolution
- `src/game/rendering/MaterialDefinition.cpp` — Material file parser (loadMaterialDefinitionAsset), resolution logic (resolveMaterialDefinition)
- `src/game/rendering/MaterialTextureLibrary.h` — Texture loading, procedural generation, RenderMaterialData assembly
- `src/game/rendering/MaterialTextureLibrary.cpp` — Procedural texture generators, disk cache integration, resolve() pipeline
- `src/game/rendering/MaterialLibrary.h` — MaterialLibrary wrapper (may be consolidated)
- `src/game/rendering/MaterialKind.h` — MaterialKind enum and defaultMaterialIdForKind() — TO BE REMOVED

### Content registry (material registration)
- `src/game/content/ContentRegistry.h` — Content resolution system; materials_ map, findMaterial()
- `src/game/content/ContentRegistry.cpp` — loadDefaults() with hardcoded material file list (lines ~510-528) — TO BE REPLACED with auto-scanner

### Rendering pipeline (material binding)
- `src/engine/rendering/geometry/Renderer.h` — Renderer that binds material uniforms
- `src/engine/rendering/geometry/Renderer.cpp` — drawScene() material uniform binding (lines ~84-109)
- `assets/shaders/game/scene.frag` — Fragment shader with MaterialKind constants and per-material branches — TO BE REFACTORED to property-driven
- `assets/shaders/game/scene.vert` — Vertex shader with uMaterialKind uniform — TO BE REFACTORED

### Scene file format (material references)
- `src/game/level/LevelDef.h` — LevelMeshPlacement struct with materialId and legacy MaterialKind
- `src/game/level/LevelDef.cpp` — Scene file parser, resolvedMaterialId() function, MaterialKind parsing
- `assets/scenes/*.scene` — Scene files with material references to be migrated

### Editor (material tooling target)
- `src/editor/ui/EditorAssetBrowserPanel.cpp` — Asset browser panel; target for materials category (follows Phase 6 scenes pattern)
- `src/editor/render/EditorAssetPreviewRenderer.h` — Preview renderer for material sphere preview

### Existing .material files
- `assets/materials/*.material` — All current material definition files (13 total)

### Procedural textures
- `src/game/rendering/MaterialTextureLibrary.cpp` — Procedural generators (generateBrickPixels, generateStonePixels, etc.)
- `src/engine/rendering/assets/AssetCache.h` — Disk cache for generated textures

### Project context
- `.planning/PROJECT.md` — Core value (Stanley Parable style), constraints (OpenGL 4.1)
- `.planning/ROADMAP.md` — Phase 7 goal and dependencies
- `.planning/phases/06-data-driven-scene-management/06-CONTEXT.md` — Phase 6 context (ContentRegistry consolidation, asset browser patterns)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- **MaterialDefinition + inheritance**: Already parses .material text files with parent inheritance. Extend, don't rewrite.
- **MaterialTextureLibrary**: Procedural texture generation pipeline with disk cache. Generator registry can be formalized from existing code.
- **EditorAssetPreviewRenderer**: Already renders preview spheres for asset inspection. Reuse for material preview.
- **EditorAssetBrowserPanel**: Phase 6 adds scene browsing pattern — material browsing follows identical pattern.
- **AssetCache**: Disk-based cache for procedural textures. Already handles invalidation by parameter hash.

### Established Patterns
- **Text-based file formats**: .scene and .material files use line-based key-value text format. Consistent across the project.
- **ContentRegistry**: Central resolution system for materials, environments, weapons, enemies. Materials integrate here.
- **Two-pass resolve**: MaterialDefinition already supports inheritance resolution via recursive parent lookup. The pattern exists.
- **ImGui inspector**: Editor uses ImGui for all inspector panels. Material inspector follows same patterns.

### Integration Points
- **ContentRegistry::loadDefaults()**: Replace hardcoded material file list with directory scanner
- **MaterialKind enum**: Remove from MaterialKind.h, all shader files, LevelDef parsing, MeshComponent
- **scene.frag/scene.vert**: Refactor from MaterialKind branching to property-driven uniforms
- **LevelDef parser**: Remove MaterialKind parsing, keep only `material <id>` syntax
- **EditorAssetBrowserPanel**: Add materials category (Phase 6 adds scenes category — same pattern)
- **Renderer::drawScene()**: Already binds all material properties as uniforms — minimal changes needed

</code_context>

<specifics>
## Specific Ideas

- The material system should feel like a proper game engine — drop files in a directory, they're available. No code changes needed to add materials.
- Property-driven uber-shader is the modern standard (Unreal, Unity HDRP). No material "types" — just properties that define behavior.
- The magenta missing-material fallback is a universal game engine convention that makes errors immediately visible.
- Material inheritance already works — this phase makes it the canonical pattern for creating variations.

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline*
*Context gathered: 2026-03-30*
