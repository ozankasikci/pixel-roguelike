---
phase: quick
plan: 260330-rwe
type: execute
wave: 1
depends_on: []
files_modified:
  - .claude/skills/procedural-texture-style.md
  - .agents/skills/procedural-texture-style.md
autonomous: true
requirements: []
must_haves:
  truths:
    - "Skill file exists in .claude/skills/ and .agents/skills/ with matching content"
    - "Skill covers all existing MaterialKind texture types (Stone, Wood, Metal, Wax, Moss, Floor, Brick, Viewmodel)"
    - "Skill documents the four-channel output pattern (albedo RGBA8, normal RGBA8, roughness R8, AO R8)"
    - "Skill documents noise primitives (valueNoise, fbm, tileable variants) and helper functions"
    - "Skill documents the ProceduralPixelData struct and generate*Pixels() pattern"
    - "Skill covers material definition integration (MaterialProceduralSource enum, .material files)"
    - "Skill covers AssetCache disk caching for procedural textures"
    - "Skill references the Stanley Parable aesthetic with warm muted color guidance"
  artifacts:
    - path: ".claude/skills/procedural-texture-style.md"
      provides: "Complete procedural texture generation skill"
      min_lines: 150
    - path: ".agents/skills/procedural-texture-style.md"
      provides: "Mirror copy for agents directory"
      min_lines: 150
  key_links:
    - from: ".claude/skills/procedural-texture-style.md"
      to: "src/game/rendering/MaterialTextureLibrary.cpp"
      via: "Documents the generate*Pixels() pattern and noise functions"
      pattern: "generateBrickPixels|generateStonePixels|generateSmoothWallPixels"
    - from: ".claude/skills/procedural-texture-style.md"
      to: "src/game/rendering/MaterialDefinition.h"
      via: "Documents MaterialProceduralSource enum values"
      pattern: "MaterialProceduralSource"
---

<objective>
Create a Claude Code skill file for procedural texture generation, following the established pattern from procedural-model-style.md.

Purpose: Give Claude a comprehensive reference for generating new procedural textures that are visually consistent with the game's Stanley Parable-inspired aesthetic, properly integrated into the material pipeline, and correctly cached on disk.

Output: .claude/skills/procedural-texture-style.md (and mirror at .agents/skills/)
</objective>

<execution_context>
@.claude/get-shit-done/workflows/execute-plan.md
@.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@.planning/STATE.md
@.claude/skills/procedural-model-style.md
@src/game/rendering/MaterialTextureLibrary.h
@src/game/rendering/MaterialTextureLibrary.cpp
@src/game/rendering/MaterialDefinition.h
@src/game/rendering/MaterialKind.h
@src/game/rendering/RetroPalette.h
@assets/materials/brick_default.material
@assets/materials/stone_default.material
@assets/materials/concrete_wall.material
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create the procedural texture style skill</name>
  <files>.claude/skills/procedural-texture-style.md, .agents/skills/procedural-texture-style.md</files>
  <action>
Create a comprehensive skill file at `.claude/skills/procedural-texture-style.md` that documents the procedural texture generation system. Follow the same structural pattern as `procedural-model-style.md` (YAML frontmatter with name/description, then markdown sections).

The skill must cover these sections, derived from the actual codebase:

**1. Frontmatter**
- name: procedural-texture-style
- description: Style guide and generation rules for procedural textures. Use when creating any new procedural texture — stone, brick, wood, metal, or other material surface patterns. Ensures visual consistency and correct pipeline integration.

**2. Core Principle**
- All procedural textures generate a four-channel texture set: albedo (RGBA8), normal (RGBA8), roughness (R8), AO (R8)
- Textures are generated as CPU-side pixel arrays (ProceduralPixelData struct), then uploaded to GL textures
- Every texture must tile seamlessly — use tileable noise functions for patterns that repeat at UV boundaries

**3. Output Format**
Document the ProceduralPixelData struct from MaterialTextureLibrary.h:
```cpp
struct ProceduralPixelData {
    std::vector<std::uint8_t> albedo;     // RGBA8, size*size*4
    std::vector<std::uint8_t> normal;     // RGBA8, size*size*4
    std::vector<std::uint8_t> roughness;  // R8, size*size
    std::vector<std::uint8_t> ao;         // R8, size*size
    int size;
};
```
- Standard texture size: 512x512 (constexpr kSize = 512)
- Normal maps are derived from a height field using `sampleHeightNormal()` in a second pass after the main pixel loop
- Height-to-normal strength factor: 3.2f (controls bump intensity)

