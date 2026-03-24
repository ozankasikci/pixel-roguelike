# Mesh Asset Pipeline Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace procedural pillar and arch generation with glTF mesh files loaded at runtime, establishing a reusable asset pipeline.

**Architecture:** Extract pure-math geometry generation into a shared header (MeshGeometry.h). An offline tool (mesh_generator) bakes composite objects into .glb files using tinygltf. The engine loads these via GltfLoader and registers them in MeshLibrary. CathedralScene places loaded meshes with simple transforms.

**Tech Stack:** C++20, CMake, tinygltf (header-only glTF loader/writer), GLM, OpenGL 4.1

---

### Task 1: Create MeshGeometry.h — pure-math geometry generators

**Files:**
- Create: `src/engine/rendering/MeshGeometry.h`
- Create: `tests/test_mesh_geometry.cpp`
- Modify: `CMakeLists.txt` (add test target)

**Step 1: Write the test**

Create `tests/test_mesh_geometry.cpp`:

```cpp
#include "engine/rendering/MeshGeometry.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <cstdio>

int main() {
    // generateCube: 6 faces * 4 verts = 24 verts, 6 faces * 2 tris * 3 = 36 indices
    {
        auto cube = generateCube(1.0f);
        assert(cube.positions.size() == 24);
        assert(cube.normals.size() == 24);
        assert(cube.indices.size() == 36);
    }

    // generatePlane: 4 verts, 6 indices
    {
        auto plane = generatePlane(1.0f);
        assert(plane.positions.size() == 4);
        assert(plane.normals.size() == 4);
        assert(plane.indices.size() == 6);
    }

    // generateCylinder: (segments+1)*2 side verts + 2 cap centers
    {
        int seg = 12;
        auto cyl = generateCylinder(1.0f, 1.0f, seg);
        int expectedVerts = (seg + 1) * 2 + 2; // 28
        assert(cyl.positions.size() == static_cast<size_t>(expectedVerts));
        assert(cyl.normals.size() == cyl.positions.size());
        // Side: seg*6, bottom cap: seg*3, top cap: seg*3
        int expectedIndices = seg * 6 + seg * 3 + seg * 3;
        assert(cyl.indices.size() == static_cast<size_t>(expectedIndices));
    }

    // mergeMeshes: two cubes merged
    {
        auto cube = generateCube(1.0f);
        auto merged = mergeMeshes({
            {cube, glm::mat4(1.0f)},
            {cube, glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f))}
        });
        assert(merged.positions.size() == 48);
        assert(merged.normals.size() == 48);
        assert(merged.indices.size() == 72);

        // Second cube's indices should be offset by 24
        assert(merged.indices[36] >= 24);
    }

    // mergeMeshes: transform applied correctly
    {
        auto plane = generatePlane(2.0f);
        glm::mat4 shift = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
        auto merged = mergeMeshes({{plane, shift}});
        // All positions should have x >= 9.0 (shifted by 10, plane half-size is 1)
        for (const auto& p : merged.positions) {
            assert(p.x >= 9.0f);
        }
    }

    printf("All mesh geometry tests passed!\n");
    return 0;
}
```

**Step 2: Create MeshGeometry.h (minimal, enough to compile the test)**

Create `src/engine/rendering/MeshGeometry.h`:

