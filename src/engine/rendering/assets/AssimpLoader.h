#pragma once

#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshGeometry.h"

#include <memory>
#include <string>
#include <vector>

class AssimpLoader {
public:
    static std::unique_ptr<Mesh> load(const std::string& filepath);
    static RawMeshData loadRaw(const std::string& filepath);

    // Load all submeshes grouped by material name.
    // Returns one NamedRawMeshData per unique material, with all submeshes
    // sharing that material merged together. Name is the Assimp material name.
    static std::vector<NamedRawMeshData> loadRawMulti(const std::string& filepath);
};
