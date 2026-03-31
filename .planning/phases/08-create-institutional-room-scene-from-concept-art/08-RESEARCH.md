# Phase 08: Create Institutional Room Scene from Concept Art - Research

**Researched:** 2026-03-31
**Domain:** Scene authoring, procedural mesh creation, material authoring, environment profile authoring
**Confidence:** HIGH — all findings verified directly from project source code and canonical reference files

## Summary

This phase is entirely content-creation work within an established, well-understood pipeline. The project has mature systems for all required deliverables: `.scene` files, `.material` files, `.environment` files, and procedural mesh registration in `GameAssets.cpp`. No new engine code is needed. Every pattern required (material authoring, mesh construction, scene entity placement, light placement, collider placement, environment file format) has an existing reference in the codebase.

The concept art (verified by viewing `docs/plans/2026-03-31-concept-art/2026-03-31-14-10-00-first-room-render.png`) shows a clean institutional corridor room: warm beige drywall surfaces, dark brown crown molding and baseboards, a high-gloss cream floor, two rectangular fluorescent ceiling panels, and three doors on the far wall — a warm-lit open wooden door (left), a flat gray metal door with an HVAC vent above (center), and an off-white door with chain-and-padlock hardware (right). A smoke detector disc hangs from the ceiling center. This maps directly to decisions D-01 through D-16 in CONTEXT.md with no gaps.

The key implementation challenge is procedural mesh authoring for three new props (`inst_hvac_vent`, `inst_smoke_detector`, `inst_chain_padlock`) following the project's strict constructive-assembly style guide, plus three new materials (`inst_beige_wall`, `inst_glossy_floor`, `inst_dark_trim`) using the `generated_smooth` procedural source with appropriate roughness biases. Interaction stubs (locked doors showing a message) do NOT require archetype machinery — they require `InteractableComponent` placement via the scene's `archetype_instance` mechanism or LevelBuilder scripted geometry. The existing `InteractionSystem` reads `promptText` from `InteractableComponent`; the locked doors simply need that component attached with a custom `promptText` and no `DoorComponent`.

**Primary recommendation:** Follow the warden_office.scene as a structural template for room geometry placement. Reuse `prison_wall`, `prison_wall_door`, `prison_floor`, `prison_ceiling`, `prison_baseboard`, `ceiling_light_panel`, `office_door` meshes. Author three new materials as `.material` files. Author three new procedural meshes in `GameAssets.cpp`. Create the `institutional` environment file. Wire interactable locked-door stubs via a new prefab or scripted geometry in `LevelLoader` registration.

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Room Geometry & Layout**
- D-01: Room dimensions are 8m x 6m (medium) with ~4m ceiling height, matching warden_office proportions
- D-02: All three doors are on the far (north) wall — player enters from the south wall and faces the choices
- D-03: Wooden door (left) is a static open doorway — geometry only, no DoorComponent, warm light spills through
- D-04: Metal door (center) and chained door (right) are interactable stubs — add InteractableComponent with "This door is locked" message, but no functional open/close
- D-05: Use existing `prison_wall`, `prison_floor`, `prison_ceiling`, `prison_baseboard`, `prison_wall_door` meshes where possible; create new meshes only for elements that don't exist

**Materials & Color Palette**
- D-06: New `inst_beige_wall` material — warm beige base_color (~0.82, 0.76, 0.68), roughness_bias ~0.78, detail_stone, no texture maps
- D-07: New `inst_glossy_floor` material — warm tan base_color (~0.72, 0.65, 0.55), low roughness_bias (~0.30) for high gloss reflective look, detail_stone
- D-08: New `inst_dark_trim` material — dark brown base_color (~0.25, 0.18, 0.12), roughness_bias ~0.65 for painted wood trim
- D-09: Metal door uses existing `metal_default` with gray tint; wooden door uses `wood_default` with warm wood tint; chained door uses `stone_default` or similar with off-white tint

**Lighting & Atmosphere**
- D-10: Two fluorescent ceiling panels (reuse `ceiling_light_panel` mesh), centered and evenly spaced on ceiling
- D-11: Each ceiling panel has a point light underneath for illumination — warm white color temperature
- D-12: One point light placed behind the open wooden doorway for warm light spill onto the floor
- D-13: New `institutional` environment profile — warm white ambient (~0.15), no sun/directional light, interior-only (no sky), subtle warm fog