```cpp
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>
#include <cstdint>
#include <utility>

struct RawMeshData {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;
};

inline RawMeshData generateCube(float size) {
    float h = size * 0.5f;

    std::vector<glm::vec3> positions = {
        // Front (+Z)
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h},
        // Back (-Z)
        { h, -h, -h}, {-h, -h, -h}, {-h,  h, -h}, { h,  h, -h},
        // Left (-X)
        {-h, -h, -h}, {-h, -h,  h}, {-h,  h,  h}, {-h,  h, -h},
        // Right (+X)
        { h, -h,  h}, { h, -h, -h}, { h,  h, -h}, { h,  h,  h},
        // Top (+Y)
        {-h,  h,  h}, { h,  h,  h}, { h,  h, -h}, {-h,  h, -h},
        // Bottom (-Y)
        {-h, -h, -h}, { h, -h, -h}, { h, -h,  h}, {-h, -h,  h},
    };

    std::vector<glm::vec3> normals = {
        {0,0,1},{0,0,1},{0,0,1},{0,0,1},
        {0,0,-1},{0,0,-1},{0,0,-1},{0,0,-1},
        {-1,0,0},{-1,0,0},{-1,0,0},{-1,0,0},
        {1,0,0},{1,0,0},{1,0,0},{1,0,0},
        {0,1,0},{0,1,0},{0,1,0},{0,1,0},
        {0,-1,0},{0,-1,0},{0,-1,0},{0,-1,0},
    };

    std::vector<uint32_t> indices;
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    return {positions, normals, indices};
}

inline RawMeshData generatePlane(float size) {
    float h = size * 0.5f;
    return {
        {{-h, 0.0f, -h}, {h, 0.0f, -h}, {h, 0.0f, h}, {-h, 0.0f, h}},
        {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}},
        {0, 1, 2, 0, 2, 3}
    };
}

inline RawMeshData generateCylinder(float radius, float height, int segments) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;

    constexpr float PI = 3.14159265f;

    // Side vertices: 2 rings
    for (int ring = 0; ring <= 1; ++ring) {
        float y = ring * height;
        for (int i = 0; i <= segments; ++i) {
            float angle = static_cast<float>(i) / segments * 2.0f * PI;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            positions.push_back(glm::vec3(x, y, z));
            normals.push_back(glm::normalize(glm::vec3(std::cos(angle), 0.0f, std::sin(angle))));
        }
    }

    // Side indices
    int stride = segments + 1;
    for (int i = 0; i < segments; ++i) {
        uint32_t bl = i, br = i + 1, tl = i + stride, tr = i + 1 + stride;
        indices.push_back(bl); indices.push_back(br); indices.push_back(tl);
        indices.push_back(br); indices.push_back(tr); indices.push_back(tl);
    }

    // Bottom cap
    uint32_t botCenter = static_cast<uint32_t>(positions.size());
    positions.push_back(glm::vec3(0, 0, 0));
    normals.push_back(glm::vec3(0, -1, 0));
    for (int i = 0; i < segments; ++i) {
        indices.push_back(botCenter);
        indices.push_back(i + 1);
        indices.push_back(i);
    }

    // Top cap
    uint32_t topCenter = static_cast<uint32_t>(positions.size());
    positions.push_back(glm::vec3(0, height, 0));
    normals.push_back(glm::vec3(0, 1, 0));
    for (int i = 0; i < segments; ++i) {
        indices.push_back(topCenter);
        indices.push_back(i + stride);
        indices.push_back(i + 1 + stride);
    }

    return {positions, normals, indices};
}

inline RawMeshData mergeMeshes(const std::vector<std::pair<RawMeshData, glm::mat4>>& parts) {
    RawMeshData result;

    for (const auto& [mesh, transform] : parts) {
        uint32_t baseIndex = static_cast<uint32_t>(result.positions.size());
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));

        for (size_t i = 0; i < mesh.positions.size(); ++i) {
            glm::vec3 pos = glm::vec3(transform * glm::vec4(mesh.positions[i], 1.0f));
            glm::vec3 norm = glm::normalize(normalMatrix * mesh.normals[i]);
            result.positions.push_back(pos);
            result.normals.push_back(norm);
        }

        for (uint32_t idx : mesh.indices) {
            result.indices.push_back(baseIndex + idx);
        }
    }

    return result;
}
```

**Step 3: Add test target to CMakeLists.txt**

At the end of `CMakeLists.txt`, add:

```cmake
# --- Tests ---
enable_testing()

add_executable(test_mesh_geometry tests/test_mesh_geometry.cpp)
target_include_directories(test_mesh_geometry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_mesh_geometry PRIVATE glm::glm)
add_test(NAME mesh_geometry COMMAND test_mesh_geometry)
```

**Step 4: Build and run the test**

