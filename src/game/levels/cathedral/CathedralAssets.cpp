#include "game/levels/cathedral/CathedralAssets.h"

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

} // namespace

void registerCathedralAssets(MeshLibrary& meshLibrary) {
    meshLibrary.registerDefaults();
    meshLibrary.registerMesh("door_frame_romanesque", createRomanesqueDoorFrameMesh());
    meshLibrary.registerMesh("door_leaf_left", createWoodDoorLeafMesh(true));
    meshLibrary.registerMesh("door_leaf_right", createWoodDoorLeafMesh(false));
    meshLibrary.registerFileAlias("pillar", "assets/meshes/pillar.glb");
    meshLibrary.registerFileAlias("arch", "assets/meshes/arch.glb");
    meshLibrary.registerFileAlias("hand", "assets/meshes/hand_with_old_dagger.glb");
}
