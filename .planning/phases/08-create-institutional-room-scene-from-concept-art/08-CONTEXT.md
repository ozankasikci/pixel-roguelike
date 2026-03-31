# Phase 8: Create Institutional Room Scene from Concept Art - Context

**Gathered:** 2026-04-01
**Status:** Ready for planning

<domain>
## Phase Boundary

Build a new `.scene` file that recreates a Stanley Parable-inspired institutional corridor room from concept art. The room features warm beige walls, dark brown crown molding and baseboards, a glossy floor, two fluorescent ceiling panels, and three doors on the far wall: an open wooden door with warm light spilling through, a closed gray metal door with an HVAC vent above, and a white chained/padlocked door. Includes a ceiling smoke detector with red LED. New procedural meshes, materials, and an environment profile are created to support the scene.

</domain>

<decisions>
## Implementation Decisions

### Room Geometry & Layout
- **D-01:** Room dimensions are 8m x 6m (medium) with ~4m ceiling height, matching warden_office proportions
- **D-02:** All three doors are on the far (north) wall — player enters from the south wall and faces the choices
- **D-03:** Wooden door (left) is a static open doorway — geometry only, no DoorComponent, warm light spills through
- **D-04:** Metal door (center) and chained door (right) are interactable stubs — add InteractableComponent with "This door is locked" message, but no functional open/close
- **D-05:** Use existing `prison_wall`, `prison_floor`, `prison_ceiling`, `prison_baseboard`, `prison_wall_door` meshes where possible; create new meshes only for elements that don't exist

### Materials & Color Palette
- **D-06:** New `inst_beige_wall` material — warm beige base_color (~0.82, 0.76, 0.68), roughness_bias ~0.78, detail_stone, no texture maps
- **D-07:** New `inst_glossy_floor` material — warm tan base_color (~0.72, 0.65, 0.55), low roughness_bias (~0.30) for high gloss reflective look, detail_stone
- **D-08:** New `inst_dark_trim` material — dark brown base_color (~0.25, 0.18, 0.12), roughness_bias ~0.65 for painted wood trim
- **D-09:** Metal door uses existing `metal_default` with gray tint; wooden door uses `wood_default` with warm wood tint; chained door uses `stone_default` or similar with off-white tint

### Lighting & Atmosphere
- **D-10:** Two fluorescent ceiling panels (reuse `ceiling_light_panel` mesh), centered and evenly spaced on ceiling
- **D-11:** Each ceiling panel has a point light underneath for illumination — warm white color temperature
- **D-12:** One point light placed behind the open wooden doorway for warm light spill onto the floor
- **D-13:** New `institutional` environment profile — warm white ambient (~0.15), no sun/directional light, interior-only (no sky), subtle warm fog

### Props & Detail Elements
- **D-14:** New `inst_hvac_vent` procedural mesh — rectangular grille mounted above metal door, static geometry only, no particle/steam effect
- **D-15:** New `inst_smoke_detector` procedural mesh — small cylinder on ceiling with white material; accompanied by a very small red point light (intensity ~0.5, radius ~0.3) for LED glow
- **D-16:** New `inst_chain_padlock` procedural mesh — 3-4 chain links (torus primitives) + box padlock body, material: metal_default with dark iron tint

### Claude's Discretion
- Exact UV scale values for new materials
- Crown molding mesh design (could be extruded profile or simple box)
- Precise light positions and intensities for best visual match
- Door frame geometry details
- Collider placement for walls and blocked doors

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Scene format & loading
- `assets/scenes/warden_office.scene` — Reference for `.scene` file format: mesh/light/collider entity syntax, material assignment, tinting, node naming
- `src/game/levels/GameAssets.cpp` — Procedural mesh registration pattern; all `registerPrisonAssets()` and `registerDefaults()` meshes

### Material system
- `assets/materials/concrete_wall.material` — Example procedural material without texture maps
- `assets/materials/metal_default.material` — Metal material reference
- `assets/materials/wood_default.material` — Wood material reference

### Environment profiles
- `src/game/content/EnvironmentDefinition.h` — Environment profile structure
- `assets/environments/` — Existing environment definition files (if directory exists)

### Interaction system
- `src/game/components/InteractableComponent.h` — InteractableComponent structure for locked door stubs
- `src/game/systems/InteractionSystem.cpp` — How interaction messages are displayed

### Concept art
- `docs/plans/2026-03-31-concept-art/2026-03-31-14-10-00-first-room-render.png` — The target concept art image

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `prison_wall`, `prison_wall_door`, `prison_floor`, `prison_ceiling`, `prison_baseboard`: Procedural meshes in GameAssets.cpp — reuse directly for room shell
- `ceiling_light_panel`: Existing fluorescent panel mesh — reuse for ceiling lights
- `office_door`: Existing door mesh — may work for the wooden door
- 23 existing `.material` files auto-discovered — new materials just need to be dropped into `assets/materials/`

### Established Patterns
- Scene file format: `mesh <name> <x> <y> <z> <sx> <sy> <sz> <rx> <ry> <rz> material <mat_id> tint <r> <g> <b> node <node_id>`
- Light format: `light <x> <y> <z> <r> <g> <b> <intensity> <radius> node <node_id>`
- Collider format: `collider_box <x> <y> <z> <hx> <hy> <hz> node <node_id>`
- Materials: data-driven `.material` files with `id`, `base_color`, `roughness_bias`, `metalness`, feature flags like `detail_stone`

### Integration Points
- New procedural meshes registered in `GameAssets.cpp` → `registerPrisonAssets()` or new `registerInstitutionalAssets()`
- New `.material` files in `assets/materials/` auto-discovered by ContentRegistry
- New environment profile in content system
- Scene file in `assets/scenes/` loadable by GenericFileScene
- InteractableComponent on metal/chained doors for locked messages

</code_context>

<specifics>
## Specific Ideas

- The concept art is explicitly Stanley Parable-inspired: clean surfaces, warm fluorescent lighting, slightly eerie liminal quality
- The room should feel like an institutional corridor or processing area — not a prison cell
- The open wooden door with warm light is the inviting path; the metal and chained doors create mystery/tension
- The glossy floor should show subtle reflections of the ceiling lights (achieved through low roughness in PBR)
- Dark brown trim (crown molding + baseboards) frames the warm beige walls — this contrast is key to the aesthetic

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 08-create-institutional-room-scene-from-concept-art*
*Context gathered: 2026-04-01*
