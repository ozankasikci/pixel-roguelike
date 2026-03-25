#pragma once

#include "game/level/LevelBuildContext.h"
#include "game/level/LevelDef.h"

#include <functional>
#include <string>

class Application;
class ContentRegistry;

struct LevelLoadRequest {
    std::string levelId;
    std::string levelPath;
    std::function<void(MeshLibrary&)> registerAssets;
    std::function<void(class LevelBuilder&)> buildScriptedGeometry;
};

class LevelLoader {
public:
    explicit LevelLoader(LevelBuildContext& context);

    void load(Application& app, const LevelLoadRequest& request);

private:
    LevelBuildContext& context_;
};