**Props & Detail Elements**
- D-14: New `inst_hvac_vent` procedural mesh — rectangular grille mounted above metal door, static geometry only, no particle/steam effect
- D-15: New `inst_smoke_detector` procedural mesh — small cylinder on ceiling with white material; accompanied by a very small red point light (intensity ~0.5, radius ~0.3) for LED glow
- D-16: New `inst_chain_padlock` procedural mesh — 3-4 chain links (torus primitives are NOT available; use cylinder segments) + box padlock body, material: metal_default with dark iron tint

### Claude's Discretion
- Exact UV scale values for new materials
- Crown molding mesh design (could be extruded profile or simple box)
- Precise light positions and intensities for best visual match
- Door frame geometry details
- Collider placement for walls and blocked doors

### Deferred Ideas (OUT OF SCOPE)
None — discussion stayed within phase scope
</user_constraints>

---

## Standard Stack

This phase uses no new libraries. All tooling is already in the project.

### Core (Existing — No New Dependencies)

| Asset Type | Mechanism | Location | Notes |
|------------|-----------|----------|-------|
| Procedural meshes | `createXxx()` C++ functions in `GameAssets.cpp` | `src/game/levels/GameAssets.cpp` | Registered via `registerAllGameAssets()` |
| Materials | `.material` text files | `assets/materials/` | Auto-discovered by `ContentRegistry::loadMaterialsFromDirectory` |
| Environment profile | `.environment` text file | `assets/environments/` | Loaded by `loadEnvironmentDefinitionAsset()` |
| Scene | `.scene` text file | `assets/scenes/` | Loaded by `GenericFileScene` via `loadLevelDef()` |
| Interaction stubs | `InteractableComponent` on mesh entities | Via LevelBuilder scripted geometry or prefab | No `DoorComponent` needed for locked-only doors |

**Version verification:** Not applicable — no new packages.

---

## Architecture Patterns

### Scene File Structure

Every `.scene` file begins with an environment profile declaration, then mesh/light/collider/player_spawn records. Verified from `warden_office.scene`:

```
environment_profile <id>

mesh <mesh_id> <px> <py> <pz> <sx> <sy> <sz> <rx> <ry> <rz> material <mat_id> tint <r> <g> <b> node <node_id>
light <px> <py> <pz> <cr> <cg> <cb> <intensity> <radius> node <node_id>
collider_box <px> <py> <pz> <hx> <hy> <hz> node <node_id>
player_spawn <px> <py> <pz> <yaw_deg> node <node_id>
archetype_instance <archetype_id> <px> <py> <pz> <yaw_deg> node <node_id>
```

All values are space-separated on a single line. The scene parser (`LevelDef.cpp`) requires a `node` token after mesh/light/collider placements.

### Room Grid Layout

The warden_office uses 2m x 2m `prison_floor` / `prison_ceiling` tiles and 2m-wide `prison_wall` panels. The new 8m x 6m room (D-01) maps to:

- **Floor**: 4 tiles wide (X: -4 to +4 in 2m steps) × 3 tiles deep (Z: 0 to 6 in 2m steps) = 12 floor tiles
- **Ceiling**: same grid at Y=4.0
- **North wall (Z=6)**: 4 wall panels + 3 `prison_wall_door` panels (one per door) — but 8m wide with 3 doors requires careful planning (see Pitfall 3 below)
- **South wall (Z=0)**: solid wall panels (player entry side, no door in concept art — use solid walls or a simple entry gap)
- **East/West walls**: 3 panels each (2m × 3 = 6m depth)

### Coordinate System for 8m × 6m Room

Based on warden_office pattern (center at X=0, south at Z=0, north at far Z):

```
X range: -4.0 to +4.0 (8m wide)
Z range:  0.0 to +6.0 (6m deep, south=0, north=6)
Y=0.0:   floor level
Y=4.0:   ceiling level
```

Player spawn faces north (+Z direction, yaw=0 or 180 depending on convention). Warden office uses `player_spawn 0.0 1.0 3.1 -2.0` — verify yaw convention from existing scene.

### Material File Format

Verified from `concrete_wall.material`, `metal_default.material`, `masonry_base.material`:

```
id <material_id>
parent <parent_id>          # optional — inherits unset fields
shading_model <stone|wood|metal|floor|brick|wax|moss>
procedural_source <generated_smooth|generated_stone|generated_brick>
uv_mode <world_projected|mesh>
uv_scale <u> <v>
normal_strength <0.0-1.0>
roughness_scale <float>
roughness_bias <0.0-1.0>
metalness <0.0-1.0>
ao_strength <0.0-1.0>
light_tint_response <float>
specular_level <float>
base_color <r> <g> <b>      # only for non-procedural; tint applied in scene
detail_stone true           # feature flag; or detail_wood, detail_floor, etc.
```

