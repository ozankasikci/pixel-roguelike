#include "editor/ui/EditorPanels.h"

#include "game/content/ContentRegistry.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <imgui.h>
#include <spdlog/spdlog.h>

namespace {

std::vector<std::string> sortedEnvironmentIds(const ContentRegistry& content) {
    std::vector<std::string> ids;
    ids.reserve(content.environments().size());
    for (const auto& [id, definition] : content.environments()) {
        (void)definition;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

} // namespace

bool renderEnvironmentPanel(EditorSceneDocument& document,
                            ContentRegistry& content,
                            std::vector<std::string>& environmentIds,
                            bool* open,
                            EditorPendingCommand& pendingCommand,
                            EditorCommandStack& commandStack) {
    static bool showSaveAsProfile = false;
    static char saveAsProfileBuffer[256]{};
    bool contentReloaded = false;
    if (open != nullptr && !*open) {
        return contentReloaded;
    }
    EnvironmentDefinition& environment = document.environment();
    if (!ImGui::Begin("Environment", open)) {
        ImGui::End();
        return contentReloaded;
    }

    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("Environment", environment.id.c_str())) {
        for (const auto& id : environmentIds) {
            const bool selected = (id == environment.id);
            if (ImGui::Selectable(id.c_str(), selected)) {
                const EditorSceneDocumentState beforeState = document.captureState();
                document.setEnvironmentId(id, content);
                commandStack.pushDocumentStateCommand(
                    "Change Environment",
                    beforeState,
                    document.captureState(),
                    document);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Save")) {
        try {
            saveEnvironmentDefinitionAsset(document.environmentAssetPath(content), environment);
            document.markEnvironmentSaved();
            content.loadDefaults();
            environmentIds = sortedEnvironmentIds(content);
            contentReloaded = true;
        } catch (const std::exception& ex) {
            spdlog::error("Environment save failed: {}", ex.what());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        showSaveAsProfile = true;
        std::snprintf(saveAsProfileBuffer, sizeof(saveAsProfileBuffer), "%s", environment.id.c_str());
        ImGui::OpenPopup("Save Environment As");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        try {
            const EditorSceneDocumentState beforeState = document.captureState();
            content.loadDefaults();
            environmentIds = sortedEnvironmentIds(content);
            contentReloaded = true;
            if (document.reloadEnvironmentFromRegistry(content)) {
                commandStack.pushDocumentStateCommand(
                    "Reload Environment",
                    beforeState,
                    document.captureState(),
                    document);
            }
        } catch (const std::exception& ex) {
            spdlog::error("Environment reload failed: {}", ex.what());
        }
    }

    if (showSaveAsProfile && ImGui::BeginPopupModal("Save Environment As", &showSaveAsProfile, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Profile Id", saveAsProfileBuffer, sizeof(saveAsProfileBuffer));
        if (ImGui::Button("Create Profile")) {
            try {
                const EditorSceneDocumentState beforeState = document.captureState();
                document.renameEnvironmentId(saveAsProfileBuffer);
                saveEnvironmentDefinitionAsset(document.environmentAssetPath(content), document.environment());
                document.markEnvironmentSaved();
                content.loadDefaults();
                environmentIds = sortedEnvironmentIds(content);
                contentReloaded = true;
                commandStack.pushDocumentStateCommand(
                    "Save Environment As",
                    beforeState,
                    document.captureState(),
                    document);
                showSaveAsProfile = false;
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& ex) {
                spdlog::error("Environment save-as failed: {}", ex.what());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            showSaveAsProfile = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::CollapsingHeader("Post", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto trackEnvItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
            if (changed) {
                document.markEnvironmentDirty();
            }
            trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
        };

        static constexpr const char* kToneMapModes[] = {
            "Linear",
            "ACES Fitted",
        };

        // --- Tone Mapping ---
        ImGui::SeparatorText("Tone Mapping");
        auto beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Tone Map", ImGui::Checkbox("Enable Tone Map", &environment.post.enableToneMap));
        beforeState = document.captureState();
        int toneMapMode = environment.post.toneMapMode;
        const bool toneMapChanged = ImGui::Combo("Tone Mapper", &toneMapMode, kToneMapModes, 2);
        if (toneMapChanged) {
            environment.post.toneMapMode = toneMapMode;
        }
        trackEnvItem(beforeState, "Change Tone Mapper", toneMapChanged);
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Exposure", ImGui::DragFloat("Exposure", &environment.post.exposure, 0.01f, 0.2f, 2.5f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Gamma", ImGui::DragFloat("Gamma", &environment.post.gamma, 0.01f, 0.5f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Contrast", ImGui::DragFloat("Contrast", &environment.post.contrast, 0.01f, 0.4f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Saturation", ImGui::DragFloat("Saturation", &environment.post.saturation, 0.01f, 0.0f, 1.5f, "%.2f"));

        // --- Bloom ---
        ImGui::SeparatorText("Bloom");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Bloom", ImGui::Checkbox("Enable Bloom", &environment.post.enableBloom));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Bloom Threshold", ImGui::DragFloat("Bloom Threshold", &environment.post.bloomThreshold, 0.01f, 0.0f, 1.5f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Bloom Intensity", ImGui::DragFloat("Bloom Intensity", &environment.post.bloomIntensity, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Bloom Radius", ImGui::DragFloat("Bloom Radius", &environment.post.bloomRadius, 0.01f, 0.5f, 5.0f, "%.2f"));

        // --- SSAO ---
        ImGui::SeparatorText("SSAO");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle SSAO", ImGui::Checkbox("Enable SSAO", &environment.post.enableSsao));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust SSAO Radius", ImGui::DragFloat("AO Radius", &environment.post.ssaoRadius, 0.01f, 0.1f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust SSAO Bias", ImGui::DragFloat("AO Bias", &environment.post.ssaoBias, 0.001f, 0.001f, 0.1f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust SSAO Strength", ImGui::DragFloat("AO Strength", &environment.post.ssaoStrength, 0.01f, 0.0f, 2.0f, "%.2f"));

        // --- Fog ---
        ImGui::SeparatorText("Fog");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Fog", ImGui::Checkbox("Enable Fog", &environment.post.enableFog));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Fog Density", ImGui::DragFloat("Fog Density", &environment.post.fogDensity, 0.001f, 0.0f, 0.5f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Fog Start", ImGui::DragFloat("Fog Start", &environment.post.fogStart, 0.1f, 0.0f, 80.0f, "%.1f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Depth View Scale", ImGui::DragFloat("Depth View Scale", &environment.post.depthViewScale, 0.001f, 0.01f, 0.30f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Fog Near Color", editColor("Fog Near", environment.post.fogNearColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Fog Far Color", editColor("Fog Far", environment.post.fogFarColor));

        // --- Shadows ---
        ImGui::SeparatorText("Shadows");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust CSM Split Blend", ImGui::DragFloat("CSM Split Blend", &environment.post.csmLambda, 0.01f, 0.0f, 1.0f, "%.2f"));
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0 = uniform linear splits\n1 = fully logarithmic splits (more detail near camera)");
        }

        // --- Edges ---
        ImGui::SeparatorText("Edges");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Edges", ImGui::Checkbox("Enable Edges", &environment.post.enableEdges));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Edge Threshold", ImGui::DragFloat("Edge Threshold", &environment.post.edgeThreshold, 0.005f, 0.0f, 1.0f, "%.3f"));

        // --- Color Grading ---
        ImGui::SeparatorText("Color Grading");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Split Tone Strength", ImGui::DragFloat("Split Strength", &environment.post.splitToneStrength, 0.01f, 0.0f, 0.6f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Split Tone Balance", ImGui::DragFloat("Split Balance", &environment.post.splitToneBalance, 0.01f, 0.15f, 0.85f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Shadow Tint", editColor("Shadow Tint", environment.post.shadowTint));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Highlight Tint", editColor("Highlight Tint", environment.post.highlightTint));

        // --- Effects ---
        ImGui::SeparatorText("Effects");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sky In Post", ImGui::Checkbox("Enable Sky", &environment.post.enableSky));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Vignette", ImGui::Checkbox("Enable Vignette", &environment.post.enableVignette));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Vignette Strength", ImGui::DragFloat("Vignette Strength", &environment.post.vignetteStrength, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Vignette Softness", ImGui::DragFloat("Vignette Softness", &environment.post.vignetteSoftness, 0.01f, 0.1f, 1.2f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Grain", ImGui::Checkbox("Enable Grain", &environment.post.enableGrain));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Grain Amount", ImGui::DragFloat("Grain Amount", &environment.post.grainAmount, 0.001f, 0.0f, 0.2f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Scanlines", ImGui::Checkbox("Enable Scanlines", &environment.post.enableScanlines));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Scanline Amount", ImGui::DragFloat("Scanline Amount", &environment.post.scanlineAmount, 0.01f, 0.0f, 0.35f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Scanline Density", ImGui::DragFloat("Scanline Density", &environment.post.scanlineDensity, 0.01f, 0.5f, 3.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sharpen", ImGui::Checkbox("Enable Sharpen", &environment.post.enableSharpen));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sharpen Amount", ImGui::DragFloat("Sharpen Amount", &environment.post.sharpenAmount, 0.01f, 0.0f, 1.0f, "%.2f"));
    }

    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto trackEnvItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
            if (changed) {
                document.markEnvironmentDirty();
            }
            trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
        };

        auto beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sky System", ImGui::Checkbox("Sky Enabled", &environment.sky.enabled));

        // --- Sky Colors ---
        ImGui::SeparatorText("Sky Colors");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Zenith Color", editColor("Zenith", environment.sky.zenithColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Horizon Color", editColor("Horizon", environment.sky.horizonColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Ground Haze Color", editColor("Ground Haze", environment.sky.groundHazeColor));

        // --- Sun ---
        ImGui::SeparatorText("Sun");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Direction", editVec3("Sun Direction", environment.sky.sunDirection, 0.01f));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Sky Sun Color", editColor("Sun Color", environment.sky.sunColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Size", ImGui::DragFloat("Sun Size", &environment.sky.sunSize, 0.001f, 0.001f, 0.10f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Glow", ImGui::DragFloat("Sun Glow", &environment.sky.sunGlow, 0.01f, 0.0f, 2.0f, "%.2f"));

        // --- Moon ---
        ImGui::SeparatorText("Moon");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Moon", ImGui::Checkbox("Moon Enabled", &environment.sky.moonEnabled));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Moon Direction", editVec3("Moon Direction", environment.sky.moonDirection, 0.01f));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Moon Color", editColor("Moon Color", environment.sky.moonColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Moon Size", ImGui::DragFloat("Moon Size", &environment.sky.moonSize, 0.001f, 0.001f, 0.10f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Moon Glow", ImGui::DragFloat("Moon Glow", &environment.sky.moonGlow, 0.01f, 0.0f, 1.0f, "%.2f"));

        // --- Panorama ---
        ImGui::SeparatorText("Panorama");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Panorama Path", editString("Panorama Path", environment.sky.panoramaPath, "assets/skies/sky.jpg"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Panorama Tint", editColor("Panorama Tint", environment.sky.panoramaTint));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Panorama Strength", ImGui::DragFloat("Panorama Strength", &environment.sky.panoramaStrength, 0.01f, 0.0f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Panorama Yaw", ImGui::DragFloat("Panorama Yaw", &environment.sky.panoramaYawOffset, 0.5f, -360.0f, 360.0f, "%.1f"));

        if (ImGui::TreeNodeEx("Cubemap", ImGuiTreeNodeFlags_DefaultOpen)) {
            static constexpr std::array<const char*, 6> kCubemapLabels{
                "Face +X",
                "Face -X",
                "Face +Y",
                "Face -Y",
                "Face +Z",
                "Face -Z",
            };
            static constexpr std::array<const char*, 6> kCubemapCommandLabels{
                "Set Cubemap Face +X",
                "Set Cubemap Face -X",
                "Set Cubemap Face +Y",
                "Set Cubemap Face -Y",
                "Set Cubemap Face +Z",
                "Set Cubemap Face -Z",
            };
            for (std::size_t i = 0; i < kCubemapLabels.size(); ++i) {
                beforeState = document.captureState();
                trackEnvItem(beforeState,
                             kCubemapCommandLabels[i],
                             editString(kCubemapLabels[i],
                                        environment.sky.cubemapFacePaths[i],
                                        "assets/skies/cubemap_face.png"));
            }
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Cubemap Tint", editColor("Cubemap Tint", environment.sky.cubemapTint));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Cubemap Strength", ImGui::DragFloat("Cubemap Strength", &environment.sky.cubemapStrength, 0.01f, 0.0f, 2.0f, "%.2f"));
            ImGui::TreePop();
        }

        // --- Clouds ---
        ImGui::SeparatorText("Clouds");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Cloud Layer A", editString("Cloud Layer A", environment.sky.cloudLayerAPath, "assets/skies/clouds_a.png"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Cloud Layer B", editString("Cloud Layer B", environment.sky.cloudLayerBPath, "assets/skies/clouds_b.png"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Cloud Tint", editColor("Cloud Tint", environment.sky.cloudTint));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Scale", ImGui::DragFloat("Cloud Scale", &environment.sky.cloudScale, 0.01f, 0.1f, 4.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Speed", ImGui::DragFloat("Cloud Speed", &environment.sky.cloudSpeed, 0.0002f, 0.0f, 0.05f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Coverage", ImGui::DragFloat("Cloud Coverage", &environment.sky.cloudCoverage, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Parallax", ImGui::DragFloat("Cloud Parallax", &environment.sky.cloudParallax, 0.001f, 0.0f, 0.20f, "%.3f"));

        // --- Horizon ---
        ImGui::SeparatorText("Horizon");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Horizon Layer", editString("Horizon Layer", environment.sky.horizonLayerPath, "assets/skies/horizon.png"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Horizon Tint", editColor("Horizon Tint", environment.sky.horizonTint));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Horizon Height", ImGui::DragFloat("Horizon Height", &environment.sky.horizonHeight, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Horizon Fade", ImGui::DragFloat("Horizon Fade", &environment.sky.horizonFade, 0.01f, 0.0f, 1.0f, "%.2f"));
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto trackEnvItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
            if (changed) {
                document.markEnvironmentDirty();
            }
            trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
        };

        auto beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Hemisphere Sky Color", editColor("Hemi Sky", environment.lighting.hemisphereSkyColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Hemisphere Ground Color", editColor("Hemi Ground", environment.lighting.hemisphereGroundColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Hemisphere Strength", ImGui::DragFloat("Hemi Strength", &environment.lighting.hemisphereStrength, 0.01f, 0.0f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Directional Lighting", ImGui::Checkbox("Enable Directional", &environment.lighting.enableDirectionalLights));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Environment Shadows", ImGui::Checkbox("Enable Shadows", &environment.lighting.enableShadows));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Shadow Bias", ImGui::DragFloat("Shadow Bias", &environment.lighting.shadowBias, 0.0001f, 0.0001f, 0.02f, "%.4f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Shadow Normal Bias", ImGui::DragFloat("Shadow Normal Bias", &environment.lighting.shadowNormalBias, 0.001f, 0.0f, 0.20f, "%.3f"));
        if (ImGui::TreeNodeEx("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Toggle Sun Light", ImGui::Checkbox("Sun Enabled", &environment.lighting.sun.enabled));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Sun Direction", editVec3("Sun Dir##slot", environment.lighting.sun.direction, 0.01f));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Sun Color", editColor("Sun Color##slot", environment.lighting.sun.color));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Sun Intensity", ImGui::DragFloat("Sun Intensity##slot", &environment.lighting.sun.intensity, 0.01f, 0.0f, 4.0f, "%.2f"));
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Toggle Fill Light", ImGui::Checkbox("Fill Enabled", &environment.lighting.fill.enabled));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Fill Direction", editVec3("Fill Dir##slot", environment.lighting.fill.direction, 0.01f));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Fill Color", editColor("Fill Color##slot", environment.lighting.fill.color));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Fill Intensity", ImGui::DragFloat("Fill Intensity##slot", &environment.lighting.fill.intensity, 0.01f, 0.0f, 4.0f, "%.2f"));
            ImGui::TreePop();
        }
    }

    ImGui::End();
    return contentReloaded;
}
