---
status: awaiting_human_verify
trigger: "QuestDoorsPack wall and floor textures (qdp_wall, qdp_floor materials) are not tiling at the correct scale in the initial scene. Textures look stretched compared to the original asset pack look."
created: 2026-04-02T00:00:00Z
updated: 2026-04-02T00:02:00Z
---

## Current Focus

hypothesis: CONFIRMED - SM_Wall has square 0-1 UVs on a 250x250 unit quad, but non-uniform scene scale (Y=3x) stretches the texture vertically. Floor texture covers entire 2x2m tile in single repeat.
test: Applied uv_scale 1.0 3.0 to wall material (compensates for 3x vertical stretch), uv_scale 2.0 2.0 to floor (doubles tiling density), created separate world_projected material for prison_wall_door meshes
expecting: Wall texture should now appear square/natural, floor should have finer detail
next_action: User verification -- run level editor and visually inspect

## Symptoms

expected: Wall and floor textures should look like the original QuestDoorsPack cement/concrete textures -- natural block scale, not stretched or overly tiled
actual: Textures appear stretched. uv_scale 1.0 looks stretched, uv_scale 4.0 looked too tiled/repeated. Currently back at 1.0.
errors: No errors -- visual issue only
reproduction: Run the level editor, open initial_scene.scene, look at the walls and floors
started: Started when QuestDoorsPack meshes (SM_Wall.fbx, SM_Floor.fbx) were added to the scene with qdp_wall/qdp_floor materials

## Eliminated

- hypothesis: Non-uniform scale affects UV coordinates at shader level
  evidence: Vertex shader passes aTexCoord through directly as vTexCoord (scene.vert line 49). Fragment shader materialUv() for mesh mode returns vTexCoord * uMaterialUvScale (scene.frag line 270). World transform does not affect UVs.
  timestamp: 2026-04-02T00:00:30Z

## Evidence

- timestamp: 2026-04-02T00:00:10Z
  checked: SM_Wall.fbx UV coordinates via custom uv_inspector tool
  found: Wall mesh is a 4-vertex quad, position range 250x250x0, UV range 0-1 on both U and V axes. Texture maps once across the entire quad.
  implication: With scene scale 0.008/0.024/0.008, wall is 2m wide x 6m tall. Texture mapped 1:1 across 2x6m = 3x vertical stretch.

- timestamp: 2026-04-02T00:00:10Z
  checked: SM_Floor.fbx UV coordinates via custom uv_inspector tool
  found: Floor mesh is a 4-vertex quad, position range 250x250, UV range 0-1. Uniform scale 0.008 = 2m x 2m tile.
  implication: Texture maps once across 2x2m floor tile. No stretch, but single repeat looks too large-scale.

- timestamp: 2026-04-02T00:00:15Z
  checked: SM_DoorA.fbx UV coordinates for reference
  found: Door mesh has positions in ~4x9m range (already at realistic scale), UVs 0-1 (atlas). At scene scale 0.23, door is ~0.9x2.1m. Looks correct.
  implication: Doors work because mesh is at human scale with proper UV atlas. Wall/floor meshes are oversized (250 units) requiring extreme scene scale factors.

- timestamp: 2026-04-02T00:00:20Z
  checked: Wall texture (T_Wall_BaseColor.png) visual content
  found: 4096x4096 texture showing ~3 horizontal rows of concrete panels. Tileable texture.
  implication: Designed for a square mapping. When stretched 3x vertically on the non-uniform wall, concrete blocks look 3x taller than wide.

- timestamp: 2026-04-02T00:00:25Z
  checked: Vertex and fragment shader UV pipeline
  found: scene.vert passes aTexCoord through as vTexCoord. scene.frag materialUv() for mesh mode returns vTexCoord * uMaterialUvScale.
  implication: uv_scale in material directly multiplies the 0-1 baked mesh UVs.

- timestamp: 2026-04-02T00:00:30Z
  checked: prison_wall_door mesh (procedural, was sharing qdp_wall material)
  found: Built from generateCube boxes, each face gets 0-1 UVs independently. Scale 1.0 1.5 1.0 in scene.
  implication: Changing uv_scale on qdp_wall to 1.0 3.0 would cause 3x vertical tiling on each box face. Created separate qdp_wall_door material with world_projected UV mode to handle this correctly.

## Resolution

root_cause: SM_Wall.fbx has UVs spanning 0-1 on a square 250x250 unit quad. Scene applies non-uniform scale (X=0.008 Y=0.024 Z=0.008, Y is 3x X/Z) making the wall 2m wide x 6m tall. Since UVs are square but the wall is 3x taller than wide, the texture is vertically stretched by factor 3. SM_Floor is uniformly scaled but the texture covers the entire 2x2m tile in a single repeat, making the concrete pattern appear too large.
fix: Set qdp_wall uv_scale to 1.0 3.0 (compensates for 3x vertical stretch). Set qdp_floor uv_scale to 2.0 2.0 (tiles texture 2x for finer detail). Created new qdp_wall_door material with world_projected UV mode and uv_scale 0.5 0.5 for the prison_wall_door procedural meshes, since they have per-face UVs incompatible with the wall panel UV correction. Updated scene file to reference qdp_wall_door for the 3 prison_wall_door entries.
verification: Build succeeds. Awaiting visual verification by user.
files_changed:
  - assets/materials/qdp_wall.material
  - assets/materials/qdp_floor.material
  - assets/materials/qdp_wall_door.material (new)
  - assets/scenes/initial_scene.scene
