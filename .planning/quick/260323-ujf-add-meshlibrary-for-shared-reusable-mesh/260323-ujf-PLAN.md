---
phase: quick
plan: 260323-ujf
type: execute
wave: 1
depends_on: []
files_modified:
  - src/engine/rendering/MeshLibrary.h
  - src/engine/rendering/MeshLibrary.cpp
  - src/game/scenes/CathedralScene.h
  - src/game/scenes/CathedralScene.cpp
  - CMakeLists.txt
autonomous: true
requirements: [quick-meshlibrary]

must_haves:
  truths:
    - "Standard mesh types (cube, plane, cylinder variants) are defined once and stored centrally"
    - "Multiple entities referencing the same mesh geometry share a single Mesh instance"
    - "CathedralScene creates zero duplicate Mesh objects for identical geometry"
    - "The rendered scene looks identical before and after the refactor"
  artifacts:
    - path: "src/engine/rendering/MeshLibrary.h"
      provides: "MeshLibrary class with get/register interface"
      exports: ["MeshLibrary"]
    - path: "src/engine/rendering/MeshLibrary.cpp"
      provides: "MeshLibrary implementation with standard primitive registration"
    - path: "src/game/scenes/CathedralScene.cpp"
      provides: "Refactored scene using MeshLibrary instead of per-entity unique_ptr<Mesh>"
  key_links:
    - from: "src/game/scenes/CathedralScene.cpp"
      to: "src/engine/rendering/MeshLibrary.h"
      via: "MeshLibrary::get() calls to retrieve shared Mesh*"
      pattern: "meshLibrary.*get"
    - from: "src/engine/rendering/MeshLibrary.cpp"
      to: "src/engine/rendering/Mesh.h"
      via: "Creates Mesh instances using Mesh constructor and static factories"
      pattern: "Mesh::create|std::make_unique<Mesh>"
    - from: "CMakeLists.txt"
      to: "src/engine/rendering/MeshLibrary.cpp"
      via: "engine_rendering static library source list"
      pattern: "MeshLibrary\\.cpp"
---

<objective>
Create a MeshLibrary class that acts as a central registry for shared, reusable Mesh instances. Refactor CathedralScene to use MeshLibrary instead of creating duplicate Mesh objects for identical geometry.

Purpose: Currently CathedralScene creates ~50+ individual Mesh objects, many of which are identical geometry (e.g., every wall, step, and tile line creates its own unit cube). A MeshLibrary eliminates this waste and provides a clean pattern for all future scenes.
Output: MeshLibrary.h/.cpp in engine/rendering, refactored CathedralScene, updated CMakeLists.txt.
</objective>

<execution_context>
@/Users/ozan/.claude/get-shit-done/workflows/execute-plan.md
@/Users/ozan/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@src/engine/rendering/Mesh.h
@src/engine/rendering/Mesh.cpp
@src/game/scenes/CathedralScene.h
@src/game/scenes/CathedralScene.cpp
@src/game/components/MeshComponent.h
@CMakeLists.txt

<interfaces>
<!-- Key types and contracts the executor needs -->

From src/engine/rendering/Mesh.h:
```cpp
class Mesh {
public:
    Mesh(const std::vector<glm::vec3>& positions,
         const std::vector<glm::vec3>& normals,
         const std::vector<uint32_t>& indices);
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    void draw() const;
    static Mesh createCube(float size);
    static Mesh createPlane(float size);
};
```

From src/game/components/MeshComponent.h:
```cpp
struct MeshComponent {
    Mesh* mesh = nullptr;     // non-owning pointer
    glm::mat4 modelOverride;
    bool useModelOverride = false;
};
```

From src/game/scenes/CathedralScene.h:
```cpp
class CathedralScene : public Scene {
    std::vector<std::unique_ptr<Mesh>> meshes_;   // owns mesh memory
    std::vector<entt::entity> entities_;
};
```
</interfaces>
</context>

<tasks>

<task type="auto">
  <name>Task 1: Create MeshLibrary class and register it in the build</name>
  <files>src/engine/rendering/MeshLibrary.h, src/engine/rendering/MeshLibrary.cpp, CMakeLists.txt</files>
  <action>
Create `src/engine/rendering/MeshLibrary.h`:
- Class `MeshLibrary` with:
  - `void registerMesh(const std::string& name, std::unique_ptr<Mesh> mesh)` -- stores a named mesh
  - `Mesh* get(const std::string& name) const` -- returns raw pointer (non-owning), returns nullptr if not found
  - `bool has(const std::string& name) const` -- check existence
  - `void registerDefaults()` -- registers the standard primitives
