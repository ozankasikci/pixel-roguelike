#include "editor/assets/EditorAssetBrowser.h"

#include "engine/core/PathUtils.h"
#include "engine/rendering/assets/ModelLoader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace {

bool isHiddenPath(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    return !name.empty() && name[0] == '.';
}

bool kindUsesDeclaredId(EditorAssetBrowserKind kind) {
    switch (kind) {
    case EditorAssetBrowserKind::Material:
    case EditorAssetBrowserKind::Environment:
    case EditorAssetBrowserKind::Prefab:
        return true;
    case EditorAssetBrowserKind::Directory:
    case EditorAssetBrowserKind::Scene:
    case EditorAssetBrowserKind::Mesh:
    case EditorAssetBrowserKind::Sky:
    case EditorAssetBrowserKind::Shader:
    case EditorAssetBrowserKind::Other:
        return false;
    }
    return false;
}

EditorAssetBrowserKind classifyAssetPath(const std::filesystem::path& path) {
    if (std::filesystem::is_directory(path)) {
        return EditorAssetBrowserKind::Directory;
    }

    const std::string ext = path.extension().string();
    if (ext == ".scene") {
        return EditorAssetBrowserKind::Scene;
    }
    if (ModelLoader::supportsPath(path)) {
        return EditorAssetBrowserKind::Mesh;
    }
    if (ext == ".material") {
        return EditorAssetBrowserKind::Material;
    }
    if (ext == ".environment") {
        return EditorAssetBrowserKind::Environment;
    }
    if (ext == ".prefab") {
        return EditorAssetBrowserKind::Prefab;
    }
    if (ext == ".vert" || ext == ".frag" || ext == ".glsl") {
        return EditorAssetBrowserKind::Shader;
    }
    if (path.string().find("assets/skies") != std::string::npos) {
        return EditorAssetBrowserKind::Sky;
    }
    return EditorAssetBrowserKind::Other;
}

std::vector<EditorAssetBrowserNode> buildTreeRecursive(const std::filesystem::path& directory,
                                                       const std::filesystem::path& relativeBase) {
    std::vector<EditorAssetBrowserNode> nodes;
    if (!std::filesystem::exists(directory)) {
        return nodes;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (isHiddenPath(entry.path())) {
            continue;
        }

        EditorAssetBrowserNode node;
        node.name = entry.path().filename().string();
        node.absolutePath = entry.path().string();
        node.relativePath = std::filesystem::relative(entry.path(), relativeBase).generic_string();
        node.directory = entry.is_directory();
        node.kind = classifyAssetPath(entry.path());
        if (node.kind == EditorAssetBrowserKind::Mesh) {
            node.meshId = editorMeshIdForAssetPath(entry.path());
        } else if (kindUsesDeclaredId(node.kind)) {
            node.declaredId = readEditorAssetDeclaredId(entry.path()).value_or("");
        }
        if (node.directory) {
            node.children = buildTreeRecursive(entry.path(), relativeBase);
        }
        nodes.push_back(std::move(node));
    }

    std::sort(nodes.begin(), nodes.end(), [](const EditorAssetBrowserNode& lhs, const EditorAssetBrowserNode& rhs) {
        if (lhs.directory != rhs.directory) {
            return lhs.directory > rhs.directory;
        }
        return lhs.name < rhs.name;
    });
    return nodes;
}

std::string trim(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(start, end - start);
}

std::filesystem::path safeCanonical(const std::filesystem::path& path) {
    std::error_code errorCode;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, errorCode);
    if (!errorCode) {
        return canonical;
    }
    return path.lexically_normal();
}

bool isPathWithin(const std::filesystem::path& candidate, const std::filesystem::path& root) {
    const std::filesystem::path normalizedCandidate = safeCanonical(candidate);
    const std::filesystem::path normalizedRoot = safeCanonical(root);

    auto candidateIt = normalizedCandidate.begin();
    auto rootIt = normalizedRoot.begin();
    for (; rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt) {
        if (candidateIt == normalizedCandidate.end() || *candidateIt != *rootIt) {
            return false;
        }
    }
    return true;
}

