#pragma once

#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshGeometry.h"
#include <memory>
#include <string>

class GltfLoader {
public:
    // Load a .glb/.gltf file, merging all scene nodes and mesh primitives
    // into a single Mesh. Missing normals are generated automatically.
    // Returns nullptr on failure.
    static std::unique_ptr<Mesh> load(const std::string& filepath);

    // Load a .glb/.gltf file and return merged CPU-side mesh data
    // (positions, normals, uvs, tangents, indices). Returns empty RawMeshData on failure.
    static RawMeshData loadRaw(const std::string& filepath);
};
