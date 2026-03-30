# Phase 7: Data-Driven Material System - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-03-30
**Phase:** 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline
**Areas discussed:** Material discovery, Shading model extensibility, Editor material tooling, Hot-reload and validation

---

## Material Discovery

### How should the engine discover materials at startup?

| Option | Description | Selected |
|--------|-------------|----------|
| Auto-scan directory | Scan assets/materials/*.material at startup. Drop a file in, it's available. No manifest to maintain. | ✓ |
| Manifest file | A materials.json or materials.list file enumerates which materials to load. Explicit control over load order. | |
| Hybrid scan + overrides | Auto-scan by default, but support a materials.cfg that can exclude or reorder specific materials. | |

**User's choice:** Auto-scan directory (Recommended)

### Should materials support subdirectories?

| Option | Description | Selected |
|--------|-------------|----------|
| Recursive scan | Scan assets/materials/**/*.material recursively. Allows organizing like materials/stone/, materials/wood/. | ✓ |
| Flat directory only | Only scan assets/materials/*.material (no subdirs). | |
| You decide | Claude picks the best approach. | |

**User's choice:** Recursive scan (Recommended)

### When a .scene file references a material that doesn't exist?

| Option | Description | Selected |
|--------|-------------|----------|
| Fallback to default + warn | Render with bright magenta "missing material" look and log a warning. Standard game engine behavior. | ✓ |
| Fallback to stone_default | Silently fall back to stone_default material. Less visually jarring but hides errors. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Fallback to default + warn

### Should duplicate material IDs be an error or last-wins?

| Option | Description | Selected |
|--------|-------------|----------|
| Error at load time | Log an error and skip the duplicate. First-loaded wins. Prevents accidental ID collisions. | ✓ |
| Last-wins silently | Later file overwrites earlier one. Enables "patching" but can mask mistakes. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Error at load time (Recommended)

### Should legacy MaterialKind references in .scene files keep working?

| Option | Description | Selected |
|--------|-------------|----------|
| Migrate all to material IDs | Update all .scene files to use 'material stone_default' syntax. Clean break. | ✓ |
| Keep legacy + deprecation warning | Legacy references still work but log a deprecation warning. | |
| Keep both permanently | Both syntaxes remain valid forever. | |

**User's choice:** Migrate all to material IDs

### Load order: should order matter?

| Option | Description | Selected |
|--------|-------------|----------|
| Two-pass: scan then resolve | First pass loads all definitions. Second pass resolves inheritance. Order doesn't matter. | ✓ |
| Alphabetical with dependency sort | Sort files alphabetically, then topologically sort by parent dependencies. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Two-pass: scan then resolve (Recommended)

---

## Shading Model Extensibility

### Should shading models become string-based or stay as enum?

| Option | Description | Selected |
|--------|-------------|----------|
| Keep enum, add via code | MaterialKind stays as a C++ enum. Simple, type-safe. | |
| String-based, shader mapping | Shading model is a string. Config maps string names to shader behavior IDs. | |
| Property-driven, no enum | Remove MaterialKind entirely. Material behavior defined by properties. One PBR uber-shader. | ✓ |

**User's choice:** Property-driven, no enum (Recommended)

### How should special behaviors work without the enum?

| Option | Description | Selected |
|--------|-------------|----------|
| Boolean feature flags in .material | Material files declare flags like 'subsurface true', 'animated true'. Shader checks these flags. | ✓ |
| Effect layers / tags | Materials have an 'effects' list. Shader has named effect implementations activated by tag. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Boolean feature flags in .material

### Should procedural generation be a material property too?

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, procedural_source property | Keep existing pattern. Generator name is a string, looked up in a registry. | ✓ |
| Separate texture pipeline | Procedural textures defined in own files (.proctex), materials reference by name. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Yes, procedural_source property (Recommended)

### Should the .material file format evolve?

| Option | Description | Selected |
|--------|-------------|----------|
| Keep current text format | Existing 'key value' line-based format. Simple and consistent with .scene format. | |
| Switch to JSON/TOML | Structured format with better nesting support. | |
| You decide | Claude picks the best approach. | ✓ |

**User's choice:** You decide

---

## Editor Material Tooling

### Should the editor have a material inspector/editor panel?

| Option | Description | Selected |
|--------|-------------|----------|
| Full material editor | Inspector panel with sliders/color pickers. Changes write back to .material files. Live preview sphere. | ✓ |
| Read-only inspector | Shows material properties for reference but editing happens in text files. | |
| No editor UI | Keep materials as hand-edited text files. | |

**User's choice:** Full material editor (Recommended)

### How should material preview work?

| Option | Description | Selected |
|--------|-------------|----------|
| Preview sphere in inspector | Small 3D sphere rendered with the material in inspector panel. | ✓ |
| Preview on selected mesh | Changes apply live to selected mesh in viewport. | |
| Both sphere + viewport | Inspector shows preview sphere AND changes reflect in viewport. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Preview sphere in inspector

### Should the editor support creating new materials?

| Option | Description | Selected |
|--------|-------------|----------|
| Create from template/parent | Right-click > New Material. Pick parent. Creates .material with inherited defaults. | ✓ |
| Duplicate existing | Only way is duplicating an existing one. | |
| Both create + duplicate | Support both creating from parent and duplicating. | |

**User's choice:** Create from template/parent (Recommended)

### Should asset browser show materials as a category?

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, materials category | Asset browser gets 'Materials' section. Click to inspect, right-click for create/rename/delete. | ✓ |
| Inspector only, no browser | Materials only appear when a mesh is selected. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Yes, materials category (Recommended)

---

## Hot-Reload and Validation

### Should materials hot-reload?

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, file watcher | Monitor assets/materials/ for changes. Auto re-parse and update. | ✓ |
| Manual reload button | Editor has a 'Reload Materials' button/shortcut. | |
| No hot-reload | Materials only load at startup. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Yes, file watcher (Recommended)

### How to handle broken inheritance chains?

| Option | Description | Selected |
|--------|-------------|----------|
| Skip material + error log | Material with broken parent is skipped. Error logged. Meshes get magenta fallback. | ✓ |
| Load with defaults for missing parent | Resolve as if no parent. Warning logged but material still usable. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Skip material + error log (Recommended)

### When should validation happen?

| Option | Description | Selected |
|--------|-------------|----------|
| Both load and save | Validate on load AND on save from editor. Editor shows errors inline. | ✓ |
| Load time only | Validate only when parsing .material files. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Both load and save (Recommended)

### Missing material fallback visual?

| Option | Description | Selected |
|--------|-------------|----------|
| Bright magenta flat color | Classic engine convention. Unmissable hot pink. No textures, no lighting response. | ✓ |
| Magenta checkerboard | Alternating magenta and black checkerboard (Unreal-style). | |
| You decide | Claude picks the best approach. | |

**User's choice:** Bright magenta flat color (Recommended)

---

## Claude's Discretion

- .material file format evolution (keep text or switch to JSON/TOML)
- File watcher implementation details
- Validation error presentation in editor
- Material inspector layout
- Migration script implementation
- Procedural texture generator registry details

## Deferred Ideas

None — discussion stayed within phase scope
