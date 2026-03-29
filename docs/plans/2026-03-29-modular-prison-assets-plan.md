# Modular Prison Assets & Warden's Office Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Create 11 procedural prison meshes and assemble them into a playable Warden's Office level.

**Architecture:** New `PrisonAssets.h/.cpp` in `src/game/levels/prison/` generates all meshes procedurally using `mergeMeshes()` (same pattern as `CathedralAssets.cpp`). A new `WardenOfficeScene` loads `warden_office.scene` using the standard `LevelLoader` pipeline. The model viewer registers all prison meshes for visual verification.

**Tech Stack:** C++20, OpenGL 4.1, GLM, EnTT, existing `MeshGeometry.h` primitives (`generateCube`, `generateCylinder`, `mergeMeshes`)

---

### Task 1: PrisonAssets — Architectural Meshes (wall, wall_window, wall_door)

**Files:**
- Create: `src/game/levels/prison/PrisonAssets.h`
- Create: `src/game/levels/prison/PrisonAssets.cpp`

**Step 1: Create the header**

Create `src/game/levels/prison/PrisonAssets.h`:

```cpp
#pragma once

#include "engine/rendering/geometry/MeshLibrary.h"

void registerPrisonAssets(MeshLibrary& meshLibrary);
```

**Step 2: Create PrisonAssets.cpp with helper + first mesh (prison_wall)**

Create `src/game/levels/prison/PrisonAssets.cpp`. This file follows the exact same pattern as `src/game/levels/cathedral/CathedralAssets.cpp`:
- Anonymous namespace with `makeModel()` helper (identical to CathedralAssets)
- Each mesh is a standalone function returning `std::unique_ptr<Mesh>`
- Each function creates a `cube` via `generateCube(1.0f)`, builds a `parts` vector, and calls `mergeMeshes()`

```cpp
#include "game/levels/prison/PrisonAssets.h"

#include "engine/rendering/geometry/MeshGeometry.h"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

glm::mat4 makeModel(const glm::vec3& position,
                    const glm::vec3& scale,
                    const glm::vec3& rotation = glm::vec3(0.0f)) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}
```

**`prison_wall` — Solid concrete slab (2m × 2.5m × 0.2m)**

A main slab with a recessed inset border on the front face to suggest poured-concrete panel joints.

```cpp
std::unique_ptr<Mesh> createPrisonWall() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main slab: 2m wide, 2.5m tall, 0.2m thick
    // Origin at bottom-center of the wall panel, Y=0 is floor level
    addBox(glm::vec3(0.0f, 1.25f, 0.0f), glm::vec3(1.0f, 1.25f, 0.1f));

    // Inset border on front face — thin raised frame 0.04m from edges
    // Top horizontal strip
    addBox(glm::vec3(0.0f, 2.46f, 0.105f), glm::vec3(0.96f, 0.02f, 0.005f));
    // Bottom horizontal strip
    addBox(glm::vec3(0.0f, 0.04f, 0.105f), glm::vec3(0.96f, 0.02f, 0.005f));
    // Left vertical strip
    addBox(glm::vec3(-0.96f, 1.25f, 0.105f), glm::vec3(0.02f, 1.21f, 0.005f));
    // Right vertical strip
    addBox(glm::vec3(0.96f, 1.25f, 0.105f), glm::vec3(0.02f, 1.21f, 0.005f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**`prison_wall_window` — Wall with barred window opening**

Four wall sections around a 0.6m × 0.4m void at 1.8m height, plus 3 vertical bars.

```cpp
std::unique_ptr<Mesh> createPrisonWallWindow() {
    auto cube = generateCube(1.0f);
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

    // Window opening: 0.6m wide, 0.4m tall, centered at Y=1.8m
    // Bottom section: 0 to 1.6m
    addBox(glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(1.0f, 0.8f, 0.1f));
    // Top section: 2.0m to 2.5m
    addBox(glm::vec3(0.0f, 2.25f, 0.0f), glm::vec3(1.0f, 0.25f, 0.1f));
    // Left section: beside window
    addBox(glm::vec3(-0.65f, 1.8f, 0.0f), glm::vec3(0.35f, 0.2f, 0.1f));
    // Right section: beside window
    addBox(glm::vec3(0.65f, 1.8f, 0.0f), glm::vec3(0.35f, 0.2f, 0.1f));

    // Window sill (thin ledge at bottom of opening)
    addBox(glm::vec3(0.0f, 1.58f, 0.04f), glm::vec3(0.32f, 0.02f, 0.06f));

    // 3 vertical bars spanning the window opening (r=0.015m)
    for (float x : {-0.15f, 0.0f, 0.15f}) {
        addCylinder(glm::vec3(x, 1.6f, 0.0f), glm::vec3(0.015f, 0.4f, 0.015f));
    }

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**`prison_wall_door` — Wall with door frame cutout**

Wall slab with a 0.9m × 2.1m cutout, steel angle insets on frame edges.

```cpp
std::unique_ptr<Mesh> createPrisonWallDoor() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Door opening: 0.9m wide, 2.1m tall, bottom at Y=0
    // Top section: 2.1m to 2.5m (full width)
    addBox(glm::vec3(0.0f, 2.3f, 0.0f), glm::vec3(1.0f, 0.2f, 0.1f));
    // Left section: beside door
    addBox(glm::vec3(-0.725f, 1.05f, 0.0f), glm::vec3(0.275f, 1.05f, 0.1f));
    // Right section: beside door
    addBox(glm::vec3(0.725f, 1.05f, 0.0f), glm::vec3(0.275f, 1.05f, 0.1f));

    // Steel angle frame insets around the door opening
    // Left jamb (thin L-angle)
    addBox(glm::vec3(-0.45f, 1.05f, 0.08f), glm::vec3(0.02f, 1.05f, 0.03f));
    // Right jamb
    addBox(glm::vec3(0.45f, 1.05f, 0.08f), glm::vec3(0.02f, 1.05f, 0.03f));
    // Top lintel
    addBox(glm::vec3(0.0f, 2.12f, 0.08f), glm::vec3(0.45f, 0.02f, 0.03f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 3: Add the registration function stub**

At the bottom of `PrisonAssets.cpp`, after the anonymous namespace closing brace:

```cpp
} // namespace