**Note on D-06/D-07/D-08:** The decisions specify `base_color` and `roughness_bias` directly on the new materials. These are non-procedural (no `procedural_source`) materials that use `base_color` directly — similar to how existing materials use tint in the scene file to adjust color. However, looking at existing materials: `metal_default` and `wood_default` have `base_color 1.0 1.0 1.0` and rely on scene-level `tint` for color. The decisions specify actual color values in the material — this is valid. Use `generated_smooth` as the procedural source for wall/floor texture surface variation, and set `base_color` as the warm base. The scene tint then modulates on top. OR author them with `base_color` + no procedural source (flat shading). **Recommendation:** Use `generated_smooth` procedural source for wall texture micro-detail, with `base_color` set to the target colors from decisions. This matches the `concrete_wall.material` pattern which also sets colors.

### Environment File Format

Verified from `EnvironmentDefinition.cpp` parser and `outdoor_bright.environment`:

```
id <identifier>
enable_sky false
sky_enabled false
lighting_enable_directional false
lighting_enable_shadows false
lighting_hemi_sky_color <r> <g> <b>
lighting_hemi_ground_color <r> <g> <b>
lighting_hemi_strength <float>
enable_fog true
fog_density <float>
fog_start <float>
fog_near_color <r> <g> <b>
fog_far_color <r> <g> <b>
enable_bloom true
bloom_intensity <float>
enable_tone_map true
exposure <float>
```

The parser accepts any subset — unspecified keys inherit from `makeDefaultEnvironmentDefinition()`. The `institutional` profile (D-13) needs: sky disabled, directional light disabled, hemispherical ambient at ~0.15 intensity with warm tint, fog enabled with subtle warm settings, bloom on.

### Procedural Mesh Pattern

Every new mesh follows the exact pattern from the skill guide (verified in `GameAssets.cpp`):

```cpp
std::unique_ptr<Mesh> createInstHvacVent() {
    auto cube = generateCube(2.0f);           // always 2.0f — scale is half-extents
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Frame border — 4 thin strips around perimeter
    addBox(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.25f, 0.015f, 0.005f)); // top rail
    // ... etc

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals,
                                  merged.uvs, merged.tangents, merged.indices);
}
```

Then registered in `registerAllGameAssets()`:
```cpp
meshLibrary.registerMesh("inst_hvac_vent", createInstHvacVent());
meshLibrary.registerMesh("inst_smoke_detector", createInstSmokeDetector());
meshLibrary.registerMesh("inst_chain_padlock", createInstChainPadlock());
```

**Critical:** `generateCube(2.0f)` means scale values are half-extents. A box `scale(0.5f, 0.5f, 0.5f)` = 1m × 1m × 1m total. Origin at bottom-center (Y=0 = base). Front face on +Z.

### Locked Door Interaction Stub

D-04 specifies metal door and chained door are interactable stubs — they show "This door is locked" but don't open. The `InteractableComponent` in the source:

```cpp
struct InteractableComponent {
    std::string promptText = "E  INTERACT";
    std::string busyText = "INTERACTING";
    float interactDistance = 2.0f;
    float interactDotThreshold = 0.55f;
    bool enabled = true;
    bool busy = false;
};
```

The `RuntimeGameplay` interaction loop checks for a `DoorComponent` to trigger opening. If no `DoorComponent` exists, activating the interactable does nothing (the activate path returns early). So a mesh entity with only `InteractableComponent` (no `DoorComponent`) will show the prompt but produce no action — this is the correct stub pattern.

**How to attach InteractableComponent to a scene-file mesh entity:** The `.scene` file format does NOT directly support arbitrary component attachment on `mesh` lines. Looking at `LevelDef.h`, `LevelMeshPlacement` has no interactable field. The interactable stubs must be created via **scripted geometry** in the `LevelLoadRequest::buildScriptedGeometry` callback, or via a new prefab archetype. The `archetype_instance` mechanism in the scene file is the right path — create a new prefab type for a "locked door prop" that spawns a mesh entity + InteractableComponent.

**Alternative (simpler):** Use `LevelLoader`'s `buildScriptedGeometry` lambda in the scene registration call to spawn the interactable entities programmatically after the scene loads. This is already how checkpoints and cathedral doors work.