Run: `cmake --build build --target test_mesh_geometry && ./build/test_mesh_geometry`

Expected: `All mesh geometry tests passed!`

**Step 5: Commit**

```bash
git add src/engine/rendering/MeshGeometry.h tests/test_mesh_geometry.cpp CMakeLists.txt
git commit -m "Add MeshGeometry.h with pure-math geometry generators and tests"
```

---

### Task 2: Refactor Mesh and MeshLibrary to use MeshGeometry.h

**Files:**
- Modify: `src/engine/rendering/Mesh.h` (add createCylinder declaration)
- Modify: `src/engine/rendering/Mesh.cpp` (delegate to MeshGeometry functions)
- Modify: `src/engine/rendering/MeshLibrary.cpp` (remove local createCylinder, use Mesh::createCylinder)

**Step 1: Add createCylinder to Mesh.h**

In `src/engine/rendering/Mesh.h`, after the `createPlane` declaration (line 26), add:

```cpp
    static Mesh createCylinder(float radius, float height, int segments);
```

**Step 2: Refactor Mesh.cpp — delegate factory methods to MeshGeometry**

Replace `Mesh::createCube` and `Mesh::createPlane` in `src/engine/rendering/Mesh.cpp` and add `createCylinder`.

Add include at the top (after existing includes):
```cpp
#include "MeshGeometry.h"
```

Replace `createCube` (lines 96-142) with:
```cpp
Mesh Mesh::createCube(float size) {
    auto data = generateCube(size);
    return Mesh(data.positions, data.normals, data.indices);
}
```

Replace `createPlane` (lines 144-164) with:
```cpp
Mesh Mesh::createPlane(float size) {
    auto data = generatePlane(size);
    return Mesh(data.positions, data.normals, data.indices);
}
```

Add new `createCylinder`:
```cpp
Mesh Mesh::createCylinder(float radius, float height, int segments) {
    auto data = generateCylinder(radius, height, segments);
    return Mesh(data.positions, data.normals, data.indices);
}
```

**Step 3: Refactor MeshLibrary.cpp — remove local createCylinder**

In `src/engine/rendering/MeshLibrary.cpp`:

1. Remove the entire `static Mesh createCylinder(...)` function (lines 9-51)
2. Remove the `#include <cmath>` include (no longer needed)
3. Update `registerDefaults()` to use `Mesh::createCylinder`:

```cpp
void MeshLibrary::registerDefaults() {
    registerMesh("cube", std::make_unique<Mesh>(Mesh::createCube(1.0f)));
    registerMesh("plane", std::make_unique<Mesh>(Mesh::createPlane(1.0f)));
    registerMesh("cylinder", std::make_unique<Mesh>(Mesh::createCylinder(1.0f, 1.0f, 12)));
    registerMesh("cylinder_wide", std::make_unique<Mesh>(Mesh::createCylinder(1.5f, 1.0f, 12)));
    registerMesh("cylinder_cap", std::make_unique<Mesh>(Mesh::createCylinder(1.4f, 1.0f, 12)));
    spdlog::info("MeshLibrary: registered {} default meshes", meshes_.size());
}
```

**Step 4: Build full project and run tests**

Run: `cmake --build build && ./build/test_mesh_geometry`

Expected: Build succeeds. Test passes. This is a pure refactor — no behavior change.

**Step 5: Run the game to verify no visual regression**

Run: `./build/pixel-roguelike`

Expected: Cathedral scene looks identical to before (pillars, arches, walls all render correctly).

**Step 6: Commit**

```bash
git add src/engine/rendering/Mesh.h src/engine/rendering/Mesh.cpp src/engine/rendering/MeshLibrary.cpp
git commit -m "Refactor Mesh and MeshLibrary to delegate geometry to MeshGeometry.h"
```

---

### Task 3: Add tinygltf dependency to CMake

**Files:**
- Modify: `CMakeLists.txt`

**Step 1: Add tinygltf FetchContent**

In `CMakeLists.txt`, after the spdlog `find_package` (line 69), add:

```cmake
# tinygltf - header-only glTF 2.0 loader/writer
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_GL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(tinygltf)
```

