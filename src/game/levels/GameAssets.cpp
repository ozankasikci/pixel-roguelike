#include "game/levels/GameAssets.h"

#include "engine/rendering/geometry/MeshGeometry.h"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Shared helpers (file-local)
// ---------------------------------------------------------------------------

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
// Cathedral helpers
// ---------------------------------------------------------------------------

void addReliefChain(std::vector<std::pair<RawMeshData, glm::mat4>>& parts,
                    const RawMeshData& cylinder,
                    const std::vector<glm::vec2>& points,
                    float radius,
                    float depth) {
    if (points.size() < 2) {
        return;
    }

    for (size_t i = 0; i + 1 < points.size(); ++i) {
        const glm::vec2 start = points[i];
        const glm::vec2 end = points[i + 1];
        const glm::vec2 delta = end - start;
        const float length = glm::length(delta);
        if (length <= 0.0001f) {
            continue;
        }

        const glm::vec2 mid = (start + end) * 0.5f;
        const float angleDeg = glm::degrees(std::atan2(delta.y, delta.x));
        parts.push_back({
            cylinder,
            makeModel(glm::vec3(mid.x, mid.y, depth),
                      glm::vec3(radius, length, radius),
                      glm::vec3(0.0f, 0.0f, -angleDeg))
        });
    }
}

std::unique_ptr<Mesh> createRomanesqueDoorFrameMesh() {
    auto cube = generateCube(1.0f);
    auto cylinder = generateCylinder(1.0f, 1.0f, 20);
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

    // Broad plinth and threshold blocks.
    addBox(glm::vec3(0.0f, -0.54f, -0.05f), glm::vec3(0.84f, 0.06f, 0.34f));
    addBox(glm::vec3(0.0f, -0.48f, 0.10f), glm::vec3(0.68f, 0.04f, 0.14f));
    addBox(glm::vec3(0.0f, -0.62f, 0.16f), glm::vec3(0.92f, 0.03f, 0.20f));

    // Thick jamb masses and outer reveal.
    addBox(glm::vec3(-0.62f, -0.02f, -0.03f), glm::vec3(0.12f, 0.92f, 0.24f));
    addBox(glm::vec3(0.62f, -0.02f, -0.03f), glm::vec3(0.12f, 0.92f, 0.24f));
    addBox(glm::vec3(-0.48f, -0.02f, 0.06f), glm::vec3(0.10f, 0.86f, 0.18f));
    addBox(glm::vec3(0.48f, -0.02f, 0.06f), glm::vec3(0.10f, 0.86f, 0.18f));

    // Engaged jamb columns with simple Romanesque bases/cushion capitals.
    for (float side : {-1.0f, 1.0f}) {
        addCylinder(glm::vec3(0.34f * side, -0.12f, 0.16f), glm::vec3(0.032f, 0.62f, 0.032f));
        addBox(glm::vec3(0.34f * side, -0.44f, 0.16f), glm::vec3(0.070f, 0.055f, 0.070f));
        addBox(glm::vec3(0.34f * side, 0.20f, 0.16f), glm::vec3(0.085f, 0.055f, 0.085f));
        addBox(glm::vec3(0.34f * side, 0.26f, 0.15f), glm::vec3(0.070f, 0.020f, 0.070f));
    }

    // Recessed semicircular archivolts, sampled as heavy stone bands.
    auto addArchivolt = [&](float radius, float depth, float bandWidth, float sideX, float sideHeight) {
        addBox(glm::vec3(-sideX, -0.02f, depth), glm::vec3(bandWidth, sideHeight, 0.08f));
        addBox(glm::vec3(sideX, -0.02f, depth), glm::vec3(bandWidth, sideHeight, 0.08f));

        std::vector<glm::vec2> archPoints;
        constexpr int kSegments = 12;
        for (int i = 0; i <= kSegments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSegments);
            const float angle = glm::pi<float>() * (1.0f - t);
            archPoints.emplace_back(std::cos(angle) * radius, 0.28f + std::sin(angle) * radius);
        }
        addReliefChain(parts, cylinder, archPoints, bandWidth * 0.55f, depth);
    };

    addArchivolt(0.44f, 0.22f, 0.030f, 0.44f, 0.62f);
    addArchivolt(0.38f, 0.16f, 0.028f, 0.38f, 0.56f);
    addArchivolt(0.32f, 0.10f, 0.026f, 0.32f, 0.50f);
    addArchivolt(0.26f, 0.04f, 0.022f, 0.26f, 0.44f);

    // Tympanum field and simple carved molding bands.
    addBox(glm::vec3(0.0f, 0.30f, -0.01f), glm::vec3(0.24f, 0.12f, 0.06f));
    addBox(glm::vec3(0.0f, 0.40f, 0.12f), glm::vec3(0.54f, 0.025f, 0.05f));
    addBox(glm::vec3(0.0f, -0.30f, 0.12f), glm::vec3(0.54f, 0.025f, 0.05f));
    addReliefChain(parts, cylinder, {
        {-0.46f, 0.34f}, {-0.28f, 0.44f}, {0.0f, 0.50f}, {0.28f, 0.44f}, {0.46f, 0.34f}
    }, 0.014f, 0.24f);

    // Side blind recesses keep the surround broad and facade-like.
    for (float side : {-1.0f, 1.0f}) {
        addBox(glm::vec3(0.82f * side, -0.06f, -0.05f), glm::vec3(0.16f, 0.72f, 0.16f));
        addBox(glm::vec3(0.82f * side, -0.34f, 0.02f), glm::vec3(0.10f, 0.04f, 0.08f));
        addReliefChain(parts, cylinder, {
            {0.73f * side, 0.14f},
            {0.76f * side, 0.24f},
            {0.82f * side, 0.28f},
            {0.88f * side, 0.24f},
            {0.91f * side, 0.14f}
        }, 0.012f, 0.10f);
    }

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}