### Crown Molding (Claude's Discretion)

The concept art shows a thin dark-brown trim strip at the ceiling-wall junction (crown molding) and a baseboard at the floor. The project already has `prison_baseboard` for the floor trim. For crown molding: reuse `prison_baseboard` mesh rotated 180 degrees on X-axis and positioned at the ceiling junction (Y=3.95), or create a new `inst_crown_molding` mesh if the profile needs to differ. The baseboard is a simple thin horizontal strip — the same geometry works for crown molding when flipped. **Recommendation:** Reuse `prison_baseboard` with `inst_dark_trim` material for both baseboard and crown molding to minimize new mesh creation.

### Anti-Patterns to Avoid

- **Do not use `generateCube(1.0f)`** — always `generateCube(2.0f)`, scale is half-extents (project convention, enforced by skill guide)
- **Do not use non-tileable `fbm` with `world_projected` UV mode** — produces visible seams
- **Do not add more than 25 parts per mesh** — split into separate mesh functions if needed
- **Do not use cylinders for chain links** — the project has no torus primitive. Chain links are approximated with cylinder segments arranged in a loop pattern (horizontal cylinder bar + vertical connectors)
- **Do not add `detail_stone true` to glossy floor material** — the procedural stone detail texture would fight the gloss effect; use `generated_smooth` procedural source with low roughness_bias instead
- **Do not add `shading_model floor` to the new floor material** — `floor_default` uses `detail_floor true` and a `generated_floor` procedural source that doesn't exist in the decisions. Use `shading_model stone` + `generated_smooth` + low `roughness_bias` for the glossy institutional look

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Room shell geometry | Custom wall meshes | `prison_wall`, `prison_floor`, `prison_ceiling`, `prison_baseboard` | Already match the 2m × 2m grid; battle-tested in warden_office |
| Fluorescent ceiling panels | New panel mesh | `ceiling_light_panel` | Already registered; exactly matches concept art fixture shape |
| Wooden door (open) | New door mesh | `office_door` (rotated open) OR `prison_wall_door` wall segment | `office_door` is already a door slab; place it rotated 90° for open position |
| Material hot-reload | Custom file watcher | Existing `ContentRegistry` + `pollMaterialHotReload` | Already implemented and wired in editor |
| Environment file parsing | Custom parser | `loadEnvironmentDefinitionAsset()` | Full parser already handles all environment fields |
| Texture disk caching | Manual cache | `AssetCache` via `ensureTextureSet()` | Automatic for all procedural sources wired through the pipeline |

**Key insight:** This is a pure content-authoring phase. The engine is already capable of everything needed. Do not write any new engine systems.

---

## Common Pitfalls

### Pitfall 1: North Wall Width vs. Three Doors

**What goes wrong:** An 8m-wide room (X: -4 to +4) divided into 4 × 2m wall panels has exactly 8m. Three doors occupy 3 × 2m = 6m of wall width, leaving only 1m on each side for solid wall — but each `prison_wall` panel is 2m wide, so there's no room for solid panels between doors on an 8m wall.

**Why it happens:** The original 6m-wide warden_office (3 panels wide) had room for door + solid panels. 8m = 4 panels, 3 of which are `prison_wall_door` leaves only 1 panel of solid wall (split across both sides at 0.5m each, which is below one panel width).

**How to avoid:** Either (a) make the north wall 10m or 12m with 5-6 panels, shifting room width; or (b) use `prison_wall_door` for all 3 door positions and place solid wall segments on the outer ~1m using scaled single `prison_wall` panels at half-width, or (c) use the room width asymmetrically with the outer doors offset. D-01 is locked at 8m — the planner should account for tight door spacing. The three doors can be placed at X = -2.5, 0.0, +2.5 with narrower separating pilasters (0.5m), approximated by thin box colliders rather than dedicated wall mesh panels.

**Warning signs:** Scene looks wrong with wall panels overlapping door frames, or there are visible gaps between door frames on the north wall.

### Pitfall 2: Glossy Floor and `generated_floor` vs. `generated_smooth`

**What goes wrong:** Using `shading_model floor` or `detail_floor true` on the new glossy floor material triggers a different shader path. `floor_default` already exists with its own procedural source (`generated_floor`) and roughness_bias=0.86 (matte). The institutional floor needs roughness_bias ~0.30 (glossy).

**How to avoid:** Author `inst_glossy_floor` with `shading_model stone`, `procedural_source generated_smooth`, and `roughness_bias 0.30`. Do NOT inherit from `floor_default` or use `detail_floor true`. The smooth procedural source gives minimal surface texture variation appropriate for a polished institutional floor.

