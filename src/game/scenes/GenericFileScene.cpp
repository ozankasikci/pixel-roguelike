#include "GenericFileScene.h"

#include "engine/audio/AudioEngine.h"
#include "engine/core/Application.h"
#include "engine/core/PathUtils.h"
#include "engine/input/InputSystem.h"
#include "game/behavior/BehaviorSystem.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelBuilder.h"
#include "game/level/LevelDef.h"
#include "game/levels/ProceduralGameAssets.h"
#include "game/runtime/GameplayEventSink.h"
#include "engine/rendering/assets/ModelLoader.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Scripted geometry registry
// ---------------------------------------------------------------------------

using ScriptedGeometryCallback = std::function<void(LevelBuilder&)>;

static std::unordered_map<std::string, ScriptedGeometryCallback>& scriptedGeometryRegistry() {
    static std::unordered_map<std::string, ScriptedGeometryCallback> registry;
    return registry;
}

// ---------------------------------------------------------------------------
// GenericFileScene implementation
// ---------------------------------------------------------------------------

void GenericFileScene::registerScriptedGeometry(const std::string& levelId,
                                                 std::function<void(LevelBuilder&)> callback) {
    scriptedGeometryRegistry()[levelId] = std::move(callback);
}

GenericFileScene::GenericFileScene(const std::string& scenePath) {
    request_.levelId   = std::filesystem::path(scenePath).stem().string();
    request_.levelPath = scenePath;
}

void GenericFileScene::onEnter(Application& app) {
    auto& content = app.getService<ContentRegistry>();

    LevelLoadRequest request;
    request.levelId = request_.levelId;
    request.levelPath = request_.levelPath;
    request.registerAssets = [](MeshLibrary& library) {
        registerProceduralAssets(library);
        const std::filesystem::path relativeBase(resolveProjectPath("."));

        // Auto-discover file-based meshes from assets/meshes/ (per D-05)
        const std::filesystem::path meshDir(resolveProjectPath("assets/meshes"));
        for (const auto& asset : ModelLoader::discoverProjectAssets(meshDir, relativeBase)) {
            if (!library.has(asset.meshId)) {
                library.registerFileAlias(asset.meshId, asset.relativePath);
            }
        }

        // Auto-discover file-based meshes from assets/packs/ subdirectories
        const std::filesystem::path packsDir(resolveProjectPath("assets/packs"));
        for (const auto& asset : ModelLoader::discoverProjectAssets(packsDir, relativeBase)) {
            if (!library.has(asset.meshId)) {
                library.registerFileAlias(asset.meshId, asset.relativePath);
            }
        }
    };

    // Look up scripted geometry from the registry (no hard-coded if-chain)
    auto it = scriptedGeometryRegistry().find(request_.levelId);
    if (it != scriptedGeometryRegistry().end()) {
        request.buildScriptedGeometry = it->second;
    } else {
        request.buildScriptedGeometry = {};
    }

    // Load level definition from scene file
    LevelDef level = loadLevelDef(resolveProjectPath(request_.levelPath));

    session_.rebuild(level, request_.levelId, request_.levelPath, content, request);

    // Store AudioEngine pointer in registry context so gameplay systems (doors, footsteps)
    // can play spatial sounds without coupling to Application.
    if (auto* audioPtr = app.tryGetService<engine::audio::AudioEngine*>()) {
        engine::audio::AudioEngine* audio = *audioPtr;
        session_.registry().ctx().insert_or_assign<engine::audio::AudioEngine*>(std::move(audio));
    }

    // Expose session so RenderSystem can access the gameplay registry
    app.emplaceService<RuntimeGameSession*>(&session_);
}

void GenericFileScene::onUpdate(Application& app, float deltaTime) {
    auto* appInput = app.tryGetService<InputSystem*>();
    if (!appInput) return;
    InputSystem& src = **appInput;
    InputSystem& dst = session_.input();

    dst.beginFrame();
    dst.setCursorLocked(src.isCursorLocked());
    dst.setWantsCaptureMouse(src.wantsCaptureMouse());
    for (int key = 0; key < InputSystem::kMaxKeys; ++key) {
        dst.setKeyPressed(key, src.isKeyPressed(key));
    }
    for (int button = 0; button < InputSystem::kMaxButtons; ++button) {
        dst.setMouseButtonPressed(button, src.isMouseButtonPressed(button));
    }
    dst.setMousePosition(src.mousePosition());
    dst.setMouseDelta(src.mouseDelta());
    dst.setScrollDelta(src.scrollDelta());
    for (const auto& event : src.keyPressEvents()) {
        dst.addKeyPressEvent(event.key, event.scancode);
    }
    for (unsigned int ch : src.typedCharacters()) {
        dst.addTypedCharacter(ch);
    }

    // Tick gameplay
    int w, h;
    glfwGetFramebufferSize(app.window().handle(), &w, &h);
    float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    session_.tick(deltaTime, aspect);

    // Drain gameplay events to application EventBus (for audio etc.)
    for (const auto& event : session_.eventSink().events()) {
        if (event.kind == GameplayEvent::Kind::PlaySound) {
            app.eventBus().publish(PlaySoundEvent{event.id, event.value});
        } else if (event.kind == GameplayEvent::Kind::ShowMessage) {
            app.eventBus().publish(ShowMessageEvent{event.id, event.value});
        }
    }
    session_.eventSink().drain();
}

void GenericFileScene::onExit(Application& app) {
    (void)app;
    session_.clear();
}
