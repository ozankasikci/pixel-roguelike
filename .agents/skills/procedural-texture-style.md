---
name: procedural-texture-style
description: Style guide and generation rules for procedural textures. Use when creating any new procedural texture — stone, brick, wood, metal, or other material surface patterns. Ensures visual consistency and correct pipeline integration.
---

# Procedural Texture Style Guide

Use this skill whenever creating or modifying procedural texture generators in this project. Every texture must follow these rules to maintain visual consistency across the game's clean, Stanley Parable-inspired aesthetic and integrate correctly into the material pipeline.

## Core Principle

All procedural textures generate a **four-channel texture set**: albedo (RGBA8), normal (RGBA8), roughness (R8), AO (R8). Textures are generated as CPU-side pixel arrays (the `ProceduralPixelData` struct), then uploaded to GL textures via `Texture2D`. Every texture must tile seamlessly — use tileable noise functions for patterns that repeat at UV boundaries.

## Output Format

The `ProceduralPixelData` struct lives inside `MaterialTextureLibrary` (private):

```cpp
struct ProceduralPixelData {
    std::vector<std::uint8_t> albedo;     // RGBA8, size*size*4
    std::vector<std::uint8_t> normal;     // RGBA8, size*size*4
    std::vector<std::uint8_t> roughness;  // R8, size*size
    std::vector<std::uint8_t> ao;         // R8, size*size
    int size;
};
```

- **Standard texture size:** 512x512 (`constexpr int kSize = 512`)
- **Normal maps** are derived from a height field using `sampleHeightNormal()` in a second pass after the main pixel loop
- **Height-to-normal strength factor:** 3.2f (hardcoded in `sampleHeightNormal`)
- All four channels are always populated — never leave roughness or AO as the default-filled value (255) unless deliberately flat

## Noise Primitives

All noise functions live in the anonymous namespace in `MaterialTextureLibrary.cpp`. They are **not accessible outside that translation unit**.

| Function | Signature | Purpose |
|----------|-----------|---------|
| `hash21` | `float hash21(const glm::vec2& p)` | Pseudo-random float [0,1] from a 2D lattice coordinate |
| `valueNoise` | `float valueNoise(const glm::vec2& p)` | Smooth value noise with hermite interpolation; non-tileable |
| `fbm` | `float fbm(glm::vec2 p)` | 4-octave fractional Brownian motion built on valueNoise; non-tileable |
| `tileableValueNoise` | `float tileableValueNoise(const glm::vec2& p, float period, const glm::vec2& seed)` | Value noise that wraps at the given period; use for seamless tiling |
| `tileableFbm` | `float tileableFbm(glm::vec2 p, float baseFreq, const glm::vec2& seed)` | 4-octave tileable fBm; seed shifts per-octave to avoid pattern repetition |
| `smooth01` | `float smooth01(float edge0, float edge1, float x)` | Hermite smoothstep remapped to [0,1] output; useful for mortar/seam masks |
| `toByte` | `std::uint8_t toByte(float value)` | Saturates to [0,1] then scales to uint8; used for every channel write |
| `sampleHeightNormal` | `glm::vec3 sampleHeightNormal(const std::vector<float>& heights, int size, int x, int y)` | Central-difference normal from a height field; wraps at texture edges |

### Which noise to use

- **`tileableFbm` / `tileableValueNoise`**: Use for any texture that will be applied with `uv_mode world_projected` or tiled via UV scale. Seams are visible at tile boundaries with non-tileable noise.
- **`fbm` / `valueNoise`**: Use only for per-element variation consumed entirely within a single cell (e.g., per-brick color variation where `p` is the integer brick coordinate, not the continuous UV).
- **Never** use non-tileable noise for the primary surface pattern of world-projected materials.

## Color Palette

Reference `RetroPalette.h` for mesh tint constants. For texture albedo, follow these Stanley Parable aesthetic rules:

- **Warm, muted base tones** — beiges (0.85-0.95), warm grays (0.70-0.82), soft yellows (0.88-0.95 with slight yellow shift)
- **Never use saturated colors** — keep everything desaturated and institutional; avoid vivid reds, blues, or greens
- **Per-element variation** — vary individual brick/tile colors by ±0.02 to ±0.05 to break monotony; use `hash21` on the cell coordinate for deterministic variation
- **Soot/weathering darkens with cool shift**: multiply by `glm::vec3(0.88f, 0.85f, 0.83f)` — slightly more blue-channel loss than red for a natural grimy look
- **Pale/highlight variants warm slightly**: multiply by `glm::vec3(1.05f, 1.03f, 1.00f)` — warm highlights on aged surfaces
- **Mortar**: neutral warm gray (0.68-0.84 range, slightly varying per course) — never pure white or pure gray
- Albedo values should stay in the **0.60–0.95 range** — never pure white or pure black at any pixel

## Texture Generation Pattern