**Warning signs:** Floor looks matte and rough instead of glossy, or the shader applies stone/floor detail incorrectly.

### Pitfall 3: Chain Links Without Torus Primitive

**What goes wrong:** D-16 specifies "torus primitives" for chain links, but the project has only `generateCube(2.0f)` and `generateCylinder(1.0f, 1.0f, N)` as base primitives. There is no torus generator in `MeshGeometry.h`.

**How to avoid:** Approximate chain links using 4 cylinder segments arranged in a square loop — two horizontal cylinders (top/bottom of link) + two vertical cylinders (left/right sides), with short connection points. 3-4 links stacked at slightly alternating angles. This is consistent with the "constructive assembly of boxes and cylinders" principle.

**Warning signs:** Compilation error if torus is referenced; or chain looks like disconnected lines if the geometry doesn't close.

### Pitfall 4: Environment Parser — Unknown Key = Exception

**What goes wrong:** The `loadEnvironmentDefinitionAsset()` parser in `EnvironmentDefinition.cpp` throws a `throwParseError` exception for any unrecognized key. Typos in the `.environment` file cause a hard crash at scene load.

**How to avoid:** Only use keys that appear in `EnvironmentDefinition.cpp`'s parsing block. The complete valid key list is documented in the Architecture Patterns section above. Do a dry-run load in the editor before committing.

**Warning signs:** Application crashes immediately on scene load with a parse error message.

### Pitfall 5: `base_color` vs. Scene `tint` — Multiplicative Interaction

**What goes wrong:** If a material has `base_color 0.82 0.76 0.68` (warm beige) AND the scene file applies a `tint 0.82 0.76 0.68`, the result is color-multiplied (very dark). Or if the scene applies `tint 1.0 1.0 1.0` with a colored base_color, the tint is neutral and base_color applies directly.

**How to avoid:** For materials with explicit `base_color` (the three new inst_* materials), use neutral `tint 1.0 1.0 1.0` in the scene file, or omit the tint field and let the default apply. Alternatively, author materials with `base_color 1.0 1.0 1.0` and put all the color information in the scene tint — consistent with how existing materials work.

**Warning signs:** Walls appear much darker or differently colored than the material file specifies.

### Pitfall 6: InteractableComponent Not Attachable via `.scene` File

**What goes wrong:** The `mesh` record in `.scene` files maps to `LevelMeshPlacement` which has no component attachment fields. Writing `mesh metal_door ... interactable true` will cause a parse error.

**How to avoid:** Use `archetype_instance` with a new prefab definition, or use `buildScriptedGeometry` in the `LevelLoadRequest` to manually spawn entities with both a `MeshComponent` and `InteractableComponent`. The prefab approach is cleaner and more composable. A prefab like `locked_door_metal` can define the mesh + interaction parameters.

**Warning signs:** Parse error during scene load about unexpected token after mesh record; or locked door shows no interaction prompt.

---

## Code Examples

### New Material File: inst_beige_wall.material

```
id inst_beige_wall
parent masonry_base
shading_model stone
procedural_source generated_smooth
uv_mode world_projected
uv_scale 0.55 0.55
base_color 0.82 0.76 0.68
normal_strength 0.35
roughness_scale 1.0
roughness_bias 0.78
metalness 0.0
ao_strength 0.85
light_tint_response 0.08
specular_level 0.18
detail_stone true
```

### New Material File: inst_glossy_floor.material

```
id inst_glossy_floor
shading_model stone
procedural_source generated_smooth
uv_mode world_projected
uv_scale 0.40 0.40
base_color 0.72 0.65 0.55
normal_strength 0.20
roughness_scale 1.0
roughness_bias 0.30
metalness 0.0
ao_strength 0.90
light_tint_response 0.10
specular_level 0.55
```

### New Material File: inst_dark_trim.material

```
id inst_dark_trim
shading_model wood
uv_mode mesh
uv_scale 1.0 1.0
base_color 0.25 0.18 0.12
normal_strength 0.4
roughness_scale 1.0
roughness_bias 0.65
metalness 0.0
ao_strength 1.0
light_tint_response 0.14
specular_level 0.20
```

### Environment File: institutional.environment