Note: if v2.9.3 is unavailable, check https://github.com/syoyo/tinygltf/releases for the latest tag.

**Step 2: Build to verify tinygltf fetches and configures**

Run: `cmake -B build -S . && cmake --build build`

Expected: tinygltf downloads, configures, project still builds.

**Step 3: Commit**

```bash
git add CMakeLists.txt
git commit -m "Add tinygltf dependency for glTF mesh loading"
```

---

### Task 4: Create mesh_generator tool

**Files:**
- Create: `tools/mesh_generator.cpp`
- Modify: `CMakeLists.txt` (add mesh_generator target)

**Step 1: Add mesh_generator target to CMakeLists.txt**

After the test section at the end of `CMakeLists.txt`, add:

```cmake
# --- Mesh Generator Tool ---
add_executable(mesh_generator tools/mesh_generator.cpp)
target_include_directories(mesh_generator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mesh_generator PRIVATE glm::glm tinygltf)
```

**Step 2: Create tools/mesh_generator.cpp**

```cpp
// Offline tool: generates .glb mesh files for composite game objects.
// Usage: run from project root. Outputs to assets/meshes/

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "engine/rendering/MeshGeometry.h"
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>

// ---- glTF writing helper ----

static bool writeGlb(const RawMeshData& mesh, const std::string& filepath) {
    tinygltf::Model model;
    model.asset.version = "2.0";
    model.asset.generator = "mesh_generator";

    // Binary buffer: positions | normals | indices
    size_t posBytes  = mesh.positions.size() * sizeof(glm::vec3);
    size_t normBytes = mesh.normals.size() * sizeof(glm::vec3);
    size_t idxBytes  = mesh.indices.size() * sizeof(uint32_t);

    tinygltf::Buffer buffer;
    buffer.data.resize(posBytes + normBytes + idxBytes);
    std::memcpy(buffer.data.data(), mesh.positions.data(), posBytes);
    std::memcpy(buffer.data.data() + posBytes, mesh.normals.data(), normBytes);
    std::memcpy(buffer.data.data() + posBytes + normBytes, mesh.indices.data(), idxBytes);
    model.buffers.push_back(std::move(buffer));

    // BufferViews: 0=positions, 1=normals, 2=indices
    {
        tinygltf::BufferView bv;
        bv.buffer = 0; bv.byteOffset = 0; bv.byteLength = posBytes;
        bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        model.bufferViews.push_back(bv);
    }
    {
        tinygltf::BufferView bv;
        bv.buffer = 0; bv.byteOffset = posBytes; bv.byteLength = normBytes;
        bv.target = TINYGLTF_TARGET_ARRAY_BUFFER;
        model.bufferViews.push_back(bv);
    }
    {
        tinygltf::BufferView bv;
        bv.buffer = 0; bv.byteOffset = posBytes + normBytes; bv.byteLength = idxBytes;
        bv.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
        model.bufferViews.push_back(bv);
    }

    // Accessors
    // 0: POSITION
    {
        tinygltf::Accessor acc;
        acc.bufferView = 0;
        acc.byteOffset = 0;
        acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        acc.count = mesh.positions.size();
        acc.type = TINYGLTF_TYPE_VEC3;
        // Compute AABB (required by glTF spec for POSITION)
        glm::vec3 minP(std::numeric_limits<float>::max());
        glm::vec3 maxP(std::numeric_limits<float>::lowest());
        for (const auto& p : mesh.positions) {
            minP = glm::min(minP, p);
            maxP = glm::max(maxP, p);
        }
        acc.minValues = {minP.x, minP.y, minP.z};
        acc.maxValues = {maxP.x, maxP.y, maxP.z};
        model.accessors.push_back(acc);
    }
    // 1: NORMAL
    {
        tinygltf::Accessor acc;
        acc.bufferView = 1;
        acc.byteOffset = 0;
        acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        acc.count = mesh.normals.size();
        acc.type = TINYGLTF_TYPE_VEC3;
        model.accessors.push_back(acc);
    }
    // 2: indices
    {
        tinygltf::Accessor acc;
        acc.bufferView = 2;
        acc.byteOffset = 0;
        acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        acc.count = mesh.indices.size();
        acc.type = TINYGLTF_TYPE_SCALAR;
        model.accessors.push_back(acc);
    }

    // Primitive → Mesh → Node → Scene
    tinygltf::Primitive prim;
    prim.attributes["POSITION"] = 0;
    prim.attributes["NORMAL"] = 1;
    prim.indices = 2;
    prim.mode = TINYGLTF_MODE_TRIANGLES;

    tinygltf::Mesh gltfMesh;
    gltfMesh.primitives.push_back(prim);
    model.meshes.push_back(gltfMesh);

    tinygltf::Node node;
    node.mesh = 0;
    model.nodes.push_back(node);

    tinygltf::Scene scene;
    scene.nodes.push_back(0);
    model.scenes.push_back(scene);
    model.defaultScene = 0;

    tinygltf::TinyGLTF writer;
    return writer.WriteGltfSceneToFile(&model, filepath,
        true,   // embedImages (N/A, no images)
        true,   // embedBuffers
        true,   // prettyPrint (ignored for binary)
        true);  // writeBinary (.glb)
}

// ---- Mesh definitions (cathedral dimensions) ----

static RawMeshData generatePillar() {
    // Dimensions match CathedralScene.cpp constants
    constexpr float wallHeight = 6.0f;
    constexpr float pillarRadius = 0.35f;

    // Shaft: full height, base radius
    auto shaft = generateCylinder(pillarRadius, wallHeight, 12);

    // Base: wider, short (radius = 1.5 * pillarRadius * 1.5 = 0.7875)
    // Scene uses cylinder_wide (r=1.5) scaled by (pillarRadius*1.5, 0.3, pillarRadius*1.5)
    // Effective: r = 1.5 * 0.525 = 0.7875, h = 0.3
    auto base = generateCylinder(0.7875f, 0.3f, 12);

    // Capital: slightly less wide, short, at top
    // Scene uses cylinder_cap (r=1.4) scaled by (pillarRadius*1.4, 0.25, pillarRadius*1.4)
    // Effective: r = 1.4 * 0.49 = 0.686, h = 0.25
    auto capital = generateCylinder(0.686f, 0.25f, 12);

    return mergeMeshes({
        {shaft,   glm::mat4(1.0f)},
        {base,    glm::mat4(1.0f)},
        {capital, glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, wallHeight - 0.25f, 0.0f))}
    });
}

static RawMeshData generateArch() {
    // Dimensions match CathedralScene.cpp addArch() call
    constexpr float corridorWidth = 8.0f;
    constexpr float halfW = corridorWidth * 0.5f;
    constexpr float wallHeight = 6.0f;

    constexpr float halfSpan = halfW - 1.2f;   // 2.8
    constexpr float baseY = wallHeight - 0.5f;  // 5.5
    constexpr float archHeight = 0.8f;
    constexpr float thickness = 0.25f;
    constexpr int segments = 12;
    constexpr float PI = 3.14159265f;

    auto unitCube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    for (int i = 0; i < segments; ++i) {
        float a0 = PI * i / segments;
        float a1 = PI * (i + 1) / segments;

        float cx0 = -halfSpan * std::cos(a0);
        float cy0 = baseY + archHeight * std::sin(a0);
        float cx1 = -halfSpan * std::cos(a1);
        float cy1 = baseY + archHeight * std::sin(a1);

        float midX = (cx0 + cx1) * 0.5f;
        float midY = (cy0 + cy1) * 0.5f;

        float dx = cx1 - cx0;
        float dy = cy1 - cy0;
        float segLen = std::sqrt(dx * dx + dy * dy);
        float angle = std::atan2(dy, dx);

        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(midX, midY, 0.0f));
        m = glm::rotate(m, angle, glm::vec3(0, 0, 1));
        m = glm::scale(m, glm::vec3(segLen, thickness, thickness));

        parts.push_back({unitCube, m});
    }

    return mergeMeshes(parts);
}

// ---- Main ----

int main() {
    const std::string outDir = "assets/meshes";
    std::filesystem::create_directories(outDir);

    printf("Generating pillar.glb...\n");
    auto pillar = generatePillar();
    if (!writeGlb(pillar, outDir + "/pillar.glb")) {
        fprintf(stderr, "ERROR: failed to write pillar.glb\n");
        return 1;
    }
    printf("  %zu vertices, %zu triangles\n",
           pillar.positions.size(), pillar.indices.size() / 3);

    printf("Generating arch.glb...\n");
    auto arch = generateArch();
    if (!writeGlb(arch, outDir + "/arch.glb")) {
        fprintf(stderr, "ERROR: failed to write arch.glb\n");
        return 1;
    }
    printf("  %zu vertices, %zu triangles\n",
           arch.positions.size(), arch.indices.size() / 3);

    printf("Done. Files written to %s/\n", outDir.c_str());
    return 0;
}
```

