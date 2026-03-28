#include "editor/render/EditorAssetPreviewRenderer.h"

#include "engine/rendering/assets/GltfLoader.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/lighting/RenderLight.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <imgui.h>
#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    const float lengthSq = glm::dot(value, value);
    if (lengthSq <= 1e-6f) {
        return fallback;
    }
    return value / std::sqrt(lengthSq);
}

RenderLight makeDirectionalLight(const glm::vec3& direction,
                                 const glm::vec3& color,
                                 float intensity) {
    RenderLight light;
    light.type = LightType::Directional;
    light.direction = safeNormalize(direction, glm::vec3(0.3f, -1.0f, 0.2f));
    light.color = color;
    light.intensity = intensity;
    return light;
}

RenderMaterialData withFallbackTextures(RenderMaterialData material,
                                        const Texture2D& albedo,
                                        const Texture2D& normal,
                                        const Texture2D& roughness,
                                        const Texture2D& ao) {
    if (material.albedoTexture == 0) {
        material.albedoTexture = albedo.id();
    }
    if (material.normalTexture == 0) {
        material.normalTexture = normal.id();
    }
    if (material.roughnessTexture == 0) {
        material.roughnessTexture = roughness.id();
    }
    if (material.aoTexture == 0) {
        material.aoTexture = ao.id();
    }
    return material;
}

} // namespace

EditorAssetPreviewRenderer::EditorAssetPreviewRenderer() = default;

EditorAssetPreviewRenderer::~EditorAssetPreviewRenderer() = default;

void EditorAssetPreviewRenderer::invalidate() {
    loadedMeshPath_.clear();
    loadedImagePath_.clear();
    meshStats_ = {};
    meshPreviewDirty_ = true;
}

void EditorAssetPreviewRenderer::ensureInitialized() {
    if (!sceneShader_) {
        sceneShader_ = std::make_unique<Shader>("assets/shaders/game/scene.vert", "assets/shaders/game/scene.frag");
        renderer_ = std::make_unique<Renderer>(sceneShader_.get());
        previewCube_ = std::make_unique<Mesh>(Mesh::createCube(1.0f));
        fallbackAlbedo_.createRGBA8(1, 1, {255, 255, 255, 255});
        fallbackNormal_.createRGBA8(1, 1, {128, 128, 255, 255});
        fallbackRoughness_.createR8(1, 1, {196});
        fallbackAo_.createR8(1, 1, {255});
    }
}

bool EditorAssetPreviewRenderer::ensureFramebuffer(const ImVec2& size) {
    const int width = std::max(64, static_cast<int>(size.x));
    const int height = std::max(64, static_cast<int>(size.y));
    if (framebuffer_.framebuffer() == 0) {
        framebuffer_.create(width, height);
        meshPreviewDirty_ = true;
        return true;
    }
    if (framebuffer_.width() != width || framebuffer_.height() != height) {
        framebuffer_.resize(width, height);
        meshPreviewDirty_ = true;
        return true;
    }
    return false;
}

bool EditorAssetPreviewRenderer::ensurePreviewMeshLoaded(const std::string& absolutePath) {
    ensureInitialized();
    if (loadedMeshPath_ == absolutePath && previewMesh_) {
        return true;
    }

    previewMesh_ = GltfLoader::load(absolutePath);
    loadedMeshPath_ = absolutePath;
    meshPreviewDirty_ = true;
    if (!previewMesh_) {
        focusMin_ = glm::vec3(-0.5f);
        focusMax_ = glm::vec3(0.5f);
        meshStats_ = {};
        return false;
    }

    focusMin_ = previewMesh_->aabbMin();
    focusMax_ = previewMesh_->aabbMax();
    meshStats_.valid = true;
    meshStats_.vertexCount = previewMesh_->vertexCount();
    meshStats_.indexCount = previewMesh_->indexCount();
    meshStats_.min = focusMin_;
    meshStats_.max = focusMax_;
    return true;
}

bool EditorAssetPreviewRenderer::updateOrbitControls() {
    if (!ImGui::IsItemHovered()) {
        return false;
    }

    ImGuiIO& io = ImGui::GetIO();
    bool changed = false;
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        orbitYawDegrees_ += io.MouseDelta.x * 0.35f;
        orbitPitchDegrees_ -= io.MouseDelta.y * 0.25f;
        orbitPitchDegrees_ = std::clamp(orbitPitchDegrees_, -75.0f, 75.0f);
        changed = true;
    }
    if (io.MouseWheel != 0.0f) {
        orbitDistance_ = std::clamp(orbitDistance_ - io.MouseWheel * 0.15f, 0.8f, 12.0f);
        changed = true;
    }
    return changed;
}

