#include "engine/core/Application.h"
#include "engine/core/PathUtils.h"
#include "engine/core/ProjectConfig.h"
#include "engine/scene/SceneManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/input/InputSystem.h"
#include "engine/ui/ImGuiLayer.h"
#include "game/modules/checkpoint/CheckpointModule.h"
#include "game/modules/door/DoorModule.h"
#include "game/modules/player_control/PlayerControlModule.h"
#include "game/systems/AudioListenerSystem.h"
#include "game/systems/RenderSystem.h"
#include "game/scenes/GenericFileScene.h"
#include "game/content/ContentRegistry.h"
#include "game/session/RunSession.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<std::string> listAvailableScenes() {
    namespace fs = std::filesystem;
    std::vector<std::string> results;
    const std::string sceneDir = resolveProjectPath("assets/scenes");
    if (!fs::exists(sceneDir)) { return results; }
    for (const auto& entry : fs::directory_iterator(sceneDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".scene") continue;
        results.push_back(entry.path().filename().string());
    }
    std::sort(results.begin(), results.end());
    return results;
}

} // namespace

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    // Parse command-line arguments
    std::string autoScreenshotPath;
    std::string scenePath;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            autoScreenshotPath = argv[i + 1];
            ++i;
        } else if (std::string(argv[i]) == "--scene" && i + 1 < argc) {
            scenePath = argv[i + 1];
            ++i;
        }
    }

    if (!hasValidProjectRoot()) {
        const char* msg =
            "Could not find assets/ directory. Make sure the game executable is in the "
            "same directory as the assets/ folder, or run from the project root.";
        spdlog::error(msg);
        std::cerr << "ERROR: " << msg << std::endl;
        return 1;
    }

    Application app(1280, 720, "Pixel Roguelike");
    app.emplaceService<RunSession>();
    auto& content = app.emplaceService<ContentRegistry>();
    try {
        content.loadDefaults();
    } catch (const std::exception& e) {
        spdlog::error("Failed to load game content: {}", e.what());
        std::cerr << "ERROR: Failed to load game content: " << e.what() << std::endl;
        return 1;
    }
    app.registry().ctx().insert_or_assign<ContentRegistry*>(&content);
    app.registry().ctx().insert_or_assign<RunSession*>(&app.getService<RunSession>());

    registerDoorModule();
    registerCheckpointModule();
    registerPlayerControlModule();

    // Register systems by phase so scheduling policy lives in the engine instead of boot order.
    // Gameplay systems (interaction, checkpoints, doors, movement, camera) are handled by
    // RuntimeGameSession — only app-level infrastructure systems remain here.
    auto& input = app.addSystem<InputSystem>(Application::UpdatePhase::Input);
    app.emplaceService<InputSystem*>(&input);
    auto& audio = app.addSystem<AudioSystem>(Application::UpdatePhase::Gameplay);
    app.emplaceService<AudioSystem*>(&audio);
    auto& audioListener = app.addSystem<AudioListenerSystem>(Application::UpdatePhase::Gameplay, audio);
    (void)audioListener;
    auto& render = app.addSystem<RenderSystem>(Application::UpdatePhase::Render, input);

    if (!autoScreenshotPath.empty()) {
        render.enableAutoScreenshot(autoScreenshotPath, 10);
    }

    // Scene resolution: (1) --scene arg, (2) project.cfg, (3) first-launch picker (D-05, D-07)
    const std::string cfgPath = resolveProjectPath("assets/project.cfg");

    if (scenePath.empty()) {
        const std::string cfgScene = readProjectCfgLastScene(cfgPath);
        if (!cfgScene.empty()) {
            const std::string resolved = resolveProjectPath("assets/scenes/" + cfgScene);
            if (std::filesystem::exists(resolved)) {
                scenePath = resolved;
            }
        }
    }

    if (scenePath.empty()) {
        // First-launch: show ImGui scene picker (D-07)
        // Temporary ImGuiLayer for picker only — RenderSystem owns its own ImGuiLayer
        // initialized during app.run(), so this standalone one runs before the game loop.
        ImGuiLayer pickerImgui;
        pickerImgui.init(app.window().handle());

        const auto scenes = listAvailableScenes();
        int selectedIndex = 0;
        bool launched = false;

        while (!launched && !glfwWindowShouldClose(app.window().handle())) {
            glfwPollEvents();
            pickerImgui.beginFrame();

            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
            ImGui::Begin("Scene Picker", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

            ImGui::Text("Select a scene to launch:");
            ImGui::Separator();

            if (scenes.empty()) {
                ImGui::TextWrapped("No .scene files found in assets/scenes/. Create a scene in the editor first.");
            } else {
                ImGui::BeginChild("SceneList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 8));
                for (int i = 0; i < static_cast<int>(scenes.size()); ++i) {
                    if (ImGui::Selectable(scenes[static_cast<std::size_t>(i)].c_str(), i == selectedIndex)) {
                        selectedIndex = i;
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        selectedIndex = i;
                        launched = true;
                    }
                }
                ImGui::EndChild();

                ImGui::BeginDisabled(scenes.empty());
                if (ImGui::Button("Launch", ImVec2(120, 0))) {
                    launched = true;
                }
                ImGui::EndDisabled();
            }

            ImGui::End();
            pickerImgui.endFrame();
            glfwSwapBuffers(app.window().handle());
        }

        pickerImgui.shutdown();

        if (launched && !scenes.empty()) {
            const std::string& chosen = scenes[static_cast<std::size_t>(selectedIndex)];
            scenePath = resolveProjectPath("assets/scenes/" + chosen);
        }

        if (scenePath.empty()) {
            // User closed window without picking
            spdlog::info("No scene selected, exiting.");
            return 0;
        }
    }

    SceneManager sceneManager;
    app.setSceneManager(&sceneManager);
    sceneManager.pushScene(std::make_unique<GenericFileScene>(scenePath), app);

    // Run the game loop (per D-04)
    app.run();

    sceneManager.popScene(app);

    spdlog::info("Shutting down");
    return 0;
}