**Step 3: Build the mesh_generator**

Run: `cmake -B build -S . && cmake --build build --target mesh_generator`

Expected: Compiles successfully.

**Step 4: Run the mesh_generator to produce .glb files**

Run from project root:
```bash
cd /Users/ozan/Projects/gsd-3d-roguelike && ./build/mesh_generator
```

Expected output:
```
Generating pillar.glb...
  84 vertices, 144 triangles
Generating arch.glb...
  288 vertices, 144 triangles
Done. Files written to assets/meshes/
```

Verify files exist:
```bash
ls -la assets/meshes/
```

Expected: `pillar.glb` and `arch.glb` present (a few KB each).

**Step 5: Commit**

```bash
git add tools/mesh_generator.cpp CMakeLists.txt assets/meshes/pillar.glb assets/meshes/arch.glb
git commit -m "Add mesh generator tool and generate pillar/arch .glb files"
```

---

### Task 5: Create GltfLoader and MeshLibrary::loadFromFile

**Files:**
- Create: `src/engine/rendering/GltfLoader.h`
- Create: `src/engine/rendering/GltfLoader.cpp`
- Modify: `src/engine/rendering/MeshLibrary.h` (add loadFromFile)
- Modify: `src/engine/rendering/MeshLibrary.cpp` (implement loadFromFile)
- Modify: `CMakeLists.txt` (add GltfLoader.cpp to engine_rendering, link tinygltf)

