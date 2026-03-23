#include "CathedralScene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// Helper: create a cylinder mesh approximation using stacked rings
static Mesh createCylinder(float radius, float height, int segments) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;

    // Generate vertices for bottom (ring 0) and top (ring 1)
    for (int ring = 0; ring <= 1; ++ring) {
        float y = ring * height;
        for (int i = 0; i <= segments; ++i) {
            float angle = (float)i / segments * 2.0f * 3.14159265f;
            float x = radius * std::cos(angle);
            float z = radius * std::sin(angle);
            positions.push_back(glm::vec3(x, y, z));
            normals.push_back(glm::normalize(glm::vec3(std::cos(angle), 0.0f, std::sin(angle))));
        }
    }

    // Wall indices
    int stride = segments + 1;
    for (int i = 0; i < segments; ++i) {
        uint32_t bl = i;
        uint32_t br = i + 1;
        uint32_t tl = i + stride;
        uint32_t tr = i + 1 + stride;
        indices.push_back(bl); indices.push_back(br); indices.push_back(tl);
        indices.push_back(br); indices.push_back(tr); indices.push_back(tl);
    }

    // Bottom cap
    uint32_t botCenter = (uint32_t)positions.size();
    positions.push_back(glm::vec3(0, 0, 0));
    normals.push_back(glm::vec3(0, -1, 0));
    for (int i = 0; i < segments; ++i) {
        indices.push_back(botCenter);
        indices.push_back(i + 1);
        indices.push_back(i);
    }

    // Top cap
    uint32_t topCenter = (uint32_t)positions.size();
    positions.push_back(glm::vec3(0, height, 0));
    normals.push_back(glm::vec3(0, 1, 0));
    for (int i = 0; i < segments; ++i) {
        indices.push_back(topCenter);
        indices.push_back(i + stride);
        indices.push_back(i + 1 + stride);
    }

    return Mesh(positions, normals, indices);
}

// Helper: create an arch between two points
static void addArch(std::vector<std::unique_ptr<Mesh>>& meshes,
                    std::vector<RenderObject>& objects,
                    float x1, float x2, float baseY, float archHeight, float z, float thickness) {
    int segments = 12;
    float midX = (x1 + x2) * 0.5f;
    float halfSpan = (x2 - x1) * 0.5f;

    for (int i = 0; i < segments; ++i) {
        float a0 = 3.14159265f * i / segments;
        float a1 = 3.14159265f * (i + 1) / segments;

        float cx0 = midX - halfSpan * std::cos(a0);
        float cy0 = baseY + archHeight * std::sin(a0);
        float cx1 = midX - halfSpan * std::cos(a1);
        float cy1 = baseY + archHeight * std::sin(a1);

        float midAX = (cx0 + cx1) * 0.5f;
        float midAY = (cy0 + cy1) * 0.5f;

        float dx = cx1 - cx0;
        float dy = cy1 - cy0;
        float segLen = std::sqrt(dx*dx + dy*dy);

        auto segMesh = std::make_unique<Mesh>(Mesh::createCube(1.0f));

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(midAX, midAY, z));
        float angle = std::atan2(dy, dx);
        model = glm::rotate(model, angle, glm::vec3(0, 0, 1));
        model = glm::scale(model, glm::vec3(segLen, thickness, thickness));

        objects.push_back({segMesh.get(), model});
        meshes.push_back(std::move(segMesh));
    }
}

