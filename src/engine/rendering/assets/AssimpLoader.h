#pragma once

#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshGeometry.h"

#include <memory>
#include <string>

class AssimpLoader {
public:
    static std::unique_ptr<Mesh> load(const std::string& filepath);
    static RawMeshData loadRaw(const std::string& filepath);
};
