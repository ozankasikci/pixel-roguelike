#include "editor/ui/inspectors/EnvironmentInspector.h"

#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/EnvironmentDefinition.h"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <string>

namespace {

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

} // namespace

void drawEnvironmentAssetInspector(EditorUiState& ui,
                                   const EditorInspectedAsset& asset,
                                   AssetInspectorSession& session,
                                   EditorSceneDocument& document,
                                   ContentRegistry& content,
                                   InspectorActionResult& result) {
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
