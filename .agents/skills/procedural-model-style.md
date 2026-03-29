---
name: procedural-model-style
description: Style guide and construction rules for procedural 3D models. Use when creating any new mesh asset — walls, furniture, weapons, props, or architectural elements. Ensures visual consistency across all procedural geometry.
---

# Procedural Model Style Guide

Use this skill whenever creating or modifying procedural mesh functions in this project. Every model must follow these rules to maintain visual consistency under the 1-bit dithered rendering pipeline.

## Core Principle

All models are **constructive assemblies of boxes and cylinders**. No smooth curves, no subdivisions, no organic shapes. The 1-bit dithered post-process turns sharp edges into strong visual reads — lean into that.

## Primitives

Only two base primitives exist:

| Primitive | Function | Usage |
|-----------|----------|-------|
| Box | `generateCube(2.0f)` | All rectangular parts. Scale values are **half-extents** (scale 1.0 = 2m total width). |
| Cylinder | `generateCylinder(1.0f, 1.0f, N)` | Bars, columns, barrels, round hardware. Scale X/Z = radius, Y = height. |

**Cylinder segment counts by size:**
- Bars, small hardware (r < 0.02m): 8 segments
- Columns, pipes (r 0.02-0.1m): 12 segments
- Large columns, barrels (r > 0.1m): 16-20 segments
- Decorative arches, capitals: 20-24 segments

**Never** use more than 24 segments. The dither shader hides polygon edges — high segment counts waste triangles for no visual benefit.

## Scale and Units

All measurements are in **meters**. Models are built at real-world scale.

| Category | Reference Dimensions |
|----------|---------------------|
| Wall panel | 2.0m wide, 2.5m tall, 0.2m thick |
| Floor/ceiling tile | 2.0m x 2.0m, 0.1m thick |
| Standard door | 0.9m wide, 2.1m tall |
| Desk | 1.2m x 0.6m, 0.75m top height |
| Chair seat height | 0.45m |
| Shelf | 0.8m wide, 0.25m deep, 0.03m thick |
| Bar/railing | 0.015m radius |
| Small hardware (hinges, handles) | 0.008-0.04m |

## Detail Levels

Detail is created by **layering boxes at different depths**, not by adding geometry complexity. The 1-bit dither shader needs silhouette variation and depth cues, not surface detail.

### Architectural (walls, floors, ceilings)
- **Main volume**: single box at the correct real-world dimensions
- **Panel joints**: thin raised strips (0.005m thick, 0.02m wide) inset 0.04m from edges
- **Frame insets**: 0.02-0.03m wide strips around openings
- Maximum parts: 5-10 per mesh

### Furniture (desks, chairs, cabinets)
- **Structural members**: boxes at real proportions (legs 0.025-0.03m cross-section)
- **Surface detail**: drawer seams (0.004m recessed strips), handle blocks
- **Hardware**: small boxes for handles, hinges, locks (0.008-0.04m scale)
- Maximum parts: 8-15 per mesh

### Props and weapons (small items)
- **Body**: 1-3 main volume boxes
- **Detail**: tapered sections using progressively smaller boxes
- **Hardware**: pommel, guard, grip as separate box parts
- Maximum parts: 5-8 per mesh

### Doors and gates
- **Main slab**: single box
- **Observation windows**: frame strips + cylinder bars
- **Hardware**: hinge plates, handle blocks, lock plates, barrel cylinders
- **Reinforcement**: horizontal strips across surfaces
- Maximum parts: 15-25 per mesh (doors are the most detailed asset type)

## Bevels and Edges

**No bevels.** All edges are sharp 90-degree box/cylinder edges. The 1-bit dithered rendering makes sharp edges read as strong lines — this is the visual signature of the game.

To suggest edge softness without actual bevels:
- Use **chamfer strips**: a thin box (0.005m thick) placed at 45 degrees along an edge
- Use **reveal lines**: a thin recessed box creating a shadow line near an edge
- Use **layered setbacks**: slightly smaller overlapping boxes to create a stepped profile

## Roundness

Round forms are approximated with cylinders, never with subdivided geometry.

