#include "editor/ui/EditorPanels.h"

#include "editor/render/EditorAssetPreviewRenderer.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelDef.h"
#include "game/rendering/EnvironmentDefinition.h"
#include "game/rendering/MaterialTextureLibrary.h"

#include <glm/common.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace {

struct AssetInspectorSession {
    std::string loadedPath;
    std::optional<MaterialDefinition> materialDraft;
    std::optional<EnvironmentDefinition> environmentDraft;
    std::optional<GameplayArchetypeDefinition> prefabDraft;
    MaterialTextureLibrary materialLibrary;
    bool materialLibraryReady = false;
    bool materialLibraryDirty = true;
    bool materialDirty = false;
    bool environmentDirty = false;
    bool prefabDirty = false;
    EditorAssetPreviewRenderer previewRenderer;
};

AssetInspectorSession& assetInspectorSession() {
    static AssetInspectorSession session;
    return session;
}

const char* assetKindLabel(EditorAssetBrowserKind kind) {
    switch (kind) {
    case EditorAssetBrowserKind::Directory:
        return "Folder";
    case EditorAssetBrowserKind::Scene:
        return "Scene";
    case EditorAssetBrowserKind::Mesh:
        return "Model";
    case EditorAssetBrowserKind::Material:
        return "Material";
    case EditorAssetBrowserKind::Environment:
        return "Environment";
    case EditorAssetBrowserKind::Prefab:
        return "Prefab";
    case EditorAssetBrowserKind::Sky:
        return "Sky";
    case EditorAssetBrowserKind::Shader:
        return "Shader";
    case EditorAssetBrowserKind::Other:
        return "File";
    }
    return "File";
}

std::string optionalString(const std::optional<std::string>& value) {
    return value.value_or("<none>");
}

void renderFileHeader(const EditorInspectedAsset& asset) {
    ImGui::TextUnformatted(assetKindLabel(asset.kind));
    ImGui::TextWrapped("%s", asset.relativePath.c_str());
    if (!asset.declaredId.empty()) {
        ImGui::TextDisabled("Id: %s", asset.declaredId.c_str());
    }
    ImGui::Separator();
}

void renderFileMetadata(const std::string& absolutePath) {
    namespace fs = std::filesystem;
    const fs::path path(absolutePath);
    ImGui::TextWrapped("Path: %s", absolutePath.c_str());
    ImGui::Text("Extension: %s", path.extension().string().c_str());
    if (fs::exists(path) && fs::is_regular_file(path)) {
        ImGui::Text("Size: %.1f KB", static_cast<double>(fs::file_size(path)) / 1024.0);
    }
}

std::string shaderSnippet(const std::string& absolutePath) {
    std::ifstream file(absolutePath);
    if (!file.is_open()) {
        return {};
    }
    std::ostringstream snippet;
    std::string line;
    int lineCount = 0;
    while (std::getline(file, line) && lineCount < 20) {
        snippet << line << '\n';
        ++lineCount;
    }
    return snippet.str();
}

void ensureMaterialLibraryReady(AssetInspectorSession& session, const ContentRegistry& content) {
    if (!session.materialLibraryReady || session.materialLibraryDirty) {
        session.materialLibrary.init(content);
        session.materialLibraryReady = true;
        session.materialLibraryDirty = false;
    }
}

void syncAssetInspectorSession(const EditorInspectedAsset& asset, AssetInspectorSession& session) {
    if (session.loadedPath == asset.absolutePath) {
        return;
    }

    session.loadedPath = asset.absolutePath;
    session.materialDraft.reset();
    session.environmentDraft.reset();
    session.prefabDraft.reset();
    session.materialDirty = false;
    session.environmentDirty = false;
    session.prefabDirty = false;
    session.previewRenderer.invalidate();

    try {
        switch (asset.kind) {
        case EditorAssetBrowserKind::Material:
            session.materialDraft = loadMaterialDefinitionAsset(asset.absolutePath);
            break;
        case EditorAssetBrowserKind::Environment:
            session.environmentDraft = loadEnvironmentDefinitionAsset(asset.absolutePath);
            break;
        case EditorAssetBrowserKind::Prefab:
            session.prefabDraft = loadGameplayArchetypeAsset(asset.absolutePath);
            break;
        default:
            break;
        }
    } catch (const std::exception& ex) {
        spdlog::warn("Failed to inspect asset '{}': {}", asset.absolutePath, ex.what());
    }
}

RenderMaterialData buildDraftMaterialPreview(const MaterialDefinition& draft,
                                             AssetInspectorSession& session,
                                             const ContentRegistry& content) {
    ensureMaterialLibraryReady(session, content);

    MaterialKind previewKind = draft.shadingModel.value_or(MaterialKind::Stone);
    std::string previewMaterialId = draft.id;
    if (content.findMaterial(previewMaterialId) == nullptr) {
        if (draft.parent.has_value() && content.findMaterial(*draft.parent) != nullptr) {
            previewMaterialId = *draft.parent;
        } else {
            previewMaterialId = std::string(defaultMaterialIdForKind(previewKind));
        }
    }

    RenderMaterialData material = session.materialLibrary.resolve(previewMaterialId, previewKind);
    material.id = draft.id;
    if (draft.shadingModel.has_value()) {
        material.shadingModel = *draft.shadingModel;
    }
    if (draft.baseColor.has_value()) {
        material.baseColor = *draft.baseColor;
    }
    if (draft.uvMode.has_value()) {
        material.uvMode = static_cast<int>(*draft.uvMode);
    }
    if (draft.uvScale.has_value()) {
        material.uvScale = *draft.uvScale;
    }
    if (draft.normalStrength.has_value()) {
        material.normalStrength = *draft.normalStrength;
    }
    if (draft.roughnessScale.has_value()) {
        material.roughnessScale = *draft.roughnessScale;
    }
    if (draft.roughnessBias.has_value()) {
        material.roughnessBias = *draft.roughnessBias;
    }
    if (draft.metalness.has_value()) {
        material.metalness = *draft.metalness;
    }
    if (draft.aoStrength.has_value()) {
        material.aoStrength = *draft.aoStrength;
    }
    if (draft.lightTintResponse.has_value()) {
        material.lightTintResponse = *draft.lightTintResponse;
    }
    return material;
}

