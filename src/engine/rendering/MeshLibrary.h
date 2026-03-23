#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include "engine/rendering/Mesh.h"

class MeshLibrary {
public:
    MeshLibrary() = default;
    ~MeshLibrary() = default;

    // Non-copyable, non-movable (owns GL resources via Mesh unique_ptrs)
    MeshLibrary(const MeshLibrary&) = delete;
    MeshLibrary& operator=(const MeshLibrary&) = delete;
    MeshLibrary(MeshLibrary&&) = delete;
    MeshLibrary& operator=(MeshLibrary&&) = delete;

    // Register a named mesh (takes ownership)
    void registerMesh(const std::string& name, std::unique_ptr<Mesh> mesh);

    // Get a non-owning raw pointer to a named mesh; returns nullptr if not found
    Mesh* get(const std::string& name) const;

    // Check if a mesh with the given name exists
    bool has(const std::string& name) const;

    // Register standard primitive meshes:
    //   "cube"          - unit cube (1x1x1, centered at origin)
    //   "plane"         - unit plane (1x1, centered at origin on XZ)
    //   "cylinder"      - unit cylinder (radius=1, height=1, 12 segments)
    //   "cylinder_wide" - wider cylinder (radius=1.5, height=1, 12 segments) for pillar bases
    //   "cylinder_cap"  - wide cylinder (radius=1.4, height=1, 12 segments) for pillar capitals
    void registerDefaults();

    // Clear all registered meshes (destroys GL resources)
    void clear();

private:
    std::unordered_map<std::string, std::unique_ptr<Mesh>> meshes_;
};
