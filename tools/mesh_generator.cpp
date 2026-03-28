// Offline tool: generates .glb mesh files for composite game objects.
// Usage: run from project root. Outputs to assets/meshes/

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

#include "engine/rendering/geometry/MeshGeometry.h"
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <limits>

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

    // Primitive -> Mesh -> Node -> Scene
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

static glm::mat4 makeModel(const glm::vec3& position,
                           const glm::vec3& scale = glm::vec3(1.0f),
                           const glm::vec3& rotationDeg = glm::vec3(0.0f)) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, glm::radians(rotationDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotationDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotationDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}

static void addArchBlocks(std::vector<std::pair<RawMeshData, glm::mat4>>& parts,
                          const RawMeshData& unitCube,
                          float halfSpan,
                          float rise,
                          float baseY,
                          float radialThickness,
                          float depth,
                          int segments,
                          float radialOffset = 0.0f,
                          float zOffset = 0.0f,
                          float lengthScale = 1.04f) {
    constexpr float PI = 3.14159265f;
    segments = std::max(segments, 4);

    for (int i = 0; i < segments; ++i) {
        float t0 = PI * static_cast<float>(i) / static_cast<float>(segments);
        float t1 = PI * static_cast<float>(i + 1) / static_cast<float>(segments);
        float tm = (t0 + t1) * 0.5f;

        glm::vec2 p0(-halfSpan * std::cos(t0), baseY + rise * std::sin(t0));
        glm::vec2 p1(-halfSpan * std::cos(t1), baseY + rise * std::sin(t1));
        glm::vec2 pm(-halfSpan * std::cos(tm), baseY + rise * std::sin(tm));

        glm::vec2 tangent = glm::normalize(p1 - p0);
        glm::vec2 outwardNormal(-tangent.y, tangent.x);
        glm::vec2 center = pm + outwardNormal * radialOffset;

        float segLen = glm::length(p1 - p0) * lengthScale;
        float angleDeg = glm::degrees(std::atan2(tangent.y, tangent.x));

        parts.push_back({
            unitCube,
            makeModel(glm::vec3(center.x, center.y, zOffset),
                      glm::vec3(segLen, radialThickness, depth),
                      glm::vec3(0.0f, 0.0f, angleDeg))
        });
    }
}

static RawMeshData generatePillar() {
    constexpr float wallHeight = 6.0f;
    constexpr int segments = 28;

    auto unitCube = generateCube(1.0f);
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;

    auto addCylinder = [&](float radius, float height, float y) {
        parts.push_back({
            generateCylinder(radius, height, segments),
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, y, 0.0f))
        });
    };

    auto addBox = [&](const glm::vec3& position,
                      const glm::vec3& scale,
                      const glm::vec3& rotation = glm::vec3(0.0f)) {
        parts.push_back({unitCube, makeModel(position, scale, rotation)});
    };

    // Broader plinth and stacked collars keep the base from reading like
    // a single cylinder while still matching the game's chunky stone style.
    addBox(glm::vec3(0.0f, 0.10f, 0.0f), glm::vec3(0.96f, 0.20f, 0.96f));
    addCylinder(0.72f, 0.12f, 0.16f);
    addCylinder(0.61f, 0.14f, 0.28f);
    addCylinder(0.50f, 0.12f, 0.42f);

    // The shaft steps through slightly different radii so it feels more
    // intentional than one straight tube without needing a new primitive type.
    addCylinder(0.36f, 2.05f, 0.54f);
    addCylinder(0.34f, 1.55f, 2.59f);
    addCylinder(0.32f, 1.12f, 4.14f);
    addCylinder(0.37f, 0.24f, 5.26f);
    addCylinder(0.46f, 0.18f, 5.50f);
    addCylinder(0.56f, 0.16f, 5.68f);

    addBox(glm::vec3(0.0f, 5.88f, 0.0f), glm::vec3(0.86f, 0.18f, 0.86f));
    addBox(glm::vec3(0.0f, wallHeight - 0.03f, 0.0f), glm::vec3(1.02f, 0.08f, 1.02f));

    return mergeMeshes(parts);
}

static RawMeshData generateArch() {
    constexpr float wallHeight = 6.0f;
    std::vector<std::pair<RawMeshData, glm::mat4>> parts;
    auto unitCube = generateCube(1.0f);

    addArchBlocks(parts, unitCube, 2.96f, 1.02f, wallHeight - 0.62f, 0.42f, 0.82f, 24, 0.02f, 0.0f, 1.08f);
    addArchBlocks(parts, unitCube, 2.78f, 0.88f, wallHeight - 0.58f, 0.22f, 0.58f, 22, -0.06f, 0.0f, 1.03f);
    addArchBlocks(parts, unitCube, 2.88f, 0.96f, wallHeight - 0.60f, 0.12f, 0.90f, 24, 0.19f, 0.0f, 1.02f);

    // Keystone and springer blocks give the arch a cleaner focal shape than a
    // uniform run of identical segments.
    parts.push_back({
        unitCube,
        makeModel(glm::vec3(0.0f, wallHeight + 0.33f, 0.0f),
                  glm::vec3(0.34f, 0.64f, 0.86f))
    });
    parts.push_back({
        unitCube,
        makeModel(glm::vec3(-2.97f, wallHeight - 0.53f, 0.0f),
                  glm::vec3(0.42f, 0.28f, 0.90f),
                  glm::vec3(0.0f, 0.0f, -4.0f))
    });
    parts.push_back({
        unitCube,
        makeModel(glm::vec3(2.97f, wallHeight - 0.53f, 0.0f),
                  glm::vec3(0.42f, 0.28f, 0.90f),
                  glm::vec3(0.0f, 0.0f, 4.0f))
    });

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