void renderMaterialDraftFields(MaterialDefinition& draft, bool& dirty) {
    dirty |= editString("Id", draft.id);
    std::string parent = draft.parent.value_or("");
    if (editString("Parent", parent, "optional")) {
        draft.parent = parent.empty() ? std::optional<std::string>{} : std::optional<std::string>{parent};
        dirty = true;
    }

    static constexpr const char* kShadingModels[] = {
        "stone", "wood", "metal", "wax", "moss", "floor", "brick", "viewmodel"
    };
    int shadingIndex = 0;
    switch (draft.shadingModel.value_or(MaterialKind::Stone)) {
    case MaterialKind::Stone: shadingIndex = 0; break;
    case MaterialKind::Wood: shadingIndex = 1; break;
    case MaterialKind::Metal: shadingIndex = 2; break;
    case MaterialKind::Wax: shadingIndex = 3; break;
    case MaterialKind::Moss: shadingIndex = 4; break;
    case MaterialKind::Floor: shadingIndex = 5; break;
    case MaterialKind::Brick: shadingIndex = 6; break;
    case MaterialKind::Viewmodel: shadingIndex = 7; break;
    }
    if (ImGui::Combo("Shading Model", &shadingIndex, kShadingModels, 8)) {
        draft.shadingModel = static_cast<MaterialKind>(shadingIndex);
        dirty = true;
    }

    auto editOptionalPath = [&](const char* label, std::optional<std::string>& value, const char* hint) {
        std::string working = value.value_or("");
        if (editString(label, working, hint)) {
            value = working.empty() ? std::optional<std::string>{} : std::optional<std::string>{working};
            dirty = true;
        }
    };

    editOptionalPath("Albedo Map", draft.albedoMapPath, "assets/materials/.../albedo.png");
    editOptionalPath("Normal Map", draft.normalMapPath, "assets/materials/.../normal.png");
    editOptionalPath("Roughness Map", draft.roughnessMapPath, "assets/materials/.../roughness.png");
    editOptionalPath("AO Map", draft.aoMapPath, "assets/materials/.../ao.png");

    glm::vec3 baseColor = draft.baseColor.value_or(glm::vec3(1.0f));
    bool useBaseColor = draft.baseColor.has_value();
    if (ImGui::Checkbox("Use Base Color", &useBaseColor)) {
        draft.baseColor = useBaseColor ? std::optional<glm::vec3>{baseColor} : std::nullopt;
        dirty = true;
    }
    if (useBaseColor && editColor("Base Color", baseColor)) {
        draft.baseColor = baseColor;
        dirty = true;
    }

    static constexpr const char* kUvModes[] = {"mesh", "world_projected"};
    int uvMode = draft.uvMode.has_value() && *draft.uvMode == MaterialUvMode::WorldProjected ? 1 : 0;
    if (ImGui::Combo("UV Mode", &uvMode, kUvModes, 2)) {
        draft.uvMode = uvMode == 0 ? MaterialUvMode::Mesh : MaterialUvMode::WorldProjected;
        dirty = true;
    }

    glm::vec2 uvScale = draft.uvScale.value_or(glm::vec2(1.0f));
    if (ImGui::DragFloat2("UV Scale", &uvScale.x, 0.01f, 0.01f, 16.0f, "%.2f")) {
        draft.uvScale = glm::max(uvScale, glm::vec2(0.01f));
        dirty = true;
    }

    auto dragOptionalFloat = [&](const char* label, std::optional<float>& value, float defaultValue, float speed, float min, float max, const char* format) {
        float working = value.value_or(defaultValue);
        if (ImGui::DragFloat(label, &working, speed, min, max, format)) {
            value = working;
            dirty = true;
        }
    };

    dragOptionalFloat("Normal Strength", draft.normalStrength, 1.0f, 0.01f, 0.0f, 4.0f, "%.2f");
    dragOptionalFloat("Roughness Scale", draft.roughnessScale, 1.0f, 0.01f, 0.0f, 4.0f, "%.2f");
    dragOptionalFloat("Roughness Bias", draft.roughnessBias, 0.0f, 0.01f, -1.0f, 1.0f, "%.2f");
    dragOptionalFloat("Metalness", draft.metalness, 0.0f, 0.01f, 0.0f, 1.0f, "%.2f");
    dragOptionalFloat("AO Strength", draft.aoStrength, 1.0f, 0.01f, 0.0f, 4.0f, "%.2f");
    dragOptionalFloat("Light Tint Response", draft.lightTintResponse, 0.18f, 0.01f, 0.0f, 1.0f, "%.2f");

    static constexpr const char* kProceduralSources[] = {
        "none", "generated_brick", "generated_stone"
    };
    int proceduralIndex = 0;
    if (draft.proceduralSource.has_value()) {
        switch (*draft.proceduralSource) {
        case MaterialProceduralSource::None: proceduralIndex = 0; break;
        case MaterialProceduralSource::GeneratedBrick: proceduralIndex = 1; break;
        case MaterialProceduralSource::GeneratedStone: proceduralIndex = 2; break;
        }
    }
    if (ImGui::Combo("Procedural Source", &proceduralIndex, kProceduralSources, 3)) {
        draft.proceduralSource = static_cast<MaterialProceduralSource>(proceduralIndex);
        dirty = true;
    }
}