std::filesystem::path makeUniqueImportPath(const std::filesystem::path& directory,
                                           const std::filesystem::path& filename) {
    const std::string stem = filename.stem().string();
    const std::string extension = filename.extension().string();

    std::filesystem::path candidate = directory / filename.filename();
    int suffix = 1;
    while (std::filesystem::exists(candidate)) {
        candidate = directory / std::filesystem::path(stem + "_" + std::to_string(suffix) + extension);
        ++suffix;
    }
    return candidate;
}

} // namespace

std::vector<EditorAssetBrowserNode> buildEditorAssetBrowserTree(const std::filesystem::path& rootPath,
                                                                const std::filesystem::path& relativeBase) {
    return buildTreeRecursive(rootPath, relativeBase);
}

std::vector<EditorAssetBrowserNode> buildProjectAssetBrowserTree() {
    const std::filesystem::path root(resolveProjectPath("assets"));
    return buildEditorAssetBrowserTree(root, std::filesystem::current_path());
}

std::optional<std::string> readEditorAssetDeclaredId(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::string line;
    while (std::getline(file, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        std::istringstream stream(trimmed);
        std::string key;
        std::string value;
        stream >> key >> value;
        if (key == "id" && !value.empty()) {
            return value;
        }
    }

    return std::nullopt;
}

std::string editorMeshIdForAssetPath(const std::filesystem::path& path) {
    return ModelLoader::meshIdForPath(path);
}

bool editorAssetKindUsesDeclaredId(EditorAssetBrowserKind kind) {
    return kindUsesDeclaredId(kind);
}

bool editorAssetSupportsExternalImport(const std::filesystem::path& path) {
    return ModelLoader::supportsPath(path);
}

std::filesystem::path editorAssetImportDirectory(const std::filesystem::path& assetsRoot,
                                                 const std::string& selectedRelativePath,
                                                 bool selectedIsDirectory) {
    const std::filesystem::path defaultDirectory = assetsRoot / "meshes";
    if (selectedRelativePath.empty()) {
        return defaultDirectory;
    }

    const std::filesystem::path projectRoot = assetsRoot.parent_path();
    std::filesystem::path selectedPath = projectRoot / std::filesystem::path(selectedRelativePath);
    if (!selectedIsDirectory) {
        selectedPath = selectedPath.parent_path();
    }

    const std::filesystem::path meshesRoot = assetsRoot / "meshes";
    if (std::filesystem::exists(selectedPath) && std::filesystem::is_directory(selectedPath) && isPathWithin(selectedPath, meshesRoot)) {
        return selectedPath;
    }
    return defaultDirectory;
}

std::vector<EditorImportedAsset> importEditorExternalAssets(const std::vector<std::filesystem::path>& sourcePaths,
                                                            const std::filesystem::path& assetsRoot,
                                                            const std::string& selectedRelativePath,
                                                            bool selectedIsDirectory) {
    std::vector<EditorImportedAsset> importedAssets;
    if (sourcePaths.empty()) {
        return importedAssets;
    }

    const std::filesystem::path projectRoot = assetsRoot.parent_path();
    const std::filesystem::path destinationDirectory = editorAssetImportDirectory(assetsRoot,
                                                                                  selectedRelativePath,
                                                                                  selectedIsDirectory);
    std::filesystem::create_directories(destinationDirectory);

    for (const auto& sourcePath : sourcePaths) {
        if (!editorAssetSupportsExternalImport(sourcePath) || !std::filesystem::exists(sourcePath) || std::filesystem::is_directory(sourcePath)) {
            continue;
        }

        std::filesystem::path destinationPath;
        if (isPathWithin(sourcePath, assetsRoot)) {
            destinationPath = sourcePath;
        } else {
            destinationPath = makeUniqueImportPath(destinationDirectory, sourcePath.filename());
            std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none);
        }

        EditorImportedAsset asset;
        asset.absolutePath = destinationPath.lexically_normal().string();
        asset.relativePath = std::filesystem::relative(destinationPath, projectRoot).generic_string();
        asset.kind = classifyAssetPath(destinationPath);
        asset.directory = std::filesystem::is_directory(destinationPath);
        if (asset.kind == EditorAssetBrowserKind::Mesh) {
            asset.meshId = editorMeshIdForAssetPath(destinationPath);
        } else if (kindUsesDeclaredId(asset.kind)) {
            asset.declaredId = readEditorAssetDeclaredId(destinationPath).value_or("");
        }
        importedAssets.push_back(std::move(asset));
    }

    return importedAssets;
}
