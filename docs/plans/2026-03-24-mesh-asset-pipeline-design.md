# Mesh Asset Pipeline: glTF Mesh Files for Composite Objects

**Date:** 2026-03-24
**Status:** Approved
**Scope:** Pillars + arches (extensible to more objects later)

## Problem

Pillars and arches are built inline in `CathedralScene.cpp` as multiple primitives with hardcoded transforms. They can't be isolated, reused, or inspected outside the engine.

## Decision Summary

| Decision | Choice |
|----------|--------|
| Mesh format | Single combined mesh per object (vertices baked) |
| File format | glTF binary (.glb) via tinygltf |
| Generation | Offline build tool produces .glb files |
| Scope | Pillars + arches now, more objects later |
| Tool location | `tools/mesh_generator.cpp` (single file) |

## Design

### 1. Extracted Geometry Header

**New file: `src/engine/rendering/MeshGeometry.h`**

Pure-math vertex/index generation, no OpenGL dependency. Shared by engine and tool.

```cpp
struct RawMeshData {
    std::vector<float> vertices;   // interleaved: pos.xyz + normal.xyz
    std::vector<uint32_t> indices;
};

RawMeshData generateCube(float size);
RawMeshData generatePlane(float size);
RawMeshData generateCylinder(float radius, float height, int segments);
RawMeshData mergeMeshes(const std::vector<std::pair<RawMeshData, glm::mat4>>& parts);
```

`mergeMeshes` applies each transform to positions and normals, reindexes, and returns a single combined `RawMeshData`.

Existing `Mesh::createCube()`, `createPlane()`, `createCylinder()` become thin wrappers that call these functions then upload to OpenGL.

### 2. Mesh Generator Tool

**`tools/mesh_generator.cpp`** — standalone executable.

- Reuses geometry functions from `MeshGeometry.h`
- Combines sub-meshes with local transforms into single vertex buffers
- Writes glTF binary via tinygltf
- Outputs:
  - `assets/meshes/pillar.glb` (shaft + base + capital)
  - `assets/meshes/arch.glb` (curved arch segments)

CMake target: `add_executable(mesh_generator tools/mesh_generator.cpp)`

### 3. Runtime glTF Loader

**New: `src/engine/rendering/GltfLoader.h/.cpp`**

```cpp
class GltfLoader {
public:
    static std::unique_ptr<Mesh> load(const std::string& filepath);
};
```

- Loads `.glb` via tinygltf
- Extracts POSITION (vec3) + NORMAL (vec3) + indices
- Returns a `Mesh` with data uploaded to OpenGL
- Only handles single-primitive meshes (our tool controls output)
- No materials, textures, or animations

**MeshLibrary addition:**

```cpp
void MeshLibrary::loadFromFile(const std::string& name, const std::string& filepath);
```

### 4. Scene Changes

`CathedralScene.cpp` replaces procedural pillar/arch generation with mesh lookups:

```cpp
meshLibrary_.loadFromFile("pillar", "assets/meshes/pillar.glb");
meshLibrary_.loadFromFile("arch", "assets/meshes/arch.glb");

for (each pillar position) {
    auto m = glm::translate(glm::mat4(1.0f), position);
    entities_.push_back(spawnMesh(registry, pillarMesh, m));
}
```

- Each pillar: 1 entity instead of 3+
- Each arch: 1 entity instead of N cube segments
- Fewer draw calls, simpler scene code
- Collision stays as cylinder/box approximations (Jolt doesn't need visual mesh)
- Walls, floors, steps, tile lines unchanged (still primitives)

### 5. File Layout

```
tools/
  mesh_generator.cpp          # NEW

src/engine/rendering/
  MeshGeometry.h              # NEW
  GltfLoader.h                # NEW
  GltfLoader.cpp              # NEW
  Mesh.cpp                    # MODIFIED — delegates to MeshGeometry.h
  MeshLibrary.h               # MODIFIED — add loadFromFile()
  MeshLibrary.cpp             # MODIFIED

src/game/scenes/
  CathedralScene.cpp          # MODIFIED

assets/meshes/
  pillar.glb                  # GENERATED
  arch.glb                    # GENERATED

CMakeLists.txt                # MODIFIED — tinygltf FetchContent, mesh_generator target
```

### 6. Build Workflow

1. `cmake --build . --target mesh_generator`
2. `./mesh_generator` — writes .glb files to `assets/meshes/`
3. `cmake --build . --target roguelike` — game loads .glb at runtime

### 7. Dependencies Added

- **tinygltf** (header-only, MIT license, via FetchContent) — used by both mesh_generator and GltfLoader

### 8. What's NOT Changing

- Shaders (mesh data format pos+normal is identical)
- Renderer / RenderSystem
- Collision shapes
- Other scene objects (walls, floors, steps)
