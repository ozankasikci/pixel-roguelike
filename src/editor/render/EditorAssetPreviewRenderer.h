#pragma once

#include "engine/rendering/assets/Texture2D.h"
#include "engine/rendering/core/Framebuffer.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/Renderer.h"

#include <glm/vec2.hpp>
#include <imgui.h>
#include <memory>
#include <string>

class Shader;

struct EditorPreviewMeshStats {
    bool valid = false;
    std::size_t vertexCount = 0;
    std::size_t indexCount = 0;
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
};

class EditorAssetPreviewRenderer {
public:
    EditorAssetPreviewRenderer();
    ~EditorAssetPreviewRenderer();

    EditorAssetPreviewRenderer(const EditorAssetPreviewRenderer&) = delete;
    EditorAssetPreviewRenderer& operator=(const EditorAssetPreviewRenderer&) = delete;

    void invalidate();
    bool drawMeshPreview(const std::string& absolutePath,
                         const RenderMaterialData& material,
                         const glm::vec3& backgroundColor,
                         const char* idSuffix);
    bool drawMaterialPreview(const RenderMaterialData& material,
                             const glm::vec3& backgroundColor,
                             const char* idSuffix);
    bool drawImagePreview(const std::string& absolutePath, const char* idSuffix);

    glm::ivec2 currentImageSize() const;
    EditorPreviewMeshStats currentMeshStats() const;

private:
    void ensureInitialized();
    bool ensureFramebuffer(const ImVec2& size);
    bool ensurePreviewMeshLoaded(const std::string& absolutePath);
    bool updateOrbitControls();
    void renderPreviewMesh(Mesh* mesh,
                           const RenderMaterialData& material,
                           const glm::vec3& backgroundColor);

    std::unique_ptr<Shader> sceneShader_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Mesh> previewMesh_;
    std::unique_ptr<Mesh> previewCube_;
    Framebuffer framebuffer_;
    Texture2D imagePreview_;
    Texture2D fallbackAlbedo_;
    Texture2D fallbackNormal_;
    Texture2D fallbackRoughness_;
    Texture2D fallbackAo_;
    std::string loadedMeshPath_;
    std::string loadedImagePath_;
    EditorPreviewMeshStats meshStats_{};
    bool meshPreviewDirty_ = true;
    glm::vec3 focusMin_{-0.5f};
    glm::vec3 focusMax_{0.5f};
    float orbitYawDegrees_ = 32.0f;
    float orbitPitchDegrees_ = 18.0f;
    float orbitDistance_ = 2.6f;
};
