#include "engine/core/Application.h"
#include "engine/scene/SceneManager.h"
#include "engine/input/InputSystem.h"
#include "engine/physics/PhysicsSystem.h"
#include "game/systems/InventorySystem.h"
#include "game/systems/PlayerMovementSystem.h"
#include "game/systems/CameraSystem.h"
#include "game/systems/CheckpointSystem.h"
#include "game/systems/DoorSystem.h"
#include "game/systems/InteractionSystem.h"
#include "game/systems/RenderSystem.h"
#include "game/scenes/SilosCloisterScene.h"
#include "game/content/ContentRegistry.h"
#include "game/session/RunSession.h"

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    // Parse --screenshot path for automated capture
    std::string autoScreenshotPath;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            autoScreenshotPath = argv[i + 1];
            ++i;
        }
    }

    Application app(1280, 720, "Pixel Roguelike");
    app.emplaceService<RunSession>();
    auto& content = app.emplaceService<ContentRegistry>();
    content.loadDefaults();
    app.registry().ctx().insert_or_assign<ContentRegistry*>(&content);

    // Register systems by phase so scheduling policy lives in the engine instead of boot order.
    auto& input = app.addSystem<InputSystem>(Application::UpdatePhase::Input);
    auto& interaction = app.addSystem<InteractionSystem>(Application::UpdatePhase::Interaction, input);
    auto& doors = app.addSystem<DoorSystem>(Application::UpdatePhase::Interaction, input);
    auto& checkpoints = app.addSystem<CheckpointSystem>(Application::UpdatePhase::Interaction, input);
    auto& physics = app.addSystem<PhysicsSystem>(Application::UpdatePhase::Physics);
    auto& inventory = app.addSystem<InventorySystem>(Application::UpdatePhase::Gameplay, input);
    auto& movement = app.addSystem<PlayerMovementSystem>(Application::UpdatePhase::Gameplay, input, physics);
    auto& camera = app.addSystem<CameraSystem>(Application::UpdatePhase::Camera, input);
    auto& render = app.addSystem<RenderSystem>(Application::UpdatePhase::Render, input);
    (void)doors;
    (void)checkpoints;
    (void)interaction;
    (void)inventory;
    (void)movement;
    (void)camera;

    if (!autoScreenshotPath.empty()) {
        render.enableAutoScreenshot(autoScreenshotPath, 10);
    }

    // Push the Silos-inspired cloister scene as the starting level.
    SceneManager sceneManager;
    app.setSceneManager(&sceneManager);
    sceneManager.pushScene(std::make_unique<SilosCloisterScene>(), app);

    // Run the game loop (per D-04)
    app.run();

    sceneManager.popScene(app);

    spdlog::info("Shutting down");
    return 0;
}