void registerPrisonAssets(MeshLibrary& meshLibrary) {
    meshLibrary.registerDefaults();
    meshLibrary.registerMesh("prison_wall", createPrisonWall());
    meshLibrary.registerMesh("prison_wall_window", createPrisonWallWindow());
    meshLibrary.registerMesh("prison_wall_door", createPrisonWallDoor());
}
```

**Step 4: Commit**

```
git add src/game/levels/prison/PrisonAssets.h src/game/levels/prison/PrisonAssets.cpp
git commit -m "Add PrisonAssets with wall, wall_window, wall_door meshes"
```

---

### Task 2: PrisonAssets — Floor, Ceiling, Baseboard

**Files:**
- Modify: `src/game/levels/prison/PrisonAssets.cpp`

Add these three functions inside the anonymous namespace, before the closing `} // namespace`.

**Step 1: Add prison_floor**

`prison_floor` — 2m × 2m flat slab (0.1m thick) with recessed border line.

```cpp
std::unique_ptr<Mesh> createPrisonFloor() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main slab: 2m × 2m, 0.1m thick, top surface at Y=0
    addBox(glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(1.0f, 0.05f, 1.0f));

    // Recessed border lines on top face (thin raised strips 0.05m from edges)
    addBox(glm::vec3(0.0f, 0.003f, -0.95f), glm::vec3(0.95f, 0.003f, 0.015f));
    addBox(glm::vec3(0.0f, 0.003f, 0.95f), glm::vec3(0.95f, 0.003f, 0.015f));
    addBox(glm::vec3(-0.95f, 0.003f, 0.0f), glm::vec3(0.015f, 0.003f, 0.95f));
    addBox(glm::vec3(0.95f, 0.003f, 0.0f), glm::vec3(0.015f, 0.003f, 0.95f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 2: Add prison_ceiling**

`prison_ceiling` — same geometry as floor, placed inverted (flat surface faces down).

```cpp
std::unique_ptr<Mesh> createPrisonCeiling() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main slab: 2m × 2m, 0.1m thick, bottom surface at Y=0
    addBox(glm::vec3(0.0f, 0.05f, 0.0f), glm::vec3(1.0f, 0.05f, 1.0f));

    // Recessed border lines on underside
    addBox(glm::vec3(0.0f, -0.003f, -0.95f), glm::vec3(0.95f, 0.003f, 0.015f));
    addBox(glm::vec3(0.0f, -0.003f, 0.95f), glm::vec3(0.95f, 0.003f, 0.015f));
    addBox(glm::vec3(-0.95f, -0.003f, 0.0f), glm::vec3(0.015f, 0.003f, 0.95f));
    addBox(glm::vec3(0.95f, -0.003f, 0.0f), glm::vec3(0.015f, 0.003f, 0.95f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 3: Add prison_baseboard**

`prison_baseboard` — 2m × 0.15m × 0.05m thin strip.

```cpp
std::unique_ptr<Mesh> createPrisonBaseboard() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main strip: 2m wide, 0.15m tall, 0.05m deep
    // Origin at bottom-center, sits against a wall
    addBox(glm::vec3(0.0f, 0.075f, 0.0f), glm::vec3(1.0f, 0.075f, 0.025f));

    // Top chamfer strip
    addBox(glm::vec3(0.0f, 0.155f, -0.008f), glm::vec3(1.0f, 0.005f, 0.018f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 4: Register the new meshes**

Add to `registerPrisonAssets()`:

```cpp
    meshLibrary.registerMesh("prison_floor", createPrisonFloor());
    meshLibrary.registerMesh("prison_ceiling", createPrisonCeiling());
    meshLibrary.registerMesh("prison_baseboard", createPrisonBaseboard());
```

**Step 5: Commit**

```
git add src/game/levels/prison/PrisonAssets.cpp
git commit -m "Add prison floor, ceiling, and baseboard meshes"
```

---

### Task 3: PrisonAssets — Prison Door

**Files:**
- Modify: `src/game/levels/prison/PrisonAssets.cpp`

**Step 1: Add prison_door mesh**

Heavy steel door (0.9m × 2.1m × 0.05m) with barred observation window, handle, hinges, lock plate.

```cpp
std::unique_ptr<Mesh> createPrisonDoor() {
    auto cube = generateCube(1.0f);
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

    // Main door slab: 0.9m wide, 2.1m tall, 0.05m thick
    // Origin at bottom-center, hinge side on left (-X)
    addBox(glm::vec3(0.0f, 1.05f, 0.0f), glm::vec3(0.45f, 1.05f, 0.025f));

    // Observation window frame at eye height (~1.5m)
    // Window void: 0.25m wide, 0.3m tall
    // We build a raised frame around the window area
    addBox(glm::vec3(0.0f, 1.5f, 0.028f), glm::vec3(0.16f, 0.18f, 0.004f));  // recessed plate
    // Frame strips around the window
    addBox(glm::vec3(0.0f, 1.65f, 0.03f), glm::vec3(0.14f, 0.01f, 0.005f));  // top
    addBox(glm::vec3(0.0f, 1.35f, 0.03f), glm::vec3(0.14f, 0.01f, 0.005f));  // bottom
    addBox(glm::vec3(-0.13f, 1.5f, 0.03f), glm::vec3(0.01f, 0.15f, 0.005f)); // left
    addBox(glm::vec3(0.13f, 1.5f, 0.03f), glm::vec3(0.01f, 0.15f, 0.005f));  // right

    // 2 vertical bars in observation window
    addCylinder(glm::vec3(-0.04f, 1.35f, 0.0f), glm::vec3(0.008f, 0.3f, 0.008f));
    addCylinder(glm::vec3(0.04f, 1.35f, 0.0f), glm::vec3(0.008f, 0.3f, 0.008f));

    // Handle (right side, ~1.0m height)
    addBox(glm::vec3(0.3f, 1.0f, 0.035f), glm::vec3(0.04f, 0.015f, 0.015f));
    // Handle grip bar
    addBox(glm::vec3(0.3f, 1.0f, 0.055f), glm::vec3(0.005f, 0.06f, 0.005f));

    // Three hinge plates on left edge
    for (float y : {0.3f, 1.05f, 1.8f}) {
        addBox(glm::vec3(-0.42f, y, 0.02f), glm::vec3(0.04f, 0.06f, 0.012f));
        // Hinge barrel
        addCylinder(glm::vec3(-0.465f, y - 0.06f, 0.02f), glm::vec3(0.012f, 0.12f, 0.012f));
    }

    // Lock plate below handle
    addBox(glm::vec3(0.3f, 0.85f, 0.03f), glm::vec3(0.04f, 0.06f, 0.008f));
    // Keyhole (small cylinder)
    addCylinder(glm::vec3(0.3f, 0.85f, 0.04f),
                glm::vec3(0.006f, 0.004f, 0.006f),
                glm::vec3(90.0f, 0.0f, 0.0f));

    // Reinforcement strip across bottom
    addBox(glm::vec3(0.0f, 0.08f, 0.028f), glm::vec3(0.44f, 0.04f, 0.005f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 2: Register**

Add to `registerPrisonAssets()`:

```cpp
    meshLibrary.registerMesh("prison_door", createPrisonDoor());
```

**Step 3: Commit**

```
git add src/game/levels/prison/PrisonAssets.cpp
git commit -m "Add prison door mesh with observation window, handle, and hinges"
```

---

### Task 4: PrisonAssets — Furniture (desk, chair, cabinet, shelf)

**Files:**
- Modify: `src/game/levels/prison/PrisonAssets.cpp`

**Step 1: Add prison_desk**

Metal desk (1.2m × 0.75m × 0.6m): flat top, four angle-iron legs, modesty panel, one drawer block.

```cpp
std::unique_ptr<Mesh> createPrisonDesk() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Desk top: 1.2m × 0.6m, 0.03m thick, at Y=0.75m
    addBox(glm::vec3(0.0f, 0.75f, 0.0f), glm::vec3(0.6f, 0.015f, 0.3f));

    // Four angle-iron legs (0.03m × 0.03m cross-section)
    float legH = 0.735f / 2.0f;  // half-height for scale
    addBox(glm::vec3(-0.56f, legH, -0.27f), glm::vec3(0.015f, legH, 0.015f));
    addBox(glm::vec3(0.56f, legH, -0.27f), glm::vec3(0.015f, legH, 0.015f));
    addBox(glm::vec3(-0.56f, legH, 0.27f), glm::vec3(0.015f, legH, 0.015f));
    addBox(glm::vec3(0.56f, legH, 0.27f), glm::vec3(0.015f, legH, 0.015f));

    // Modesty panel (back, between rear legs)
    addBox(glm::vec3(0.0f, 0.36f, 0.27f), glm::vec3(0.56f, 0.34f, 0.005f));

    // Drawer block (right side)
    addBox(glm::vec3(0.35f, 0.58f, 0.0f), glm::vec3(0.18f, 0.15f, 0.26f));
    // Drawer handle
    addBox(glm::vec3(0.35f, 0.58f, -0.27f), glm::vec3(0.06f, 0.008f, 0.005f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 2: Add prison_chair**

Metal chair (0.45m × 0.45m seat, 0.85m total height): seat slab, four legs, backrest.

```cpp
std::unique_ptr<Mesh> createPrisonChair() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Seat: 0.45m × 0.45m, 0.025m thick, at Y=0.45m
    addBox(glm::vec3(0.0f, 0.45f, 0.0f), glm::vec3(0.225f, 0.0125f, 0.225f));

    // Four legs (0.025m × 0.025m cross-section)
    float legH = 0.45f / 2.0f;
    addBox(glm::vec3(-0.19f, legH, -0.19f), glm::vec3(0.0125f, legH, 0.0125f));
    addBox(glm::vec3(0.19f, legH, -0.19f), glm::vec3(0.0125f, legH, 0.0125f));
    addBox(glm::vec3(-0.19f, legH, 0.19f), glm::vec3(0.0125f, legH, 0.0125f));
    addBox(glm::vec3(0.19f, legH, 0.19f), glm::vec3(0.0125f, legH, 0.0125f));

    // Backrest: 0.45m wide, 0.38m tall, 0.02m thick
    addBox(glm::vec3(0.0f, 0.655f, 0.2f), glm::vec3(0.225f, 0.19f, 0.01f));

    // Cross brace between back legs
    addBox(glm::vec3(0.0f, 0.15f, 0.19f), glm::vec3(0.19f, 0.008f, 0.008f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 3: Add prison_cabinet**

Filing cabinet (0.45m × 1.3m × 0.6m): tall box with three drawer seam lines and handles.

```cpp
std::unique_ptr<Mesh> createPrisonCabinet() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main body: 0.45m wide, 1.3m tall, 0.6m deep
    // Origin at bottom-center
    addBox(glm::vec3(0.0f, 0.65f, 0.0f), glm::vec3(0.225f, 0.65f, 0.3f));

    // Three drawer seam lines on front face (thin recessed strips)
    for (float y : {0.435f, 0.87f}) {
        addBox(glm::vec3(0.0f, y, -0.305f), glm::vec3(0.21f, 0.004f, 0.004f));
    }

    // Three drawer handles (small rectangles on front)
    for (float y : {0.25f, 0.65f, 1.08f}) {
        addBox(glm::vec3(0.06f, y, -0.31f), glm::vec3(0.035f, 0.008f, 0.008f));
    }

    // Top edge lip
    addBox(glm::vec3(0.0f, 1.305f, 0.0f), glm::vec3(0.23f, 0.005f, 0.305f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 4: Add prison_shelf**

Wall shelf (0.8m × 0.03m × 0.25m): flat slab with two L-bracket supports.

```cpp
std::unique_ptr<Mesh> createPrisonShelf() {
    auto cube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Shelf surface: 0.8m wide, 0.03m thick, 0.25m deep
    // Origin at the wall-contact point, shelf extends in -Z
    addBox(glm::vec3(0.0f, 0.0f, -0.125f), glm::vec3(0.4f, 0.015f, 0.125f));

    // Two L-bracket supports underneath
    for (float x : {-0.25f, 0.25f}) {
        // Vertical arm (against wall)
        addBox(glm::vec3(x, -0.06f, 0.0f), glm::vec3(0.01f, 0.06f, 0.01f));
        // Horizontal arm (under shelf)
        addBox(glm::vec3(x, -0.02f, -0.07f), glm::vec3(0.01f, 0.005f, 0.07f));
    }

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}
```

**Step 5: Register all furniture meshes**

Add to `registerPrisonAssets()`:

```cpp
    meshLibrary.registerMesh("prison_desk", createPrisonDesk());
    meshLibrary.registerMesh("prison_chair", createPrisonChair());
    meshLibrary.registerMesh("prison_cabinet", createPrisonCabinet());
    meshLibrary.registerMesh("prison_shelf", createPrisonShelf());
```

**Step 6: Commit**

```
git add src/game/levels/prison/PrisonAssets.cpp
git commit -m "Add prison furniture meshes: desk, chair, cabinet, shelf"
```

---

### Task 5: CMake — Add PrisonAssets to game_content target

**Files:**
- Modify: `src/game/CMakeLists.txt:1-12`

**Step 1: Add PrisonAssets.cpp to game_content**

In `src/game/CMakeLists.txt`, add the new source file to the `game_content` target. Insert after line 4 (`levels/cathedral/CathedralAssets.cpp`):

```cmake
add_library(game_content STATIC
    content/ContentRegistry.cpp
    level/LevelDef.cpp
    levels/cathedral/CathedralAssets.cpp
    levels/prison/PrisonAssets.cpp
    rendering/EnvironmentDefinition.cpp
    rendering/EnvironmentProfile.cpp
    rendering/MaterialDefinition.cpp
)
```

**Step 2: Build to verify compilation**

Run: `cmake --build build --target game_content 2>&1 | tail -5`
Expected: Build succeeds with no errors.

**Step 3: Commit**

```
git add src/game/CMakeLists.txt
git commit -m "Add PrisonAssets to game_content CMake target"
```

---

### Task 6: Model Viewer — Register Prison Meshes + Presets

**Files:**
- Modify: `apps/model_viewer/main.cpp:10` (add include)
- Modify: `apps/model_viewer/main.cpp:49-63` (add presets)
- Modify: `apps/model_viewer/main.cpp:145` (add registration call)

**Step 1: Add include**

After line 10 (`#include "game/levels/cathedral/CathedralAssets.h"`), add:

```cpp
#include "game/levels/prison/PrisonAssets.h"
```

**Step 2: Add viewer presets for prison meshes**

In the `buildPresets()` function, after the last existing preset (`cylinder_cap`, line 62), add these entries before the closing `};`:

```cpp
        {"prison_wall",        RetroPalette::Stone, MaterialKind::Stone, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 1.25f, 0.0f), 4.0f, 90.0f, 10.0f},
        {"prison_wall_window", RetroPalette::Stone, MaterialKind::Stone, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 1.25f, 0.0f), 4.0f, 90.0f, 10.0f},
        {"prison_wall_door",   RetroPalette::Stone, MaterialKind::Stone, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 1.25f, 0.0f), 4.0f, 90.0f, 10.0f},
        {"prison_floor",       RetroPalette::Stone, MaterialKind::Stone, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f),              3.6f, 60.0f, 50.0f},
        {"prison_ceiling",     RetroPalette::Stone, MaterialKind::Stone, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f),              3.6f, 60.0f, -50.0f},
        {"prison_baseboard",   RetroPalette::Iron,  MaterialKind::Metal, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 0.075f, 0.0f), 2.0f, 90.0f, 10.0f},
        {"prison_door",        RetroPalette::Iron,  MaterialKind::Metal, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 1.05f, 0.0f), 3.6f, 90.0f, 8.0f},
        {"prison_desk",        RetroPalette::Iron,  MaterialKind::Metal, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 0.4f, 0.0f),  3.0f, 60.0f, 20.0f},
        {"prison_chair",       RetroPalette::Iron,  MaterialKind::Metal, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 0.4f, 0.0f),  2.4f, 60.0f, 20.0f},
        {"prison_cabinet",     RetroPalette::Iron,  MaterialKind::Metal, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 0.65f, 0.0f), 3.2f, 60.0f, 15.0f},
        {"prison_shelf",       RetroPalette::Iron,  MaterialKind::Metal, glm::vec3(1.0f),                glm::vec3(0.0f),  glm::vec3(0.0f, 0.0f, 0.0f),  2.0f, 60.0f, 15.0f},
```

**Step 3: Add registration call**

After line 145 (`registerCathedralAssets(meshLibrary);`), add:

```cpp
    registerPrisonAssets(meshLibrary);
```

Note: Both `registerCathedralAssets` and `registerPrisonAssets` call `registerDefaults()` internally. `MeshLibrary::registerMesh` overwrites duplicates, so calling both is harmless — the defaults just get re-registered.

**Step 4: Build and verify**

Run: `cmake --build build --target procedural-model-viewer 2>&1 | tail -5`
Expected: Build succeeds.

Run: `./build/apps/model_viewer/procedural-model-viewer --list`
Expected: All 23 meshes listed (12 cathedral/primitive + 11 prison).

**Step 5: Commit**

```
git add apps/model_viewer/main.cpp
git commit -m "Add prison mesh presets to model viewer"
```

---

### Task 7: Warden's Office Scene File

**Files:**
- Create: `assets/scenes/warden_office.scene`

**Step 1: Create the scene file**

The room is 6m × 8m (3 tiles × 4 tiles on the 2m grid), ceiling at 2.5m. All coordinates in world space. The room is centered at origin on X, with Z from 0 to 8.

Reference the `.scene` format from `silos_cloister.scene`:
```
mesh <name> <px> <py> <pz> <sx> <sy> <sz> <rx> <ry> <rz> material <id> tint <r> <g> <b>
```

The `<sx> <sy> <sz>` values are **scale** applied to the mesh. Since our prison meshes are already at real-world size, all scales should be `1.0 1.0 1.0`. The `<rx> <ry> <rz>` are rotation in degrees.

Wall orientation: `prison_wall` is created with its face on the +Z side. So:
- Back wall (Z=8): faces -Z → rotation `0 180 0`
- Front wall (Z=0): faces +Z → rotation `0 0 0`
- Left wall (X=-1): faces +X → rotation `0 90 0`
  - Note: walls are 2m wide, room is 6m. X goes from -3 to +3. Left wall center X = -3 (wall extends from -4 to -2 on its local X, but since the wall's local X is along its width and the wall is placed at X=-3, we need to position it at X = -3 + wall_thickness/2 or just -3.0. The wall face is at Z=+0.1 in local space, so at X=-3 with 90° Y rotation, the face is at X=-3+0.1=-2.9 which faces inward.)
  - Actually, let me reconsider. The wall mesh is 2m wide (local X: -1 to +1), 2.5m tall (local Y: 0 to 2.5), 0.2m thick (local Z: -0.1 to +0.1), with the inset border on the +Z face. When we rotate 90° around Y, +Z becomes +X. So the "pretty" face points in the +X direction.
  - For the left wall at X=-3: rotate `0 90 0`, the interior face faces +X (inward). Place at X=-3.

Let me define the coordinate system clearly:
- Room spans X: -3 to +3, Z: 0 to 8, Y: 0 to 2.5
- Wall mesh: 2m wide (X: -1 to +1), 2.5m tall (Y: 0 to 2.5), 0.2m thick (Z: -0.1 to +0.1), front face at Z=+0.1

Wall placements:

**Back wall (Z=8):** 3 panels, rotated 180° Y so front faces inward (-Z). Center panel is `prison_wall_window`.
- Left: `prison_wall` at (-2, 0, 8) rot (0, 180, 0)
- Center: `prison_wall_window` at (0, 0, 8) rot (0, 180, 0)
- Right: `prison_wall` at (2, 0, 8) rot (0, 180, 0)

**Front wall (Z=0):** 3 panels, front faces inward (+Z). Center panel is `prison_wall_door`.
- Left: `prison_wall` at (-2, 0, 0) rot (0, 0, 0)
- Center: `prison_wall_door` at (0, 0, 0) rot (0, 0, 0)
- Right: `prison_wall` at (2, 0, 0) rot (0, 0, 0)

**Left wall (X=-3):** 4 panels, rotated 90° Y so front faces +X (inward).
- `prison_wall` at (-3, 0, 1) rot (0, 90, 0)
- `prison_wall` at (-3, 0, 3) rot (0, 90, 0)
- `prison_wall` at (-3, 0, 5) rot (0, 90, 0)
- `prison_wall` at (-3, 0, 7) rot (0, 90, 0)

**Right wall (X=3):** 4 panels, rotated -90° Y so front faces -X (inward).
- `prison_wall` at (3, 0, 1) rot (0, -90, 0)
- `prison_wall` at (3, 0, 3) rot (0, -90, 0)
- `prison_wall` at (3, 0, 5) rot (0, -90, 0)
- `prison_wall` at (3, 0, 7) rot (0, -90, 0)

Floor tiles (3×4 grid, centered at each 2m cell):
Columns at X: -2, 0, 2. Rows at Z: 1, 3, 5, 7.

Ceiling tiles same positions at Y=2.5.

Create `assets/scenes/warden_office.scene`:

```
environment_profile default

# ── Floor (3×4 grid of 2m tiles) ──
mesh prison_floor -2.0 0.0 1.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.32 0.30 0.28
mesh prison_floor  0.0 0.0 1.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.30 0.28 0.26
mesh prison_floor  2.0 0.0 1.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.31 0.29 0.27
mesh prison_floor -2.0 0.0 3.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.30 0.28 0.26
mesh prison_floor  0.0 0.0 3.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.32 0.30 0.28
mesh prison_floor  2.0 0.0 3.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.30 0.28 0.26
mesh prison_floor -2.0 0.0 5.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.31 0.29 0.27
mesh prison_floor  0.0 0.0 5.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.30 0.28 0.26
mesh prison_floor  2.0 0.0 5.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.32 0.30 0.28
mesh prison_floor -2.0 0.0 7.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.30 0.28 0.26
mesh prison_floor  0.0 0.0 7.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.31 0.29 0.27
mesh prison_floor  2.0 0.0 7.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.30 0.28 0.26

# ── Ceiling (3×4 grid at Y=2.5) ──
mesh prison_ceiling -2.0 2.5 1.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.28 0.26 0.24
mesh prison_ceiling  0.0 2.5 1.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.27 0.25 0.23
mesh prison_ceiling  2.0 2.5 1.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.28 0.26 0.24
mesh prison_ceiling -2.0 2.5 3.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.27 0.25 0.23
mesh prison_ceiling  0.0 2.5 3.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.28 0.26 0.24
mesh prison_ceiling  2.0 2.5 3.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.27 0.25 0.23
mesh prison_ceiling -2.0 2.5 5.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.28 0.26 0.24
mesh prison_ceiling  0.0 2.5 5.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.27 0.25 0.23
mesh prison_ceiling  2.0 2.5 5.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.28 0.26 0.24
mesh prison_ceiling -2.0 2.5 7.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.27 0.25 0.23
mesh prison_ceiling  0.0 2.5 7.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.28 0.26 0.24
mesh prison_ceiling  2.0 2.5 7.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.27 0.25 0.23

# ── Back wall (Z=8, rotated 180° to face inward) ──
mesh prison_wall         -2.0 0.0 8.0 1.0 1.0 1.0 0.0 180.0 0.0 material stone_default tint 0.35 0.33 0.30
mesh prison_wall_window   0.0 0.0 8.0 1.0 1.0 1.0 0.0 180.0 0.0 material stone_default tint 0.34 0.32 0.29
mesh prison_wall          2.0 0.0 8.0 1.0 1.0 1.0 0.0 180.0 0.0 material stone_default tint 0.36 0.34 0.31

# ── Front wall (Z=0, faces +Z inward) ──
mesh prison_wall         -2.0 0.0 0.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.35 0.33 0.30
mesh prison_wall_door     0.0 0.0 0.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.34 0.32 0.29
mesh prison_wall          2.0 0.0 0.0 1.0 1.0 1.0 0.0 0.0 0.0 material stone_default tint 0.36 0.34 0.31

# ── Left wall (X=-3, rotated 90° to face +X inward) ──
mesh prison_wall -3.0 0.0 1.0 1.0 1.0 1.0 0.0 90.0 0.0 material stone_default tint 0.33 0.31 0.28
mesh prison_wall -3.0 0.0 3.0 1.0 1.0 1.0 0.0 90.0 0.0 material stone_default tint 0.34 0.32 0.29
mesh prison_wall -3.0 0.0 5.0 1.0 1.0 1.0 0.0 90.0 0.0 material stone_default tint 0.33 0.31 0.28
mesh prison_wall -3.0 0.0 7.0 1.0 1.0 1.0 0.0 90.0 0.0 material stone_default tint 0.34 0.32 0.29

# ── Right wall (X=3, rotated -90° to face -X inward) ──
mesh prison_wall 3.0 0.0 1.0 1.0 1.0 1.0 0.0 -90.0 0.0 material stone_default tint 0.34 0.32 0.29
mesh prison_wall 3.0 0.0 3.0 1.0 1.0 1.0 0.0 -90.0 0.0 material stone_default tint 0.33 0.31 0.28
mesh prison_wall 3.0 0.0 5.0 1.0 1.0 1.0 0.0 -90.0 0.0 material stone_default tint 0.34 0.32 0.29
mesh prison_wall 3.0 0.0 7.0 1.0 1.0 1.0 0.0 -90.0 0.0 material stone_default tint 0.33 0.31 0.28

# ── Prison door (in the door frame, front wall center) ──
mesh prison_door 0.0 0.0 0.05 1.0 1.0 1.0 0.0 0.0 0.0 material metal_default tint 0.12 0.10 0.08

# ── Furniture ──
# Desk: centered in room at roughly (0, 0, 4.5)
mesh prison_desk 0.0 0.0 4.5 1.0 1.0 1.0 0.0 180.0 0.0 material metal_default tint 0.14 0.12 0.10

# Chair: south of desk, facing north (toward desk)
mesh prison_chair 0.0 0.0 3.6 1.0 1.0 1.0 0.0 0.0 0.0 material metal_default tint 0.12 0.10 0.08

# Filing cabinet: back-right corner
mesh prison_cabinet 2.2 0.0 7.4 1.0 1.0 1.0 0.0 180.0 0.0 material metal_default tint 0.15 0.13 0.10

# Wall shelf: back-left wall (mounted at Y=1.6)
mesh prison_shelf -2.85 1.6 6.5 1.0 1.0 1.0 0.0 -90.0 0.0 material metal_default tint 0.13 0.11 0.09

# ── Baseboards (along all walls) ──
# Back wall baseboards
mesh prison_baseboard -2.0 0.0 7.9 1.0 1.0 1.0 0.0 180.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard  0.0 0.0 7.9 1.0 1.0 1.0 0.0 180.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard  2.0 0.0 7.9 1.0 1.0 1.0 0.0 180.0 0.0 material metal_default tint 0.10 0.08 0.06
# Front wall baseboards (skip center — door frame)
mesh prison_baseboard -2.0 0.0 0.1 1.0 1.0 1.0 0.0 0.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard  2.0 0.0 0.1 1.0 1.0 1.0 0.0 0.0 0.0 material metal_default tint 0.10 0.08 0.06
# Left wall baseboards
mesh prison_baseboard -2.9 0.0 1.0 1.0 1.0 1.0 0.0 90.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard -2.9 0.0 3.0 1.0 1.0 1.0 0.0 90.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard -2.9 0.0 5.0 1.0 1.0 1.0 0.0 90.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard -2.9 0.0 7.0 1.0 1.0 1.0 0.0 90.0 0.0 material metal_default tint 0.10 0.08 0.06
# Right wall baseboards
mesh prison_baseboard 2.9 0.0 1.0 1.0 1.0 1.0 0.0 -90.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard 2.9 0.0 3.0 1.0 1.0 1.0 0.0 -90.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard 2.9 0.0 5.0 1.0 1.0 1.0 0.0 -90.0 0.0 material metal_default tint 0.10 0.08 0.06
mesh prison_baseboard 2.9 0.0 7.0 1.0 1.0 1.0 0.0 -90.0 0.0 material metal_default tint 0.10 0.08 0.06

# ── Lighting ──
# Harsh desk lamp — spot light pointing down at the desk
spot_light 0.0 2.4 4.5 0.0 -1.0 0.0 1.0 0.95 0.85 5.0 1.2 25.0 35.0 0

# Dim window glow — faint exterior light through barred window
light 0.0 1.8 7.8 0.6 0.65 0.75 4.0 0.15

# Faint ambient fill from ceiling (barely visible)
light 0.0 2.3 2.0 0.8 0.75 0.7 6.0 0.08

# ── Colliders ──
# Back wall
collider_box 0.0 1.25 8.0 3.0 1.25 0.1
# Front wall left
collider_box -2.0 1.25 0.0 1.0 1.25 0.1
# Front wall right
collider_box 2.0 1.25 0.0 1.0 1.25 0.1
# Front wall top (above door)
collider_box 0.0 2.3 0.0 1.0 0.2 0.1
# Left wall
collider_box -3.0 1.25 4.0 0.1 1.25 4.0
# Right wall
collider_box 3.0 1.25 4.0 0.1 1.25 4.0
# Desk
collider_box 0.0 0.375 4.5 0.6 0.375 0.3
# Filing cabinet
collider_box 2.2 0.65 7.4 0.225 0.65 0.3

# ── Player spawn ──
# Spawn at chair position, facing south toward the door
player_spawn 0.0 0.0 3.6 -2.0
```

**Step 2: Commit**

```
git add assets/scenes/warden_office.scene
git commit -m "Add warden_office.scene level layout"
```

---

### Task 8: WardenOfficeScene — Scene Class + CMake

**Files:**
- Create: `src/game/scenes/WardenOfficeScene.h`
- Create: `src/game/scenes/WardenOfficeScene.cpp`
- Modify: `src/game/CMakeLists.txt:26-44` (add to gameplay target)

**Step 1: Create WardenOfficeScene.h**

Follow the exact pattern of `SilosCloisterScene.h`:

```cpp
#pragma once

#include "engine/scene/Scene.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "game/level/LevelLoader.h"

#include <entt/entt.hpp>
#include <vector>

class WardenOfficeScene : public Scene {
public:
    void onEnter(Application& app) override;
    void onExit(Application& app) override;

private:
    MeshLibrary meshLibrary_;
    std::vector<entt::entity> entities_;
    LevelLoadRequest request_{
        .levelId = "warden_office",
        .levelPath = "assets/scenes/warden_office.scene",
    };
};
```

**Step 2: Create WardenOfficeScene.cpp**

Follow the exact pattern of `SilosCloisterScene.cpp`, but use `registerPrisonAssets`:

```cpp
#include "WardenOfficeScene.h"

#include "engine/core/Application.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelLoader.h"
#include "game/levels/prison/PrisonAssets.h"
#include "game/rendering/MeshAssetProvider.h"
#include "game/rendering/EnvironmentProfile.h"

void WardenOfficeScene::onEnter(Application& app) {
    entities_.clear();
    LevelBuildContext context{
        .registry = app.registry(),
        .meshLibrary = meshLibrary_,
        .entities = entities_,
    };
    request_.registerAssets = [](MeshLibrary& library) {
        registerPrisonAssets(library);
    };
    request_.buildScriptedGeometry = {};
    LevelLoader loader(context);
    loader.load(app, request_);
}

void WardenOfficeScene::onExit(Application& app) {
    for (auto entity : entities_) {
        if (app.registry().valid(entity)) {
            app.registry().destroy(entity);
        }
    }
    entities_.clear();
    auto& ctx = app.registry().ctx();
    if (ctx.contains<MeshAssetProvider>()) {
        ctx.erase<MeshAssetProvider>();
    }
    if (ctx.contains<ActiveEnvironmentProfile>()) {
        ctx.erase<ActiveEnvironmentProfile>();
    }
    meshLibrary_.clear();
}
```

**Step 3: Add to CMakeLists.txt**

In `src/game/CMakeLists.txt`, add `scenes/WardenOfficeScene.cpp` to the `gameplay` target. Insert after `scenes/SilosCloisterScene.cpp` (line 35):

```cmake
    scenes/SilosCloisterScene.cpp
    scenes/WardenOfficeScene.cpp
```

**Step 4: Build**

Run: `cmake --build build --target gameplay 2>&1 | tail -5`
Expected: Build succeeds.

**Step 5: Commit**

```
git add src/game/scenes/WardenOfficeScene.h src/game/scenes/WardenOfficeScene.cpp src/game/CMakeLists.txt
git commit -m "Add WardenOfficeScene loading warden_office.scene with prison assets"
```

---

### Task 9: Wire WardenOfficeScene Into the Application

**Files:**
- This task depends on how scenes are selected at startup. Check how `CathedralScene` and `SilosCloisterScene` are registered in the application or scene manager.

**Step 1: Find the scene registration point**

Search for where `SilosCloisterScene` or `CathedralScene` is instantiated/registered. Look in:
- `src/engine/core/Application.cpp` or `main.cpp`
- `src/engine/scene/SceneManager.h/.cpp`
- `apps/` — the main executable entry point

The pattern will likely be something like `sceneManager.registerScene<SilosCloisterScene>("silos_cloister")`. Add the same for `WardenOfficeScene`.

**Step 2: Register the WardenOfficeScene**

Add an include for `game/scenes/WardenOfficeScene.h` and register it the same way as the other scenes. If the app supports `--scene warden_office` via `GenericFileScene`, update `GenericFileScene` to use `registerPrisonAssets` when loading prison scenes — or just register `WardenOfficeScene` as a named scene.

**Step 3: Commit**

```
git commit -m "Register WardenOfficeScene in application scene list"
```

---

### Task 10: Full Build + Visual Verification

**Step 1: Clean rebuild**

Run: `cmake --build build --target pixel-roguelike 2>&1 | tail -10`
Expected: Build succeeds with no errors or warnings related to prison assets.

**Step 2: Verify model viewer**

Run: `./build/apps/model_viewer/procedural-model-viewer --mesh prison_wall`
Use `[`/`]` to cycle through all prison meshes. Toggle `TAB` for stylized view. Verify:
- All 11 prison meshes render correctly
- Walls have visible inset borders
- Window has bars
- Door frame has visible cutout
- Door has observation window, handle, hinges
- Furniture looks proportional
- Materials read correctly under 1-bit dither

**Step 3: Verify warden_office.scene**

Launch with: `./build/apps/pixel-roguelike --scene assets/scenes/warden_office.scene`
Verify:
- Room renders as enclosed box
- Can walk around inside
- Furniture placed correctly
- Lighting creates harsh overhead + dim window ambiance
- Colliders prevent walking through walls
- 1-bit dither effect works on prison materials

**Step 4: Commit any fixes needed**

If anything needs adjustment (positions, scales, lighting values), fix and commit.