bool renderEnvironmentDraftFields(EnvironmentDefinition& environment) {
    bool dirty = false;

    if (ImGui::CollapsingHeader("Post", ImGuiTreeNodeFlags_DefaultOpen)) {
        static constexpr const char* kPaletteVariants[] = {"Neutral", "Dungeon", "Meadow", "Dusk", "Arcane", "Cathedral"};
        static constexpr const char* kToneMapModes[] = {"Linear", "ACES Fitted"};
        dirty |= editString("Id", environment.id);
        dirty |= ImGui::Combo("Palette Variant", &environment.post.paletteVariant, kPaletteVariants, 6);
        dirty |= ImGui::Checkbox("Enable Sky", &environment.post.enableSky);
        dirty |= ImGui::Checkbox("Enable Dither", &environment.post.enableDither);
        dirty |= ImGui::Checkbox("Enable Edges", &environment.post.enableEdges);
        dirty |= ImGui::Checkbox("Enable Fog", &environment.post.enableFog);
        dirty |= ImGui::Checkbox("Enable Tone Map", &environment.post.enableToneMap);
        dirty |= ImGui::Checkbox("Enable Bloom", &environment.post.enableBloom);
        dirty |= ImGui::Checkbox("Enable Vignette", &environment.post.enableVignette);
        dirty |= ImGui::Checkbox("Enable Grain", &environment.post.enableGrain);
        dirty |= ImGui::Checkbox("Enable Scanlines", &environment.post.enableScanlines);
        dirty |= ImGui::Checkbox("Enable Sharpen", &environment.post.enableSharpen);
        dirty |= ImGui::Combo("Tone Mapper", &environment.post.toneMapMode, kToneMapModes, 2);
        dirty |= ImGui::DragFloat("Threshold Bias", &environment.post.thresholdBias, 0.002f, -0.5f, 0.5f, "%.3f");
        dirty |= ImGui::DragFloat("Pattern Scale", &environment.post.patternScale, 1.0f, 0.0f, 512.0f, "%.1f");
        dirty |= ImGui::DragFloat("Edge Threshold", &environment.post.edgeThreshold, 0.005f, 0.0f, 1.0f, "%.3f");
        dirty |= ImGui::DragFloat("Fog Density", &environment.post.fogDensity, 0.001f, 0.0f, 0.5f, "%.3f");
        dirty |= ImGui::DragFloat("Fog Start", &environment.post.fogStart, 0.1f, 0.0f, 80.0f, "%.1f");
        dirty |= ImGui::DragFloat("Depth View Scale", &environment.post.depthViewScale, 0.001f, 0.01f, 0.30f, "%.3f");
        dirty |= ImGui::DragFloat("Exposure", &environment.post.exposure, 0.01f, 0.2f, 2.5f, "%.2f");
        dirty |= ImGui::DragFloat("Gamma", &environment.post.gamma, 0.01f, 0.5f, 2.0f, "%.2f");
        dirty |= ImGui::DragFloat("Contrast", &environment.post.contrast, 0.01f, 0.4f, 2.0f, "%.2f");
        dirty |= ImGui::DragFloat("Saturation", &environment.post.saturation, 0.01f, 0.0f, 1.5f, "%.2f");
        dirty |= ImGui::DragFloat("Bloom Threshold", &environment.post.bloomThreshold, 0.01f, 0.0f, 1.5f, "%.2f");
        dirty |= ImGui::DragFloat("Bloom Intensity", &environment.post.bloomIntensity, 0.01f, 0.0f, 1.0f, "%.2f");
        dirty |= ImGui::DragFloat("Bloom Radius", &environment.post.bloomRadius, 0.01f, 0.5f, 5.0f, "%.2f");
        dirty |= ImGui::DragFloat("Vignette Strength", &environment.post.vignetteStrength, 0.01f, 0.0f, 1.0f, "%.2f");
        dirty |= ImGui::DragFloat("Vignette Softness", &environment.post.vignetteSoftness, 0.01f, 0.1f, 1.2f, "%.2f");
        dirty |= ImGui::DragFloat("Grain Amount", &environment.post.grainAmount, 0.001f, 0.0f, 0.2f, "%.3f");
        dirty |= ImGui::DragFloat("Scanline Amount", &environment.post.scanlineAmount, 0.01f, 0.0f, 0.35f, "%.2f");
        dirty |= ImGui::DragFloat("Scanline Density", &environment.post.scanlineDensity, 0.01f, 0.5f, 3.0f, "%.2f");
        dirty |= ImGui::DragFloat("Sharpen Amount", &environment.post.sharpenAmount, 0.01f, 0.0f, 1.0f, "%.2f");
        dirty |= ImGui::DragFloat("Split Strength", &environment.post.splitToneStrength, 0.01f, 0.0f, 0.6f, "%.2f");
        dirty |= ImGui::DragFloat("Split Balance", &environment.post.splitToneBalance, 0.01f, 0.15f, 0.85f, "%.2f");
        dirty |= editColor("Fog Near", environment.post.fogNearColor);
        dirty |= editColor("Fog Far", environment.post.fogFarColor);
        dirty |= editColor("Shadow Tint", environment.post.shadowTint);
        dirty |= editColor("Highlight Tint", environment.post.highlightTint);
    }

    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen)) {
        dirty |= ImGui::Checkbox("Sky Enabled", &environment.sky.enabled);
        dirty |= editColor("Zenith", environment.sky.zenithColor);
        dirty |= editColor("Horizon", environment.sky.horizonColor);
        dirty |= editColor("Ground Haze", environment.sky.groundHazeColor);
        dirty |= editVec3("Sun Direction", environment.sky.sunDirection, 0.01f);
        dirty |= editColor("Sun Color", environment.sky.sunColor);
        dirty |= ImGui::DragFloat("Sun Size", &environment.sky.sunSize, 0.001f, 0.001f, 0.10f, "%.3f");
        dirty |= ImGui::DragFloat("Sun Glow", &environment.sky.sunGlow, 0.01f, 0.0f, 2.0f, "%.2f");
        dirty |= editString("Panorama Path", environment.sky.panoramaPath, "assets/skies/sky.jpg");
        dirty |= editColor("Panorama Tint", environment.sky.panoramaTint);
        dirty |= ImGui::DragFloat("Panorama Strength", &environment.sky.panoramaStrength, 0.01f, 0.0f, 2.0f, "%.2f");
        dirty |= ImGui::DragFloat("Panorama Yaw", &environment.sky.panoramaYawOffset, 0.5f, -360.0f, 360.0f, "%.1f");
        if (ImGui::TreeNodeEx("Cubemap", ImGuiTreeNodeFlags_DefaultOpen)) {
            static constexpr const char* kCubemapLabels[6] = {"Face +X", "Face -X", "Face +Y", "Face -Y", "Face +Z", "Face -Z"};
            for (std::size_t i = 0; i < 6; ++i) {
                dirty |= editString(kCubemapLabels[i], environment.sky.cubemapFacePaths[i], "assets/skies/cubemap_face.png");
            }
            dirty |= editColor("Cubemap Tint", environment.sky.cubemapTint);
            dirty |= ImGui::DragFloat("Cubemap Strength", &environment.sky.cubemapStrength, 0.01f, 0.0f, 2.0f, "%.2f");
            ImGui::TreePop();
        }
        dirty |= ImGui::Checkbox("Moon Enabled", &environment.sky.moonEnabled);
        dirty |= editVec3("Moon Direction", environment.sky.moonDirection, 0.01f);
        dirty |= editColor("Moon Color", environment.sky.moonColor);
        dirty |= ImGui::DragFloat("Moon Size", &environment.sky.moonSize, 0.001f, 0.001f, 0.10f, "%.3f");
        dirty |= ImGui::DragFloat("Moon Glow", &environment.sky.moonGlow, 0.01f, 0.0f, 1.0f, "%.2f");
        if (ImGui::TreeNodeEx("Sky Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
            dirty |= editString("Cloud Layer A", environment.sky.cloudLayerAPath, "assets/skies/clouds_a.png");
            dirty |= editString("Cloud Layer B", environment.sky.cloudLayerBPath, "assets/skies/clouds_b.png");
            dirty |= editString("Horizon Layer", environment.sky.horizonLayerPath, "assets/skies/horizon.png");
            ImGui::TreePop();
        }
        dirty |= editColor("Cloud Tint", environment.sky.cloudTint);
        dirty |= ImGui::DragFloat("Cloud Scale", &environment.sky.cloudScale, 0.01f, 0.1f, 4.0f, "%.2f");
        dirty |= ImGui::DragFloat("Cloud Speed", &environment.sky.cloudSpeed, 0.0002f, 0.0f, 0.05f, "%.3f");
        dirty |= ImGui::DragFloat("Cloud Coverage", &environment.sky.cloudCoverage, 0.01f, 0.0f, 1.0f, "%.2f");
        dirty |= ImGui::DragFloat("Cloud Parallax", &environment.sky.cloudParallax, 0.001f, 0.0f, 0.20f, "%.3f");
        dirty |= editColor("Horizon Tint", environment.sky.horizonTint);
        dirty |= ImGui::DragFloat("Horizon Height", &environment.sky.horizonHeight, 0.01f, 0.0f, 1.0f, "%.2f");
        dirty |= ImGui::DragFloat("Horizon Fade", &environment.sky.horizonFade, 0.01f, 0.0f, 1.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        dirty |= editColor("Hemi Sky", environment.lighting.hemisphereSkyColor);
        dirty |= editColor("Hemi Ground", environment.lighting.hemisphereGroundColor);
        dirty |= ImGui::DragFloat("Hemi Strength", &environment.lighting.hemisphereStrength, 0.01f, 0.0f, 2.0f, "%.2f");
        dirty |= ImGui::Checkbox("Enable Directional", &environment.lighting.enableDirectionalLights);
        dirty |= ImGui::Checkbox("Enable Shadows", &environment.lighting.enableShadows);
        dirty |= ImGui::DragFloat("Shadow Bias", &environment.lighting.shadowBias, 0.0001f, 0.0001f, 0.02f, "%.4f");
        dirty |= ImGui::DragFloat("Shadow Normal Bias", &environment.lighting.shadowNormalBias, 0.001f, 0.0f, 0.20f, "%.3f");
        if (ImGui::TreeNodeEx("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            dirty |= ImGui::Checkbox("Sun Enabled", &environment.lighting.sun.enabled);
            dirty |= editVec3("Sun Dir", environment.lighting.sun.direction, 0.01f);
            dirty |= editColor("Sun Color", environment.lighting.sun.color);
            dirty |= ImGui::DragFloat("Sun Intensity", &environment.lighting.sun.intensity, 0.01f, 0.0f, 4.0f, "%.2f");
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
            dirty |= ImGui::Checkbox("Fill Enabled", &environment.lighting.fill.enabled);
            dirty |= editVec3("Fill Dir", environment.lighting.fill.direction, 0.01f);
            dirty |= editColor("Fill Color", environment.lighting.fill.color);
            dirty |= ImGui::DragFloat("Fill Intensity", &environment.lighting.fill.intensity, 0.01f, 0.0f, 4.0f, "%.2f");
            ImGui::TreePop();
        }
    }

    return dirty;
}

bool renderPrefabDraftFields(GameplayArchetypeDefinition& prefab) {
    bool dirty = false;
    dirty |= editString("Id", prefab.id);
    static constexpr const char* kKinds[] = {"checkpoint", "double_door"};
    int kindIndex = prefab.kind == GameplayArchetypeKind::Checkpoint ? 0 : 1;
    if (ImGui::Combo("Type", &kindIndex, kKinds, 2)) {
        prefab.kind = kindIndex == 0 ? GameplayArchetypeKind::Checkpoint : GameplayArchetypeKind::DoubleDoor;
        dirty = true;
    }

    switch (prefab.kind) {
    case GameplayArchetypeKind::Checkpoint:
        dirty |= editVec3("Position", prefab.checkpoint.position);
        dirty |= editVec3("Respawn Position", prefab.checkpoint.respawnPosition);
        dirty |= ImGui::DragFloat("Interact Distance", &prefab.checkpoint.interactDistance, 0.05f, 0.1f, 20.0f, "%.2f");
        dirty |= ImGui::DragFloat("Interact Dot", &prefab.checkpoint.interactDotThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
        dirty |= editVec3("Light Position", prefab.checkpoint.lightPosition);
        dirty |= editColor("Light Color", prefab.checkpoint.lightColor);
        dirty |= ImGui::DragFloat("Light Radius", &prefab.checkpoint.lightRadius, 0.05f, 0.1f, 30.0f, "%.2f");
        dirty |= ImGui::DragFloat("Light Intensity", &prefab.checkpoint.lightIntensity, 0.01f, 0.0f, 10.0f, "%.2f");
        break;
    case GameplayArchetypeKind::DoubleDoor:
        dirty |= editString("Left Leaf Mesh", prefab.doubleDoor.leftLeafMeshName);
        dirty |= editString("Right Leaf Mesh", prefab.doubleDoor.rightLeafMeshName);
        dirty |= editVec3("Root Position", prefab.doubleDoor.rootPosition);
        dirty |= editVec3("Left Hinge", prefab.doubleDoor.leftHingePosition);
        dirty |= editVec3("Right Hinge", prefab.doubleDoor.rightHingePosition);
        dirty |= editVec3("Leaf Scale", prefab.doubleDoor.leafScale, 0.02f);
        dirty |= ImGui::DragFloat("Closed Yaw", &prefab.doubleDoor.closedYaw, 0.5f, -360.0f, 360.0f, "%.1f");
        dirty |= ImGui::DragFloat("Open Angle", &prefab.doubleDoor.openAngle, 0.5f, 1.0f, 180.0f, "%.1f");
        dirty |= ImGui::DragFloat("Interact Distance", &prefab.doubleDoor.interactDistance, 0.05f, 0.1f, 20.0f, "%.2f");
        dirty |= ImGui::DragFloat("Interact Dot", &prefab.doubleDoor.interactDotThreshold, 0.01f, 0.0f, 1.0f, "%.2f");
        dirty |= ImGui::DragFloat("Open Duration", &prefab.doubleDoor.openDuration, 0.01f, 0.1f, 10.0f, "%.2f");
        break;
    }
    return dirty;
}

void renderSceneAssetInspector(const EditorInspectedAsset& asset) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    try {
        const LevelDef level = loadLevelDef(asset.absolutePath);
        ImGui::Separator();
        ImGui::Text("Environment: %s", level.environmentId.c_str());
        ImGui::Text("Meshes: %zu", level.meshes.size());
        ImGui::Text("Lights: %zu", level.lights.size());
        ImGui::Text("Box Colliders: %zu", level.boxColliders.size());
        ImGui::Text("Cylinder Colliders: %zu", level.cylinderColliders.size());
        ImGui::Text("Archetypes: %zu", level.archetypes.size());
        ImGui::Text("Player Spawn: %s", level.hasPlayerSpawn ? "Yes" : "No");
    } catch (const std::exception& ex) {
        ImGui::TextWrapped("Failed to load scene: %s", ex.what());
    }
}

void renderMeshAssetInspector(const EditorInspectedAsset& asset, AssetInspectorSession& session) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (!asset.meshId.empty()) {
        ImGui::Text("Mesh Id: %s", asset.meshId.c_str());
    }

    RenderMaterialData material;
    material.id = "mesh_preview";
    material.shadingModel = MaterialKind::Stone;
    material.baseColor = glm::vec3(0.92f, 0.90f, 0.86f);
    if (session.previewRenderer.drawMeshPreview(asset.absolutePath, material, glm::vec3(0.08f, 0.09f, 0.10f), "mesh_asset_preview")) {
        const EditorPreviewMeshStats stats = session.previewRenderer.currentMeshStats();
        if (stats.valid) {
            ImGui::Text("Vertices: %zu", stats.vertexCount);
            ImGui::Text("Indices: %zu", stats.indexCount);
            ImGui::Text("Bounds Min: %.2f %.2f %.2f", stats.min.x, stats.min.y, stats.min.z);
            ImGui::Text("Bounds Max: %.2f %.2f %.2f", stats.max.x, stats.max.y, stats.max.z);
        }
    } else {
        ImGui::TextUnformatted("Failed to load mesh preview.");
    }
}

void renderMaterialAssetInspector(EditorUiState& ui,
                                  const EditorInspectedAsset& asset,
                                  AssetInspectorSession& session,
                                  ContentRegistry& content,
                                  InspectorActionResult& result) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (!session.materialDraft.has_value()) {
        ImGui::TextUnformatted("Failed to load material asset.");
        return;
    }

    MaterialDefinition& material = *session.materialDraft;
    bool dirty = false;
    if (ImGui::Button("Save Material")) {
        try {
            saveMaterialDefinitionAsset(asset.absolutePath, material);
            content.loadDefaults();
            session.materialLibraryDirty = true;
            session.materialDirty = false;
            ui.inspectedAsset.declaredId = material.id;
            result.contentReloaded = true;
            result.previewDirty = true;
        } catch (const std::exception& ex) {
            spdlog::error("Material save failed: {}", ex.what());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Material")) {
        try {
            material = loadMaterialDefinitionAsset(asset.absolutePath);
            session.materialLibraryDirty = true;
            session.materialDirty = false;
        } catch (const std::exception& ex) {
            spdlog::error("Material reload failed: {}", ex.what());
        }
    }

    renderMaterialDraftFields(material, dirty);
    if (dirty) {
        session.materialDirty = true;
    }
    if (session.materialDirty) {
        ImGui::TextDisabled("Unsaved changes");
    }

    RenderMaterialData previewMaterial = buildDraftMaterialPreview(material, session, content);
    session.previewRenderer.drawMaterialPreview(previewMaterial, glm::vec3(0.08f, 0.09f, 0.10f), "material_asset_preview");
    if (session.materialDirty) {
        ImGui::TextDisabled("Preview uses the current draft for scalar/color overrides and saved maps.");
    }
}

void renderEnvironmentAssetInspector(EditorUiState& ui,
                                     const EditorInspectedAsset& asset,
                                     AssetInspectorSession& session,
                                     EditorSceneDocument& document,
                                     ContentRegistry& content,
                                     InspectorActionResult& result) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (!session.environmentDraft.has_value()) {
        ImGui::TextUnformatted("Failed to load environment asset.");
        return;
    }

    EnvironmentDefinition& environment = *session.environmentDraft;
    if (ImGui::Button("Save Environment")) {
        try {
            saveEnvironmentDefinitionAsset(asset.absolutePath, environment);
            content.loadDefaults();
            session.environmentDirty = false;
            ui.inspectedAsset.declaredId = environment.id;
            result.contentReloaded = true;
            result.previewDirty = true;
        } catch (const std::exception& ex) {
            spdlog::error("Environment save failed: {}", ex.what());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Environment")) {
        try {
            environment = loadEnvironmentDefinitionAsset(asset.absolutePath);
            session.environmentDirty = false;
        } catch (const std::exception& ex) {
            spdlog::error("Environment reload failed: {}", ex.what());
        }
    }

    const bool dirty = renderEnvironmentDraftFields(environment);
    if (dirty) {
        session.environmentDirty = true;
        const std::string activeEnvironmentPath = document.environmentAssetPath(content);
        if (!activeEnvironmentPath.empty()
            && std::filesystem::weakly_canonical(std::filesystem::path(activeEnvironmentPath))
                == std::filesystem::weakly_canonical(std::filesystem::path(asset.absolutePath))) {
            document.environment() = environment;
            document.markEnvironmentDirty();
            result.previewDirty = true;
        }
    }
    if (session.environmentDirty) {
        ImGui::TextDisabled("Unsaved changes");
    }

    std::string previewImage = environment.sky.panoramaPath;
    if (previewImage.empty()) {
        for (const std::string& face : environment.sky.cubemapFacePaths) {
            if (!face.empty()) {
                previewImage = face;
                break;
            }
        }
    }
    if (!previewImage.empty()) {
        session.previewRenderer.drawImagePreview(std::filesystem::absolute(previewImage).string(), "environment_sky_preview");
    }
}

void renderPrefabAssetInspector(EditorUiState& ui,
                                const EditorInspectedAsset& asset,
                                AssetInspectorSession& session,
                                ContentRegistry& content,
                                InspectorActionResult& result) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (!session.prefabDraft.has_value()) {
        ImGui::TextUnformatted("Failed to load prefab asset.");
        return;
    }

    GameplayArchetypeDefinition& prefab = *session.prefabDraft;
    if (ImGui::Button("Save Prefab")) {
        try {
            saveGameplayArchetypeAsset(asset.absolutePath, prefab);
            content.loadDefaults();
            session.prefabDirty = false;
            ui.inspectedAsset.declaredId = prefab.id;
            result.contentReloaded = true;
            result.previewDirty = true;
        } catch (const std::exception& ex) {
            spdlog::error("Prefab save failed: {}", ex.what());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Prefab")) {
        try {
            prefab = loadGameplayArchetypeAsset(asset.absolutePath);
            session.prefabDirty = false;
        } catch (const std::exception& ex) {
            spdlog::error("Prefab reload failed: {}", ex.what());
        }
    }

    const bool dirty = renderPrefabDraftFields(prefab);
    if (dirty) {
        session.prefabDirty = true;
    }
    if (session.prefabDirty) {
        ImGui::TextDisabled("Unsaved changes");
    }
    ImGui::TextDisabled("Prefab preview is metadata-only in this first pass.");
}

void renderSkyAssetInspector(const EditorInspectedAsset& asset, AssetInspectorSession& session) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (session.previewRenderer.drawImagePreview(asset.absolutePath, "sky_asset_preview")) {
        const glm::ivec2 imageSize = session.previewRenderer.currentImageSize();
        ImGui::Text("Dimensions: %d x %d", imageSize.x, imageSize.y);
    } else {
        ImGui::TextUnformatted("No image preview available.");
    }
}

