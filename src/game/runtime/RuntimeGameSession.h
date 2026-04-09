#pragma once

#include "engine/input/InputSystem.h"
#include "engine/physics/PhysicsSystem.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/ui/ImGuiLayer.h"
#include "game/level/LevelBuildContext.h"
#include "game/level/LevelLoader.h"
#include "game/rendering/EnvironmentDebugSync.h"
#include "game/rendering/RuntimeSceneRenderer.h"
#include "game/runtime/GameplayEventSink.h"
#include "game/runtime/RuntimeGameplay.h"
#include "game/session/RunSession.h"

#include "engine/ecs/GameRegistry.h"

#include <memory>
#include <string>
#include <vector>

class ContentRegistry;
struct LevelDef;
struct RuntimeMutableSnapshot;

struct RuntimeSessionPerformanceStats {
    // --- Setup stats (populated during rebuild / reset operations) ---
    double rebuildMs = 0.0;
    double resetForPlayMs = 0.0;
    double rendererInitMs = 0.0;
    double rendererPrewarmMs = 0.0;
    double lastRenderMs = 0.0;

    // --- Per-frame tick stats (populated each tick() call) ---
    double interactionMs = 0.0;
    double checkpointsMs = 0.0;
    double physicsMs = 0.0;
    double inventoryMs = 0.0;
    double movementMs = 0.0;
    double cameraMs = 0.0;
    double totalTickMs = 0.0;
};

class RuntimeGameSession {
public:
    RuntimeGameSession();
    ~RuntimeGameSession();

    void rebuild(const LevelDef& level,
                 const std::string& levelId,
                 const std::string& levelPath,
                 ContentRegistry& content,
                 const LevelLoadRequest& request = {},
                 bool contentChanged = false);
    void clear();
    void resetForPlay();
    void tick(float deltaTime, float aspect);
    void prewarmRenderer(ContentRegistry& content);
    bool setPrimaryCameraView(const glm::vec3& position,
                              float yaw,
                              float pitch,
                              const std::optional<float>& fov = std::nullopt);
    void setEnvironmentOverride(const EnvironmentDefinition& definition);
    void clearEnvironmentOverride();
    RuntimeSceneRenderOutput render(float deltaTime,
                                    int internalWidth,
                                    int internalHeight,
                                    int outputWidth,
                                    int outputHeight,
                                    GLuint targetFramebuffer = 0);

    GameRegistry& registry() { return registry_; }
    const GameRegistry& registry() const { return registry_; }
    MeshLibrary& meshLibrary() { return meshLibrary_; }
    const MeshLibrary& meshLibrary() const { return meshLibrary_; }
    RunSession& runSession() { return runSession_; }
    const RunSession& runSession() const { return runSession_; }
    InputSystem& input() { return inputSystem_; }
    const InputSystem& input() const { return inputSystem_; }
    DebugParams& debugParams() { return debugParams_; }
    const DebugParams& debugParams() const { return debugParams_; }
    const RuntimeSessionPerformanceStats& performanceStats() const { return performanceStats_; }
    const SceneRenderPipelineStats& pipelineStats() const { return renderer_.pipelineStats(); }
    GameplayEventSink& eventSink() { return eventSink_; }
    float elapsedTime() const { return elapsedTime_; }

private:
    void ensureInitialized();
    void ensureRendererInitialized();
    void captureBaselineState();
    void restoreBaselineState();
    void resetTransientRuntimeState();
    void clearEntities();

    GameRegistry registry_;
    MeshLibrary meshLibrary_;
    std::vector<entt::entity> entities_;
    RunSession runSession_;
    InputSystem inputSystem_;
    PhysicsSystem physics_;
    DebugParams debugParams_;
    RuntimeSceneRenderer renderer_;
    RuntimeEnvironmentSyncState environmentSyncState_;
    RuntimeSessionPerformanceStats performanceStats_;
    std::unique_ptr<RuntimeMutableSnapshot> baselineSnapshot_;
    GameplayEventSink eventSink_;
    ContentRegistry* content_ = nullptr;
    float elapsedTime_ = 0.0f;
    bool physicsInitialized_ = false;
    bool rendererInitialized_ = false;
};
