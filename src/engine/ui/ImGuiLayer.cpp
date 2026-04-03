#include "ImGuiLayer.h"
#include "engine/core/PathUtils.h"
#include "engine/rendering/geometry/Renderer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <array>
#include <filesystem>
#include <string_view>

namespace {

struct FontPresetDefinition {
    std::string_view label;
    std::array<const char*, 3> candidates;
    float size = 17.0f;
    float rasterizerMultiply = 1.05f;
};

const FontPresetDefinition& fontPresetDefinition(ImGuiFontPreset preset) {
    static const FontPresetDefinition kSystemSans{
        "System Sans",
        {
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/HelveticaNeue.ttc",
            "/System/Library/Fonts/Helvetica.ttc",
        },
        17.0f,
        1.05f,
    };
    static const FontPresetDefinition kVerdana{
        "Verdana",
        {
            "/System/Library/Fonts/Supplemental/Verdana.ttf",
            "/System/Library/Fonts/Supplemental/Arial.ttf",
            "/System/Library/Fonts/SFNS.ttf",
        },
        16.5f,
        1.02f,
    };
    static const FontPresetDefinition kAvenirNext{
        "Avenir Next",
        {
            "/System/Library/Fonts/Supplemental/Avenir Next.ttc",
            "/System/Library/Fonts/Supplemental/Avenir.ttc",
            "/System/Library/Fonts/SFNS.ttf",
        },
        17.0f,
        1.06f,
    };
    static const FontPresetDefinition kHelveticaNeue{
        "Helvetica Neue",
        {
            "/System/Library/Fonts/HelveticaNeue.ttc",
            "/System/Library/Fonts/Helvetica.ttc",
            "/System/Library/Fonts/SFNS.ttf",
        },
        17.0f,
        1.05f,
    };
    static const FontPresetDefinition kTrebuchet{
        "Trebuchet MS",
        {
            "/System/Library/Fonts/Supplemental/Trebuchet MS.ttf",
            "/System/Library/Fonts/Supplemental/Tahoma.ttf",
            "/System/Library/Fonts/SFNS.ttf",
        },
        16.75f,
        1.03f,
    };
    static const FontPresetDefinition kInterUnity{
        "Inter (Unity)",
        {
            "assets/fonts/editor/Inter-Variable.ttf",
            "/System/Library/Fonts/SFNS.ttf",
            "/System/Library/Fonts/HelveticaNeue.ttc",
        },
        17.0f,
        1.05f,
    };
    static const FontPresetDefinition kRobotoUnreal{
        "Roboto (Unreal)",
        {
            "assets/fonts/editor/Roboto-Variable.ttf",
            "/System/Library/Fonts/HelveticaNeue.ttc",
            "/System/Library/Fonts/SFNS.ttf",
        },
        17.0f,
        1.04f,
    };
    static const FontPresetDefinition kJetBrainsMonoGodot{
        "JetBrains Mono (Godot Code)",
        {
            "assets/fonts/editor/JetBrainsMono-Variable.ttf",
            "/System/Library/Fonts/SFNSMono.ttf",
            "/System/Library/Fonts/SFNS.ttf",
        },
        16.25f,
        1.02f,
    };

    switch (preset) {
    case ImGuiFontPreset::SystemSans:
        return kSystemSans;
    case ImGuiFontPreset::Verdana:
        return kVerdana;
    case ImGuiFontPreset::AvenirNext:
        return kAvenirNext;
    case ImGuiFontPreset::HelveticaNeue:
        return kHelveticaNeue;
    case ImGuiFontPreset::TrebuchetMS:
        return kTrebuchet;
    case ImGuiFontPreset::InterUnity:
        return kInterUnity;
    case ImGuiFontPreset::RobotoUnreal:
        return kRobotoUnreal;
    case ImGuiFontPreset::JetBrainsMonoGodot:
        return kJetBrainsMonoGodot;
    }

    return kSystemSans;
}

bool configureFontPreset(ImGuiIO& io, ImGuiFontPreset preset, std::string& loadedFontPath) {
    const auto& definition = fontPresetDefinition(preset);

    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    fontConfig.RasterizerMultiply = definition.rasterizerMultiply;

    io.Fonts->Clear();

    for (const char* fontPath : definition.candidates) {
        const std::string resolvedFontPath = resolveProjectPath(fontPath);
        if (!std::filesystem::exists(resolvedFontPath)) {
            continue;
        }

        if (ImFont* font = io.Fonts->AddFontFromFileTTF(resolvedFontPath.c_str(), definition.size, &fontConfig)) {
            io.FontDefault = font;
            loadedFontPath = resolvedFontPath;
            return true;
        }
    }

    io.FontDefault = io.Fonts->AddFontDefault(&fontConfig);
    loadedFontPath.clear();
    return false;
}

} // namespace

