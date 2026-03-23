#pragma once

#include "Mesh.h"
#include <memory>
#include <string>

class GltfLoader {
public:
    // Load a .glb/.gltf file and return a Mesh with the first primitive's
    // POSITION and NORMAL attributes. Returns nullptr on failure.
    static std::unique_ptr<Mesh> load(const std::string& filepath);
};
