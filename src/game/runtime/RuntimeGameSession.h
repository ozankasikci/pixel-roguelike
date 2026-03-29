#pragma once

#include "engine/physics/PhysicsSystem.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/ui/ImGuiLayer.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelLoader.h"
#include "game/rendering/EnvironmentDebugSync.h"
#include "game/rendering/RuntimeSceneRenderer.h"
#include "game/runtime/RuntimeGameplay.h"
#include "game/runtime/RuntimeInputState.h"
#include "game/session/RunSession.h"

#include <entt/entt.hpp>

#include <memory>
#include <string>
#include <vector>

class ContentRegistry;
struct LevelDef;
struct RuntimeMutableSnapshot;

struct RuntimeSessionPerformanceStats {
    double rebuildMs = 0.0;
    double resetForPlayMs = 0.0;
    double rendererInitMs = 0.0;
    double rendererPrewarmMs = 0.0;
    double lastRenderMs = 0.0;
};

class RuntimeGameSession {
public:
    RuntimeGameSession();
    ~RuntimeGameSession();

    void rebuild(const LevelDef& level,
                 const std::string& levelId,
                 const std::string& levelPath,
                 ContentRegistry& content,
                 const LevelLoadRequest& request = {});
    void clear();
    void resetForPlay();
    void tick(float deltaTime, float aspect);
    void prewarmRenderer(ContentRegistry& content);
    void setEnvironmentOverride(const EnvironmentDefinition& definition);
    void clearEnvironmentOverride();
    RuntimeSceneRenderOutput render(float deltaTime,
                                    int internalWidth,
                                    int internalHeight,
                                    int outputWidth,
                                    int outputHeight,
                                    GLuint targetFramebuffer = 0);

    entt::registry& registry() { return registry_; }
    const entt::registry& registry() const { return registry_; }
    MeshLibrary& meshLibrary() { return meshLibrary_; }
    const MeshLibrary& meshLibrary() const { return meshLibrary_; }
    RunSession& runSession() { return runSession_; }
    const RunSession& runSession() const { return runSession_; }
    RuntimeInputState& input() { return input_; }
    const RuntimeInputState& input() const { return input_; }
    DebugParams& debugParams() { return debugParams_; }
    const DebugParams& debugParams() const { return debugParams_; }
    const RuntimeSessionPerformanceStats& performanceStats() const { return performanceStats_; }

private:
    void ensureInitialized();
    void ensureRendererInitialized();
    void captureBaselineState();
    void restoreBaselineState();
    void resetTransientRuntimeState();
    void clearEntities();

    entt::registry registry_;
    MeshLibrary meshLibrary_;
    std::vector<entt::entity> entities_;
    RunSession runSession_;
    RuntimeInputState input_;
    PhysicsSystem physics_;
    DebugParams debugParams_;
    RuntimeSceneRenderer renderer_;
    RuntimeEnvironmentSyncState environmentSyncState_;
    RuntimeSessionPerformanceStats performanceStats_;
    std::unique_ptr<RuntimeMutableSnapshot> baselineSnapshot_;
    ContentRegistry* content_ = nullptr;
    bool physicsInitialized_ = false;
    bool rendererInitialized_ = false;
};
