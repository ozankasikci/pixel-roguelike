#pragma once

#include "game/level/LevelBuildContext.h"
#include "game/level/LevelDef.h"

#include <functional>
#include <string>

class ContentRegistry;
class RunSession;

struct LevelLoadRequest {
    std::string levelId;
    std::string levelPath;
    std::function<void(MeshLibrary&)> registerAssets;
    std::function<void(class LevelBuilder&)> buildScriptedGeometry;
};

/// Explicit context for a level load. Callers must supply content and session
/// pointers; levelDef is optional — if null the loader reads it from
/// request.levelPath on disk.
struct LevelLoadArgs {
    ContentRegistry* content = nullptr;  // required
    RunSession* session      = nullptr;  // required
    const LevelDef* levelDef = nullptr;  // optional: if null, load from request.levelPath
};

class LevelLoader {
public:
    explicit LevelLoader(LevelBuildContext& context);

    void load(const LevelLoadRequest& request, const LevelLoadArgs& args);

private:
    LevelBuildContext& context_;
};