**4. Noise Primitives**
Document all available noise functions from the anonymous namespace in MaterialTextureLibrary.cpp:
| Function | Signature | Purpose |
| hash21 | `float hash21(const glm::vec2& p)` | Pseudo-random float from 2D coordinate |
| valueNoise | `float valueNoise(const glm::vec2& p)` | Smooth value noise with hermite interpolation |
| fbm | `float fbm(glm::vec2 p)` | 4-octave fractional Brownian motion (non-tileable) |
| tileableValueNoise | `float tileableValueNoise(const glm::vec2& p, float period, const glm::vec2& seed)` | Value noise that wraps at given period |
| tileableFbm | `float tileableFbm(glm::vec2 p, float baseFreq, const glm::vec2& seed)` | 4-octave tileable fBm |
| smooth01 | `float smooth01(float edge0, float edge1, float x)` | Hermite smoothstep mapped to [0,1] |

Include guidance: use `tileableFbm`/`tileableValueNoise` for any texture that will be applied with `uv_mode world_projected` or tiled via UV scale. Use non-tileable `fbm` only for per-brick/per-tile variation where the noise is consumed within a single cell.

**5. Color Palette**
Reference RetroPalette.h constants. Document the Stanley Parable aesthetic rules for textures:
- Warm, muted base tones (beiges, warm grays, soft yellows)
- Vary per-element colors slightly (+-0.02 to +-0.05) to break monotony
- Soot/weathering darkens with cool shift (multiply by vec3(0.88, 0.85, 0.83))
- Pale/highlight variants warm slightly (multiply by vec3(1.05, 1.03, 1.00))
- Never use saturated colors — keep everything desaturated and institutional