Every `generate*Pixels()` function follows this exact two-pass structure:

```cpp
MaterialTextureLibrary::ProceduralPixelData MaterialTextureLibrary::generateMyTexturePixels() const {
    constexpr int kSize = 512;

    ProceduralPixelData result;
    result.size = kSize;
    result.albedo.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.normal.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.roughness.resize(static_cast<size_t>(kSize * kSize), 255);
    result.ao.resize(static_cast<size_t>(kSize * kSize), 255);
    std::vector<float> height(static_cast<size_t>(kSize * kSize), 0.0f);

    // Pass 1: Generate albedo, roughness, AO, and height field
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kSize);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize);
            // ... noise sampling, color computation ...
            const size_t pixelIndex = static_cast<size_t>(y * kSize + x);
            const size_t colorIndex = pixelIndex * 4;
            result.albedo[colorIndex + 0] = toByte(color.r);
            result.albedo[colorIndex + 1] = toByte(color.g);
            result.albedo[colorIndex + 2] = toByte(color.b);
            result.albedo[colorIndex + 3] = 255;
            result.roughness[pixelIndex] = toByte(localRoughness);
            result.ao[pixelIndex]        = toByte(localAo);
            height[pixelIndex]           = localHeight;
        }
    }

    // Pass 2: Derive normal map from height field
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            glm::vec3 n = sampleHeightNormal(height, kSize, x, y);
            const size_t colorIndex = static_cast<size_t>(y * kSize + x) * 4;
            result.normal[colorIndex + 0] = toByte(n.x * 0.5f + 0.5f);
            result.normal[colorIndex + 1] = toByte(n.y * 0.5f + 0.5f);
            result.normal[colorIndex + 2] = toByte(n.z * 0.5f + 0.5f);
            result.normal[colorIndex + 3] = 255;
        }
    }
    return result;
}
```

### Rules for the pattern

1. **Always initialize all four vectors before the loop** — resize with a default so unset pixels are never garbage
2. **Height is a float intermediate** — never write height directly to a channel; it feeds `sampleHeightNormal` in pass 2
3. **Normal encoding is always `n * 0.5f + 0.5f`** — maps [-1,1] to [0,1] before `toByte`
4. **UV origin at pixel center** — `(x + 0.5f) / kSize` not `x / kSize`
5. **Alpha channel of albedo and normal is always 255** — these are opaque surface textures

## PBR Value Ranges

| Channel | Typical Range | Notes |
|---------|---------------|-------|
| Albedo R/G/B | 0.60–0.95 | Warm muted tones; never pure white or black |
| Height (intermediate) | -0.03 to +0.02 | Very subtle displacement; mortar/seams are negative, face is positive |
| Roughness | 0.65–0.95 | Most surfaces are rough; polished metal only (0.30–0.50) |
| AO | 0.82–1.00 | Subtle darkening in crevices; mortar joints ~0.86, open faces ~0.96–1.0 |
| Normal strength | Via .material definition | Set `normal_strength 0.4`–`1.0` in the .material file, not baked into the texture |

Smooth wall (GeneratedSmooth) uses the lower end of height range (±0.004) for a nearly-flat surface. Brick and stone use the full range.

## Material Pipeline Integration

Follow all six steps when adding a new procedural texture type:

**Step 1 — Add enum value to `MaterialProceduralSource` in `MaterialDefinition.h`:**

```cpp
enum class MaterialProceduralSource {
    None            = 0,
    GeneratedBrick  = 1,
    GeneratedStone  = 2,
    GeneratedSmooth = 3,
    GeneratedMyTexture = 4,  // New entry — pick next integer
};
```

**Step 2 — Add token parsing in `MaterialDefinition.cpp`:**

Two functions need updating:
- `tryParseMaterialProceduralSourceToken()` — add an `else if (token == "generated_my_texture") { source = MaterialProceduralSource::GeneratedMyTexture; return true; }` branch
- `materialProceduralSourceToken()` (or the serialization counterpart) — add the reverse mapping case

**Step 3 — Declare and implement the generator:**

In `MaterialTextureLibrary.h` (private section):
```cpp
ProceduralPixelData generateMyTexturePixels() const;
```

In `MaterialTextureLibrary.cpp`: implement following the two-pass pattern above.

**Step 4 — Wire the new source in `ensureTextureSet()` in `MaterialTextureLibrary.cpp`:**

Find the `if (resolved.proceduralSource == MaterialProceduralSource::GeneratedBrick)` block. Add an `else if` branch:
```cpp
} else if (resolved.proceduralSource == MaterialProceduralSource::GeneratedMyTexture) {
    pixels = generateMyTexturePixels();
}
```
Also update the `isProcedural` boolean at the top of `ensureTextureSet()` to include the new source.

**Step 5 — Create a `.material` file in `assets/materials/`:**