**Step 1: Create GltfLoader.h**

Create `src/engine/rendering/GltfLoader.h`:

```cpp
#pragma once

#include "Mesh.h"
#include <memory>
#include <string>

class GltfLoader {
public:
    // Load a .glb/.gltf file and return a Mesh with the first primitive's
    // POSITION and NORMAL attributes. Returns nullptr on failure.
    static std::unique_ptr<Mesh> load(const std::string& filepath);
};
```

**Step 2: Create GltfLoader.cpp**

Create `src/engine/rendering/GltfLoader.cpp`:

```cpp
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "GltfLoader.h"

#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <cstring>

std::unique_ptr<Mesh> GltfLoader::load(const std::string& filepath) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
    if (!warn.empty()) spdlog::warn("GltfLoader: {}", warn);
    if (!ok) {
        spdlog::error("GltfLoader: failed to load '{}': {}", filepath, err);
        return nullptr;
    }

    if (model.meshes.empty() || model.meshes[0].primitives.empty()) {
        spdlog::error("GltfLoader: no mesh primitives in '{}'", filepath);
        return nullptr;
    }

    const auto& primitive = model.meshes[0].primitives[0];

    // Helper: extract float data from an accessor
    auto getFloatData = [&](int accessorIdx) -> const float* {
        const auto& acc = model.accessors[accessorIdx];
        const auto& bv = model.bufferViews[acc.bufferView];
        const auto& buf = model.buffers[bv.buffer];
        return reinterpret_cast<const float*>(buf.data.data() + bv.byteOffset + acc.byteOffset);
    };

    // POSITION
    auto posIt = primitive.attributes.find("POSITION");
    if (posIt == primitive.attributes.end()) {
        spdlog::error("GltfLoader: no POSITION attribute in '{}'", filepath);
        return nullptr;
    }
    const auto& posAcc = model.accessors[posIt->second];
    const float* posData = getFloatData(posIt->second);

    std::vector<glm::vec3> positions(posAcc.count);
    std::memcpy(positions.data(), posData, posAcc.count * sizeof(glm::vec3));

    // NORMAL
    std::vector<glm::vec3> normals;
    auto normIt = primitive.attributes.find("NORMAL");
    if (normIt != primitive.attributes.end()) {
        const auto& normAcc = model.accessors[normIt->second];
        const float* normData = getFloatData(normIt->second);
        normals.resize(normAcc.count);
        std::memcpy(normals.data(), normData, normAcc.count * sizeof(glm::vec3));
    } else {
        // Default up-facing normals if missing
        normals.resize(posAcc.count, glm::vec3(0, 1, 0));
    }

    // INDICES
    if (primitive.indices < 0) {
        spdlog::error("GltfLoader: no indices in '{}'", filepath);
        return nullptr;
    }
    const auto& idxAcc = model.accessors[primitive.indices];
    const auto& idxBv = model.bufferViews[idxAcc.bufferView];
    const auto& idxBuf = model.buffers[idxBv.buffer];
    const uint8_t* idxRaw = idxBuf.data.data() + idxBv.byteOffset + idxAcc.byteOffset;

    std::vector<uint32_t> indices(idxAcc.count);
    if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
        std::memcpy(indices.data(), idxRaw, idxAcc.count * sizeof(uint32_t));
    } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
        const uint16_t* src = reinterpret_cast<const uint16_t*>(idxRaw);
        for (size_t i = 0; i < idxAcc.count; ++i) indices[i] = src[i];
    } else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        for (size_t i = 0; i < idxAcc.count; ++i) indices[i] = idxRaw[i];
    }

    spdlog::info("GltfLoader: loaded '{}' ({} verts, {} tris)",
                 filepath, positions.size(), indices.size() / 3);

    return std::make_unique<Mesh>(positions, normals, indices);
}
```