**6. Texture Generation Pattern**
Document the exact code pattern every generate*Pixels() function follows:
```cpp
ProceduralPixelData MaterialTextureLibrary::generateMyTexturePixels() const {
    constexpr int kSize = 512;
    ProceduralPixelData result;
    result.size = kSize;
    result.albedo.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.normal.resize(static_cast<size_t>(kSize * kSize * 4), 255);
    result.roughness.resize(static_cast<size_t>(kSize * kSize), 255);
    result.ao.resize(static_cast<size_t>(kSize * kSize), 255);
    std::vector<float> height(static_cast<size_t>(kSize * kSize), 0.0f);

    // Pass 1: Generate albedo, roughness, AO, and height
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
            result.ao[pixelIndex] = toByte(localAo);
            height[pixelIndex] = localHeight;
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

**7. Material Pipeline Integration**
Document the full registration flow for a new procedural texture:

Step 1 — Add enum value to `MaterialProceduralSource` in MaterialDefinition.h:
```cpp
enum class MaterialProceduralSource {
    None = 0,
    GeneratedBrick = 1,
    GeneratedStone = 2,
    GeneratedSmooth = 3,
    GeneratedMyTexture = 4,  // New entry
};
```

Step 2 — Add token parsing in MaterialDefinition.cpp (`tryParseMaterialProceduralSourceToken` and `materialProceduralSourceToken`).

Step 3 — Add `generateMyTexturePixels()` method declaration to MaterialTextureLibrary.h and implementation to .cpp.

Step 4 — Wire the new source in `ensureTextureSet()` in MaterialTextureLibrary.cpp (add else-if branch alongside GeneratedBrick/GeneratedStone/GeneratedSmooth).

Step 5 — Create .material file in `assets/materials/` referencing the new procedural_source token.

Step 6 — Update `useProceduralDetail` assignment in `resolve()` if the new texture should use direct sampling (no textureNoTile anti-tiling).

**8. PBR Value Ranges**
Document typical value ranges for each channel, extracted from existing textures:

| Channel | Typical Range | Notes |
| Albedo R/G/B | 0.60-0.95 | Warm muted tones; never pure white or black |
| Height | -0.03 to +0.02 | Very subtle displacement; mortar/seams are negative, face is positive |
| Roughness | 0.65-0.95 | Most surfaces are rough; smooth only for polished metal (0.3-0.5) |
| AO | 0.82-1.0 | Subtle darkening in crevices; mortar joints ~0.86, open faces ~0.96-1.0 |
| Normal strength | Via material definition | normalStrength 0.4-1.0 in .material file, not baked into the texture |

**9. Disk Cache Integration**
Document that procedural textures are automatically cached via AssetCache:
- Cache key: string from `textureKeyFor()` (procedural source enum + map paths)
- Hash: `AssetCache::hashBytes()` on the key string
- Four cache entries per texture set: `key + "_albedo"`, `key + "_normal"`, `key + "_roughness"`, `key + "_ao"`
- Cache check happens in `ensureTextureSet()` before generation
- No action needed from the texture author — caching is automatic if the generation is wired into `ensureTextureSet()`

**10. Existing Procedural Textures Reference**
Table of existing implementations with key characteristics:

| Source | Token | Characteristics |
| GeneratedBrick | generated_brick | Course/brick grid layout, mortar seams, per-brick color variation, chip/soot weathering |
| GeneratedStone | generated_stone | Multi-octave noise layering, vein patterns, damp patches, mineral specks |
| GeneratedSmooth | generated_smooth | Tileable fBm only, minimal height, warm institutional wall surface |

**11. Anti-Patterns**
- Never generate textures larger than 512x512 — diminishing returns at this art style
- Never use saturated or vivid colors — the aesthetic is warm and muted
- Never skip the height field / normal map pass — flat normals look wrong under area lights
- Never hardcode texture unit bindings — the renderer handles texture binding
- Never bypass the AssetCache — always wire new sources through ensureTextureSet()
- Never use non-tileable noise for world_projected UV mode textures — visible seams will appear

After creating the file at `.claude/skills/procedural-texture-style.md`, copy it identically to `.agents/skills/procedural-texture-style.md` to maintain the mirror pattern used by the existing model skill.
  </action>
  <verify>
    <automated>test -f /Users/ozan/Projects/gsd-3d-roguelike/.claude/skills/procedural-texture-style.md && test -f /Users/ozan/Projects/gsd-3d-roguelike/.agents/skills/procedural-texture-style.md && diff /Users/ozan/Projects/gsd-3d-roguelike/.claude/skills/procedural-texture-style.md /Users/ozan/Projects/gsd-3d-roguelike/.agents/skills/procedural-texture-style.md && wc -l /Users/ozan/Projects/gsd-3d-roguelike/.claude/skills/procedural-texture-style.md | awk '{if ($1 >= 150) print "PASS: " $1 " lines"; else print "FAIL: only " $1 " lines"}'</automated>
  </verify>
  <done>
    - Skill file exists at both paths with identical content
    - File is at least 150 lines covering all 11 sections
    - Follows same YAML frontmatter + markdown structure as procedural-model-style.md
    - Documents all noise primitives, the four-channel output format, generation pattern, material pipeline integration, PBR value ranges, disk cache, color palette, and anti-patterns
  </done>
</task>

</tasks>

<verification>
- Both skill files exist and are identical: `diff .claude/skills/procedural-texture-style.md .agents/skills/procedural-texture-style.md`
- Skill has YAML frontmatter with name and description fields
- Skill references actual codebase types: ProceduralPixelData, MaterialProceduralSource, MaterialTextureLibrary, AssetCache
- Skill documents all three existing generators: generateBrickPixels, generateStonePixels, generateSmoothWallPixels
- Skill covers the full integration flow (enum, parser, generator, wiring, .material file)
</verification>

<success_criteria>
A Claude executor reading only this skill file can create a new procedural texture type (e.g., wood grain, metal plate) that:
1. Produces correct four-channel output (albedo, normal, roughness, AO)
2. Uses the right noise functions for the UV mode
3. Stays within the warm/muted color palette
4. Integrates properly into the material pipeline (enum, parser, generator, material file)
5. Gets automatically disk-cached without extra work
</success_criteria>

<output>
After completion, create `.planning/quick/260330-rwe-create-a-claude-code-skill-for-procedura/260330-rwe-SUMMARY.md`
</output>
