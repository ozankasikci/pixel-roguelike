#include "editor/assets/EditorAssetBrowser.h"

#include "engine/core/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

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
    if (ext == ".glb" || ext == ".gltf") {
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
    return path.stem().string();
}

bool editorAssetKindUsesDeclaredId(EditorAssetBrowserKind kind) {
    return kindUsesDeclaredId(kind);
}
