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

// ---------------------------------------------------------------------------
// Task 1: Architectural meshes
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createPrisonWall() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main slab: 2m wide, 3.5m tall, 0.2m thick
    // Origin at bottom-center of the wall panel, Y=0 is floor level
    addBox(glm::vec3(0.0f, 1.75f, 0.0f), glm::vec3(1.0f, 1.75f, 0.1f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createPrisonWallWindow() {
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

    // Window opening: 0.6m wide, 0.4m tall, centered at Y=1.8m
    // Bottom section: 0 to 1.6m
    addBox(glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(1.0f, 0.8f, 0.1f));
    // Top section: 2.0m to 3.5m
    addBox(glm::vec3(0.0f, 2.75f, 0.0f), glm::vec3(1.0f, 0.75f, 0.1f));
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createPrisonWallDoor() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Door opening: 0.9m wide, 2.1m tall, bottom at Y=0
    // Top section: 2.1m to 3.5m (full width)
    addBox(glm::vec3(0.0f, 2.8f, 0.0f), glm::vec3(1.0f, 0.7f, 0.1f));
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

// ---------------------------------------------------------------------------
// Task 2: Floor, ceiling, baseboard
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createPrisonFloor() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main slab: 2m × 2m, 0.1m thick, top surface at Y=0
    addBox(glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(1.0f, 0.05f, 1.0f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createPrisonCeiling() {
    auto cube = generateCube(2.0f);
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createPrisonBaseboard() {
    auto cube = generateCube(2.0f);
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

// ---------------------------------------------------------------------------
// Task 3: Prison door
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createPrisonDoor() {
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

// ---------------------------------------------------------------------------
// Task 4: Furniture
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createPrisonDesk() {
    auto cube = generateCube(2.0f);
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createPrisonChair() {
    auto cube = generateCube(2.0f);
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createWardenChair() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // ── Structural: legs ──
    // Four legs (0.03m x 0.03m cross-section, 0.45m tall)
    // Back legs extend up to support backrest (0.85m total)
    float frontLegH = 0.45f / 2.0f;
    addBox(glm::vec3(-0.2f, frontLegH, -0.2f), glm::vec3(0.015f, frontLegH, 0.015f));   // Front-left leg
    addBox(glm::vec3(0.2f, frontLegH, -0.2f), glm::vec3(0.015f, frontLegH, 0.015f));    // Front-right leg
    float backLegH = 0.85f / 2.0f;
    addBox(glm::vec3(-0.2f, backLegH, 0.2f), glm::vec3(0.015f, backLegH, 0.015f));      // Back-left leg
    addBox(glm::vec3(0.2f, backLegH, 0.2f), glm::vec3(0.015f, backLegH, 0.015f));       // Back-right leg

    // ── Structural: seat ──
    // Seat slab: 0.44m x 0.44m, 0.03m thick, at Y=0.45m
    addBox(glm::vec3(0.0f, 0.45f, 0.0f), glm::vec3(0.22f, 0.015f, 0.22f));

    // ── Structural: backrest ──
    // Backrest panel: 0.44m wide, 0.32m tall, 0.02m thick
    addBox(glm::vec3(0.0f, 0.69f, 0.2f), glm::vec3(0.22f, 0.16f, 0.01f));

    // ── Structural: armrests ──
    // Left armrest horizontal bar
    addBox(glm::vec3(-0.2f, 0.62f, 0.0f), glm::vec3(0.015f, 0.01f, 0.2f));
    // Right armrest horizontal bar
    addBox(glm::vec3(0.2f, 0.62f, 0.0f), glm::vec3(0.015f, 0.01f, 0.2f));
    // Left armrest front support (vertical)
    addBox(glm::vec3(-0.2f, 0.535f, -0.18f), glm::vec3(0.012f, 0.075f, 0.012f));
    // Right armrest front support (vertical)
    addBox(glm::vec3(0.2f, 0.535f, -0.18f), glm::vec3(0.012f, 0.075f, 0.012f));

    // ── Detail: cross braces ──
    // Front cross brace between front legs
    addBox(glm::vec3(0.0f, 0.12f, -0.19f), glm::vec3(0.185f, 0.008f, 0.008f));
    // Back cross brace between back legs
    addBox(glm::vec3(0.0f, 0.12f, 0.19f), glm::vec3(0.185f, 0.008f, 0.008f));
    // Left side brace
    addBox(glm::vec3(-0.19f, 0.12f, 0.0f), glm::vec3(0.008f, 0.008f, 0.185f));

    // ── Detail: backrest top rail ──
    // Slightly wider rail along top of backrest
    addBox(glm::vec3(0.0f, 0.855f, 0.2f), glm::vec3(0.23f, 0.012f, 0.015f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createPrisonCabinet() {
    auto cube = generateCube(2.0f);
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createPrisonShelf() {
    auto cube = generateCube(2.0f);
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
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

// ---------------------------------------------------------------------------
// Task: Stanley Parable office additions
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createCeilingLightPanel() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Recessed rectangular fluorescent ceiling panel: 0.9m long x 0.3m wide
    // Origin at center of panel; Y=0 is ceiling surface level, panel hangs below.
    // Outer frame: 0.015m wide strips, 0.01m thick forming the border
    // Long sides (along X axis)
    addBox(glm::vec3(0.0f, -0.005f, -0.15f), glm::vec3(0.45f, 0.005f, 0.015f));  // Front long frame strip
    addBox(glm::vec3(0.0f, -0.005f, 0.15f), glm::vec3(0.45f, 0.005f, 0.015f));   // Back long frame strip
    // Short sides (along Z axis)
    addBox(glm::vec3(-0.45f, -0.005f, 0.0f), glm::vec3(0.015f, 0.005f, 0.135f)); // Left short frame strip
    addBox(glm::vec3(0.45f, -0.005f, 0.0f), glm::vec3(0.015f, 0.005f, 0.135f));  // Right short frame strip

    // Panel surface: translucent light diffuser, recessed 0.035m above frame level
    // 0.9m x 0.3m, 0.005m thick, sits at Y=-0.035m
    addBox(glm::vec3(0.0f, -0.035f, 0.0f), glm::vec3(0.435f, 0.0025f, 0.12f));   // Diffuser panel

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

std::unique_ptr<Mesh> createPrisonWallLargeWindow() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Large office window wall: 2m wide, 3.5m tall, 0.2m thick
    // Window opening: 1.4m wide, 1.2m tall, centered at Y=1.8m (eye to above-head)
    // Bottom section: 0 to 1.2m
    addBox(glm::vec3(0.0f, 0.6f, 0.0f), glm::vec3(1.0f, 0.6f, 0.1f));
    // Top section: 2.4m to 3.5m
    addBox(glm::vec3(0.0f, 2.95f, 0.0f), glm::vec3(1.0f, 0.55f, 0.1f));
    // Left section beside window (0.3m wide strip)
    addBox(glm::vec3(-0.85f, 1.8f, 0.0f), glm::vec3(0.15f, 0.6f, 0.1f));
    // Right section beside window (0.3m wide strip)
    addBox(glm::vec3(0.85f, 1.8f, 0.0f), glm::vec3(0.15f, 0.6f, 0.1f));

    // Window sill: thin ledge at bottom of opening, 0.01m thick, 0.06m deep
    addBox(glm::vec3(0.0f, 1.205f, 0.06f), glm::vec3(0.7f, 0.005f, 0.06f));

    // Single thin mullion: vertical center divider (0.02m wide) for visual interest
    addBox(glm::vec3(0.0f, 1.8f, 0.0f), glm::vec3(0.01f, 0.6f, 0.105f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

} // namespace

void registerPrisonAssets(MeshLibrary& meshLibrary) {
    meshLibrary.registerDefaults();
    // Architectural walls
    meshLibrary.registerMesh("prison_wall", createPrisonWall());
    meshLibrary.registerMesh("prison_wall_window", createPrisonWallWindow());
    meshLibrary.registerMesh("prison_wall_door", createPrisonWallDoor());
    // Floor, ceiling, baseboard
    meshLibrary.registerMesh("prison_floor", createPrisonFloor());
    meshLibrary.registerMesh("prison_ceiling", createPrisonCeiling());
    meshLibrary.registerMesh("prison_baseboard", createPrisonBaseboard());
    // Door
    meshLibrary.registerMesh("prison_door", createPrisonDoor());
    // Furniture
    meshLibrary.registerMesh("prison_desk", createPrisonDesk());
    meshLibrary.registerMesh("prison_chair", createPrisonChair());
    meshLibrary.registerMesh("warden_chair", createWardenChair());
    meshLibrary.registerMesh("prison_cabinet", createPrisonCabinet());
    meshLibrary.registerMesh("prison_shelf", createPrisonShelf());
    // Stanley Parable office additions
    meshLibrary.registerMesh("ceiling_light_panel", createCeilingLightPanel());
    meshLibrary.registerMesh("prison_wall_large_window", createPrisonWallLargeWindow());
}