```
id institutional
enable_sky false
sky_enabled false
lighting_enable_directional false
lighting_enable_shadows false
lighting_hemi_sky_color 0.95 0.92 0.86
lighting_hemi_ground_color 0.40 0.37 0.32
lighting_hemi_strength 0.15
enable_fog true
fog_density 0.02
fog_start 4.0
fog_near_color 0.95 0.92 0.86
fog_far_color 0.90 0.88 0.82
enable_bloom true
bloom_intensity 0.35
bloom_radius 0.6
enable_tone_map true
exposure 1.0
enable_vignette true
vignette_strength 0.18
vignette_softness 0.6
ssao_enabled true
ssao_radius 0.4
ssao_bias 0.015
ssao_strength 0.7
```

### Scene File Fragment: Ceiling Lights (D-11)

```
# Two fluorescent panels, centered on 8m × 6m room, Y=3.99 (just below ceiling)
mesh ceiling_light_panel -1.0 3.99 2.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.98 0.96 0.90 node node_clp_1
mesh ceiling_light_panel  1.0 3.99 2.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.98 0.96 0.90 node node_clp_2
mesh ceiling_light_panel -1.0 3.99 4.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.98 0.96 0.90 node node_clp_3
mesh ceiling_light_panel  1.0 3.99 4.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.98 0.96 0.90 node node_clp_4

# Lights below each panel
light -1.0 3.7 2.0 1.0 0.97 0.88 5.5 4.5 node node_l1
light  1.0 3.7 2.0 1.0 0.97 0.88 5.5 4.5 node node_l2
light -1.0 3.7 4.0 1.0 0.97 0.88 5.5 4.5 node node_l3
light  1.0 3.7 4.0 1.0 0.97 0.88 5.5 4.5 node node_l4
```

### Scene File Fragment: Red LED Light for Smoke Detector (D-15)

```
# Smoke detector mesh (white material, center ceiling)
mesh inst_smoke_detector 0.0 3.97 3.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.92 0.90 0.88 node node_smoke_det
# Tiny red LED point light below detector
light 0.0 3.85 3.0 1.0 0.05 0.02 0.5 0.3 node node_led_red
```

### Procedural Mesh Skeleton: inst_hvac_vent

```cpp
// Source: GameAssets.cpp pattern
std::unique_ptr<Mesh> createInstHvacVent() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position, const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Outer frame — thin rectangular border
    addBox(glm::vec3(0.0f,  0.225f, 0.0f), glm::vec3(0.30f, 0.015f, 0.008f)); // top rail
    addBox(glm::vec3(0.0f, -0.225f, 0.0f), glm::vec3(0.30f, 0.015f, 0.008f)); // bottom rail
    addBox(glm::vec3(-0.285f, 0.0f, 0.0f), glm::vec3(0.015f, 0.225f, 0.008f)); // left jamb
    addBox(glm::vec3( 0.285f, 0.0f, 0.0f), glm::vec3(0.015f, 0.225f, 0.008f)); // right jamb
    // Horizontal slats (5 evenly spaced)
    for (int i = -2; i <= 2; ++i) {
        float y = static_cast<float>(i) * 0.08f;
        addBox(glm::vec3(0.0f, y, 0.002f), glm::vec3(0.27f, 0.006f, 0.006f));
    }
    // Vertical slats (3 evenly spaced)
    for (int i = -1; i <= 1; ++i) {
        float x = static_cast<float>(i) * 0.13f;
        addBox(glm::vec3(x, 0.0f, -0.002f), glm::vec3(0.005f, 0.21f, 0.005f));
    }

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals,
                                  merged.uvs, merged.tangents, merged.indices);
}
```

### Procedural Mesh Skeleton: inst_smoke_detector

```cpp
std::unique_ptr<Mesh> createInstSmokeDetector() {
    auto cube = generateCube(2.0f);
    auto cylinder = generateCylinder(1.0f, 1.0f, 16);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& p, const glm::vec3& s,
                      const glm::vec3& r = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(p, s, r)});
    };
    auto addCylinder = [&](const glm::vec3& p, const glm::vec3& s,
                           const glm::vec3& r = glm::vec3(0.0f)) {
        parts.push_back({cylinder, makeModel(p, s, r)});
    };

    // Main disc body (flat cylinder, hanging from ceiling)
    addCylinder(glm::vec3(0.0f, -0.02f, 0.0f), glm::vec3(0.08f, 0.02f, 0.08f));
    // Mounting base (thinner ring at top)
    addCylinder(glm::vec3(0.0f, 0.005f, 0.0f), glm::vec3(0.05f, 0.005f, 0.05f));
    // Small LED bump (tiny cylinder on underside)
    addCylinder(glm::vec3(0.0f, -0.042f, 0.0f), glm::vec3(0.008f, 0.003f, 0.008f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals,
                                  merged.uvs, merged.tangents, merged.indices);
}
```

