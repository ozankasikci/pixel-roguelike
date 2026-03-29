---
phase: quick
plan: 260329-wgh
type: execute
wave: 1
depends_on: []
files_modified:
  - assets/scenes/warden_office.scene
  - assets/defs/environments/default.environment
  - src/game/levels/prison/PrisonAssets.cpp
autonomous: false
---

<objective>
Rework the warden office scene to match the Stanley Parable office aesthetic. The previous quick task (260329-uy6) brightened surface tints and expanded light radii, but the scene still lacks the defining visual elements of a Stanley Parable office: recessed fluorescent-style ceiling light panels, large bright windows flooding the room with daylight, and the specific warm beige/cream institutional color palette.

This task adds the geometry, lighting, and color tuning needed to achieve the target look:
1. Recessed rectangular ceiling light fixtures (fluorescent panel style, warm white, evenly distributed)
2. Large bright windows on the back wall (bright white daylight fill, replacing the small barred prison window)
3. Warm beige/cream walls, tan/brown carpet floor
4. Soft, even, warm illumination -- institutional/corporate liminal quality

Purpose: Transform the warden office from a prison-style room into a Stanley Parable-quality office environment.
Output: Updated scene file, environment profile, and new ceiling light panel mesh.
</objective>

<context>
@assets/scenes/warden_office.scene
@assets/defs/environments/default.environment
@src/game/levels/prison/PrisonAssets.cpp
@src/game/rendering/EnvironmentProfile.cpp
@src/engine/rendering/post/PostProcessParams.h
@.claude/skills/procedural-model-style.md
</context>

<tasks>

<task type="auto">
  <name>Task 1: Add ceiling light panel mesh and large window wall mesh, rework scene geometry and lighting</name>
  <files>
    src/game/levels/prison/PrisonAssets.cpp
    assets/scenes/warden_office.scene
    assets/defs/environments/default.environment
  </files>
  <action>
**A) Create ceiling light panel mesh in PrisonAssets.cpp**

Add `createCeilingLightPanel()` function following the procedural model style guide pattern. This is a recessed rectangular fluorescent panel light -- the defining visual element of Stanley Parable offices.

Dimensions: 0.9m long x 0.3m wide, recessed 0.04m into ceiling. Origin at center of panel, Y=0 is ceiling surface level.
- Outer frame: thin box frame (0.015m wide strips, 0.01m thick) forming a rectangle around the panel opening
- Panel surface: flat box (0.9m x 0.3m, 0.005m thick) recessed 0.035m above frame, representing the translucent light diffuser
- Use `stone_default` material in scene with very bright tint (0.95 0.93 0.88) -- the panel itself glows from the point light placed directly below it

Register as `"ceiling_light_panel"` in `registerPrisonAssets()`.

**B) Create large office window wall mesh in PrisonAssets.cpp**

Add `createPrisonWallLargeWindow()` function. This is a wide office window -- NOT a small barred prison window. Stanley Parable offices have large, bright, clean windows.

Based on `createPrisonWall()` (2m wide, 3.5m tall, 0.2m thick wall panel), but with a large window opening:
- Window opening: 1.4m wide, 1.2m tall, centered at Y=1.8m (eye height to above head)
- Bottom section: 0 to 1.2m (below window)
- Top section: 2.4m to 3.5m (above window)
- Left section: beside window (0.3m wide strip)
- Right section: beside window (0.3m wide strip)
- Window sill: thin ledge at bottom of opening (0.01m thick, 0.06m deep protrusion)
- NO bars -- this is an office, not a cell. Clean open window.
- Optional: single thin mullion (vertical divider, 0.02m wide) in center of window for visual interest

Register as `"prison_wall_large_window"` in `registerPrisonAssets()`.

**C) Rework warden_office.scene**

Keep `environment_profile default` on line 1.

**Floor tiles (nodes 1-12):** Keep positions/scale/rotations identical. Change material to `floor_default` with tint `0.55 0.45 0.32` (warm tan/brown carpet look -- not gray, distinctly brown). Vary slightly across tiles: alternate between `0.55 0.45 0.32` and `0.56 0.46 0.33` for subtle variation.

**Ceiling tiles (nodes 13-24):** Keep positions at Y=3.5. Change tint to `0.92 0.90 0.86` (warm off-white, clean acoustic ceiling tile feel). Material stays `stone_default`.

**Walls (nodes 25-38):**
- Back wall (nodes 25-27, at Z=8.0, facing 180 degrees): Replace the center panel (node 26, currently `prison_wall_window`) with `prison_wall_large_window`. Keep same position/scale/rotation. Tint all back wall panels: `0.82 0.78 0.70` (warm beige/cream).
- Front wall (nodes 28-30, at Z=0.0): Keep as-is but tint `0.82 0.78 0.70`.
- Left wall (nodes 31-34, at X=-3.0): Tint `0.80 0.76 0.68` (slightly darker for depth, still warm beige).
- Right wall (nodes 35-38, at X=3.0): Tint `0.80 0.76 0.68`.

**Furniture:**
- Door (node 39): Keep metal_default, tint `0.40 0.38 0.34` (warm medium gray-brown, office door).
- Desk (node 40): Change to `wood_default`, tint `0.52 0.42 0.28` (warm medium wood -- office desk).
- Warden chair (node 41): Keep `wood_default`, tint `0.50 0.38 0.24` (warm wood chair).
- Cabinet (node 42): Keep `metal_default`, tint `0.55 0.53 0.50` (warm light gray -- office filing cabinet, NOT dark).
- Shelf (node 43): Keep `metal_default`, tint `0.50 0.48 0.44` (warm light gray shelf).
- Baseboards (nodes 44-56): Tint `0.42 0.38 0.32` (warm medium brown, darker than walls as expected for baseboards).

