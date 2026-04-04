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
    if (!beginCompactEditorPanelWindow("Environment", open)) {
        ImGui::End();
        return contentReloaded;
    }

    ImGui::SetNextItemWidth(180.0f);
    if (beginInspectorPropertyTable("EnvironmentPanelHeader")) {
        renderInspectorPropertyRow("Environment", [&]() {
            if (ImGui::BeginCombo("##value", environment.id.c_str())) {
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
            return false;
        }, EditorInspectorFieldKind::Enum);
        endInspectorPropertyTable();
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
        if (beginInspectorPropertyTable("EnvironmentSaveAsPopup")) {
            renderInspectorPropertyRow("Profile Id", [&]() { return ImGui::InputText("##value", saveAsProfileBuffer, sizeof(saveAsProfileBuffer)); });
            endInspectorPropertyTable();
        }
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

        if (!beginInspectorPropertyTable("EnvironmentPanelPost")) {
            ImGui::End();
            return contentReloaded;
        }

        // --- Tone Mapping ---
        ImGui::SeparatorText("Tone Mapping");
        auto beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Tone Map", renderInspectorPropertyRow("Enable Tone Map", [&]() { return ImGui::Checkbox("##value", &environment.post.enableToneMap); }));
        if (environment.post.enableToneMap) {
            beforeState = document.captureState();
            int toneMapMode = environment.post.toneMapMode;
            const bool toneMapChanged = renderInspectorPropertyRow("Tone Mapper", [&]() { return ImGui::Combo("##value", &toneMapMode, kToneMapModes, 2); },
                                                                   EditorInspectorFieldKind::Enum);
            if (toneMapChanged) {
                environment.post.toneMapMode = toneMapMode;
            }
            trackEnvItem(beforeState, "Change Tone Mapper", toneMapChanged);
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Exposure", renderInspectorPropertyRow("Exposure", [&]() { return ImGui::DragFloat("##value", &environment.post.exposure, 0.01f, 0.2f, 2.5f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Gamma", renderInspectorPropertyRow("Gamma", [&]() { return ImGui::DragFloat("##value", &environment.post.gamma, 0.01f, 0.5f, 2.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Contrast", renderInspectorPropertyRow("Contrast", [&]() { return ImGui::DragFloat("##value", &environment.post.contrast, 0.01f, 0.4f, 2.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Saturation", renderInspectorPropertyRow("Saturation", [&]() { return ImGui::DragFloat("##value", &environment.post.saturation, 0.01f, 0.0f, 1.5f, "%.2f"); }));
        }

        // --- Bloom ---
        ImGui::SeparatorText("Bloom");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Bloom", renderInspectorPropertyRow("Enable Bloom", [&]() { return ImGui::Checkbox("##value", &environment.post.enableBloom); }));
        if (environment.post.enableBloom) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Bloom Threshold", renderInspectorPropertyRow("Bloom Threshold", [&]() { return ImGui::DragFloat("##value", &environment.post.bloomThreshold, 0.01f, 0.0f, 1.5f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Bloom Intensity", renderInspectorPropertyRow("Bloom Intensity", [&]() { return ImGui::DragFloat("##value", &environment.post.bloomIntensity, 0.01f, 0.0f, 1.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Bloom Radius", renderInspectorPropertyRow("Bloom Radius", [&]() { return ImGui::DragFloat("##value", &environment.post.bloomRadius, 0.01f, 0.5f, 5.0f, "%.2f"); }));
        }

        // --- SSAO ---
        ImGui::SeparatorText("SSAO");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle SSAO", renderInspectorPropertyRow("Enable SSAO", [&]() { return ImGui::Checkbox("##value", &environment.post.enableSsao); }));
        if (environment.post.enableSsao) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Toggle SSAO Half Resolution", renderInspectorPropertyRow("AO Half Resolution", [&]() { return ImGui::Checkbox("##value", &environment.post.ssaoHalfResolution); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust SSAO Radius", renderInspectorPropertyRow("AO Radius", [&]() { return ImGui::DragFloat("##value", &environment.post.ssaoRadius, 0.01f, 0.1f, 2.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust SSAO Bias", renderInspectorPropertyRow("AO Bias", [&]() { return ImGui::DragFloat("##value", &environment.post.ssaoBias, 0.001f, 0.001f, 0.1f, "%.3f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust SSAO Strength", renderInspectorPropertyRow("AO Strength", [&]() { return ImGui::DragFloat("##value", &environment.post.ssaoStrength, 0.01f, 0.0f, 2.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust SSAO Fade Start", renderInspectorPropertyRow("AO Fade Start", [&]() { return ImGui::DragFloat("##value", &environment.post.ssaoFadeStart, 0.1f, 0.0f, 120.0f, "%.1f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust SSAO Fade End", renderInspectorPropertyRow("AO Fade End", [&]() { return ImGui::DragFloat("##value", &environment.post.ssaoFadeEnd, 0.1f, 0.0f, 160.0f, "%.1f"); }));
        }

        // --- Fog ---
        ImGui::SeparatorText("Fog");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Fog", renderInspectorPropertyRow("Enable Fog", [&]() { return ImGui::Checkbox("##value", &environment.post.enableFog); }));
        if (environment.post.enableFog) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Fog Density", renderInspectorPropertyRow("Fog Density", [&]() { return ImGui::DragFloat("##value", &environment.post.fogDensity, 0.001f, 0.0f, 0.5f, "%.3f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Fog Start", renderInspectorPropertyRow("Fog Start", [&]() { return ImGui::DragFloat("##value", &environment.post.fogStart, 0.1f, 0.0f, 80.0f, "%.1f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Depth View Scale", renderInspectorPropertyRow("Depth View Scale", [&]() { return ImGui::DragFloat("##value", &environment.post.depthViewScale, 0.001f, 0.01f, 0.30f, "%.3f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Fog Near Color", renderInspectorPropertyRow("Fog Near", [&]() { return editColor("##value", environment.post.fogNearColor); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Fog Far Color", renderInspectorPropertyRow("Fog Far", [&]() { return editColor("##value", environment.post.fogFarColor); }));
        }

        // --- Shadows ---
        ImGui::SeparatorText("Shadows");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust CSM Split Blend", renderInspectorPropertyRow("CSM Split Blend", [&]() { return ImGui::DragFloat("##value", &environment.post.csmLambda, 0.01f, 0.0f, 1.0f, "%.2f"); }));
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("0 = uniform linear splits\n1 = fully logarithmic splits (more detail near camera)");
        }

        // --- Edges ---
        ImGui::SeparatorText("Edges");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Edges", renderInspectorPropertyRow("Enable Edges", [&]() { return ImGui::Checkbox("##value", &environment.post.enableEdges); }));
        if (environment.post.enableEdges) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Edge Threshold", renderInspectorPropertyRow("Edge Threshold", [&]() { return ImGui::DragFloat("##value", &environment.post.edgeThreshold, 0.005f, 0.0f, 1.0f, "%.3f"); }));
        }

        // --- Color Grading ---
        ImGui::SeparatorText("Color Grading");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Split Tone Strength", renderInspectorPropertyRow("Split Strength", [&]() { return ImGui::DragFloat("##value", &environment.post.splitToneStrength, 0.01f, 0.0f, 0.6f, "%.2f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Split Tone Balance", renderInspectorPropertyRow("Split Balance", [&]() { return ImGui::DragFloat("##value", &environment.post.splitToneBalance, 0.01f, 0.15f, 0.85f, "%.2f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Shadow Tint", renderInspectorPropertyRow("Shadow Tint", [&]() { return editColor("##value", environment.post.shadowTint); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Highlight Tint", renderInspectorPropertyRow("Highlight Tint", [&]() { return editColor("##value", environment.post.highlightTint); }));

        // --- Effects ---
        ImGui::SeparatorText("Effects");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Vignette", renderInspectorPropertyRow("Enable Vignette", [&]() { return ImGui::Checkbox("##value", &environment.post.enableVignette); }));
        if (environment.post.enableVignette) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Vignette Strength", renderInspectorPropertyRow("Vignette Strength", [&]() { return ImGui::DragFloat("##value", &environment.post.vignetteStrength, 0.01f, 0.0f, 1.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Vignette Softness", renderInspectorPropertyRow("Vignette Softness", [&]() { return ImGui::DragFloat("##value", &environment.post.vignetteSoftness, 0.01f, 0.1f, 1.2f, "%.2f"); }));
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Grain", renderInspectorPropertyRow("Enable Grain", [&]() { return ImGui::Checkbox("##value", &environment.post.enableGrain); }));
        if (environment.post.enableGrain) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Grain Amount", renderInspectorPropertyRow("Grain Amount", [&]() { return ImGui::DragFloat("##value", &environment.post.grainAmount, 0.001f, 0.0f, 0.2f, "%.3f"); }));
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Scanlines", renderInspectorPropertyRow("Enable Scanlines", [&]() { return ImGui::Checkbox("##value", &environment.post.enableScanlines); }));
        if (environment.post.enableScanlines) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Scanline Amount", renderInspectorPropertyRow("Scanline Amount", [&]() { return ImGui::DragFloat("##value", &environment.post.scanlineAmount, 0.01f, 0.0f, 0.35f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Scanline Density", renderInspectorPropertyRow("Scanline Density", [&]() { return ImGui::DragFloat("##value", &environment.post.scanlineDensity, 0.01f, 0.5f, 3.0f, "%.2f"); }));
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sharpen", renderInspectorPropertyRow("Enable Sharpen", [&]() { return ImGui::Checkbox("##value", &environment.post.enableSharpen); }));
        if (environment.post.enableSharpen) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Sharpen Amount", renderInspectorPropertyRow("Sharpen Amount", [&]() { return ImGui::DragFloat("##value", &environment.post.sharpenAmount, 0.01f, 0.0f, 1.0f, "%.2f"); }));
        }

        endInspectorPropertyTable();
    }

    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto trackEnvItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
            if (changed) {
                document.markEnvironmentDirty();
            }
            trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
        };

        if (!beginInspectorPropertyTable("EnvironmentPanelSkyBase")) {
            ImGui::End();
            return contentReloaded;
        }

        auto beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sky System", renderInspectorPropertyRow("Sky Enabled", [&]() { return ImGui::Checkbox("##value", &environment.sky.enabled); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sky In Post", renderInspectorPropertyRow("Show Sky", [&]() { return ImGui::Checkbox("##value", &environment.post.enableSky); }));

        // --- Sky Colors ---
        ImGui::SeparatorText("Sky Colors");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Zenith Color", renderInspectorPropertyRow("Zenith", [&]() { return editColor("##value", environment.sky.zenithColor); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Horizon Color", renderInspectorPropertyRow("Horizon", [&]() { return editColor("##value", environment.sky.horizonColor); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Ground Haze Color", renderInspectorPropertyRow("Ground Haze", [&]() { return editColor("##value", environment.sky.groundHazeColor); }));

        // --- Sun ---
        ImGui::SeparatorText("Sun");
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Direction", renderInspectorPropertyRow("Sun Direction", [&]() { return editVec3("##value", environment.sky.sunDirection, 0.01f); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Sky Sun Color", renderInspectorPropertyRow("Sun Color", [&]() { return editColor("##value", environment.sky.sunColor); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Size", renderInspectorPropertyRow("Sun Size", [&]() { return ImGui::DragFloat("##value", &environment.sky.sunSize, 0.001f, 0.001f, 0.10f, "%.3f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Glow", renderInspectorPropertyRow("Sun Glow", [&]() { return ImGui::DragFloat("##value", &environment.sky.sunGlow, 0.01f, 0.0f, 2.0f, "%.2f"); }));
        endInspectorPropertyTable();

        // --- Moon ---
        ImGui::SeparatorText("Moon");
        if (!beginInspectorPropertyTable("EnvironmentPanelSkyMoon")) {
            ImGui::End();
            return contentReloaded;
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Moon", renderInspectorPropertyRow("Moon Enabled", [&]() { return ImGui::Checkbox("##value", &environment.sky.moonEnabled); }));
        if (environment.sky.moonEnabled) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Moon Direction", renderInspectorPropertyRow("Moon Direction", [&]() { return editVec3("##value", environment.sky.moonDirection, 0.01f); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Moon Color", renderInspectorPropertyRow("Moon Color", [&]() { return editColor("##value", environment.sky.moonColor); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Moon Size", renderInspectorPropertyRow("Moon Size", [&]() { return ImGui::DragFloat("##value", &environment.sky.moonSize, 0.001f, 0.001f, 0.10f, "%.3f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Moon Glow", renderInspectorPropertyRow("Moon Glow", [&]() { return ImGui::DragFloat("##value", &environment.sky.moonGlow, 0.01f, 0.0f, 1.0f, "%.2f"); }));
        }
        endInspectorPropertyTable();

        // --- Panorama ---
        ImGui::SeparatorText("Panorama");
        if (!beginInspectorPropertyTable("EnvironmentPanelSkyPanorama")) {
            ImGui::End();
            return contentReloaded;
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Panorama Path", renderInspectorPropertyRow("Panorama Path", [&]() { return editString("##value", environment.sky.panoramaPath, "assets/skies/sky.jpg"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Panorama Tint", renderInspectorPropertyRow("Panorama Tint", [&]() { return editColor("##value", environment.sky.panoramaTint); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Panorama Strength", renderInspectorPropertyRow("Panorama Strength", [&]() { return ImGui::DragFloat("##value", &environment.sky.panoramaStrength, 0.01f, 0.0f, 2.0f, "%.2f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Panorama Yaw", renderInspectorPropertyRow("Panorama Yaw", [&]() { return ImGui::DragFloat("##value", &environment.sky.panoramaYawOffset, 0.5f, -360.0f, 360.0f, "%.1f"); }));
        endInspectorPropertyTable();

        if (ImGui::TreeNodeEx("Cubemap", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!beginInspectorPropertyTable("EnvironmentPanelSkyCubemap")) {
                ImGui::End();
                return contentReloaded;
            }
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
                             renderInspectorPropertyRow(kCubemapLabels[i], [&]() {
                                 return editString("##value",
                                                   environment.sky.cubemapFacePaths[i],
                                                   "assets/skies/cubemap_face.png");
                             }));
            }
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Cubemap Tint", renderInspectorPropertyRow("Cubemap Tint", [&]() { return editColor("##value", environment.sky.cubemapTint); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Cubemap Strength", renderInspectorPropertyRow("Cubemap Strength", [&]() { return ImGui::DragFloat("##value", &environment.sky.cubemapStrength, 0.01f, 0.0f, 2.0f, "%.2f"); }));
            endInspectorPropertyTable();
            ImGui::TreePop();
        }

        // --- Clouds ---
        ImGui::SeparatorText("Clouds");
        if (!beginInspectorPropertyTable("EnvironmentPanelSkyClouds")) {
            ImGui::End();
            return contentReloaded;
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Cloud Layer A", renderInspectorPropertyRow("Cloud Layer A", [&]() { return editString("##value", environment.sky.cloudLayerAPath, "assets/skies/clouds_a.png"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Cloud Layer B", renderInspectorPropertyRow("Cloud Layer B", [&]() { return editString("##value", environment.sky.cloudLayerBPath, "assets/skies/clouds_b.png"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Cloud Tint", renderInspectorPropertyRow("Cloud Tint", [&]() { return editColor("##value", environment.sky.cloudTint); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Scale", renderInspectorPropertyRow("Cloud Scale", [&]() { return ImGui::DragFloat("##value", &environment.sky.cloudScale, 0.01f, 0.1f, 4.0f, "%.2f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Speed", renderInspectorPropertyRow("Cloud Speed", [&]() { return ImGui::DragFloat("##value", &environment.sky.cloudSpeed, 0.0002f, 0.0f, 0.05f, "%.3f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Coverage", renderInspectorPropertyRow("Cloud Coverage", [&]() { return ImGui::DragFloat("##value", &environment.sky.cloudCoverage, 0.01f, 0.0f, 1.0f, "%.2f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Parallax", renderInspectorPropertyRow("Cloud Parallax", [&]() { return ImGui::DragFloat("##value", &environment.sky.cloudParallax, 0.001f, 0.0f, 0.20f, "%.3f"); }));
        endInspectorPropertyTable();

        // --- Horizon ---
        ImGui::SeparatorText("Horizon");
        if (!beginInspectorPropertyTable("EnvironmentPanelSkyHorizon")) {
            ImGui::End();
            return contentReloaded;
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Horizon Layer", renderInspectorPropertyRow("Horizon Layer", [&]() { return editString("##value", environment.sky.horizonLayerPath, "assets/skies/horizon.png"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Horizon Tint", renderInspectorPropertyRow("Horizon Tint", [&]() { return editColor("##value", environment.sky.horizonTint); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Horizon Height", renderInspectorPropertyRow("Horizon Height", [&]() { return ImGui::DragFloat("##value", &environment.sky.horizonHeight, 0.01f, 0.0f, 1.0f, "%.2f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Horizon Fade", renderInspectorPropertyRow("Horizon Fade", [&]() { return ImGui::DragFloat("##value", &environment.sky.horizonFade, 0.01f, 0.0f, 1.0f, "%.2f"); }));
        endInspectorPropertyTable();
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto trackEnvItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
            if (changed) {
                document.markEnvironmentDirty();
            }
            trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
        };

        if (!beginInspectorPropertyTable("EnvironmentPanelLightingBase")) {
            ImGui::End();
            return contentReloaded;
        }

        auto beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Hemisphere Sky Color", renderInspectorPropertyRow("Hemi Sky", [&]() { return editColor("##value", environment.lighting.hemisphereSkyColor); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Hemisphere Ground Color", renderInspectorPropertyRow("Hemi Ground", [&]() { return editColor("##value", environment.lighting.hemisphereGroundColor); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Hemisphere Strength", renderInspectorPropertyRow("Hemi Strength", [&]() { return ImGui::DragFloat("##value", &environment.lighting.hemisphereStrength, 0.01f, 0.0f, 2.0f, "%.2f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Directional Lighting", renderInspectorPropertyRow("Enable Directional", [&]() { return ImGui::Checkbox("##value", &environment.lighting.enableDirectionalLights); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Environment Shadows", renderInspectorPropertyRow("Enable Shadows", [&]() { return ImGui::Checkbox("##value", &environment.lighting.enableShadows); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Shadow Bias", renderInspectorPropertyRow("Shadow Bias", [&]() { return ImGui::DragFloat("##value", &environment.lighting.shadowBias, 0.0001f, 0.0001f, 0.02f, "%.4f"); }));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Shadow Normal Bias", renderInspectorPropertyRow("Shadow Normal Bias", [&]() { return ImGui::DragFloat("##value", &environment.lighting.shadowNormalBias, 0.001f, 0.0f, 0.20f, "%.3f"); }));
        endInspectorPropertyTable();
        if (ImGui::TreeNodeEx("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!beginInspectorPropertyTable("EnvironmentPanelLightingSun")) {
                ImGui::End();
                return contentReloaded;
            }
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Toggle Sun Light", renderInspectorPropertyRow("Sun Enabled", [&]() { return ImGui::Checkbox("##value", &environment.lighting.sun.enabled); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Sun Direction", renderInspectorPropertyRow("Sun Dir", [&]() { return editVec3("##value", environment.lighting.sun.direction, 0.01f); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Sun Color", renderInspectorPropertyRow("Sun Color", [&]() { return editColor("##value", environment.lighting.sun.color); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Sun Intensity", renderInspectorPropertyRow("Sun Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.sun.intensity, 0.01f, 0.0f, 4.0f, "%.2f"); }));
            endInspectorPropertyTable();
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!beginInspectorPropertyTable("EnvironmentPanelLightingFill")) {
                ImGui::End();
                return contentReloaded;
            }
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Toggle Fill Light", renderInspectorPropertyRow("Fill Enabled", [&]() { return ImGui::Checkbox("##value", &environment.lighting.fill.enabled); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Fill Direction", renderInspectorPropertyRow("Fill Dir", [&]() { return editVec3("##value", environment.lighting.fill.direction, 0.01f); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Fill Color", renderInspectorPropertyRow("Fill Color", [&]() { return editColor("##value", environment.lighting.fill.color); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Fill Intensity", renderInspectorPropertyRow("Fill Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.fill.intensity, 0.01f, 0.0f, 4.0f, "%.2f"); }));
            endInspectorPropertyTable();
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Player Torch", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (!beginInspectorPropertyTable("EnvironmentPanelLightingTorch")) {
                ImGui::End();
                return contentReloaded;
            }
            // Enable + master
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Toggle Torch", renderInspectorPropertyRow("Torch Enabled", [&]() { return ImGui::Checkbox("##value", &environment.lighting.torch.enabled); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Torch Master Intensity", renderInspectorPropertyRow("Master Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.masterIntensity, 0.01f, 0.0f, 3.0f, "%.2f"); }));

            // Main spotlight
            ImGui::SeparatorText("Spotlight");
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Torch Color", renderInspectorPropertyRow("Torch Color", [&]() { return editColor("##value", environment.lighting.torch.torchColor); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Torch Intensity", renderInspectorPropertyRow("Torch Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.torchIntensity, 0.01f, 0.0f, 2.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Torch Radius", renderInspectorPropertyRow("Torch Radius", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.torchRadius, 0.1f, 0.5f, 20.0f, "%.1f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Torch Inner Cone", renderInspectorPropertyRow("Inner Cone", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.torchInnerConeDegrees, 0.5f, 5.0f, 90.0f, "%.1f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Torch Outer Cone", renderInspectorPropertyRow("Outer Cone", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.torchOuterConeDegrees, 0.5f, 10.0f, 120.0f, "%.1f"); }));

            // Spill
            ImGui::SeparatorText("Spill");
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Spill Color", renderInspectorPropertyRow("Spill Color", [&]() { return editColor("##value", environment.lighting.torch.spillColor); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Spill Intensity", renderInspectorPropertyRow("Spill Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.spillIntensity, 0.01f, 0.0f, 5.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Spill Radius", renderInspectorPropertyRow("Spill Radius", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.spillRadius, 0.1f, 0.5f, 20.0f, "%.1f"); }));

            // Halo
            ImGui::SeparatorText("Halo");
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Halo Color", renderInspectorPropertyRow("Halo Color", [&]() { return editColor("##value", environment.lighting.torch.haloColor); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Halo Intensity", renderInspectorPropertyRow("Halo Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.haloIntensity, 0.01f, 0.0f, 5.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Halo Radius", renderInspectorPropertyRow("Halo Radius", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.haloRadius, 0.1f, 0.5f, 20.0f, "%.1f"); }));

            // Hand glow
            ImGui::SeparatorText("Hand Glow");
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Hand Glow Color", renderInspectorPropertyRow("Glow Color", [&]() { return editColor("##value", environment.lighting.torch.handGlowColor); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Hand Glow Intensity", renderInspectorPropertyRow("Glow Intensity", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.handGlowIntensity, 0.01f, 0.0f, 1.0f, "%.2f"); }));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Hand Glow Radius", renderInspectorPropertyRow("Glow Radius", [&]() { return ImGui::DragFloat("##value", &environment.lighting.torch.handGlowRadius, 0.01f, 0.1f, 5.0f, "%.2f"); }));

            endInspectorPropertyTable();
            ImGui::TreePop();
        }
    }

    ImGui::End();
    return contentReloaded;
}
