#pragma once

#include "engine/scene/Scene.h"
#include "game/level/LevelLoader.h"
#include "game/runtime/RuntimeGameSession.h"

#include <functional>
#include <string>

class LevelBuilder;

/// Scene that loads any .scene file by path via LevelLoader.
/// Used by the runtime --scene argument (D-18) to open an arbitrary scene file.
class GenericFileScene : public Scene {
public:
    explicit GenericFileScene(const std::string& scenePath);

    void onEnter(Application& app) override;
    void onUpdate(Application& app, float deltaTime) override;
    void onExit(Application& app) override;

    /// Register a scripted geometry callback for a specific level ID.
    /// The callback is invoked after the .scene file is loaded, allowing
    /// code-driven entities (doors, knobs) to be placed for that level.
    static void registerScriptedGeometry(const std::string& levelId,
                                          std::function<void(LevelBuilder&)> callback);

private:
    RuntimeGameSession session_;
    LevelLoadRequest request_;
};