std::unique_ptr<Mesh> createWoodDoorLeafMesh(bool leftLeaf) {
    auto cube = generateCube(1.0f);
    auto cylinder = generateCylinder(1.0f, 1.0f, 24);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;
    const float hand = leftLeaf ? 1.0f : -1.0f;
    const float hingeSideX = -0.45f * hand;
    const float meetingEdgeX = 0.45f * hand;

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

    // Core timber volume and heavy perimeter frame.
    addBox(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.98f, 1.02f, 0.66f));
    addBox(glm::vec3(0.0f, 0.46f, 0.12f), glm::vec3(0.92f, 0.08f, 0.18f));
    addBox(glm::vec3(0.0f, -0.46f, 0.12f), glm::vec3(0.92f, 0.08f, 0.18f));
    addBox(glm::vec3(-0.41f, 0.0f, 0.12f), glm::vec3(0.08f, 0.86f, 0.18f));
    addBox(glm::vec3(0.41f, 0.0f, 0.12f), glm::vec3(0.08f, 0.86f, 0.18f));

    // Vertical planks: structural and readable, not ornate.
    for (float x : {-0.28f, -0.09f, 0.10f, 0.29f}) {
        addBox(glm::vec3(x, 0.0f, 0.02f), glm::vec3(0.14f, 0.84f, 0.12f));
    }

    // Semicircular blind-arch panel language on the leaves.
    addBox(glm::vec3(-0.16f, -0.05f, 0.20f), glm::vec3(0.018f, 0.58f, 0.04f));
    addBox(glm::vec3(0.16f, -0.05f, 0.20f), glm::vec3(0.018f, 0.58f, 0.04f));
    addReliefChain(parts, cylinder, {
        {-0.16f, 0.22f}, {-0.11f, 0.31f}, {-0.04f, 0.37f}, {0.04f, 0.37f}, {0.11f, 0.31f}, {0.16f, 0.22f}
    }, 0.018f, 0.22f);
    addBox(glm::vec3(0.0f, -0.35f, 0.20f), glm::vec3(0.18f, 0.020f, 0.04f));
    addBox(glm::vec3(0.0f, -0.08f, 0.18f), glm::vec3(0.28f, 0.020f, 0.03f));

    // Simple moulding bands and meeting stile.
    addBox(glm::vec3(0.0f, 0.33f, 0.18f), glm::vec3(0.32f, 0.020f, 0.04f));
    addBox(glm::vec3(0.0f, 0.05f, 0.18f), glm::vec3(0.34f, 0.020f, 0.04f));
    addBox(glm::vec3(0.0f, -0.22f, 0.18f), glm::vec3(0.34f, 0.020f, 0.04f));
    addBox(glm::vec3(meetingEdgeX - 0.12f * hand, 0.0f, 0.22f), glm::vec3(0.040f, 0.90f, 0.05f));
    addBox(glm::vec3(meetingEdgeX - 0.17f * hand, 0.0f, 0.18f), glm::vec3(0.016f, 0.88f, 0.03f));

    // Rear planking and braces so the door reads like a built object from both sides.
    for (float x : {-0.28f, 0.0f, 0.28f}) {
        addBox(glm::vec3(x, 0.0f, -0.12f), glm::vec3(0.18f, 0.84f, 0.10f));
    }
    addBox(glm::vec3(0.0f, 0.28f, -0.16f), glm::vec3(0.74f, 0.08f, 0.07f));
    addBox(glm::vec3(0.0f, -0.24f, -0.16f), glm::vec3(0.74f, 0.08f, 0.07f));
    addBox(glm::vec3(0.03f * hand, 0.02f, -0.18f), glm::vec3(0.82f, 0.06f, 0.06f), glm::vec3(0.0f, 0.0f, 30.0f * hand));
    addBox(glm::vec3(-0.03f * hand, -0.02f, -0.18f), glm::vec3(0.82f, 0.06f, 0.06f), glm::vec3(0.0f, 0.0f, -30.0f * hand));

    // Meeting edge rebate and hinge-side stop.
    addBox(glm::vec3(meetingEdgeX, 0.0f, 0.16f), glm::vec3(0.05f, 0.92f, 0.12f));
    addBox(glm::vec3(meetingEdgeX - 0.03f * hand, 0.0f, 0.22f), glm::vec3(0.018f, 0.92f, 0.08f));
    addBox(glm::vec3(meetingEdgeX - 0.055f * hand, 0.0f, 0.10f), glm::vec3(0.012f, 0.88f, 0.06f));
    addBox(glm::vec3(hingeSideX, 0.0f, 0.18f), glm::vec3(0.06f, 0.88f, 0.10f));

    // Hinge barrels and broad Romanesque strap hardware.
    for (float y : {0.30f, 0.0f, -0.30f}) {
        addCylinder(glm::vec3(hingeSideX - 0.05f * hand, y, 0.20f), glm::vec3(0.026f, 0.18f, 0.026f));
        addBox(glm::vec3(hingeSideX - 0.015f * hand, y, 0.18f), glm::vec3(0.08f, 0.05f, 0.06f));
    }
    addBox(glm::vec3(hingeSideX + 0.11f * hand, 0.30f, 0.20f), glm::vec3(0.26f, 0.035f, 0.03f));
    addBox(glm::vec3(hingeSideX + 0.13f * hand, 0.00f, 0.20f), glm::vec3(0.30f, 0.035f, 0.03f));
    addBox(glm::vec3(hingeSideX + 0.10f * hand, -0.30f, 0.20f), glm::vec3(0.24f, 0.035f, 0.03f));

    // Ring pull and lock plate near the center.
    addBox(glm::vec3(meetingEdgeX - 0.11f * hand, -0.02f, 0.24f), glm::vec3(0.045f, 0.14f, 0.025f));
    addCylinder(glm::vec3(meetingEdgeX - 0.14f * hand, -0.02f, 0.28f),
                glm::vec3(0.032f, 0.012f, 0.032f),
                glm::vec3(90.0f, 0.0f, 0.0f));
    addBox(glm::vec3(meetingEdgeX - 0.14f * hand, -0.02f, 0.28f), glm::vec3(0.016f, 0.06f, 0.010f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents, merged.indices);
}

