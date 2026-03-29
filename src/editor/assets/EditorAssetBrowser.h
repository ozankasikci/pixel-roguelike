#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

enum class EditorAssetBrowserKind {
    Directory,
    Scene,
    Mesh,
    Material,
    Environment,
    Prefab,
    Sky,
    Shader,
    Other,
};

struct EditorAssetBrowserNode {
    std::string name;
    std::string relativePath;
    std::string absolutePath;
    EditorAssetBrowserKind kind = EditorAssetBrowserKind::Other;
    bool directory = false;
    std::string declaredId;
    std::string meshId;
    std::vector<EditorAssetBrowserNode> children;
};

struct EditorImportedAsset {
    std::string relativePath;
    std::string absolutePath;
    EditorAssetBrowserKind kind = EditorAssetBrowserKind::Other;
    bool directory = false;
    std::string declaredId;
    std::string meshId;
};

std::vector<EditorAssetBrowserNode> buildEditorAssetBrowserTree(const std::filesystem::path& rootPath,
                                                                const std::filesystem::path& relativeBase);
std::vector<EditorAssetBrowserNode> buildProjectAssetBrowserTree();
std::optional<std::string> readEditorAssetDeclaredId(const std::filesystem::path& path);
std::string editorMeshIdForAssetPath(const std::filesystem::path& path);
bool editorAssetKindUsesDeclaredId(EditorAssetBrowserKind kind);
bool editorAssetSupportsExternalImport(const std::filesystem::path& path);
std::filesystem::path editorAssetImportDirectory(const std::filesystem::path& assetsRoot,
                                                 const std::string& selectedRelativePath,
                                                 bool selectedIsDirectory);
std::vector<EditorImportedAsset> importEditorExternalAssets(const std::vector<std::filesystem::path>& sourcePaths,
                                                            const std::filesystem::path& assetsRoot,
                                                            const std::string& selectedRelativePath,
                                                            bool selectedIsDirectory);