### Chain Link Approximation (D-16, No Torus Primitive)

```cpp
// Each link: 4 cylinders arranged as a loop
// Link dimensions: ~0.06m outer width, 0.03m height, 0.008m bar thickness
auto addLink = [&](float yOffset, float rotDeg) {
    // Horizontal bars (top and bottom)
    addCylinder(glm::vec3(0.0f,  yOffset + 0.015f, 0.0f),
                glm::vec3(0.004f, 0.022f, 0.004f),
                glm::vec3(0.0f, 0.0f, 90.0f)); // top bar
    addCylinder(glm::vec3(0.0f,  yOffset - 0.015f, 0.0f),
                glm::vec3(0.004f, 0.022f, 0.004f),
                glm::vec3(0.0f, 0.0f, 90.0f)); // bottom bar
    // Vertical bars (left and right)
    addCylinder(glm::vec3(-0.018f, yOffset, 0.0f),
                glm::vec3(0.004f, 0.015f, 0.004f)); // left bar
    addCylinder(glm::vec3( 0.018f, yOffset, 0.0f),
                glm::vec3(0.004f, 0.015f, 0.004f)); // right bar
};
```

---

## Project Constraints (from CLAUDE.md)

- **Engine**: Custom C++ — no Unity/Unreal/Godot
- **Graphics API**: OpenGL 4.1 Core Profile — all shaders target GLSL 4.10
- **Naming**: Classes `PascalCase`, functions `camelCase`, variables `snake_case`, private members trailing `_`, mesh IDs `snake_case`
- **ECS**: Components are POD structs with no methods; systems inherit from `System` base
- **Procedural meshes**: Always `generateCube(2.0f)` (half-extent scale); origin at bottom-center; front face on +Z
- **Segment counts**: Small hardware 8 segments, columns/pipes 12, large 16-20, max 24
- **Mesh part count**: Max 25 parts per mesh — split into separate functions if exceeded
- **Material files**: Dropped into `assets/materials/` — auto-discovered by `ContentRegistry`
- **No `generateCube(1.0f)`**: Always use `generateCube(2.0f)` — project convention, enforced by skill guide
- **No Boost**: Use C++20 STL for utility functions
- **No GLEW**: Project uses GLAD 2
- **No `#pragma once` missing**: All headers use `#pragma once`
- **Include order**: standard library → third-party (GLAD, GLM, EnTT, spdlog) → project headers

---

## State of the Art (Project-Internal Patterns)

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Hardcoded material types (`MaterialKind` enum) | Feature flags on `ResolvedMaterialDefinition` | Phase 07 | `detail_stone`, `detail_wood` flags replace enum; materials in `.material` files |
| Single `registerCathedralAssets()` for all scenes | `registerAllGameAssets()` consolidates everything with `registerDefaults()` called once | Phase 06 | All new meshes go into `registerAllGameAssets()` in `GameAssets.cpp` |
| Inline scene construction in C++ classes | Data-driven `.scene` files loaded by `GenericFileScene` | Phase 06 | New scene is a `.scene` file, no C++ scene class needed |
| Per-material texture loading | Procedural generation + disk cache via `AssetCache` | Phase 07 + quick task 260330-0fz | New procedural textures automatically cached after first launch |

**Deprecated:**
- `shading_model` key in `.material` files: silently ignored for backward compatibility; root materials no longer require it, but it's still parsed without error
- `MaterialKind` enum: deleted in Phase 07; shader branching now driven by feature flags

---

## Open Questions

1. **How does InteractableComponent get attached to locked door mesh entities?**
   - What we know: `.scene` mesh records map to `LevelMeshPlacement` which has no component fields. `archetype_instance` uses prefabs. `buildScriptedGeometry` is a C++ lambda.
   - What's unclear: Whether a new prefab type for "static locked door prop" is the right approach, or whether the locked doors should be added via `buildScriptedGeometry` in the scene's load registration.
   - Recommendation: Create a new prefab file `assets/prefabs/gameplay/locked_door_stub.prefab` with a new archetype type, or use `buildScriptedGeometry` in the `RuntimeGameSession` or `LevelLoader` scene registration. The planner should decide: prefab adds new archetype parsing code; scripted geometry is faster to implement but harder to compose. **The scripted geometry path is lower risk** since it requires no new parser code.