const char* imguiFontPresetLabel(ImGuiFontPreset preset) {
    return fontPresetDefinition(preset).label.data();
}

void ImGuiLayer::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410 core");
    initialized_ = true;

    spdlog::info("ImGuiLayer initialized");
}

void ImGuiLayer::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
    spdlog::info("ImGuiLayer shutdown");
}

void ImGuiLayer::requestFontPreset(ImGuiFontPreset preset) {
    if (preset == fontPreset_) {
        return;
    }
    pendingFontPreset_ = preset;
}

void ImGuiLayer::applyPendingFontPreset() {
    if (!pendingFontPreset_.has_value() && initialized_) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const ImGuiFontPreset targetPreset = pendingFontPreset_.value_or(fontPreset_);
    std::string loadedFontPath;
    const bool loaded = configureFontPreset(io, targetPreset, loadedFontPath);

    fontPreset_ = targetPreset;
    pendingFontPreset_.reset();
    activeFontPath_ = loadedFontPath;

    if (loaded) {
        spdlog::info("Loaded ImGui font preset '{}' from '{}'",
                     imguiFontPresetLabel(fontPreset_),
                     activeFontPath_);
    } else {
        spdlog::warn("Falling back to ImGui default font for preset '{}'",
                     imguiFontPresetLabel(fontPreset_));
    }
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    applyPendingFontPreset();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::renderOverlay(DebugParams& params, std::vector<RenderLight>& lights) {
    ImGui::Begin("Debug Overlay");

    if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* viewModes[] = {
            "Final",
            "Scene Color",
            "Normals",
            "Depth",
            "Sky",
            "Sun Direct",
            "Sun Shadow",
            "CSM UV Bounds",
            "Cascade Index",
        };
        const char* toneMapModes[] = {"Linear", "ACES Fitted"};
        ImGui::Combo("View Mode", &params.post.debugViewMode, viewModes, 9);
        ImGui::Checkbox("Enable Sky", &params.post.enableSky);
        ImGui::Checkbox("Enable Edges", &params.post.enableEdges);
        ImGui::Checkbox("Enable Fog", &params.post.enableFog);
        ImGui::Checkbox("Enable Tone Map", &params.post.enableToneMap);
        ImGui::Checkbox("Enable Bloom", &params.post.enableBloom);
        ImGui::Checkbox("Enable Vignette", &params.post.enableVignette);
        ImGui::Checkbox("Enable Grain", &params.post.enableGrain);
        ImGui::Checkbox("Enable Scanlines", &params.post.enableScanlines);
        ImGui::Checkbox("Enable Sharpen", &params.post.enableSharpen);
        ImGui::Combo("Tone Mapper", &params.post.toneMapMode, toneMapModes, 2);
    }

    // ------------------------------------------------------------------
    // Display section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (params.post.debugViewMode == 3) {
            ImGui::SliderFloat("Depth View Scale", &params.post.depthViewScale, 0.01f, 0.30f, "%.3f");
        }

        const char* resOptions[] = {"480p (854x480)", "540p (960x540)", "720p (1280x720)"};
        int prevIndex = params.internalResIndex;
        ImGui::Combo("Resolution", &params.internalResIndex, resOptions, 3);
        if (params.internalResIndex != prevIndex) {
            params.resolutionChanged = true;
        }
    }

    // ------------------------------------------------------------------
    // Post-Process section (edges + fog)
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Post-Process", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!params.post.enableEdges);
        ImGui::SliderFloat("Edge Threshold", &params.post.edgeThreshold, 0.0f, 1.0f, "%.2f");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!params.post.enableFog);
        ImGui::SliderFloat("Fog Density", &params.post.fogDensity, 0.0f, 0.5f, "%.3f");
        ImGui::SliderFloat("Fog Start", &params.post.fogStart, 0.0f, 20.0f, "%.1f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Color Grading", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!params.post.enableToneMap);
        ImGui::SliderFloat("Exposure", &params.post.exposure, 0.2f, 2.5f, "%.2f");
        ImGui::SliderFloat("Gamma", &params.post.gamma, 0.6f, 1.8f, "%.2f");
        ImGui::EndDisabled();
        ImGui::SliderFloat("Contrast", &params.post.contrast, 0.5f, 1.8f, "%.2f");
        ImGui::SliderFloat("Saturation", &params.post.saturation, 0.0f, 1.5f, "%.2f");
        ImGui::SliderFloat("Split Strength", &params.post.splitToneStrength, 0.0f, 0.6f, "%.2f");
        ImGui::SliderFloat("Split Balance", &params.post.splitToneBalance, 0.15f, 0.85f, "%.2f");
        ImGui::ColorEdit3("Shadow Tint", &params.post.shadowTint.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit3("Highlight Tint", &params.post.highlightTint.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
    }

    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!params.post.enableSky);
        ImGui::SliderFloat("Sun Size", &params.post.sky.sunSize, 0.002f, 0.08f, "%.3f");
        ImGui::SliderFloat("Sun Glow", &params.post.sky.sunGlow, 0.0f, 1.2f, "%.2f");
        ImGui::SliderFloat("Cloud Scale", &params.post.sky.cloudScale, 0.25f, 4.0f, "%.2f");
        ImGui::SliderFloat("Cloud Speed", &params.post.sky.cloudSpeed, 0.0f, 0.04f, "%.3f");
        ImGui::SliderFloat("Cloud Coverage", &params.post.sky.cloudCoverage, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Cloud Parallax", &params.post.sky.cloudParallax, 0.0f, 0.20f, "%.3f");
        ImGui::SliderFloat("Horizon Fade", &params.post.sky.horizonFade, 0.02f, 0.60f, "%.2f");
        ImGui::ColorEdit3("Zenith Color", &params.post.sky.zenithColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit3("Horizon Color", &params.post.sky.horizonColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit3("Cloud Tint", &params.post.sky.cloudTint.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Glow", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!params.post.enableBloom);
        ImGui::SliderFloat("Bloom Threshold", &params.post.bloomThreshold, 0.1f, 1.2f, "%.2f");
        ImGui::SliderFloat("Bloom Intensity", &params.post.bloomIntensity, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Bloom Radius", &params.post.bloomRadius, 0.5f, 5.0f, "%.2f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Ambient Occlusion", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable SSAO", &params.post.enableSsao);
        ImGui::BeginDisabled(!params.post.enableSsao);
        ImGui::Checkbox("AO Half Resolution", &params.post.ssaoHalfResolution);
        ImGui::SliderFloat("AO Radius", &params.post.ssaoRadius, 0.1f, 2.0f, "%.2f");
        ImGui::SliderFloat("AO Bias", &params.post.ssaoBias, 0.001f, 0.1f, "%.3f");
        ImGui::SliderFloat("AO Strength", &params.post.ssaoStrength, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("AO Fade Start", &params.post.ssaoFadeStart, 0.0f, 120.0f, "%.1f");
        ImGui::SliderFloat("AO Fade End", &params.post.ssaoFadeEnd, 0.0f, 160.0f, "%.1f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Lens", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::BeginDisabled(!params.post.enableVignette);
        ImGui::SliderFloat("Vignette Strength", &params.post.vignetteStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Vignette Softness", &params.post.vignetteSoftness, 0.1f, 1.2f, "%.2f");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!params.post.enableGrain);
        ImGui::SliderFloat("Grain Amount", &params.post.grainAmount, 0.0f, 0.2f, "%.3f");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!params.post.enableScanlines);
        ImGui::SliderFloat("Scanline Amount", &params.post.scanlineAmount, 0.0f, 0.35f, "%.2f");
        ImGui::SliderFloat("Scanline Density", &params.post.scanlineDensity, 0.5f, 3.0f, "%.2f");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!params.post.enableSharpen);
        ImGui::SliderFloat("Sharpen Amount", &params.post.sharpenAmount, 0.0f, 1.0f, "%.2f");
        ImGui::EndDisabled();
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* shadowSizeOptions[] = {"512", "1024", "2048"};
        ImGui::Checkbox("Enable Shadows", &params.lighting.shadowsEnabled);
        ImGui::Combo("Shadow Map Size", &params.lighting.shadowMapResolutionIndex, shadowSizeOptions, 3);
        ImGui::SliderFloat("Shadow Bias", &params.lighting.shadowBias, 0.0001f, 0.02f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Shadow Normal Bias", &params.lighting.shadowNormalBias, 0.0f, 0.20f, "%.3f");
        ImGui::ColorEdit3("Hemi Sky", &params.lighting.hemisphereSkyColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit3("Hemi Ground", &params.lighting.hemisphereGroundColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::SliderFloat("Hemi Strength", &params.lighting.hemisphereStrength, 0.0f, 1.2f, "%.2f");
        ImGui::Checkbox("Enable Directional System", &params.lighting.enableDirectionalLights);
        ImGui::BeginDisabled(!params.lighting.enableDirectionalLights);
        if (ImGui::TreeNodeEx("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Sun Enabled", &params.lighting.sunDirectional.enabled);
            ImGui::ColorEdit3("Sun Color", &params.lighting.sunDirectional.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Sun Intensity", &params.lighting.sunDirectional.intensity, 0.0f, 3.0f, "%.2f");
            ImGui::DragFloat3("Sun Direction", &params.lighting.sunDirectional.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Fill Enabled", &params.lighting.fillDirectional.enabled);
            ImGui::BeginDisabled(!params.lighting.fillDirectional.enabled);
            ImGui::ColorEdit3("Fill Color", &params.lighting.fillDirectional.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Fill Intensity", &params.lighting.fillDirectional.intensity, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat3("Fill Direction", &params.lighting.fillDirectional.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
            ImGui::EndDisabled();
            ImGui::TreePop();
        }
        ImGui::EndDisabled();
        if (ImGui::TreeNodeEx("Player Torch", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Torch Enabled", &params.lighting.torch.enabled);
            ImGui::BeginDisabled(!params.lighting.torch.enabled);
            ImGui::SliderFloat("Master Intensity", &params.lighting.torch.masterIntensity, 0.0f, 3.0f, "%.2f");
            ImGui::Separator();

            ImGui::Text("Spotlight");
            ImGui::ColorEdit3("Torch Color", &params.lighting.torch.torchColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Torch Intensity", &params.lighting.torch.torchIntensity, 0.0f, 2.0f, "%.3f");
            ImGui::SliderFloat("Torch Radius", &params.lighting.torch.torchRadius, 0.5f, 15.0f, "%.1f");
            ImGui::SliderFloat("Inner Cone", &params.lighting.torch.torchInnerConeDegrees, 5.0f, 90.0f, "%.0f deg");
            ImGui::SliderFloat("Outer Cone", &params.lighting.torch.torchOuterConeDegrees, 10.0f, 120.0f, "%.0f deg");
            ImGui::Separator();

            ImGui::Text("Spill Light");
            ImGui::ColorEdit3("Spill Color", &params.lighting.torch.spillColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Spill Intensity", &params.lighting.torch.spillIntensity, 0.0f, 8.0f, "%.2f");
            ImGui::SliderFloat("Spill Radius", &params.lighting.torch.spillRadius, 1.0f, 20.0f, "%.1f");
            ImGui::Separator();

            ImGui::Text("Halo");
            ImGui::ColorEdit3("Halo Color", &params.lighting.torch.haloColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Halo Intensity", &params.lighting.torch.haloIntensity, 0.0f, 5.0f, "%.2f");
            ImGui::SliderFloat("Halo Radius", &params.lighting.torch.haloRadius, 1.0f, 15.0f, "%.1f");
            ImGui::Separator();

            ImGui::Text("Hand Glow");
            ImGui::ColorEdit3("Glow Color", &params.lighting.torch.handGlowColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Glow Intensity", &params.lighting.torch.handGlowIntensity, 0.0f, 0.5f, "%.3f");
            ImGui::SliderFloat("Glow Radius", &params.lighting.torch.handGlowRadius, 0.2f, 5.0f, "%.2f");

            ImGui::EndDisabled();
            ImGui::TreePop();
        }
    }

    // ------------------------------------------------------------------
    // Camera section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::Text("Position: %.1f, %.1f, %.1f",
                    params.camera.position.x, params.camera.position.y, params.camera.position.z);
        ImGui::Text("Direction: %.2f, %.2f, %.2f",
                    params.camera.direction.x, params.camera.direction.y, params.camera.direction.z);
        ImGui::SliderFloat("FOV", &params.camera.fov, 45.0f, 120.0f);
        ImGui::SliderFloat("Speed", &params.camera.moveSpeed, 0.5f, 10.0f);
    }

    // ------------------------------------------------------------------
    // Performance section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Performance")) {
        ImGui::Text("FPS: %.0f", params.fps);
        ImGui::Text("Frame: %.2f ms", params.frameTimeMs);
        ImGui::Text("Draw calls: %d", params.drawCalls);
    }

    // ------------------------------------------------------------------
    // Lights section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Lights")) {
        for (int i = 0; i < static_cast<int>(lights.size()); ++i) {
            ImGui::PushID(i);
            ImGui::Text("Light %d", i);
            const char* typeLabel = "Point";
            if (lights[i].type == LightType::Spot) {
                typeLabel = "Spot";
            } else if (lights[i].type == LightType::Directional) {
                typeLabel = "Directional";
            }
            ImGui::Text("Type: %s", typeLabel);
            if (lights[i].type != LightType::Directional) {
                ImGui::DragFloat3("Pos##", &lights[i].position.x, 0.1f);
            }
            ImGui::ColorEdit3("Color##", &lights[i].color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Intensity##", &lights[i].intensity, 0.0f, 2.0f);
            if (lights[i].type != LightType::Directional) {
                ImGui::SliderFloat("Radius##", &lights[i].radius, 1.0f, 20.0f);
            }
            if (lights[i].type != LightType::Point) {
                ImGui::DragFloat3("Dir##", &lights[i].direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
            }
            if (lights[i].type == LightType::Spot) {
                ImGui::SliderFloat("Inner##", &lights[i].innerConeDegrees, 5.0f, 60.0f, "%.1f");
                ImGui::SliderFloat("Outer##", &lights[i].outerConeDegrees, 8.0f, 75.0f, "%.1f");
                ImGui::Text("Shadowed: %s", lights[i].shadowIndex >= 0 ? "Yes" : "No");
            }
            ImGui::Separator();
            ImGui::PopID();
        }
    }

    ImGui::End();
}
