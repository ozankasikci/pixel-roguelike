# Per-Material Weathering System

**Date:** 2026-04-11
**Status:** Approved

## Goal

Make levels look lived-in, worn, and visually varied by adding per-material weathering effects to the existing procedural detail system. Materials that opt in automatically show cavity grime, edge wear, dust accumulation, damp staining, and macro-scale color variation — all driven by geometry context in the fragment shader. Materials that don't opt in are visually unchanged.

## Design Principles

- **Per-material, not universal.** Each MaterialKind defines its own weathering behavior. Rust belongs on metal, not wood. Moss belongs on stone, not metal.
- **Opt-in via `.material` files.** Existing materials are unaffected. Set `weathering_enabled true` to activate.
- **Zero authoring burden per entity.** No scene file changes, no per-instance parameters, no vertex painting. The material knows how to weather itself based on geometry context.
- **Shader-only rendering changes.** No new render passes, no new ECS components, no pipeline changes.

## Architecture

### Approach: Separate Weathering Functions (Approach B)

Weathering logic lives in a dedicated section of the shader, separate from base detail functions. After `applyMaterialDetail()` produces the base albedo, `applyMaterialWeathering()` runs as a second pass that modifies albedo and roughness in-place.

```
Material Detail Pipeline:
  materialBaseColor
    → applyMaterialDetail()     [existing: brick pattern, stone seams, wood grain, etc.]
    → applyMaterialWeathering() [new: cavity grime, edge wear, dust, damp, macro noise]
    → final albedo + roughness
```

### Weathering Inputs (5 Signals)

The shader computes these signals from data it already has — no new textures or geometry data needed:

1. **Cavity/curvature mask** — `fwidth(vWorldNormal)` detects edges vs. crevices in screen-space. Convex edges get wear; concave areas accumulate grime. Essentially free (GPU already computes partial derivatives for texture filtering).

2. **Up-facing mask** — `dot(normalize(vWorldNormal), vec3(0,1,0))` detects horizontal surfaces. Dust, moss, and rain accumulate on surfaces that face up.

3. **Height gradient** — `vWorldPos.y` normalized to a useful range. Lower surfaces are damper/muddier, higher surfaces are drier/dustier.

4. **Multi-scale world-space noise** — FBM sampled at 2-3 frequencies at `vWorldPos`. Large blotchy variation (scale ~0.1) + medium detail (scale ~0.5). Breaks uniformity so adjacent surfaces of the same material look different.

5. **Macro noise for spatial zones** — Larger-scale noise used specifically by floors to create "high traffic" vs. "neglected corner" variation, since floors are always up-facing and at constant height.

### Curvature Approximation (OpenGL 4.1)

Screen-space curvature via `fwidth()`:

```glsl
float curvature = length(fwidth(vWorldNormal));
```

**Catches well:** Hard mesh edges, geometric creases, ridges, transitions between angled surfaces.

**Misses:** Smooth concavities, spatially enclosed areas that are geometrically flat (wall-floor junctions). Mitigated by height gradient + up-facing mask covering those spatial cases.

**Performance:** Essentially free — reuses GPU partial derivatives already computed for texture filtering.

**Future enhancement:** Baking curvature into vertex colors at mesh load time would give better quality but requires vertex format and mesh pipeline changes. Not needed for this iteration.

### Utility Functions

Shared math helpers in the shader that any per-MaterialKind weathering function can call:

```glsl
float weatherCavityMask(vec3 worldNormal);              // crevice/joint detection
float weatherEdgeWearMask(vec3 worldNormal);             // convex edge detection (inverse of cavity)
float weatherUpFacingMask(vec3 worldNormal);             // horizontal surface detection
float weatherHeightGradient(float worldY);               // ground-to-ceiling gradient
float weatherMacroNoise(vec3 worldPos, float scale);     // multi-octave world-space noise
```

These are pure functions — they compute raw signals. The per-MaterialKind functions decide what each signal means.

### Per-MaterialKind Weathering Functions

```glsl
void weatherStone(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos, /* uniforms */);
void weatherBrick(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos, /* uniforms */);
void weatherWood(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos, /* uniforms */);
void weatherFloor(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos, /* uniforms */);
void weatherMetal(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos, /* uniforms */);
```

### Dispatcher

```glsl
void applyMaterialWeathering(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    if (uWeatheringEnabled == 0) return;

    if (uMaterialStoneDetail != 0)      weatherStone(albedo, roughness, N, worldPos);
    else if (uMaterialBrickDetail != 0)  weatherBrick(albedo, roughness, N, worldPos);
    else if (uMaterialWoodDetail != 0)   weatherWood(albedo, roughness, N, worldPos);
    else if (uMaterialFloorDetail != 0)  weatherFloor(albedo, roughness, N, worldPos);
    else if (uMaterialMetalness > 0.5)   weatherMetal(albedo, roughness, N, worldPos);
}
```

## Per-MaterialKind Weathering Behaviors

### Stone
| Signal | Effect | Roughness |
|--------|--------|-----------|
| Cavity mask | Dark grime in crevices/seams (tinted by `weatheringDirtColor`) | Rougher |
| Edge wear | Lighter, smoother stone on convex edges (worn by contact) | Smoother |
| Up-facing | Dust or lichen on horizontal ledges | Slightly rougher |
| Height gradient | Damp darkening on lower courses, drier/lighter higher up | — |
| Macro noise | Large blotchy color variation across adjacent blocks | — |