**Step 3: Add loadFromFile to MeshLibrary.h**

In `src/engine/rendering/MeshLibrary.h`, after the `registerDefaults()` declaration (line 35), add:

```cpp
    // Load a mesh from a .glb/.gltf file and register it under the given name
    void loadFromFile(const std::string& name, const std::string& filepath);
```

**Step 4: Implement loadFromFile in MeshLibrary.cpp**

In `src/engine/rendering/MeshLibrary.cpp`, add include at top:
```cpp
#include "engine/rendering/GltfLoader.h"
```

Add the method before `clear()`:
```cpp
void MeshLibrary::loadFromFile(const std::string& name, const std::string& filepath) {
    auto mesh = GltfLoader::load(filepath);
    if (mesh) {
        registerMesh(name, std::move(mesh));
    } else {
        spdlog::error("MeshLibrary: failed to load mesh '{}' from '{}'", name, filepath);
    }
}
```

**Step 5: Update CMakeLists.txt — add GltfLoader.cpp and tinygltf link**

In the `engine_rendering` library definition, add `GltfLoader.cpp`:

```cmake
add_library(engine_rendering STATIC
    src/engine/rendering/Shader.cpp
    src/engine/rendering/Framebuffer.cpp
    src/engine/rendering/DitherPass.cpp
    src/engine/rendering/Mesh.cpp
    src/engine/rendering/MeshLibrary.cpp
    src/engine/rendering/Renderer.cpp
    src/engine/rendering/GltfLoader.cpp
)
```

Add tinygltf link to engine_rendering:
```cmake
target_link_libraries(engine_rendering PUBLIC engine_core glm::glm)
target_link_libraries(engine_rendering PRIVATE tinygltf)
```

**Step 6: Build to verify compilation**

Run: `cmake --build build`

Expected: Compiles with no errors. The loader isn't called yet — just verifying it builds.

**Step 7: Commit**