// ---------------------------------------------------------------------------
// Prison architectural meshes
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createPrisonWall() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main slab: 2m wide, 4.0m tall, 0.2m thick
    // Origin at bottom-center of the wall panel, Y=0 is floor level
    addBox(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(1.0f, 2.0f, 0.1f));

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
    // Top section: 2.0m to 4.0m
    addBox(glm::vec3(0.0f, 3.0f, 0.0f), glm::vec3(1.0f, 1.0f, 0.1f));
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
    // Top section: 2.1m to 4.0m (full width)
    addBox(glm::vec3(0.0f, 3.05f, 0.0f), glm::vec3(1.0f, 0.95f, 0.1f));
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
// Prison floor, ceiling, baseboard
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createPrisonFloor() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Main slab: 2m x 2m, 0.1m thick, top surface at Y=0
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

    // Main slab: 2m x 2m, 0.1m thick, bottom surface at Y=0
    addBox(glm::vec3(0.0f, 0.05f, 0.0f), glm::vec3(1.0f, 0.05f, 1.0f));

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
// Office door (warden's office — clean Stanley Parable style)
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createOfficeDoor() {
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

    // Main door slab: 0.9m wide, 2.1m tall, 0.045m thick
    // Origin at bottom-center, hinge side on left (-X)
    addBox(glm::vec3(0.0f, 1.05f, 0.0f), glm::vec3(0.45f, 1.05f, 0.0225f));

    // Raised stile and rail frame on front face
    // Left stile
    addBox(glm::vec3(-0.39f, 1.05f, 0.026f), glm::vec3(0.04f, 0.98f, 0.004f));
    // Right stile
    addBox(glm::vec3(0.39f, 1.05f, 0.026f), glm::vec3(0.04f, 0.98f, 0.004f));
    // Top rail
    addBox(glm::vec3(0.0f, 2.0f, 0.026f), glm::vec3(0.35f, 0.04f, 0.004f));
    // Bottom rail
    addBox(glm::vec3(0.0f, 0.10f, 0.026f), glm::vec3(0.35f, 0.05f, 0.004f));
    // Lock rail (divides upper window from lower panel)
    addBox(glm::vec3(0.0f, 1.18f, 0.026f), glm::vec3(0.35f, 0.035f, 0.004f));

    // Upper panel: rectangular office vision window
    // Glass pane (recessed into door face)
    addBox(glm::vec3(0.0f, 1.62f, 0.020f), glm::vec3(0.26f, 0.30f, 0.003f));
    // Window frame strips
    addBox(glm::vec3(0.0f, 1.925f, 0.026f), glm::vec3(0.27f, 0.008f, 0.004f));  // top
    addBox(glm::vec3(0.0f, 1.315f, 0.026f), glm::vec3(0.27f, 0.008f, 0.004f));  // bottom
    addBox(glm::vec3(-0.265f, 1.62f, 0.026f), glm::vec3(0.008f, 0.305f, 0.004f));  // left
    addBox(glm::vec3(0.265f, 1.62f, 0.026f), glm::vec3(0.008f, 0.305f, 0.004f));  // right

    // Lower panel: solid recessed area
    addBox(glm::vec3(0.0f, 0.60f, 0.018f), glm::vec3(0.30f, 0.44f, 0.003f));

    // Lever handle (right side, 1.0m height)
    // Escutcheon plate
    addBox(glm::vec3(0.30f, 1.0f, 0.026f), glm::vec3(0.022f, 0.032f, 0.006f));
    // Lever arm
    addBox(glm::vec3(0.30f, 1.0f, 0.040f), glm::vec3(0.05f, 0.007f, 0.007f));
    // Lever return (grip tip)
    addBox(glm::vec3(0.35f, 0.985f, 0.040f), glm::vec3(0.006f, 0.015f, 0.006f));

    // Lock plate and keyhole
    addBox(glm::vec3(0.30f, 0.87f, 0.026f), glm::vec3(0.018f, 0.04f, 0.004f));
    addCylinder(glm::vec3(0.30f, 0.87f, 0.032f),
                glm::vec3(0.005f, 0.003f, 0.005f),
                glm::vec3(90.0f, 0.0f, 0.0f));

    // Three butt hinges on left edge
    for (float y : {0.25f, 1.05f, 1.85f}) {
        // Hinge plate
        addBox(glm::vec3(-0.43f, y, 0.0f), glm::vec3(0.025f, 0.04f, 0.01f));
        // Hinge barrel
        addCylinder(glm::vec3(-0.46f, y - 0.04f, 0.0f), glm::vec3(0.008f, 0.08f, 0.008f));
    }

    // Kick plate at bottom
    addBox(glm::vec3(0.0f, 0.04f, 0.026f), glm::vec3(0.38f, 0.035f, 0.003f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

// ---------------------------------------------------------------------------
// Prison furniture
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createPrisonDesk() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Desk top: 1.2m x 0.6m, 0.03m thick, at Y=0.75m
    addBox(glm::vec3(0.0f, 0.75f, 0.0f), glm::vec3(0.6f, 0.015f, 0.3f));

    // Four angle-iron legs (0.03m x 0.03m cross-section)
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

    // Seat: 0.45m x 0.45m, 0.025m thick, at Y=0.45m
    addBox(glm::vec3(0.0f, 0.45f, 0.0f), glm::vec3(0.225f, 0.0125f, 0.225f));

    // Four legs (0.025m x 0.025m cross-section)
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

    // Structural: legs
    float frontLegH = 0.45f / 2.0f;
    addBox(glm::vec3(-0.2f, frontLegH, -0.2f), glm::vec3(0.015f, frontLegH, 0.015f));
    addBox(glm::vec3(0.2f, frontLegH, -0.2f), glm::vec3(0.015f, frontLegH, 0.015f));
    float backLegH = 0.85f / 2.0f;
    addBox(glm::vec3(-0.2f, backLegH, 0.2f), glm::vec3(0.015f, backLegH, 0.015f));
    addBox(glm::vec3(0.2f, backLegH, 0.2f), glm::vec3(0.015f, backLegH, 0.015f));

    // Structural: seat
    addBox(glm::vec3(0.0f, 0.45f, 0.0f), glm::vec3(0.22f, 0.015f, 0.22f));

    // Structural: backrest
    addBox(glm::vec3(0.0f, 0.69f, 0.2f), glm::vec3(0.22f, 0.16f, 0.01f));

    // Structural: armrests
    addBox(glm::vec3(-0.2f, 0.62f, 0.0f), glm::vec3(0.015f, 0.01f, 0.2f));
    addBox(glm::vec3(0.2f, 0.62f, 0.0f), glm::vec3(0.015f, 0.01f, 0.2f));
    addBox(glm::vec3(-0.2f, 0.535f, -0.18f), glm::vec3(0.012f, 0.075f, 0.012f));
    addBox(glm::vec3(0.2f, 0.535f, -0.18f), glm::vec3(0.012f, 0.075f, 0.012f));

    // Detail: cross braces
    addBox(glm::vec3(0.0f, 0.12f, -0.19f), glm::vec3(0.185f, 0.008f, 0.008f));
    addBox(glm::vec3(0.0f, 0.12f, 0.19f), glm::vec3(0.185f, 0.008f, 0.008f));
    addBox(glm::vec3(-0.19f, 0.12f, 0.0f), glm::vec3(0.008f, 0.008f, 0.185f));

    // Detail: backrest top rail
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
    addBox(glm::vec3(0.0f, 0.65f, 0.0f), glm::vec3(0.225f, 0.65f, 0.3f));

    // Three drawer seam lines on front face
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
// Prison office additions
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
    // Long sides (along X axis)
    addBox(glm::vec3(0.0f, -0.005f, -0.15f), glm::vec3(0.45f, 0.005f, 0.015f));
    addBox(glm::vec3(0.0f, -0.005f, 0.15f), glm::vec3(0.45f, 0.005f, 0.015f));
    // Short sides (along Z axis)
    addBox(glm::vec3(-0.45f, -0.005f, 0.0f), glm::vec3(0.015f, 0.005f, 0.135f));
    addBox(glm::vec3(0.45f, -0.005f, 0.0f), glm::vec3(0.015f, 0.005f, 0.135f));

    // Diffuser panel
    addBox(glm::vec3(0.0f, -0.035f, 0.0f), glm::vec3(0.435f, 0.0025f, 0.12f));

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

    // Large office window wall: 2m wide, 4.0m tall, 0.2m thick
    // Bottom section: 0 to 1.2m
    addBox(glm::vec3(0.0f, 0.6f, 0.0f), glm::vec3(1.0f, 0.6f, 0.1f));
    // Top section: 2.4m to 4.0m
    addBox(glm::vec3(0.0f, 3.2f, 0.0f), glm::vec3(1.0f, 0.8f, 0.1f));
    // Left section beside window (0.3m wide strip)
    addBox(glm::vec3(-0.85f, 1.8f, 0.0f), glm::vec3(0.15f, 0.6f, 0.1f));
    // Right section beside window (0.3m wide strip)
    addBox(glm::vec3(0.85f, 1.8f, 0.0f), glm::vec3(0.15f, 0.6f, 0.1f));

    // Window sill
    addBox(glm::vec3(0.0f, 1.205f, 0.06f), glm::vec3(0.7f, 0.005f, 0.06f));

    // Single thin mullion: vertical center divider
    addBox(glm::vec3(0.0f, 1.8f, 0.0f), glm::vec3(0.01f, 0.6f, 0.105f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals, merged.uvs, merged.tangents,
                                  merged.indices);
}

// ---------------------------------------------------------------------------
// Institutional room assets
// ---------------------------------------------------------------------------

std::unique_ptr<Mesh> createInstHvacVent() {
    auto cube = generateCube(2.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position, const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };

    // Outer frame
    addBox(glm::vec3(0.0f,  0.225f, 0.0f), glm::vec3(0.30f, 0.015f, 0.008f)); // top rail
    addBox(glm::vec3(0.0f, -0.225f, 0.0f), glm::vec3(0.30f, 0.015f, 0.008f)); // bottom rail
    addBox(glm::vec3(-0.285f, 0.0f, 0.0f), glm::vec3(0.015f, 0.225f, 0.008f)); // left jamb
    addBox(glm::vec3( 0.285f, 0.0f, 0.0f), glm::vec3(0.015f, 0.225f, 0.008f)); // right jamb

    // Horizontal slats (5 evenly spaced)
    for (int i = -2; i <= 2; ++i) {
        float y = static_cast<float>(i) * 0.08f;
        addBox(glm::vec3(0.0f, y, 0.002f), glm::vec3(0.27f, 0.006f, 0.006f));
    }

    // Vertical slats (3 evenly spaced, behind horizontal)
    for (int i = -1; i <= 1; ++i) {
        float x = static_cast<float>(i) * 0.13f;
        addBox(glm::vec3(x, 0.0f, -0.002f), glm::vec3(0.005f, 0.21f, 0.005f));
    }

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals,
                                  merged.uvs, merged.tangents, merged.indices);
}

std::unique_ptr<Mesh> createInstSmokeDetector() {
    auto cylinder = generateCylinder(1.0f, 1.0f, 16);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

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

std::unique_ptr<Mesh> createInstChainPadlock() {
    auto cube = generateCube(2.0f);
    auto cylinder = generateCylinder(1.0f, 1.0f, 8);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addBox = [&](const glm::vec3& position, const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({cube, makeModel(position, scale, rotation)});
    };
    auto addCylinder = [&](const glm::vec3& p, const glm::vec3& s,
                           const glm::vec3& r = glm::vec3(0.0f)) {
        parts.push_back({cylinder, makeModel(p, s, r)});
    };

    // --- Chain links (3 links, alternating orientation) ---
    // Each link: 4 cylinders in a rectangular loop
    // Link dimensions: ~0.05m wide, 0.03m tall, 0.008m bar thickness
    auto addLink = [&](float yOffset, float zRotDeg) {
        // Top bar (horizontal)
        addCylinder(glm::vec3(0.0f, yOffset + 0.015f, 0.0f),
                    glm::vec3(0.004f, 0.022f, 0.004f),
                    glm::vec3(0.0f, 0.0f, 90.0f + zRotDeg));
        // Bottom bar (horizontal)
        addCylinder(glm::vec3(0.0f, yOffset - 0.015f, 0.0f),
                    glm::vec3(0.004f, 0.022f, 0.004f),
                    glm::vec3(0.0f, 0.0f, 90.0f + zRotDeg));
        // Left bar (vertical)
        addCylinder(glm::vec3(-0.018f, yOffset, 0.0f),
                    glm::vec3(0.004f, 0.015f, 0.004f));
        // Right bar (vertical)
        addCylinder(glm::vec3(0.018f, yOffset, 0.0f),
                    glm::vec3(0.004f, 0.015f, 0.004f));
    };

    addLink(0.12f, 0.0f);    // Link 1 (top, flat)
    addLink(0.06f, 90.0f);   // Link 2 (middle, rotated 90 deg)
    addLink(0.0f, 0.0f);     // Link 3 (bottom, flat)

    // --- Padlock body ---
    addBox(glm::vec3(0.0f, -0.055f, 0.0f), glm::vec3(0.02f, 0.025f, 0.01f)); // main body
    // Keyhole detail (tiny inset box)
    addBox(glm::vec3(0.0f, -0.06f, 0.011f), glm::vec3(0.004f, 0.006f, 0.001f)); // keyhole plate

    // --- Padlock shackle (U-shape above body) ---
    // Left vertical
    addCylinder(glm::vec3(-0.012f, -0.025f, 0.0f), glm::vec3(0.003f, 0.012f, 0.003f));
    // Right vertical
    addCylinder(glm::vec3(0.012f, -0.025f, 0.0f), glm::vec3(0.003f, 0.012f, 0.003f));
    // Top horizontal connecting bar
    addCylinder(glm::vec3(0.0f, -0.013f, 0.0f), glm::vec3(0.003f, 0.012f, 0.003f),
                glm::vec3(0.0f, 0.0f, 90.0f));

    RawMeshData merged = mergeMeshes(parts);
    return std::make_unique<Mesh>(merged.positions, merged.normals,
                                  merged.uvs, merged.tangents, merged.indices);
}

} // namespace

void registerAllGameAssets(MeshLibrary& meshLibrary) {
    meshLibrary.registerDefaults();

    // Cathedral assets
    meshLibrary.registerMesh("door_frame_romanesque", createRomanesqueDoorFrameMesh());
    meshLibrary.registerMesh("door_leaf_left", createWoodDoorLeafMesh(true));
    meshLibrary.registerMesh("door_leaf_right", createWoodDoorLeafMesh(false));
    meshLibrary.registerFileAlias("pillar", "assets/meshes/pillar.glb");
    meshLibrary.registerFileAlias("arch", "assets/meshes/arch.glb");
    meshLibrary.registerFileAlias("hand", "assets/meshes/hand_with_old_dagger.glb");

    // Prison assets
    // Architectural walls
    meshLibrary.registerMesh("prison_wall", createPrisonWall());
    meshLibrary.registerMesh("prison_wall_window", createPrisonWallWindow());
    meshLibrary.registerMesh("prison_wall_door", createPrisonWallDoor());
    // Floor, ceiling, baseboard
    meshLibrary.registerMesh("prison_floor", createPrisonFloor());
    meshLibrary.registerMesh("prison_ceiling", createPrisonCeiling());
    meshLibrary.registerMesh("prison_baseboard", createPrisonBaseboard());
    // Door
    meshLibrary.registerMesh("office_door", createOfficeDoor());
    // Furniture
    meshLibrary.registerMesh("prison_desk", createPrisonDesk());
    meshLibrary.registerMesh("prison_chair", createPrisonChair());
    meshLibrary.registerMesh("warden_chair", createWardenChair());
    meshLibrary.registerMesh("prison_cabinet", createPrisonCabinet());
    meshLibrary.registerMesh("prison_shelf", createPrisonShelf());
    // Office additions
    meshLibrary.registerMesh("ceiling_light_panel", createCeilingLightPanel());
    meshLibrary.registerMesh("prison_wall_large_window", createPrisonWallLargeWindow());

    // Institutional room assets
    meshLibrary.registerMesh("inst_hvac_vent", createInstHvacVent());
    meshLibrary.registerMesh("inst_smoke_detector", createInstSmokeDetector());
    meshLibrary.registerMesh("inst_chain_padlock", createInstChainPadlock());
}
