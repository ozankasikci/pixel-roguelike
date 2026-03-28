#pragma once

#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshGeometry.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct DiscoveredModelAsset {
    std::string meshId;
    std::string relativePath;
    std::filesystem::path absolutePath;
};

class ModelLoader {
public:
    static bool supportsExtension(const std::string& extension);
    static bool supportsPath(const std::filesystem::path& path);
    static std::string meshIdForPath(const std::filesystem::path& path);
    static std::vector<DiscoveredModelAsset> discoverProjectAssets(const std::filesystem::path& rootPath,
                                                                   const std::filesystem::path& relativeBase);
    static std::unique_ptr<Mesh> load(const std::string& filepath);
    static RawMeshData loadRaw(const std::string& filepath);
};