```bash
git add src/engine/rendering/GltfLoader.h src/engine/rendering/GltfLoader.cpp \
        src/engine/rendering/MeshLibrary.h src/engine/rendering/MeshLibrary.cpp \
        CMakeLists.txt
git commit -m "Add GltfLoader and MeshLibrary::loadFromFile for .glb mesh loading"
```

---

### Task 6: Update CathedralScene to use loaded meshes

**Files:**
- Modify: `src/game/scenes/CathedralScene.cpp`

**Step 1: Load pillar and arch meshes**

In `CathedralScene::onEnter()`, after `meshLibrary_.registerDefaults();` (line 87), add:

```cpp
    meshLibrary_.loadFromFile("pillar", "assets/meshes/pillar.glb");
    meshLibrary_.loadFromFile("arch", "assets/meshes/arch.glb");
```

After the existing mesh pointer declarations (lines 91-95), add:

```cpp
    Mesh* pillarMesh = meshLibrary_.get("pillar");
    Mesh* archMesh   = meshLibrary_.get("arch");
```

**Step 2: Replace pillar/arch spawning code**

Replace the entire pillars section (lines 132-171 — from `// Pillars` through the arch `addArch` call) with:

```cpp
    // Pillars and arches (loaded from .glb mesh files)
    float pillarRadius  = 0.35f;
    float pillarSpacing = 4.0f;
    int pillarCount = static_cast<int>(corridorLength / pillarSpacing);

    for (int i = 0; i < pillarCount; ++i) {
        float z = -2.0f - i * pillarSpacing;

        for (float side : {-1.0f, 1.0f}) {
            float x = side * (halfW - 1.2f);
            glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
            entities_.push_back(spawnMesh(registry, pillarMesh, m));
        }

        // Arch — baked at correct XY, just translate Z
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, z));
        entities_.push_back(spawnMesh(registry, archMesh, m));
    }
```

**Step 3: Remove the addArch helper function**

Delete the `addArch` function (lines 24-55) from the file. It is no longer used.

**Step 4: Remove unused mesh pointers**

Remove these lines from the mesh pointer declarations since they are no longer used by pillar/arch code:

```cpp
    Mesh* cyl      = meshLibrary_.get("cylinder");
    Mesh* cylWide  = meshLibrary_.get("cylinder_wide");
    Mesh* cylCap   = meshLibrary_.get("cylinder_cap");
```

Only keep them if other code in the scene still references them. Currently nothing else uses them, so remove all three.

**Step 5: Build and run**

Run: `cmake --build build && ./build/pixel-roguelike`

Expected: Cathedral renders with pillars and arches looking identical to before. Each pillar is now 1 entity instead of 3+, each arch is 1 entity instead of 12.

Verify in the ImGui debug overlay (if it shows entity count) that the total entity count is lower.

**Step 6: Commit**

```bash
git add src/game/scenes/CathedralScene.cpp
git commit -m "Replace procedural pillar/arch generation with loaded .glb meshes"
```

---

### Task 7: Final verification and cleanup

**Step 1: Run the full test suite**

```bash
cd /Users/ozan/Projects/gsd-3d-roguelike
cmake --build build && ./build/test_mesh_geometry
```

Expected: `All mesh geometry tests passed!`

**Step 2: Run the game and verify visuals**

```bash
./build/pixel-roguelike
```

Verify:
- Pillars render with shaft, base, and capital
- Arches render as curved segments between pillar tops
- 1-bit dithering shader correctly outlines pillar/arch edges
- Collision works (player cannot walk through pillars)
- No visual glitches compared to before

**Step 3: Verify .glb files are viewable externally**

Open `assets/meshes/pillar.glb` in an online viewer (e.g., https://gltf-viewer.donmccurdy.com/) or drag into Blender to confirm the mesh geometry is correct.

**Step 4: Verify clean build from scratch**

```bash
rm -rf build
cmake -B build -S .
cmake --build build
./build/test_mesh_geometry
./build/mesh_generator
./build/pixel-roguelike
```

Expected: Everything builds and runs from a clean state.
