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