- A column is a single cylinder, not a high-poly mesh
- A barrel is a cylinder with slightly wider cylinder caps
- An arch is a chain of cylinders following a semicircular path (`addReliefChain`)
- A ring pull is a single cylinder rotated 90 degrees

**Never approximate roundness with many small boxes.** Use cylinders.

## Construction Pattern

Every procedural mesh function follows this exact pattern:

```cpp
std::unique_ptr<Mesh> createMyAsset() {
    auto cube = generateCube(2.0f);
    auto cylinder = generateCylinder(1.0f, 1.0f, 12);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    auto addCylinder = [&](const glm::vec3& position,
                           const glm::vec3& scale,
                           const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cylinder, makeModel(position, scale, rotation)});
    };

    // Build parts here — comments describe each part's purpose
    // Position = center of the part in local space
    // Scale = half-extents (with 2.0 cube) or radius/height (cylinder)

    addBox(glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.6f, 0.5f, 0.3f));  // Main body

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals,
                                  merged.uvs, merged.tangents, merged.indices);
}
```

### Rules for the pattern:
1. **Always use `generateCube(2.0f)`** — scale values are half-extents
2. **Origin at bottom-center** — Y=0 is the floor/base of the object
3. **Front face on +Z** — the "pretty" side faces +Z in local space
4. **Comment every part** — describe what it represents ("// Left hinge plate")
5. **Group parts logically** — structural first, then detail, then hardware
6. Only use `addCylinder` when the part is actually round (bars, columns, barrels)

## Registration

Register meshes in the appropriate `register*Assets()` function:

```cpp
meshLibrary.registerMesh("category_name", createMyAsset());
```

Naming convention: `category_name` using snake_case. Examples:
- `prison_wall`, `prison_desk`, `prison_door`
- `cathedral_pillar`, `cathedral_arch`
- `weapon_dagger`, `weapon_mace`

## Material Assignment

Each mesh is assigned a `MaterialKind` at the scene level, not in the mesh itself. Available kinds:

| MaterialKind | Use For |
|--------------|---------|
| Stone | Concrete, stone walls, floors, ceilings |
| Wood | Wooden furniture, doors, planks |
| Metal | Iron/steel hardware, bars, industrial furniture |
| Wax | Candles, waxy surfaces |
| Moss | Organic growth, aged surfaces |
| Floor | Floor-specific stone treatment |
| Brick | Brick walls, masonry |
| Viewmodel | Player's hands and held items |

## Tint Colors

Use `RetroPalette` constants for consistent tinting:

| Constant | RGB | Use For |
|----------|-----|---------|
| Stone | 0.37, 0.30, 0.20 | Generic stone surfaces |
| CarvedStone | 0.50, 0.41, 0.27 | Detailed carved stone |
| Iron | 0.10, 0.08, 0.06 | Dark metal, prison bars |
| PaleSteel | 0.54, 0.49, 0.40 | Lighter metal surfaces |
| OldWood | 0.29, 0.15, 0.06 | Aged wooden surfaces |
| Flagstone | 0.23, 0.17, 0.10 | Floor stones |

Vary tints slightly (+-0.02) between adjacent tiles/panels to break visual monotony.

## Visual Verification

After creating any new mesh:
1. Add a `ViewerPreset` to `apps/model_viewer/main.cpp`
2. View with `procedural-model-viewer --mesh your_mesh_name`
3. Toggle stylized mode (`TAB`) to verify 1-bit dither reads correctly
4. Check that silhouette edges are clear and detail features cast visible shadows
5. Rotate (`SPACE` for auto-spin) to verify the mesh reads well from all angles

## Anti-Patterns

**Never do these:**
- Subdivide geometry for smoothness — use cylinders instead
- Add more than 25 parts to a single mesh — split into separate meshes
- Use scale values smaller than 0.003m — they disappear under dithering
- Create floating geometry with no visual connection to the main volume
- Use `generateCube(1.0f)` — always use `generateCube(2.0f)` for half-extent scale convention
- Add texture coordinates manually — `generateCube`/`generateCylinder` provide UVs, `mergeMeshes` preserves them