**Lighting -- replace existing lights with Stanley Parable ceiling panel lights:**

Remove the existing spot_light (node 57) and all three point lights (nodes 58-60).

Add 6 ceiling light fixtures, each consisting of a `ceiling_light_panel` mesh placed on the ceiling and a point light directly below it:

Layout: 3 rows (Z=2.0, Z=4.0, Z=6.0) x 2 columns (X=-1.0, X=1.0) -- evenly distributed across the 6x8m room.

For each fixture:
- Mesh: `ceiling_light_panel` at (X, 3.49, Z) with scale (1,1,1) rotation (0,0,0), material `stone_default`, tint `0.98 0.96 0.90` (bright warm white -- the "glowing" panel surface).
- Point light: at (X, 3.2, Z) with color `1.0 0.97 0.90` (warm white), radius `6.0`, intensity `4.0`.

Add 2 window fill lights (simulating daylight through the large back window):
- Light at (0.0, 2.0, 7.5) color `1.0 0.98 0.94` radius `8.0` intensity `5.0` -- main window daylight
- Light at (0.0, 1.2, 7.8) color `1.0 0.98 0.94` radius `6.0` intensity `3.0` -- lower window fill

Keep all existing colliders unchanged (nodes 61-70).
Keep player_spawn unchanged (node 70 -> now renumbered).

**D) Tune environment profile (default.environment)**

Adjust for a bright, warm, evenly-lit indoor scene:
- `exposure 1.0` (natural, not overblown)
- `contrast 1.02` (very slight -- Stanley Parable is low contrast)
- `saturation 0.88` (slightly desaturated -- muted warm tones)
- `bloom_threshold 0.80` (only bright ceiling panels bloom slightly)
- `bloom_intensity 0.06` (subtle bloom)
- `bloom_radius 1.5` (soft bloom spread)
- `vignette_strength 0.08` (barely there)
- `enable_fog false` (indoor office, no fog needed)
- `enable_grain false` (clean image for Stanley Parable look)
- `fog_density 0.005` (minimal, in case fog is re-enabled)
- `fog_start 30.0` (far)
- `lighting_hemi_sky_color 0.50 0.48 0.44` (warm hemisphere, provides ambient fill)
- `lighting_hemi_ground_color 0.15 0.13 0.10` (warm ground bounce)
- `lighting_hemi_strength 0.35` (moderate ambient)
- `split_tone_strength 0.03` (minimal split toning)
- `shadow_tint 0.90 0.88 0.84` (warm shadows, not cool)
- `highlight_tint 0.98 0.96 0.90` (warm highlights)

  </action>
  <verify>
Build the project:
```
cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake --build . --target pixel-roguelike -j8
```
Verify:
- Build succeeds with no errors
- `ceiling_light_panel` and `prison_wall_large_window` meshes registered (grep PrisonAssets.cpp for registerMesh)
- Scene file has 6 ceiling_light_panel mesh entries and 8 point lights (6 ceiling + 2 window)
- Scene file has `prison_wall_large_window` on back wall center
- Environment file has `enable_fog false` and `enable_grain false`
  </verify>
  <done>
Scene file contains: ceiling light panel meshes with warm tints at 6 evenly-spaced ceiling positions, large window wall on back wall, warm beige walls, tan carpet floor, bright warm ceiling, appropriately-tinted furniture. Environment tuned for clean warm indoor look. All point lights use warm white (1.0 0.97 0.90 range), no cool/blue tones anywhere. Build succeeds.
  </done>
</task>

<task type="checkpoint:human-verify" gate="blocking">
  <what-built>Complete Stanley Parable office aesthetic rework: ceiling light panels, large windows, warm color palette, even soft lighting, tuned environment profile.</what-built>
  <how-to-verify>
1. Build and run: `cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake --build . --target pixel-roguelike -j8 && ./pixel-roguelike`
2. Look up at ceiling: should see 6 recessed rectangular light panels (3 rows x 2 columns) arranged evenly. They should appear as bright warm white rectangles against the off-white ceiling.
3. Look at back wall: should see a large bright window (no bars) flooding daylight into the room. The window opening should be noticeably larger than before.
4. Check walls: all walls should be warm beige/cream. No gray, no cool tones. Clean institutional look.
5. Check floor: warm tan/brown carpet tone. Not gray stone-looking.
6. Check furniture: desk should look like warm wood (not dark metal), cabinet and shelf warm light gray (not dark), chair warm wood.
7. Overall lighting: room should feel evenly lit with warm white overhead light, no harsh shadows, no dark corners. The ceiling lights should provide most illumination, with window light as a secondary fill.
8. Overall mood: should feel like a clean, slightly eerie corporate/institutional office -- the Stanley Parable look. Bright, warm, liminal.
  </how-to-verify>
  <resume-signal>Type "approved" or describe what needs adjusting (too bright/dark, wrong colors, lighting issues, etc.)</resume-signal>
</task>

</tasks>

<verification>
- Build succeeds: `cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake --build . --target pixel-roguelike -j8`
- Scene file parseable (game loads without crash)
- Visual appearance matches Stanley Parable office reference
</verification>

<success_criteria>
The warden office visually reads as a Stanley Parable-style office: recessed fluorescent ceiling panels, large bright windows, warm beige walls, tan carpet floor, even soft warm lighting throughout. The room feels institutional, clean, and slightly eerie -- not like a prison cell.
</success_criteria>