void EditorAssetPreviewRenderer::renderPreviewMesh(Mesh* mesh,
                                                   const RenderMaterialData& requestedMaterial,
                                                   const glm::vec3& backgroundColor) {
    if (mesh == nullptr) {
        return;
    }

    const glm::vec3 boundsMin = (mesh == previewMesh_.get()) ? focusMin_ : glm::vec3(-0.5f);
    const glm::vec3 boundsMax = (mesh == previewMesh_.get()) ? focusMax_ : glm::vec3(0.5f);
    const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    const glm::vec3 extent = glm::max(boundsMax - boundsMin, glm::vec3(0.001f));
    const float radius = std::max({extent.x, extent.y, extent.z}) * 0.5f;

    const float yaw = glm::radians(orbitYawDegrees_);
    const float pitch = glm::radians(orbitPitchDegrees_);
    const glm::vec3 forward(
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw));
    const glm::vec3 cameraPos = center + forward * (radius * orbitDistance_ + 0.8f);
    const glm::mat4 view = glm::lookAt(cameraPos, center, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(40.0f),
                                                  static_cast<float>(framebuffer_.width()) / static_cast<float>(framebuffer_.height()),
                                                  0.05f,
                                                  50.0f);

    framebuffer_.bind();
    glViewport(0, 0, framebuffer_.width(), framebuffer_.height());
    glEnable(GL_DEPTH_TEST);
    glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    LightingEnvironment lighting;
    lighting.hemisphereSkyColor = glm::vec3(0.22f, 0.23f, 0.25f);
    lighting.hemisphereGroundColor = glm::vec3(0.05f, 0.05f, 0.06f);
    lighting.hemisphereStrength = 0.36f;
    lighting.enableDirectionalLights = true;
    lighting.enableShadows = false;

    std::vector<RenderLight> lights;
    lights.push_back(makeDirectionalLight(glm::vec3(-0.45f, -1.0f, -0.35f), glm::vec3(1.0f, 0.97f, 0.92f), 1.2f));
    lights.push_back(makeDirectionalLight(glm::vec3(0.35f, -0.55f, 0.45f), glm::vec3(0.64f, 0.72f, 0.84f), 0.35f));

    RenderMaterialData material = withFallbackTextures(requestedMaterial,
                                                       fallbackAlbedo_,
                                                       fallbackNormal_,
                                                       fallbackRoughness_,
                                                       fallbackAo_);
    material.useMaterialMaps = material.useMaterialMaps || material.albedoTexture != fallbackAlbedo_.id();

    const RenderObject object{
        mesh,
        glm::translate(glm::mat4(1.0f), -center),
        glm::vec3(1.0f),
        material
    };
    renderer_->drawScene({object}, lights, lighting, ShadowRenderData{}, view, projection, cameraPos);
    framebuffer_.unbind();
}

bool EditorAssetPreviewRenderer::drawMeshPreview(const std::string& absolutePath,
                                                 const RenderMaterialData& material,
                                                 const glm::vec3& backgroundColor,
                                                 const char* idSuffix) {
    ensureInitialized();
    const ImVec2 size(std::max(180.0f, ImGui::GetContentRegionAvail().x), 200.0f);
    ensureFramebuffer(size);
    const bool loaded = ensurePreviewMeshLoaded(absolutePath);
    if (loaded && meshPreviewDirty_) {
        renderPreviewMesh(previewMesh_.get(), material, backgroundColor);
        meshPreviewDirty_ = false;
    }

    ImGui::TextUnformatted("Preview");
    ImGui::PushID(idSuffix == nullptr ? "mesh_preview" : idSuffix);
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(framebuffer_.colorTexture())),
                 size,
                 ImVec2(0.0f, 1.0f),
                 ImVec2(1.0f, 0.0f));
    if (updateOrbitControls()) {
        meshPreviewDirty_ = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drag to orbit, scroll to zoom");
    }
    ImGui::PopID();
    return loaded;
}

bool EditorAssetPreviewRenderer::drawMaterialPreview(const RenderMaterialData& material,
                                                     const glm::vec3& backgroundColor,
                                                     const char* idSuffix) {
    ensureInitialized();
    const ImVec2 size(std::max(180.0f, ImGui::GetContentRegionAvail().x), 200.0f);
    ensureFramebuffer(size);
    focusMin_ = glm::vec3(-0.5f);
    focusMax_ = glm::vec3(0.5f);
    renderPreviewMesh(previewCube_.get(), material, backgroundColor);

    ImGui::TextUnformatted("Preview");
    ImGui::PushID(idSuffix == nullptr ? "material_preview" : idSuffix);
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(framebuffer_.colorTexture())),
                 size,
                 ImVec2(0.0f, 1.0f),
                 ImVec2(1.0f, 0.0f));
    updateOrbitControls();
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Drag to orbit, scroll to zoom");
    }
    ImGui::PopID();
    return true;
}

bool EditorAssetPreviewRenderer::drawImagePreview(const std::string& absolutePath, const char* idSuffix) {
    ensureInitialized();
    if (loadedImagePath_ != absolutePath) {
        imagePreview_ = Texture2D{};
        loadedImagePath_ = absolutePath;
        imagePreview_.createRGBA8FromFile(absolutePath);
    }
    if (imagePreview_.id() == 0) {
        return false;
    }

    const float availableWidth = std::max(180.0f, ImGui::GetContentRegionAvail().x);
    const float aspect = imagePreview_.height() > 0
        ? static_cast<float>(imagePreview_.width()) / static_cast<float>(imagePreview_.height())
        : 1.0f;
    const float previewHeight = availableWidth / std::max(aspect, 0.1f);
    ImGui::TextUnformatted("Preview");
    ImGui::PushID(idSuffix == nullptr ? "image_preview" : idSuffix);
    ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(imagePreview_.id())),
                 ImVec2(availableWidth, std::clamp(previewHeight, 96.0f, 260.0f)),
                 ImVec2(0.0f, 1.0f),
                 ImVec2(1.0f, 0.0f));
    ImGui::PopID();
    return true;
}

glm::ivec2 EditorAssetPreviewRenderer::currentImageSize() const {
    return glm::ivec2(imagePreview_.width(), imagePreview_.height());
}

EditorPreviewMeshStats EditorAssetPreviewRenderer::currentMeshStats() const {
    return meshStats_;
}