### Brick
| Signal | Effect | Roughness |
|--------|--------|-----------|
| Cavity mask | Mortar darkening in joints, soot accumulation | Rougher |
| Edge wear | Exposed lighter clay on corners where soot chips off | Slightly smoother |
| Up-facing | Dust on tops of protruding bricks | — |
| Height gradient | Efflorescence (white salt) near ground, soot darkening higher | — |
| Macro noise | Per-brick color drift beyond base detail | — |

### Wood
| Signal | Effect | Roughness |
|--------|--------|-----------|
| Cavity mask | Dark grain filling, dirt in plank cracks | Rougher |
| Edge wear | Bleached wood on exposed corners, high-contact edges | Smoother |
| Up-facing | Water stain rings on horizontal surfaces (desks, shelves) | — |
| Height gradient | Damp/rot darkening near floor level | Rougher |
| Macro noise | Plank-to-plank color variation (some more sun-bleached) | — |

### Floor
| Signal | Effect | Roughness |
|--------|--------|-----------|
| Cavity mask | Grime in slab seams and chips | Rougher |
| Edge wear | Lighter wear on slab edges from foot traffic | Smoother |
| Up-facing | N/A (always 1.0) — replaced by macro noise spatial zones | — |
| Height gradient | N/A (floors are flat) — skipped | — |
| Macro noise | Dominant signal — worn paths vs. grimy neglected corners | Worn=smoother, corners=rougher |

### Metal
| Signal | Effect | Roughness |
|--------|--------|-----------|
| Cavity mask | Rust/oxidation in crevices and joints | Much rougher |
| Edge wear | Polished bare metal on handles, contact points, edges | Much smoother |
| Up-facing | Water spots, patina on horizontal surfaces | — |
| Height gradient | Rust/drip stains running downward | — |
| Macro noise | Patchy oxidation breaking up uniform metallic look | — |

## Material Definition Format

New optional fields in `.material` files:

```
weathering_enabled       bool    (default: false)
weathering_dirt_strength float   (default: 0.0) — cavity grime intensity
weathering_dirt_color    vec3    (default: 0.2 0.16 0.12) — grime tint
weathering_edge_wear_strength float (default: 0.0) — convex edge wear intensity
weathering_dust_strength float   (default: 0.0) — up-facing dust/accumulation
weathering_damp_strength float   (default: 0.0) — height-based moisture
weathering_noise_scale   float   (default: 0.3) — macro variation frequency
```

### Examples

```
# Heavy dungeon weathering
id stone_dungeon
parent stone_default
weathering_enabled true
weathering_dirt_strength 0.7
weathering_dirt_color 0.18 0.14 0.10
weathering_edge_wear_strength 0.5
weathering_dust_strength 0.3
weathering_damp_strength 0.6
weathering_noise_scale 0.4
```

```
# Subtle office wear
id stone_office
parent stone_default
weathering_enabled true
weathering_dirt_strength 0.15
weathering_dirt_color 0.25 0.22 0.18
weathering_edge_wear_strength 0.1
weathering_dust_strength 0.05
weathering_damp_strength 0.1
weathering_noise_scale 0.3
```

```
# No weathering — unchanged behavior
id stone_default
parent masonry_base
procedural_source generated_stone
```

Inheritance works as-is. A child that sets `weathering_enabled true` but omits strengths gets resolved defaults (0.0). A child can inherit from a weathered parent and override individual strengths.

## Files Changed

| File | Change |
|------|--------|
| `src/game/rendering/MaterialDefinition.h` | 7 new optional fields on `MaterialDefinition`, 7 new fields with defaults on `ResolvedMaterialDefinition` |
| `src/game/rendering/MaterialDefinition.cpp` | Parse new fields from `.material` files, resolve through inheritance |
| `src/engine/rendering/geometry/Renderer.h` | 7 new fields on `RenderMaterialData` |
| `src/engine/rendering/geometry/Renderer.cpp` | Set 7 new uniforms when material changes |
| `src/game/rendering/MaterialTextureLibrary.cpp` | Propagate resolved weathering fields to `RenderMaterialData` |
| `assets/shaders/game/scene.frag` | New uniforms, `applyMaterialWeathering()` dispatcher call site after `applyMaterialDetail()`, and a clearly separated weathering section containing utility functions + 5 per-MaterialKind weathering functions. All code lives in `scene.frag` as a distinct block (OpenGL 4.1 has no `#include` — if the engine adds shader concatenation later, this section can be extracted to a separate file without changes) |

## What Does NOT Change

- Scene file format (`.scene`)
- LevelDef, LevelBuilder, LevelLoader
- ECS components
- RenderObject structure (no per-instance data)
- Render pipeline (no new passes)
- Post-processing shaders
- Texture loading or caching
- Editor UI

## Future Layers (Out of Scope)

These can be added independently on top of this system later:
- **Vertex color painting** — artist-painted blend masks for manual weathering control
- **Decal system** — projected textures for localized storytelling (stains, scorch marks, graffiti)
- **Baked curvature maps** — higher quality cavity/edge detection via vertex colors or textures