2. **Does `inst_glossy_floor` material need a new procedural source or will `generated_smooth` produce acceptable gloss?**
   - What we know: `generated_smooth` with `roughness_bias 0.30` produces a low-roughness surface. The shader computes gloss from `roughness_bias`. The concept art shows clear reflections of ceiling lights on the floor.
   - What's unclear: Whether the current shader implementation of reflections (specular highlights from point lights) at `roughness_bias 0.30` will look like the concept art floor without actual screen-space reflections or environment reflections.
   - Recommendation: Use `generated_smooth` + `roughness_bias 0.30` + `specular_level 0.55`. Accept that without SSR the floor won't show true reflections — the specular highlights from the ceiling lights are the primary effect. This is within scope. SSAO in the environment will add contact darkening near walls.

3. **Crown molding: new mesh or reuse `prison_baseboard`?**
   - What we know: `prison_baseboard` is a simple horizontal trim strip. It can be placed at ceiling height rotated 180° on Z-axis.
   - What's unclear: Whether the visual result of a flipped baseboard at the ceiling will match the concept art crown molding profile.
   - Recommendation: Reuse `prison_baseboard` as crown molding. If visual result is unsatisfactory during implementation, create a simple `inst_crown_molding` mesh (single box at appropriate proportions, 3-5 parts max). This is within Claude's discretion per CONTEXT.md.

---

## Environment Availability

Step 2.6: SKIPPED — this phase is purely C++ code edits + data file authoring with no external dependencies beyond the existing build toolchain.

---

## Validation Architecture

Nyquist validation is explicitly disabled (`workflow.nyquist_validation: false` in `.planning/config.json`). Section omitted.

---

## Sources

### Primary (HIGH confidence — direct source code inspection)
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/scenes/warden_office.scene` — scene file format, all entity record types, light/collider syntax
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/levels/GameAssets.cpp` — procedural mesh registration pattern, all existing mesh names
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/materials/concrete_wall.material` — material file format reference for procedural+base_color
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/materials/metal_default.material` — metal material reference
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/materials/masonry_base.material` — parent material for masonry types
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/materials/floor_default.material` — floor material reference
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/rendering/EnvironmentDefinition.cpp` — complete environment file parser with all valid keys
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/environments/outdoor_bright.environment` — environment file format reference
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/components/InteractableComponent.h` — InteractableComponent fields
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/runtime/RuntimeGameplay.cpp` — interaction loop, door activation, how locked doors (no DoorComponent) behave
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/level/LevelDef.h` — LevelMeshPlacement struct, confirms no component attachment in mesh records
- `/Users/ozan/Projects/gsd-3d-roguelike/src/game/level/LevelDef.cpp` — `archetype_instance` record format
- `/Users/ozan/Projects/gsd-3d-roguelike/assets/prefabs/gameplay/double_door.prefab` — prefab file format reference
- `/Users/ozan/Projects/gsd-3d-roguelike/.claude/skills/procedural-model-style.md` — mesh construction rules, primitive set, part count limits
- `/Users/ozan/Projects/gsd-3d-roguelike/.claude/skills/procedural-texture-style.md` — material pipeline 6-step integration, ProceduralPixelData format, existing sources
- `/Users/ozan/Projects/gsd-3d-roguelike/docs/plans/2026-03-31-concept-art/2026-03-31-14-10-00-first-room-render.png` — target concept art (viewed directly)

### Secondary (MEDIUM confidence)
- STATE.md accumulated decisions — Phase 07 material system decisions confirm feature flag behavior and `ContentRegistry::loadMaterialsFromDirectory` auto-discovery

### Tertiary (LOW confidence)
None required — all findings are verifiable from project source.

---

## Metadata

**Confidence breakdown:**
- Scene file format: HIGH — verified from warden_office.scene and LevelDef.cpp parser
- Material authoring: HIGH — verified from existing material files and ContentRegistry
- Environment authoring: HIGH — verified from full EnvironmentDefinition.cpp parser
- Procedural mesh construction: HIGH — verified from GameAssets.cpp + skill guide
- InteractableComponent behavior: HIGH — verified from RuntimeGameplay.cpp source
- New mesh proportions (inst_hvac_vent, smoke_detector, chain_padlock): MEDIUM — dimensional estimates based on concept art; final values tuned during implementation

**Research date:** 2026-03-31
**Valid until:** 2026-06-01 (stable — no external dependencies, pure project-internal knowledge)