void renderShaderAssetInspector(const EditorInspectedAsset& asset) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    const std::string snippet = shaderSnippet(asset.absolutePath);
    if (!snippet.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted("Preview");
        ImGui::BeginChild("ShaderSnippet", ImVec2(0.0f, 220.0f), true);
        ImGui::TextUnformatted(snippet.c_str());
        ImGui::EndChild();
    }
}

void renderOtherAssetInspector(const EditorInspectedAsset& asset) {
    renderFileHeader(asset);
    renderFileMetadata(asset.absolutePath);
    if (asset.directory) {
        ImGui::TextUnformatted("Folder selected.");
    } else {
        ImGui::TextUnformatted("No specialized inspector yet.");
    }
}

void renderSceneSelectionInspector(EditorSceneDocument& document,
                                   const std::vector<std::uint64_t>& selectedIds,
                                   const ContentRegistry& content,
                                   const std::vector<std::string>& meshIds,
                                   const std::vector<std::string>& materialIds,
                                   const std::vector<std::string>& archetypeIds,
                                   EditorPendingCommand& pendingCommand,
                                   EditorCommandStack& commandStack) {
    if (selectedIds.empty()) {
        ImGui::TextUnformatted("No scene selection");
        return;
    }

    const auto trackSceneItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
        if (changed) {
            document.markSceneDirty();
        }
        trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
    };

    if (selectedIds.size() > 1) {
        ImGui::Text("%zu objects selected", selectedIds.size());
        const auto meshObjects = selectedMeshObjects(document, selectedIds);
        if (!meshObjects.empty()) {
            ImGui::Separator();
            ImGui::Text("Mesh Materials");
            const std::string currentMaterialLabel = materialSelectionLabel(meshObjects);
            if (ImGui::BeginCombo("Material Id", currentMaterialLabel.c_str())) {
                for (const auto& materialId : materialIds) {
                    const bool selected = materialId == currentMaterialLabel;
                    if (ImGui::Selectable(materialId.c_str(), selected)) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        if (applyMaterialToMeshes(meshObjects, materialId, content, document)) {
                            commandStack.pushDocumentStateCommand(
                                meshObjects.size() == 1 ? "Assign Mesh Material" : "Assign Mesh Materials",
                                beforeState,
                                document.captureState(),
                                document);
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("Applies to %zu selected mesh%s", meshObjects.size(), meshObjects.size() == 1 ? "" : "es");
        }
        return;
    }

    EditorSceneObject* object = document.findObject(selectedIds.front());
    if (object == nullptr) {
        ImGui::TextUnformatted("Selection missing");
        return;
    }

    ImGui::TextUnformatted(editorSceneObjectKindName(object->kind));
    ImGui::TextDisabled("Id: %llu", static_cast<unsigned long long>(object->id));
    ImGui::Separator();

    const std::uint64_t currentParentId = document.parentObjectId(object->id);
    if (document.supportsParenting(object->id) || currentParentId != 0) {
        std::string parentLabel = "<None>";
        if (currentParentId != 0) {
            if (const EditorSceneObject* parentObject = document.findObject(currentParentId)) {
                parentLabel = editorSceneObjectLabel(*parentObject);
            }
        }

        if (ImGui::BeginCombo("Parent", parentLabel.c_str())) {
            const bool noParentSelected = (currentParentId == 0);
            if (ImGui::Selectable("<None>", noParentSelected)) {
                const EditorSceneDocumentState beforeState = document.captureState();
                if (document.clearParent(object->id)) {
                    commandStack.pushDocumentStateCommand("Clear Parent", beforeState, document.captureState(), document);
                }
            }
            if (noParentSelected) {
                ImGui::SetItemDefaultFocus();
            }

            for (const auto& candidate : document.objects()) {
                if (candidate.id == object->id) {
                    continue;
                }
                if (!document.canSetParent(object->id, candidate.id) && candidate.id != currentParentId) {
                    continue;
                }
                const bool selected = (candidate.id == currentParentId);
                if (ImGui::Selectable(editorSceneObjectLabel(candidate).c_str(), selected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    if (document.setParent(object->id, candidate.id)) {
                        commandStack.pushDocumentStateCommand("Set Parent", beforeState, document.captureState(), document);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    switch (object->kind) {
    case EditorSceneObjectKind::Mesh: {
        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        const std::string currentMaterialLabel = mesh.materialId.empty()
            ? std::string(defaultMaterialIdForKind(mesh.material.value_or(MaterialKind::Stone)))
            : mesh.materialId;
        if (ImGui::BeginCombo("Mesh Id", mesh.meshId.c_str())) {
            for (const auto& meshId : meshIds) {
                const bool selected = meshId == mesh.meshId;
                if (ImGui::Selectable(meshId.c_str(), selected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    mesh.meshId = meshId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand("Change Mesh Asset", beforeState, document.captureState(), document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("Material Id", currentMaterialLabel.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == currentMaterialLabel;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    if (applyMaterialToMeshes({object}, materialId, content, document)) {
                        commandStack.pushDocumentStateCommand("Change Mesh Material", beforeState, document.captureState(), document);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Mesh", editVec3("Position", mesh.position));
        beforeState = document.captureState();
        const bool scaleChanged = editVec3("Scale", mesh.scale, 0.02f);
        if (scaleChanged) {
            mesh.scale = glm::max(mesh.scale, glm::vec3(0.01f));
        }
        trackSceneItem(beforeState, "Scale Mesh", scaleChanged);
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Rotate Mesh", editVec3("Rotation", mesh.rotation, 0.5f));
        bool hasTint = mesh.tint.has_value();
        beforeState = document.captureState();
        const bool tintToggleChanged = ImGui::Checkbox("Use Tint", &hasTint);
        if (tintToggleChanged) {
            mesh.tint = hasTint ? std::optional<glm::vec3>{mesh.tint.value_or(glm::vec3(1.0f))} : std::nullopt;
        }
        trackSceneItem(beforeState, "Toggle Mesh Tint", tintToggleChanged);
        if (mesh.tint.has_value()) {
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Change Mesh Tint", editColor("Tint", *mesh.tint));
        }
        break;
    }
    case EditorSceneObjectKind::Light: {
        auto& light = std::get<LevelLightPlacement>(object->payload);
        int typeIndex = static_cast<int>(light.type);
        const char* lightTypes[] = {"Point", "Spot", "Directional"};
        if (ImGui::Combo("Light Type", &typeIndex, lightTypes, 3)) {
            const EditorSceneDocumentState beforeState = document.captureState();
            light.type = static_cast<LightType>(typeIndex);
            document.markSceneDirty();
            commandStack.pushDocumentStateCommand("Change Light Type", beforeState, document.captureState(), document);
        }
        if (light.type != LightType::Directional) {
            const auto beforeState = document.captureState();
            trackSceneItem(beforeState, "Move Light", editVec3("Position", light.position));
        }
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Light Direction", editVec3("Direction", light.direction, 0.01f));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Change Light Color", editColor("Color", light.color));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Light Intensity", ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 10.0f, "%.2f"));
        if (light.type != LightType::Directional) {
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Adjust Light Radius", ImGui::DragFloat("Radius", &light.radius, 0.05f, 0.1f, 40.0f, "%.2f"));
        }
        if (light.type == LightType::Spot) {
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Adjust Spot Inner Cone", ImGui::DragFloat("Inner Cone", &light.innerConeDegrees, 0.5f, 1.0f, 85.0f, "%.1f"));
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Adjust Spot Outer Cone", ImGui::DragFloat("Outer Cone", &light.outerConeDegrees, 0.5f, 1.0f, 89.0f, "%.1f"));
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Toggle Spot Shadows", ImGui::Checkbox("Casts Shadows", &light.castsShadows));
        }
        break;
    }
    case EditorSceneObjectKind::BoxCollider: {
        auto& box = std::get<LevelBoxColliderPlacement>(object->payload);
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Box Collider", editVec3("Position", box.position));
        beforeState = document.captureState();
        const bool extentsChanged = editVec3("Half Extents", box.halfExtents, 0.02f);
        if (extentsChanged) {
            box.halfExtents = glm::max(box.halfExtents, glm::vec3(0.01f));
        }
        trackSceneItem(beforeState, "Resize Box Collider", extentsChanged);
        break;
    }
    case EditorSceneObjectKind::CylinderCollider: {
        auto& cylinder = std::get<LevelCylinderColliderPlacement>(object->payload);
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Cylinder Collider", editVec3("Position", cylinder.position));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Cylinder Radius", ImGui::DragFloat("Radius", &cylinder.radius, 0.02f, 0.05f, 20.0f, "%.2f"));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Cylinder Height", ImGui::DragFloat("Half Height", &cylinder.halfHeight, 0.02f, 0.05f, 20.0f, "%.2f"));
        break;
    }
    case EditorSceneObjectKind::PlayerSpawn: {
        auto& spawn = std::get<LevelPlayerSpawn>(object->payload);
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Player Spawn", editVec3("Position", spawn.position));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Fall Respawn Height", ImGui::DragFloat("Fall Respawn Y", &spawn.fallRespawnY, 0.1f, -100.0f, 100.0f, "%.2f"));
        break;
    }
    case EditorSceneObjectKind::Archetype: {
        auto& archetype = std::get<LevelArchetypePlacement>(object->payload);
        if (ImGui::BeginCombo("Archetype Id", archetype.archetypeId.c_str())) {
            for (const auto& archetypeId : archetypeIds) {
                const bool selected = archetypeId == archetype.archetypeId;
                if (ImGui::Selectable(archetypeId.c_str(), selected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    archetype.archetypeId = archetypeId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand("Change Archetype", beforeState, document.captureState(), document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Archetype", editVec3("Position", archetype.position));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Rotate Archetype", ImGui::DragFloat("Yaw", &archetype.yawDegrees, 0.5f, -360.0f, 360.0f, "%.1f"));
        break;
    }
    }
}

} // namespace

InspectorActionResult renderInspector(EditorUiState& ui,
                                      EditorSceneDocument& document,
                                      const std::vector<std::uint64_t>& selectedIds,
                                      ContentRegistry& content,
                                      const std::vector<std::string>& meshIds,
                                      const std::vector<std::string>& materialIds,
                                      const std::vector<std::string>& archetypeIds,
                                      bool* open,
                                      EditorPendingCommand& pendingCommand,
                                      EditorCommandStack& commandStack) {
    InspectorActionResult result;
    if (open != nullptr && !*open) {
        return result;
    }
    if (!ImGui::Begin("Inspector", open)) {
        ImGui::End();
        return result;
    }

    AssetInspectorSession& session = assetInspectorSession();

    if (ui.inspectorContext == EditorInspectorContext::AssetSelection && !ui.inspectedAsset.relativePath.empty()) {
        syncAssetInspectorSession(ui.inspectedAsset, session);
        switch (ui.inspectedAsset.kind) {
        case EditorAssetBrowserKind::Scene:
            renderSceneAssetInspector(ui.inspectedAsset);
            break;
        case EditorAssetBrowserKind::Mesh:
            renderMeshAssetInspector(ui.inspectedAsset, session);
            break;
        case EditorAssetBrowserKind::Material:
            renderMaterialAssetInspector(ui, ui.inspectedAsset, session, content, result);
            break;
        case EditorAssetBrowserKind::Environment:
            renderEnvironmentAssetInspector(ui, ui.inspectedAsset, session, document, content, result);
            break;
        case EditorAssetBrowserKind::Prefab:
            renderPrefabAssetInspector(ui, ui.inspectedAsset, session, content, result);
            break;
        case EditorAssetBrowserKind::Sky:
            renderSkyAssetInspector(ui.inspectedAsset, session);
            break;
        case EditorAssetBrowserKind::Shader:
            renderShaderAssetInspector(ui.inspectedAsset);
            break;
        case EditorAssetBrowserKind::Directory:
        case EditorAssetBrowserKind::Other:
            renderOtherAssetInspector(ui.inspectedAsset);
            break;
        }
    } else {
        renderSceneSelectionInspector(document,
                                      selectedIds,
                                      content,
                                      meshIds,
                                      materialIds,
                                      archetypeIds,
                                      pendingCommand,
                                      commandStack);
    }

    ImGui::End();
    return result;
}
