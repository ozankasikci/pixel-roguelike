#include "editor/ui/EditorPanels.h"

#include "editor/render/EditorAssetPreviewRenderer.h"
#include "editor/ui/inspectors/ArchetypeInspector.h"
#include "editor/ui/inspectors/ColliderInspector.h"
#include "editor/ui/inspectors/GroupInspector.h"
#include "editor/ui/inspectors/LightInspector.h"
#include "editor/ui/inspectors/MeshInspector.h"
#include "editor/ui/inspectors/PlayerSpawnInspector.h"
#include "editor/ui/inspectors/ReflectionProbeInspector.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelDef.h"
#include "game/rendering/EnvironmentDefinition.h"
#include "game/rendering/MaterialTextureLibrary.h"

#include <glm/common.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
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

// -----------------------------------------------------------------------
// Asset inspector helpers
// -----------------------------------------------------------------------

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

    std::string previewMaterialId = draft.id;
    if (content.findMaterial(previewMaterialId) == nullptr) {
        if (draft.parent.has_value() && content.findMaterial(*draft.parent) != nullptr) {
            previewMaterialId = *draft.parent;
        } else {
            previewMaterialId = "stone_default";
        }
    }

    RenderMaterialData material = session.materialLibrary.resolve(previewMaterialId);
    material.id = draft.id;
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
    if (!beginInspectorPropertyTable("MaterialDraftFields")) {
        return;
    }

    dirty |= renderInspectorPropertyRow("Id", [&]() { return editString("##value", draft.id); });
    std::string parent = draft.parent.value_or("");
    if (renderInspectorPropertyRow("Parent", [&]() { return editString("##value", parent, "optional"); })) {
        draft.parent = parent.empty() ? std::optional<std::string>{} : std::optional<std::string>{parent};
        dirty = true;
    }

    auto editOptionalPath = [&](const char* label, std::optional<std::string>& value, const char* hint) {
        std::string working = value.value_or("");
        if (renderInspectorPropertyRow(label, [&]() { return editString("##value", working, hint); })) {
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
    if (renderInspectorPropertyRow("Use Base Color", [&]() { return ImGui::Checkbox("##value", &useBaseColor); })) {
        draft.baseColor = useBaseColor ? std::optional<glm::vec3>{baseColor} : std::nullopt;
        dirty = true;
    }
    if (useBaseColor && renderInspectorPropertyRow("Base Color", [&]() { return editColor("##value", baseColor); })) {
        draft.baseColor = baseColor;
        dirty = true;
    }

    static constexpr const char* kUvModes[] = {"mesh", "world_projected"};
    int uvMode = draft.uvMode.has_value() && *draft.uvMode == MaterialUvMode::WorldProjected ? 1 : 0;
    if (renderInspectorPropertyRow("UV Mode", [&]() { return ImGui::Combo("##value", &uvMode, kUvModes, 2); },
                                  EditorInspectorFieldKind::Enum)) {
        draft.uvMode = uvMode == 0 ? MaterialUvMode::Mesh : MaterialUvMode::WorldProjected;
        dirty = true;
    }

    glm::vec2 uvScale = draft.uvScale.value_or(glm::vec2(1.0f));
    if (renderInspectorPropertyRow("UV Scale", [&]() { return ImGui::DragFloat2("##value", &uvScale.x, 0.01f, 0.01f, 16.0f, "%.2f"); })) {
        draft.uvScale = glm::max(uvScale, glm::vec2(0.01f));
        dirty = true;
    }

    auto dragOptionalFloat = [&](const char* label, std::optional<float>& value, float defaultValue, float speed, float min, float max, const char* format) {
        float working = value.value_or(defaultValue);
        if (renderInspectorPropertyRow(label, [&]() { return ImGui::DragFloat("##value", &working, speed, min, max, format); })) {
            value = working;
            dirty = true;
        }
    };

    dragOptionalFloat("Normal Strength", draft.normalStrength, 1.0f, 0.01f, 0.0f, 4.0f, "%.2f");
    dragOptionalFloat("Roughness Scale", draft.roughnessScale, 1.0f, 0.01f, 0.0f, 4.0f, "%.2f");
    dragOptionalFloat("Roughness Bias", draft.roughnessBias, 0.82f, 0.01f, 0.0f, 1.0f, "%.2f");
    dragOptionalFloat("Metalness", draft.metalness, 0.0f, 0.01f, 0.0f, 1.0f, "%.2f");
    dragOptionalFloat("Specular Level", draft.specularLevel, 0.20f, 0.01f, 0.0f, 2.0f, "%.2f");
    dragOptionalFloat("AO Strength", draft.aoStrength, 1.0f, 0.01f, 0.0f, 2.0f, "%.2f");
    dragOptionalFloat("Light Tint Response", draft.lightTintResponse, 0.18f, 0.01f, 0.0f, 1.0f, "%.2f");
    dragOptionalFloat("Emissive Strength", draft.emissiveStrength, 0.0f, 0.05f, 0.0f, 10.0f, "%.2f");

    static constexpr const char* kProceduralSources[] = {
        "none", "generated_brick", "generated_stone", "generated_smooth", "generated_floor", "generated_ceiling"
    };
    int proceduralIndex = 0;
    if (draft.proceduralSource.has_value()) {
        switch (*draft.proceduralSource) {
        case MaterialProceduralSource::None: proceduralIndex = 0; break;
        case MaterialProceduralSource::GeneratedBrick: proceduralIndex = 1; break;
        case MaterialProceduralSource::GeneratedStone: proceduralIndex = 2; break;
        case MaterialProceduralSource::GeneratedSmooth: proceduralIndex = 3; break;
        case MaterialProceduralSource::GeneratedFloor: proceduralIndex = 4; break;
        case MaterialProceduralSource::GeneratedCeiling: proceduralIndex = 5; break;
        }
    }
    if (renderInspectorPropertyRow("Procedural Source", [&]() { return ImGui::Combo("##value", &proceduralIndex, kProceduralSources, 6); },
                                  EditorInspectorFieldKind::Enum)) {
        draft.proceduralSource = static_cast<MaterialProceduralSource>(proceduralIndex);
        dirty = true;
    }

    endInspectorPropertyTable();

    // Feature Flags
    if (ImGui::CollapsingHeader("Feature Flags", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!beginInspectorPropertyTable("MaterialFeatureFlags")) {
            return;
        }
        auto editOptionalBool = [&](const char* label, std::optional<bool>& value) {
            bool working = value.value_or(false);
            if (renderInspectorPropertyRow(label, [&]() { return ImGui::Checkbox("##value", &working); })) {
                value = working;
                dirty = true;
            }
        };
        editOptionalBool("Animated (wax flame flicker)", draft.animated);
        editOptionalBool("Subsurface (moss SSS)", draft.subsurface);
        editOptionalBool("Detail: Brick", draft.detailBrick);
        editOptionalBool("Detail: Wood", draft.detailWood);
        editOptionalBool("Detail: Stone", draft.detailStone);
        editOptionalBool("Detail: Floor", draft.detailFloor);
        endInspectorPropertyTable();
    }
}

bool renderEnvironmentDraftFields(EnvironmentDefinition& environment) {
    bool dirty = false;

    if (ImGui::CollapsingHeader("Post", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!beginInspectorPropertyTable("EnvironmentDraftPost")) {
            return dirty;
        }
        static constexpr const char* kToneMapModes[] = {"Linear", "ACES Fitted"};
        dirty |= renderInspectorPropertyRow("Id", [&]() { return editString("##value", environment.id); });
        dirty |= renderInspectorPropertyRow("Enable Sky", [&]() { return ImGui::Checkbox("##value", &environment.post.enableSky); });
        dirty |= renderInspectorPropertyRow("Enable Edges", [&]() { return ImGui::Checkbox("##value", &environment.post.enableEdges); });
        dirty |= renderInspectorPropertyRow("Enable Fog", [&]() { return ImGui::Checkbox("##value", &environment.post.enableFog); });
        dirty |= renderInspectorPropertyRow("Enable Tone Map", [&]() { return ImGui::Checkbox("##value", &environment.post.enableToneMap); });
        dirty |= renderInspectorPropertyRow("Enable Bloom", [&]() { return ImGui::Checkbox("##value", &environment.post.enableBloom); });
        dirty |= renderInspectorPropertyRow("Enable Vignette", [&]() { return ImGui::Checkbox("##value", &environment.post.enableVignette); });
        dirty |= renderInspectorPropertyRow("Enable Grain", [&]() { return ImGui::Checkbox("##value", &environment.post.enableGrain); });
        dirty |= renderInspectorPropertyRow("Enable Scanlines", [&]() { return ImGui::Checkbox("##value", &environment.post.enableScanlines); });
        dirty |= renderInspectorPropertyRow("Enable Sharpen", [&]() { return ImGui::Checkbox("##value", &environment.post.enableSharpen); });
        dirty |= renderInspectorPropertyRow("Tone Mapper", [&]() { return ImGui::Combo("##value", &environment.post.toneMapMode, kToneMapModes, 2); },
                                            EditorInspectorFieldKind::Enum);
        dirty |= renderInspectorPropertyRow("Edge Threshold", [&]() { return ImGui::DragFloat("##value", &environment.post.edgeThreshold, 0.005f, 0.0f, 1.0f, "%.3f"); });
        dirty |= renderInspectorPropertyRow("Fog Density", [&]() { return ImGui::DragFloat("##value", &environment.post.fogDensity, 0.001f, 0.0f, 0.5f, "%.3f"); });
        dirty |= renderInspectorPropertyRow("Fog Start", [&]() { return ImGui::DragFloat("##value", &environment.post.fogStart, 0.1f, 0.0f, 80.0f, "%.1f"); });
        dirty |= renderInspectorPropertyRow("Exposure", [&]() { return ImGui::DragFloat("##value", &environment.post.exposure, 0.01f, 0.2f, 2.5f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Gamma", [&]() { return ImGui::DragFloat("##value", &environment.post.gamma, 0.01f, 0.5f, 2.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Contrast", [&]() { return ImGui::DragFloat("##value", &environment.post.contrast, 0.01f, 0.4f, 2.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Saturation", [&]() { return ImGui::DragFloat("##value", &environment.post.saturation, 0.01f, 0.0f, 1.5f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Bloom Threshold", [&]() { return ImGui::DragFloat("##value", &environment.post.bloomThreshold, 0.01f, 0.0f, 1.5f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Bloom Intensity", [&]() { return ImGui::DragFloat("##value", &environment.post.bloomIntensity, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Bloom Radius", [&]() { return ImGui::DragFloat("##value", &environment.post.bloomRadius, 0.01f, 0.5f, 5.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Vignette Strength", [&]() { return ImGui::DragFloat("##value", &environment.post.vignetteStrength, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Vignette Softness", [&]() { return ImGui::DragFloat("##value", &environment.post.vignetteSoftness, 0.01f, 0.1f, 1.2f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Grain Amount", [&]() { return ImGui::DragFloat("##value", &environment.post.grainAmount, 0.001f, 0.0f, 0.2f, "%.3f"); });
        dirty |= renderInspectorPropertyRow("Scanline Amount", [&]() { return ImGui::DragFloat("##value", &environment.post.scanlineAmount, 0.01f, 0.0f, 0.35f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Scanline Density", [&]() { return ImGui::DragFloat("##value", &environment.post.scanlineDensity, 0.01f, 0.5f, 3.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Sharpen Amount", [&]() { return ImGui::DragFloat("##value", &environment.post.sharpenAmount, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Split Strength", [&]() { return ImGui::DragFloat("##value", &environment.post.splitToneStrength, 0.01f, 0.0f, 0.6f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Split Balance", [&]() { return ImGui::DragFloat("##value", &environment.post.splitToneBalance, 0.01f, 0.15f, 0.85f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Fog Near", [&]() { return editColor("##value", environment.post.fogNearColor); });
        dirty |= renderInspectorPropertyRow("Fog Far", [&]() { return editColor("##value", environment.post.fogFarColor); });
        dirty |= renderInspectorPropertyRow("Shadow Tint", [&]() { return editColor("##value", environment.post.shadowTint); });
        dirty |= renderInspectorPropertyRow("Highlight Tint", [&]() { return editColor("##value", environment.post.highlightTint); });
        endInspectorPropertyTable();
    }

    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!beginInspectorPropertyTable("EnvironmentDraftSky")) {
            return dirty;
        }
        dirty |= renderInspectorPropertyRow("Zenith", [&]() { return editColor("##value", environment.sky.zenithColor); });
        dirty |= renderInspectorPropertyRow("Horizon", [&]() { return editColor("##value", environment.sky.horizonColor); });
        dirty |= renderInspectorPropertyRow("Ground Haze", [&]() { return editColor("##value", environment.sky.groundHazeColor); });
        dirty |= renderInspectorPropertyRow("Sun Size", [&]() { return ImGui::DragFloat("##value", &environment.sky.sunSize, 0.001f, 0.001f, 0.10f, "%.3f"); });
        dirty |= renderInspectorPropertyRow("Sun Glow", [&]() { return ImGui::DragFloat("##value", &environment.sky.sunGlow, 0.01f, 0.0f, 2.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Panorama Path", [&]() { return editString("##value", environment.sky.panoramaPath, "assets/skies/sky.jpg"); });
        dirty |= renderInspectorPropertyRow("Panorama Tint", [&]() { return editColor("##value", environment.sky.panoramaTint); });
        dirty |= renderInspectorPropertyRow("Panorama Strength", [&]() { return ImGui::DragFloat("##value", &environment.sky.panoramaStrength, 0.01f, 0.0f, 2.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Panorama Yaw", [&]() { return ImGui::DragFloat("##value", &environment.sky.panoramaYawOffset, 0.5f, -360.0f, 360.0f, "%.1f"); });
        endInspectorPropertyTable();
        if (ImGui::TreeNodeEx("Cubemap", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!beginInspectorPropertyTable("EnvironmentDraftCubemap")) {
                return dirty;
            }
            static constexpr const char* kCubemapLabels[6] = {"Face +X", "Face -X", "Face +Y", "Face -Y", "Face +Z", "Face -Z"};
            for (std::size_t i = 0; i < 6; ++i) {
                dirty |= renderInspectorPropertyRow(kCubemapLabels[i], [&]() { return editString("##value", environment.sky.cubemapFacePaths[i], "assets/skies/cubemap_face.png"); });
            }
            dirty |= renderInspectorPropertyRow("Cubemap Tint", [&]() { return editColor("##value", environment.sky.cubemapTint); });
            dirty |= renderInspectorPropertyRow("Cubemap Strength", [&]() { return ImGui::DragFloat("##value", &environment.sky.cubemapStrength, 0.01f, 0.0f, 2.0f, "%.2f"); });
            endInspectorPropertyTable();
            ImGui::TreePop();
        }
        if (!beginInspectorPropertyTable("EnvironmentDraftMoon")) {
            return dirty;
        }
        dirty |= renderInspectorPropertyRow("Moon Enabled", [&]() { return ImGui::Checkbox("##value", &environment.sky.moonEnabled); });
        dirty |= renderInspectorPropertyRow("Moon Direction", [&]() { return editVec3("##value", environment.sky.moonDirection, 0.01f); });
        dirty |= renderInspectorPropertyRow("Moon Color", [&]() { return editColor("##value", environment.sky.moonColor); });
        dirty |= renderInspectorPropertyRow("Moon Size", [&]() { return ImGui::DragFloat("##value", &environment.sky.moonSize, 0.001f, 0.001f, 0.10f, "%.3f"); });
        dirty |= renderInspectorPropertyRow("Moon Glow", [&]() { return ImGui::DragFloat("##value", &environment.sky.moonGlow, 0.01f, 0.0f, 1.0f, "%.2f"); });
        endInspectorPropertyTable();
        if (ImGui::TreeNodeEx("Sky Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!beginInspectorPropertyTable("EnvironmentDraftSkyLayers")) {
                return dirty;
            }
            dirty |= renderInspectorPropertyRow("Cloud Layer A", [&]() { return editString("##value", environment.sky.cloudLayerAPath, "assets/skies/clouds_a.png"); });
            dirty |= renderInspectorPropertyRow("Cloud Layer B", [&]() { return editString("##value", environment.sky.cloudLayerBPath, "assets/skies/clouds_b.png"); });
            dirty |= renderInspectorPropertyRow("Horizon Layer", [&]() { return editString("##value", environment.sky.horizonLayerPath, "assets/skies/horizon.png"); });
            endInspectorPropertyTable();
            ImGui::TreePop();
        }
        if (!beginInspectorPropertyTable("EnvironmentDraftCloudsHorizon")) {
            return dirty;
        }
        dirty |= renderInspectorPropertyRow("Cloud Tint", [&]() { return editColor("##value", environment.sky.cloudTint); });
        dirty |= renderInspectorPropertyRow("Cloud Scale", [&]() { return ImGui::DragFloat("##value", &environment.sky.cloudScale, 0.01f, 0.1f, 4.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Cloud Speed", [&]() { return ImGui::DragFloat("##value", &environment.sky.cloudSpeed, 0.0002f, 0.0f, 0.05f, "%.3f"); });
        dirty |= renderInspectorPropertyRow("Cloud Coverage", [&]() { return ImGui::DragFloat("##value", &environment.sky.cloudCoverage, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Cloud Parallax", [&]() { return ImGui::DragFloat("##value", &environment.sky.cloudParallax, 0.001f, 0.0f, 0.20f, "%.3f"); });
        dirty |= renderInspectorPropertyRow("Horizon Tint", [&]() { return editColor("##value", environment.sky.horizonTint); });
        dirty |= renderInspectorPropertyRow("Horizon Height", [&]() { return ImGui::DragFloat("##value", &environment.sky.horizonHeight, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Horizon Fade", [&]() { return ImGui::DragFloat("##value", &environment.sky.horizonFade, 0.01f, 0.0f, 1.0f, "%.2f"); });
        endInspectorPropertyTable();
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!beginInspectorPropertyTable("EnvironmentDraftLighting")) {
            return dirty;
        }
        dirty |= renderInspectorPropertyRow("Hemi Sky", [&]() { return editColor("##value", environment.lighting.hemisphereSkyColor); });
        dirty |= renderInspectorPropertyRow("Hemi Ground", [&]() { return editColor("##value", environment.lighting.hemisphereGroundColor); });
        dirty |= renderInspectorPropertyRow("Hemi Strength", [&]() { return ImGui::DragFloat("##value", &environment.lighting.hemisphereStrength, 0.01f, 0.0f, 2.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Enable Directional", [&]() { return ImGui::Checkbox("##value", &environment.lighting.enableDirectionalLights); });
        dirty |= renderInspectorPropertyRow("Enable Shadows", [&]() { return ImGui::Checkbox("##value", &environment.lighting.enableShadows); });
        dirty |= renderInspectorPropertyRow("Shadow Bias", [&]() { return ImGui::DragFloat("##value", &environment.lighting.shadowBias, 0.0001f, 0.0001f, 0.02f, "%.4f"); });
        dirty |= renderInspectorPropertyRow("Shadow Normal Bias", [&]() { return ImGui::DragFloat("##value", &environment.lighting.shadowNormalBias, 0.001f, 0.0f, 0.20f, "%.3f"); });
        endInspectorPropertyTable();
        if (ImGui::TreeNodeEx("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!beginInspectorPropertyTable("EnvironmentDraftSun")) {
                return dirty;
            }
            dirty |= renderInspectorPropertyRow("Sun Enabled", [&]() { return ImGui::Checkbox("##value", &environment.lighting.sun.enabled); });
            dirty |= renderInspectorPropertyRow("Sun Dir", [&]() { return editVec3("##value", environment.lighting.sun.direction, 0.01f); });
            dirty |= renderInspectorPropertyRow("Sun Color", [&]() { return editColor("##value", environment.lighting.sun.color); });
            dirty |= renderInspectorPropertyRow("Sun Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.sun.intensity, 0.01f, 0.0f, 4.0f, "%.2f"); });
            endInspectorPropertyTable();
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!beginInspectorPropertyTable("EnvironmentDraftFill")) {
                return dirty;
            }
            dirty |= renderInspectorPropertyRow("Fill Enabled", [&]() { return ImGui::Checkbox("##value", &environment.lighting.fill.enabled); });
            dirty |= renderInspectorPropertyRow("Fill Dir", [&]() { return editVec3("##value", environment.lighting.fill.direction, 0.01f); });
            dirty |= renderInspectorPropertyRow("Fill Color", [&]() { return editColor("##value", environment.lighting.fill.color); });
            dirty |= renderInspectorPropertyRow("Fill Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.fill.intensity, 0.01f, 0.0f, 4.0f, "%.2f"); });
            endInspectorPropertyTable();
            ImGui::TreePop();
        }
    }

    return dirty;
}

bool renderPrefabDraftFields(GameplayArchetypeDefinition& prefab) {
    bool dirty = false;
    if (!beginInspectorPropertyTable("PrefabDraftFields")) {
        return false;
    }
    dirty |= renderInspectorPropertyRow("Id", [&]() { return editString("##value", prefab.id); });
    static constexpr const char* kKinds[] = {"checkpoint", "double_door"};
    int kindIndex = prefab.kind == GameplayArchetypeKind::Checkpoint ? 0 : 1;
    if (renderInspectorPropertyRow("Type", [&]() { return ImGui::Combo("##value", &kindIndex, kKinds, 2); },
                                  EditorInspectorFieldKind::Enum)) {
        prefab.kind = kindIndex == 0 ? GameplayArchetypeKind::Checkpoint : GameplayArchetypeKind::DoubleDoor;
        dirty = true;
    }

    switch (prefab.kind) {
    case GameplayArchetypeKind::Checkpoint:
        dirty |= renderInspectorPropertyRow("Position", [&]() { return editVec3("##value", prefab.checkpoint.position); });
        dirty |= renderInspectorPropertyRow("Respawn Position", [&]() { return editVec3("##value", prefab.checkpoint.respawnPosition); });
        dirty |= renderInspectorPropertyRow("Interact Distance", [&]() { return ImGui::DragFloat("##value", &prefab.checkpoint.interactDistance, 0.05f, 0.1f, 20.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Interact Dot", [&]() { return ImGui::DragFloat("##value", &prefab.checkpoint.interactDotThreshold, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Light Position", [&]() { return editVec3("##value", prefab.checkpoint.lightPosition); });
        dirty |= renderInspectorPropertyRow("Light Color", [&]() { return editColor("##value", prefab.checkpoint.lightColor); });
        dirty |= renderInspectorPropertyRow("Light Radius", [&]() { return ImGui::DragFloat("##value", &prefab.checkpoint.lightRadius, 0.05f, 0.1f, 30.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Light Intensity", [&]() { return ImGui::DragFloat("##value", &prefab.checkpoint.lightIntensity, 0.01f, 0.0f, 10.0f, "%.2f"); });
        break;
    case GameplayArchetypeKind::DoubleDoor:
        dirty |= renderInspectorPropertyRow("Left Leaf Mesh", [&]() { return editString("##value", prefab.doubleDoor.leftLeafMeshName); });
        dirty |= renderInspectorPropertyRow("Right Leaf Mesh", [&]() { return editString("##value", prefab.doubleDoor.rightLeafMeshName); });
        dirty |= renderInspectorPropertyRow("Root Position", [&]() { return editVec3("##value", prefab.doubleDoor.rootPosition); });
        dirty |= renderInspectorPropertyRow("Left Hinge", [&]() { return editVec3("##value", prefab.doubleDoor.leftHingePosition); });
        dirty |= renderInspectorPropertyRow("Right Hinge", [&]() { return editVec3("##value", prefab.doubleDoor.rightHingePosition); });
        dirty |= renderInspectorPropertyRow("Leaf Scale", [&]() { return editVec3("##value", prefab.doubleDoor.leafScale, 0.02f); });
        dirty |= renderInspectorPropertyRow("Closed Yaw", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.closedYaw, 0.5f, -360.0f, 360.0f, "%.1f"); });
        dirty |= renderInspectorPropertyRow("Open Angle", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.openAngle, 0.5f, 1.0f, 180.0f, "%.1f"); });
        dirty |= renderInspectorPropertyRow("Interact Distance", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.interactDistance, 0.05f, 0.1f, 20.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Interact Dot", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.interactDotThreshold, 0.01f, 0.0f, 1.0f, "%.2f"); });
        dirty |= renderInspectorPropertyRow("Open Duration", [&]() { return ImGui::DragFloat("##value", &prefab.doubleDoor.openDuration, 0.01f, 0.1f, 10.0f, "%.2f"); });
        break;
    }
    endInspectorPropertyTable();
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
        ImGui::Text("Colliders: %zu", level.colliders.size());
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
    material.specularLevel = 0.20f;
    material.detailStone = true;
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

    // Validation error state (persists per frame)
    static std::string validationError;

    if (ImGui::Button("Save Material")) {
        std::string errorOut;
        if (!content.validateMaterialDefinition(material, errorOut)) {
            validationError = errorOut;
        } else {
            validationError.clear();
            try {
                saveMaterialDefinitionAsset(asset.absolutePath, material);
                content.addMaterial(material, asset.absolutePath);
                session.materialLibraryDirty = true;
                session.materialDirty = false;
                ui.inspectedAsset.declaredId = material.id;
                result.contentReloaded = true;
                result.previewDirty = true;
            } catch (const std::exception& ex) {
                spdlog::error("Material save failed: {}", ex.what());
                validationError = ex.what();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Material")) {
        try {
            material = loadMaterialDefinitionAsset(asset.absolutePath);
            session.materialLibraryDirty = true;
            session.materialDirty = false;
            validationError.clear();
        } catch (const std::exception& ex) {
            spdlog::error("Material reload failed: {}", ex.what());
        }
    }

    if (!validationError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Validation error: %s", validationError.c_str());
    }

    // Preview sphere at top of inspector (before property groups)
    RenderMaterialData previewMaterial = buildDraftMaterialPreview(material, session, content);
    session.previewRenderer.drawMaterialPreview(previewMaterial, glm::vec3(0.08f, 0.09f, 0.10f), "material_asset_preview");
    if (session.materialDirty) {
        ImGui::TextDisabled("Preview uses current draft values; save to persist.");
    }

    ImGui::Separator();
    renderMaterialDraftFields(material, dirty);
    if (dirty) {
        session.materialDirty = true;
    }
    if (session.materialDirty) {
        ImGui::TextDisabled("Unsaved changes");
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

// -----------------------------------------------------------------------
// Scene selection inspector — thin dispatcher to per-type inspectors
// -----------------------------------------------------------------------

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

    if (!beginInspectorPropertyTable("SceneSelectionProperties")) {
        return;
    }

    // Parent picker — shown for all object types
    const auto renderParentPicker = [&]() {
        const std::uint64_t currentParentId = document.parentObjectId(object->id);
        if (!(document.supportsParenting(object->id) || currentParentId != 0)) {
            return;
        }

        std::string parentLabel = "<None>";
        if (currentParentId != 0) {
            if (const EditorSceneObject* parentObject = document.findObject(currentParentId)) {
                parentLabel = editorSceneObjectLabel(*parentObject);
            }
        }

        renderInspectorPropertyRow("Parent", [&]() {
            if (ImGui::BeginCombo("##value", parentLabel.c_str())) {
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
            return false;
        });
    };

    renderParentPicker();

    // The table is open at this point. Each per-type inspector closes it before
    // rendering any post-table sections (behaviors, interactable).
    const EditorSceneDocumentState beforeState = document.captureState();

    switch (object->kind) {
    case EditorSceneObjectKind::Mesh:
        drawMeshInspector(std::get<LevelMeshPlacement>(object->payload), document, meshIds, materialIds,
                          content, commandStack, pendingCommand, beforeState);
        return;  // table closed by drawMeshInspector
    case EditorSceneObjectKind::Light:
        drawLightInspector(std::get<LevelLightPlacement>(object->payload), document,
                           commandStack, pendingCommand, beforeState);
        return;  // table closed by drawLightInspector
    case EditorSceneObjectKind::Collider:
        drawColliderInspector(std::get<LevelColliderPlacement>(object->payload), document,
                              commandStack, pendingCommand, beforeState);
        return;  // table closed by drawColliderInspector
    case EditorSceneObjectKind::ReflectionProbe:
        drawReflectionProbeInspector(std::get<LevelReflectionProbePlacement>(object->payload), document,
                                     commandStack, pendingCommand, beforeState);
        break;  // table closed by drawReflectionProbeInspector
    case EditorSceneObjectKind::PlayerSpawn:
        drawPlayerSpawnInspector(std::get<LevelPlayerSpawn>(object->payload), document,
                                 commandStack, pendingCommand, beforeState);
        break;  // table closed by drawPlayerSpawnInspector
    case EditorSceneObjectKind::Archetype:
        drawArchetypeInspector(std::get<LevelArchetypePlacement>(object->payload), document,
                               archetypeIds, commandStack, pendingCommand, beforeState);
        break;  // table closed by drawArchetypeInspector
    case EditorSceneObjectKind::Group:
        drawGroupInspector(std::get<LevelGroupNode>(object->payload), document,
                           commandStack, pendingCommand, beforeState);
        break;  // table closed by drawGroupInspector
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
    if (!beginCompactEditorPanelWindow("Inspector", open)) {
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