```
id my_texture_default
parent masonry_base
shading_model stone
procedural_source generated_my_texture
uv_mode world_projected
uv_scale 0.45 0.45
normal_strength 0.6
roughness_scale 1.0
roughness_bias 0.0
ao_strength 0.9
light_tint_response 0.12
```

**Step 6 — Update `useProceduralDetail` in `resolve()` if needed:**

The `useProceduralDetail` flag in `MaterialTextureLibrary.cpp::resolve()` currently enables `textureNoTile` anti-tiling only for `GeneratedBrick` and `GeneratedStone`. If your new texture has a strongly repeating pattern that would show tiling artifacts, add it to this condition. For smooth/subtle textures, omit it (like `GeneratedSmooth`).

## Disk Cache Integration

Procedural textures are automatically cached via `AssetCache`. No special work is needed from the texture author as long as the source is wired through `ensureTextureSet()`.

- **Cache key:** built by `textureKeyFor()` — a pipe-separated string of `proceduralSource` integer + all four map paths
- **Hash:** `AssetCache::hashBytes(key.data(), key.size())` applied to the key string
- **Four cache entries per texture set:** `key + "_albedo"`, `key + "_normal"`, `key + "_roughness"`, `key + "_ao"`
- **Cache check** happens at the top of `ensureTextureSet()` before any generation
- On cache hit: all four textures loaded from disk, GL textures created — no CPU generation
- On cache miss: `generate*Pixels()` called, GL textures uploaded, then **four `AssetCache::writeTextureCache()` calls** write the result to disk for the next launch

The texture channels have different component counts: albedo/normal use `channels = 4` (RGBA8); roughness/AO use `channels = 1` (R8). Match these when calling `writeTextureCache`.

## Existing Procedural Textures Reference

| Source | Token | Key Characteristics |
|--------|-------|---------------------|
| `GeneratedBrick` | `generated_brick` | 12-course brick grid layout; per-brick color variation via `hash21`; mortar seams via `smooth01`; chip/soot weathering via non-tileable `fbm` on brick-local UV; rougher than smooth (~0.80) |
| `GeneratedStone` | `generated_stone` | Multi-octave `fbm` layering for macro/mid/micro variation; sinusoidal vein patterns; damp patches with cool color shift; mineral specks; roughness 0.72–0.88 |
| `GeneratedSmooth` | `generated_smooth` | All `tileableFbm` — no non-tileable noise; minimal height (±0.004); warm institutional wall surface (0.82–0.88); lowest roughness of the three (0.68–0.78) |

## Material Definition Fields Reference

| Field | Type | Typical Values | Notes |
|-------|------|----------------|-------|
| `id` | string | `my_texture_default` | Must match the key used in ContentRegistry and scene files |
| `parent` | string | `masonry_base` | Inherits unset fields from parent definition |
| `shading_model` | MaterialKind | `stone`, `brick`, `wood`, `metal` | Controls shader branching for material type |
| `procedural_source` | MaterialProceduralSource | `generated_brick`, `generated_stone`, `generated_smooth` | Triggers CPU generation + caching |
| `uv_mode` | MaterialUvMode | `world_projected`, `mesh` | `world_projected` requires tileable noise |
| `uv_scale` | vec2 | `0.18 0.18` to `0.55 0.55` | Smaller values = larger pattern in world space |
| `normal_strength` | float | `0.4`–`1.0` | Applied in shader; do not bake strength into texture |
| `roughness_scale` | float | `1.0` | Multiplier on texture roughness channel |
| `roughness_bias` | float | `0.0`–`0.05` | Additive bias on roughness |
| `ao_strength` | float | `0.8`–`1.0` | Blend factor between full AO and no AO |
| `light_tint_response` | float | `0.08`–`0.20` | How strongly the material tints from colored lights |
| `emissive_strength` | float | `0.0` default | Non-zero + bloom = perceived light emission |

## Anti-Patterns

**Never do these:**

- Generate textures larger than 512x512 — diminishing returns at this art style and cache cost grows with square of size
- Use saturated or vivid colors — the aesthetic is warm and muted; maximum saturation is ~0.15 in HSL terms
- Skip the height field / normal map pass — flat normals look wrong under area lights and the CSM shadow maps
- Hardcode texture unit bindings — the renderer handles texture binding via `RenderMaterialData` uniform fields
- Bypass `AssetCache` — always wire new sources through `ensureTextureSet()` or the first launch takes 200-400ms generating textures
- Use non-tileable `fbm` for world-projected UV mode textures — visible seams appear at every tile boundary
- Use the same seed for multiple noise layers — visually redundant layering; shift seeds explicitly (e.g., `glm::vec2(7.2f, 1.3f)` vs `glm::vec2(4.7f, 11.6f)`)
- Set `localHeight` to zero for all pixels — the height field drives the normal map; a flat height gives a flat (identity) normal map that ignores surface micro-detail
- Write the height value directly to roughness or AO — they are independent channels with different semantic ranges