void CathedralScene::build() {
    meshes_.clear();
    objects_.clear();
    lights_.clear();

    float corridorLength = 30.0f;
    float corridorWidth  = 8.0f;
    float wallHeight     = 6.0f;

    // ------------------------------------------------------------------
    // Floor: offset to center on corridor (z=0 to z=-corridorLength)
    // ------------------------------------------------------------------
    auto floorMesh = std::make_unique<Mesh>(Mesh::createPlane(corridorLength));
    glm::mat4 floorModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -corridorLength * 0.5f));
    floorModel = glm::scale(floorModel, glm::vec3(corridorWidth / corridorLength, 1.0f, 1.0f));
    objects_.push_back({floorMesh.get(), floorModel});
    meshes_.push_back(std::move(floorMesh));

    // ------------------------------------------------------------------
    // Ceiling: offset to center on corridor
    // ------------------------------------------------------------------
    auto ceilMesh = std::make_unique<Mesh>(Mesh::createPlane(corridorLength));
    glm::mat4 ceilModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, wallHeight, -corridorLength * 0.5f));
    ceilModel = glm::scale(ceilModel, glm::vec3(corridorWidth / corridorLength, -1.0f, 1.0f));
    objects_.push_back({ceilMesh.get(), ceilModel});
    meshes_.push_back(std::move(ceilMesh));

    // ------------------------------------------------------------------
    // Side walls
    // ------------------------------------------------------------------
    float halfW = corridorWidth * 0.5f;
    for (float side : {-1.0f, 1.0f}) {
        auto wallMesh = std::make_unique<Mesh>(Mesh::createCube(1.0f));
        glm::mat4 wallModel = glm::translate(glm::mat4(1.0f),
            glm::vec3(side * halfW, wallHeight * 0.5f, -corridorLength * 0.5f));
        wallModel = glm::scale(wallModel, glm::vec3(0.3f, wallHeight, corridorLength));
        objects_.push_back({wallMesh.get(), wallModel});
        meshes_.push_back(std::move(wallMesh));
    }

    // ------------------------------------------------------------------
    // Back wall
    // ------------------------------------------------------------------
    {
        auto wallMesh = std::make_unique<Mesh>(Mesh::createCube(1.0f));
        glm::mat4 wallModel = glm::translate(glm::mat4(1.0f),
            glm::vec3(0.0f, wallHeight * 0.5f, -corridorLength));
        wallModel = glm::scale(wallModel, glm::vec3(corridorWidth, wallHeight, 0.3f));
        objects_.push_back({wallMesh.get(), wallModel});
        meshes_.push_back(std::move(wallMesh));
    }

    // ------------------------------------------------------------------
    // Cylindrical pillars along the corridor
    // ------------------------------------------------------------------
    float pillarRadius = 0.35f;
    float pillarSpacing = 4.0f;
    int pillarCount = (int)(corridorLength / pillarSpacing);

    for (int i = 0; i < pillarCount; ++i) {
        float z = -2.0f - i * pillarSpacing;

        for (float side : {-1.0f, 1.0f}) {
            float x = side * (halfW - 1.2f);

            auto pillar = std::make_unique<Mesh>(createCylinder(pillarRadius, wallHeight, 12));
            glm::mat4 pillarModel = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
            objects_.push_back({pillar.get(), pillarModel});
            meshes_.push_back(std::move(pillar));

            // Pillar base (wider)
            auto base = std::make_unique<Mesh>(createCylinder(pillarRadius * 1.5f, 0.3f, 12));
            glm::mat4 baseModel = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z));
            objects_.push_back({base.get(), baseModel});
            meshes_.push_back(std::move(base));

            // Pillar capital (wider, at top)
            auto cap = std::make_unique<Mesh>(createCylinder(pillarRadius * 1.4f, 0.25f, 12));
            glm::mat4 capModel = glm::translate(glm::mat4(1.0f), glm::vec3(x, wallHeight - 0.25f, z));
            objects_.push_back({cap.get(), capModel});
            meshes_.push_back(std::move(cap));
        }

        // Arches connecting pillar pairs
        float archX1 = -(halfW - 1.2f);
        float archX2 =  (halfW - 1.2f);
        addArch(meshes_, objects_, archX1, archX2, wallHeight - 0.5f, 0.8f, z, 0.25f);
    }

    // ------------------------------------------------------------------
    // Floor tiles (subtle grid of thin raised edges)
    // ------------------------------------------------------------------
    float tileSize = 2.0f;
    for (float tz = 0; tz > -corridorLength; tz -= tileSize) {
        auto tileLine = std::make_unique<Mesh>(Mesh::createCube(1.0f));
        glm::mat4 tileModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.01f, tz));
        tileModel = glm::scale(tileModel, glm::vec3(corridorWidth, 0.02f, 0.05f));
        objects_.push_back({tileLine.get(), tileModel});
        meshes_.push_back(std::move(tileLine));
    }

    // ------------------------------------------------------------------
    // Steps at the far end
    // ------------------------------------------------------------------
    for (int s = 0; s < 4; ++s) {
        auto step = std::make_unique<Mesh>(Mesh::createCube(1.0f));
        float sz = -corridorLength + 2.0f + s * 0.6f;
        float sy = s * 0.15f;
        glm::mat4 stepModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, sy + 0.075f, sz));
        stepModel = glm::scale(stepModel, glm::vec3(corridorWidth * 0.6f, 0.15f, 0.6f));
        objects_.push_back({step.get(), stepModel});
        meshes_.push_back(std::move(step));
    }

    // ------------------------------------------------------------------
    // Point lights: torches along the corridor walls
    // ------------------------------------------------------------------
    for (int i = 0; i < pillarCount; i += 2) {
        float z = -2.0f - i * pillarSpacing;

        for (float side : {-1.0f, 1.0f}) {
            PointLight light;
            light.position  = glm::vec3(side * (halfW - 1.0f), wallHeight * 0.65f, z);
            light.color     = glm::vec3(1.0f, 1.0f, 1.0f);
            light.radius    = 12.0f;
            light.intensity = 1.2f;
            lights_.push_back(light);
        }
    }

    // Bright light near entrance
    PointLight entrance;
    entrance.position  = glm::vec3(0.0f, wallHeight * 0.7f, 2.0f);
    entrance.color     = glm::vec3(1.0f, 1.0f, 1.0f);
    entrance.radius    = 12.0f;
    entrance.intensity = 0.8f;
    lights_.push_back(entrance);
}
