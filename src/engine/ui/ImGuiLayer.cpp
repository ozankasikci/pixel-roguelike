#include "ImGuiLayer.h"
#include "engine/rendering/geometry/Renderer.h"
#include "game/content/ContentRegistry.h"
#include "game/session/RunSession.h"
#include "game/ui/InventoryMenuState.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

void ImGuiLayer::init(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410 core");

    spdlog::info("ImGuiLayer initialized");
}

void ImGuiLayer::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    spdlog::info("ImGuiLayer shutdown");
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ImGuiLayer::renderOverlay(DebugParams& params, std::vector<RenderLight>& lights) {
    ImGui::Begin("Debug Overlay");

    if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* viewModes[] = {"Final", "Scene Color", "Normals", "Depth", "Sky"};
        const char* toneMapModes[] = {"Linear", "ACES Fitted"};
        ImGui::Combo("View Mode", &params.post.debugViewMode, viewModes, 5);
        ImGui::Checkbox("Enable Sky", &params.post.enableSky);
        ImGui::Checkbox("Enable Dither", &params.post.enableDither);
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
    // Dither section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Dither", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Threshold Bias", &params.post.thresholdBias, -0.5f, 0.5f);
        ImGui::SliderFloat("Pattern Scale", &params.post.patternScale, 0.0f, 512.0f, "%.1f");
        if (params.post.patternScale <= 1.0f) {
            ImGui::TextUnformatted("Pattern Scale: Auto (matches internal resolution)");
        }
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
        ImGui::Checkbox("Enable Shadows", &params.shadowsEnabled);
        ImGui::Combo("Shadow Map Size", &params.shadowMapResolutionIndex, shadowSizeOptions, 3);
        ImGui::SliderFloat("Shadow Bias", &params.shadowBias, 0.0001f, 0.02f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Shadow Normal Bias", &params.shadowNormalBias, 0.0f, 0.20f, "%.3f");
        ImGui::ColorEdit3("Hemi Sky", &params.hemisphereSkyColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit3("Hemi Ground", &params.hemisphereGroundColor.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
        ImGui::SliderFloat("Hemi Strength", &params.hemisphereStrength, 0.0f, 1.2f, "%.2f");
        ImGui::Checkbox("Enable Directional System", &params.enableDirectionalLights);
        ImGui::BeginDisabled(!params.enableDirectionalLights);
        if (ImGui::TreeNodeEx("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Sun Enabled", &params.sunDirectional.enabled);
            ImGui::ColorEdit3("Sun Color", &params.sunDirectional.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Sun Intensity", &params.sunDirectional.intensity, 0.0f, 3.0f, "%.2f");
            ImGui::DragFloat3("Sun Direction", &params.sunDirectional.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Fill Enabled", &params.fillDirectional.enabled);
            ImGui::BeginDisabled(!params.fillDirectional.enabled);
            ImGui::ColorEdit3("Fill Color", &params.fillDirectional.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
            ImGui::SliderFloat("Fill Intensity", &params.fillDirectional.intensity, 0.0f, 2.0f, "%.2f");
            ImGui::DragFloat3("Fill Direction", &params.fillDirectional.direction.x, 0.01f, -1.0f, 1.0f, "%.2f");
            ImGui::EndDisabled();
            ImGui::TreePop();
        }
        ImGui::EndDisabled();
        ImGui::SliderFloat("Torch Inner Cone", &params.playerTorchInnerConeDegrees, 5.0f, 50.0f, "%.1f");
        ImGui::SliderFloat("Torch Outer Cone", &params.playerTorchOuterConeDegrees, 8.0f, 65.0f, "%.1f");
    }

    // ------------------------------------------------------------------
    // Camera section
    // ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Camera")) {
        ImGui::Text("Position: %.1f, %.1f, %.1f",
                    params.cameraPos.x, params.cameraPos.y, params.cameraPos.z);
        ImGui::Text("Direction: %.2f, %.2f, %.2f",
                    params.cameraDir.x, params.cameraDir.y, params.cameraDir.z);
        ImGui::SliderFloat("FOV", &params.cameraFov, 45.0f, 120.0f);
        ImGui::SliderFloat("Speed", &params.cameraSpeed, 0.5f, 10.0f);
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

#include "game/components/PlayerMovementComponent.h"

void ImGuiLayer::renderMovementOverlay(PlayerMovementComponent& movement, bool grounded) {
    ImGui::Begin("Movement");

    // Ground state
    const char* stateStr = grounded ? "ON GROUND" : "IN AIR";
    ImGui::Text("State: %s", stateStr);
    ImGui::Text("Velocity: %.1f, %.1f, %.1f",
        movement.velocity.x, movement.velocity.y, movement.velocity.z);
    ImGui::Text("Speed: %.1f", glm::length(glm::vec2(movement.velocity.x, movement.velocity.z)));

    ImGui::Separator();

    // Ground movement
    if (ImGui::CollapsingHeader("Ground Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Max Speed", &movement.maxGroundSpeed, 1.0f, 15.0f);
        ImGui::SliderFloat("Acceleration", &movement.acceleration, 5.0f, 60.0f);
        ImGui::SliderFloat("Deceleration", &movement.deceleration, 5.0f, 60.0f);
    }

    // Air movement
    if (ImGui::CollapsingHeader("Air Movement")) {
        ImGui::SliderFloat("Air Accel", &movement.airAcceleration, 1.0f, 30.0f);
    }

    // Gravity & jump
    if (ImGui::CollapsingHeader("Jump & Gravity")) {
        ImGui::SliderFloat("Gravity", &movement.gravity, -30.0f, -5.0f);
        ImGui::SliderFloat("Jump Impulse", &movement.jumpImpulse, 2.0f, 12.0f);
        ImGui::SliderFloat("Hold Gravity Scale", &movement.jumpHoldGravityScale, 0.1f, 1.0f);
        ImGui::SliderFloat("Max Hold Time", &movement.maxJumpHoldTime, 0.1f, 0.6f);
    }

    ImGui::End();
}

#include "game/components/ViewmodelComponent.h"

void ImGuiLayer::renderViewmodelOverlay(ViewmodelComponent& vm) {
    ImGui::Begin("Viewmodel");

    ImGui::DragFloat3("Position", &vm.viewOffset.x, 0.005f, -1.0f, 1.0f, "%.3f");
    ImGui::DragFloat3("Rotation", &vm.rotation.x, 0.5f, -360.0f, 360.0f, "%.1f");
    ImGui::DragFloat3("Scale", &vm.scale.x, 0.0001f, 0.0001f, 0.5f, "%.4f");

    ImGui::Separator();
    ImGui::DragFloat3("Mesh Center", &vm.meshCenter.x, 0.5f, -500.0f, 500.0f, "%.1f");

    if (ImGui::CollapsingHeader("Bob Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Bob Speed", &vm.bobSpeed, 0.1f, 0.0f, 10.0f, "%.1f");
        ImGui::DragFloat("Bob Amplitude", &vm.bobAmplitude, 0.001f, 0.0f, 0.1f, "%.3f");
    }

    ImGui::End();
}

void ImGuiLayer::renderInteractionPrompt(const char* text, bool busy) {
    if (text == nullptr || text[0] == '\0') return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 promptSize(360.0f, 54.0f);
    const ImVec2 promptPos(
        viewport->Pos.x + (viewport->Size.x - promptSize.x) * 0.5f,
        viewport->Pos.y + viewport->Size.y - 118.0f
    );

    ImGui::SetNextWindowPos(promptPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(promptSize, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(busy ? 0.62f : 0.48f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoInputs;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 14.0f));
    ImGui::Begin("Interaction Prompt", nullptr, flags);
    ImGui::SetWindowFontScale(busy ? 1.18f : 1.1f);
    ImGui::TextUnformatted(text);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void ImGuiLayer::renderInventory(InventoryMenuState& menu,
                                 const RunSession& session,
                                 const ContentRegistry& content,
                                 const EffectiveEquipmentView& equipment) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + 52.0f, viewport->Pos.y + 48.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - 104.0f, viewport->Size.y - 96.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 18.0f));
    ImGui::Begin("Inventory", nullptr, flags);
    ImGui::TextUnformatted("EQUIPMENT INVENTORY");
    ImGui::Separator();

    const char* categories[] = {"Right Hand", "Left Hand", "Owned Weapons"};
    const float currentBurden = equipment.burden;
    const float maxBurden = session.maxEquipLoad <= 0.0f ? 1.0f : session.maxEquipLoad;
    const float burdenRatio = currentBurden / maxBurden;

    ImGui::BeginChild("InventoryCategories", ImVec2(180.0f, 0.0f), true);
    for (int i = 0; i < 3; ++i) {
        if (ImGui::Selectable(categories[i], menu.selectedCategory == i)) {
            menu.selectedCategory = i;
            if (i == 0) {
                menu.targetedHand = EquipmentHand::Right;
            } else if (i == 1) {
                menu.targetedHand = EquipmentHand::Left;
            }
        }
    }
    ImGui::Separator();
    ImGui::Text("BURDEN %.1f / %.1f", currentBurden, maxBurden);
    ImGui::ProgressBar(burdenRatio, ImVec2(-1.0f, 10.0f), "");
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("InventoryList", ImVec2(260.0f, 0.0f), true);
    for (int index = 0; index < static_cast<int>(session.ownedWeapons.size()); ++index) {
        const auto& entry = session.ownedWeapons[static_cast<std::size_t>(index)];
        const auto* weapon = content.findWeapon(entry.definitionId);
        std::string label = weapon != nullptr ? weapon->displayName : entry.definitionId;
        if (equipment.twoHanded && equipment.rightHandWeaponId == entry.definitionId) {
            label += "  [2H]";
        } else {
            if (equipment.rightHandWeaponId == entry.definitionId) {
                label += "  [R]";
            }
            if (equipment.leftHandWeaponId == entry.definitionId) {
                label += "  [L]";
            }
        }

        if (ImGui::Selectable(label.c_str(), menu.selectedItem == index)) {
            menu.selectedItem = index;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("InventoryDetails", ImVec2(0.0f, 0.0f), true);
    if (!session.ownedWeapons.empty() && menu.selectedItem >= 0 && menu.selectedItem < static_cast<int>(session.ownedWeapons.size())) {
        const auto& entry = session.ownedWeapons[static_cast<std::size_t>(menu.selectedItem)];
        const auto* weapon = content.findWeapon(entry.definitionId);
        if (weapon != nullptr) {
            ImGui::Text("%s", weapon->displayName.c_str());
            ImGui::Separator();
            ImGui::Text("Slot: %s", weapon->slot.c_str());
            ImGui::Text("Handedness: %s", weapon->handedness == WeaponHandedness::TwoHanded ? "Two-Handed" : "One-Handed");
            ImGui::Text("Weight: %.1f", weapon->equipWeight);
            ImGui::Text("Category: %s", weapon->category.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", weapon->description.c_str());
            ImGui::Spacing();

            const bool categoryTargetsHand = menu.selectedCategory == 0 || menu.selectedCategory == 1;
            if (categoryTargetsHand) {
                const bool isEquippedInTargetHand = (menu.targetedHand == EquipmentHand::Right)
                    ? equipment.rightHandWeaponId == entry.definitionId
                    : equipment.leftHandWeaponId == entry.definitionId;

                if (ImGui::Button(menu.targetedHand == EquipmentHand::Right ? "Equip To Right Hand" : "Equip To Left Hand", ImVec2(-1.0f, 0.0f))) {
                    menu.pendingAction = InventoryMenuState::PendingActionType::Equip;
                    menu.pendingHand = menu.targetedHand;
                    menu.pendingWeaponId = entry.definitionId;
                }

                if (isEquippedInTargetHand && ImGui::Button(menu.targetedHand == EquipmentHand::Right ? "Unequip Right Hand" : "Unequip Left Hand", ImVec2(-1.0f, 0.0f))) {
                    menu.pendingAction = InventoryMenuState::PendingActionType::Unequip;
                    menu.pendingHand = menu.targetedHand;
                    menu.pendingWeaponId.clear();
                }
            } else {
                if (ImGui::Button("Equip To Right Hand", ImVec2(-1.0f, 0.0f))) {
                    menu.pendingAction = InventoryMenuState::PendingActionType::Equip;
                    menu.pendingHand = EquipmentHand::Right;
                    menu.pendingWeaponId = entry.definitionId;
                }
                if (ImGui::Button("Equip To Left Hand", ImVec2(-1.0f, 0.0f))) {
                    menu.pendingAction = InventoryMenuState::PendingActionType::Equip;
                    menu.pendingHand = EquipmentHand::Left;
                    menu.pendingWeaponId = entry.definitionId;
                }
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Right Hand: %s", equipment.rightHandWeaponId.empty() ? "Empty" : equipment.rightHandWeaponId.c_str());
    ImGui::Text("Left Hand: %s", equipment.leftHandWeaponId.empty() ? "Empty" : equipment.leftHandWeaponId.c_str());
    ImGui::Text("Press I or Escape to close");
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar(2);
}