- Private storage: `std::unordered_map<std::string, std::unique_ptr<Mesh>> meshes_`
- Non-copyable, non-movable (owns GL resources via Mesh unique_ptrs)
- Include `<string>`, `<memory>`, `<unordered_map>` and forward-declare Mesh

Create `src/engine/rendering/MeshLibrary.cpp`:
- Implement all methods
- `registerDefaults()` creates and registers these standard meshes:
  - `"cube"` -- `Mesh::createCube(1.0f)` (unit cube, scenes scale via modelOverride)
  - `"plane"` -- `Mesh::createPlane(1.0f)` (unit plane, scenes scale as needed)
  - `"cylinder"` -- cylinder with radius=1.0, height=1.0, 12 segments (unit cylinder, scaled by scenes)
  - `"cylinder_wide"` -- cylinder with radius=1.5, height=1.0, 12 segments (for pillar bases -- the 1.5x ratio matches CathedralScene's pillarRadius*1.5f pattern)
  - `"cylinder_cap"` -- cylinder with radius=1.4, height=1.0, 12 segments (for pillar capitals -- the 1.4x ratio matches CathedralScene's pillarRadius*1.4f pattern)
- Move the `createCylinder` helper function from CathedralScene.cpp into MeshLibrary.cpp as a static local helper (same implementation: stacked rings with bottom/top caps). Use it in registerDefaults().
- IMPORTANT: The cylinder meshes should be created at UNIT scale (radius=1, height=1 for "cylinder"; radius=1.5, height=1 for "cylinder_wide"; radius=1.4, height=1 for "cylinder_cap") so scenes can scale them via modelOverride transforms. This avoids needing many cylinder variants for different absolute sizes.

Update `CMakeLists.txt`:
- Add `src/engine/rendering/MeshLibrary.cpp` to the `engine_rendering` static library source list (after Mesh.cpp).
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake .. -DPython_EXECUTABLE=$(pwd)/../.venv/bin/python3 && cmake --build . 2>&1 | tail -20</automated>
  </verify>
  <done>MeshLibrary.h and MeshLibrary.cpp exist with the full API. CMakeLists.txt includes MeshLibrary.cpp in engine_rendering. Project compiles successfully.</done>
</task>

<task type="auto">
  <name>Task 2: Refactor CathedralScene to use MeshLibrary</name>
  <files>src/game/scenes/CathedralScene.h, src/game/scenes/CathedralScene.cpp</files>
  <action>
Refactor CathedralScene to eliminate duplicate mesh creation by using MeshLibrary.

Update `src/game/scenes/CathedralScene.h`:
- Remove `#include "engine/rendering/Mesh.h"` (no longer owns meshes directly)
- Add forward declaration of `class MeshLibrary` or include MeshLibrary.h
- Replace `std::vector<std::unique_ptr<Mesh>> meshes_` with `MeshLibrary meshLibrary_` (scene owns its MeshLibrary instance -- this keeps the same lifetime semantics where the scene owns the mesh memory, entities hold raw pointers, and meshes are destroyed in onExit when the scene clears)
- Keep `std::vector<entt::entity> entities_`

Update `src/game/scenes/CathedralScene.cpp`:
- Remove the `createCylinder` static helper function (moved to MeshLibrary.cpp)
- In `onEnter()`, call `meshLibrary_.registerDefaults()` at the top
- Replace every `std::make_unique<Mesh>(Mesh::createCube(1.0f))` with `meshLibrary_.get("cube")`
- Replace every `std::make_unique<Mesh>(Mesh::createPlane(corridorLength))` with `meshLibrary_.get("plane")`
  - NOTE: The current code creates planes at `corridorLength` size (30.0f). With the unit plane from MeshLibrary, the modelOverride scale must be adjusted. Currently the floor does: translate then scale by (corridorWidth/corridorLength, 1, 1). With a unit plane, the floor modelOverride should translate to (0, 0, -corridorLength*0.5) then scale by (corridorWidth/2, 1, corridorLength/2) since the unit plane is 1x1 (extends -0.5 to 0.5). Similarly for ceiling.
- Replace `createCylinder(pillarRadius, wallHeight, 12)` with `meshLibrary_.get("cylinder")` and adjust the pillar modelOverride to include scaling by (pillarRadius, wallHeight, pillarRadius). The unit cylinder has radius=1, height=1.
- Replace `createCylinder(pillarRadius * 1.5f, 0.3f, 12)` (pillar base) with `meshLibrary_.get("cylinder_wide")` and adjust modelOverride to scale by (pillarRadius, 0.3f, pillarRadius) -- the 1.5x ratio is baked into the "cylinder_wide" mesh itself.
- Replace `createCylinder(pillarRadius * 1.4f, 0.25f, 12)` (pillar capital) with `meshLibrary_.get("cylinder_cap")` and adjust modelOverride to scale by (pillarRadius, 0.25f, pillarRadius) -- the 1.4x ratio is baked into the "cylinder_cap" mesh.
- The `addArch` helper already creates cubes -- update it to accept a `MeshLibrary&` parameter instead of `std::vector<std::unique_ptr<Mesh>>&`. It should use `meshLibrary.get("cube")` instead of creating `std::make_unique<Mesh>(Mesh::createCube(1.0f))`. Remove the meshes vector parameter; it no longer pushes mesh ownership. Keep the registry and entities parameters.
- Remove ALL `meshes_.push_back(std::move(...))` calls since mesh ownership is now in MeshLibrary.

In `onExit()`:
- Keep entity cleanup as-is
- Replace `meshes_.clear()` with clearing/resetting the MeshLibrary (either a `clear()` method, or just let the MeshLibrary destructor handle it when the scene is destroyed, or call `meshLibrary_ = MeshLibrary{}` to reset). Simplest: add a `void clear()` method to MeshLibrary that clears the unordered_map.

CRITICAL: The rendered scene MUST look identical. Every modelOverride matrix must produce the same final transform. Double-check the math:
- Unit cube (1x1x1, centered at origin): scale by desired size, then translate -- same as before since old code used createCube(1.0f) anyway
- Unit plane (1x1, extends -0.5 to 0.5 on XZ): floor was createPlane(30), which extends -15 to 15. New unit plane extends -0.5 to 0.5, so scale by (corridorWidth, 1, corridorLength) and translate by (0, 0, -corridorLength*0.5). But check: the old floor had `scale(corridorWidth/corridorLength, 1, 1)` applied AFTER `createPlane(corridorLength)` which was 30x30. So old floor was 30*(corridorWidth/30) = corridorWidth wide, 30*1 = 30 long. New floor with unit plane: scale by (corridorWidth/2, 1, corridorLength/2) then translate. Actually -- just scale by (corridorWidth, 1, corridorLength) since the unit plane goes from -0.5 to 0.5, so scaling by L gives -L/2 to L/2, matching the old createPlane(L) range of -L/2 to L/2. So: translate(0, 0, -corridorLength*0.5) then scale(corridorWidth, 1, corridorLength). Wait -- the old code was: createPlane(corridorLength)=30x30 plane, then translate(0,0,-15), then scale(8/30, 1, 1). That gives a plane: x range = 30 * (8/30) / 2 = +/-4 (=corridorWidth/2), z range = 30/2 = +/-15, shifted by -15 so z range = -30 to 0. With unit plane (1x1): translate(0,0,-15) then scale(corridorWidth, 1, corridorLength) gives x range = +/-(corridorWidth/2)=+/-4, z range = +/-0.5*corridorLength = +/-15, shifted by -15 = -30 to 0. Correct.
- Unit cylinder (radius=1, height=1): old pillar was createCylinder(0.35, 6, 12). New: get("cylinder") + scale(0.35, 6, 0.35) in modelOverride. translate stays the same.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake --build . 2>&1 | tail -20</automated>
  </verify>
  <done>CathedralScene uses MeshLibrary for all mesh access. Zero std::make_unique&lt;Mesh&gt; calls remain in CathedralScene.cpp. The scene compiles and the rendered output is visually identical (same geometry, same transforms).</done>
</task>

</tasks>

<verification>
1. Project builds cleanly with no warnings related to MeshLibrary or CathedralScene
2. Run the application and visually confirm the cathedral scene looks identical (pillars, walls, arches, floor, ceiling, steps, tile lines all present and correctly positioned)
3. Grep CathedralScene.cpp for `make_unique<Mesh>` -- should return zero matches
4. Grep CathedralScene.cpp for `meshLibrary_` -- should show multiple get() calls
</verification>

<success_criteria>
- MeshLibrary class exists in src/engine/rendering/ with get/register/registerDefaults API
- CathedralScene.cpp has zero duplicate mesh allocations -- all mesh access goes through MeshLibrary
- The project compiles and links successfully
- The rendered cathedral scene is visually identical to before the refactor
</success_criteria>

<output>
After completion, create `.planning/quick/260323-ujf-add-meshlibrary-for-shared-reusable-mesh/260323-ujf-SUMMARY.md`
</output>
