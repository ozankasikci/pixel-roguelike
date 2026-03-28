#pragma once

#include "game/level/LevelBuildContext.h"
#include "game/session/RunSession.h"
#include "engine/ui/ImGuiLayer.h"

#include <entt/entt.hpp>

#include <vector>

class ContentRegistry;
class EditorSceneDocument;

class EditorRuntimePreviewSession {
public:
    EditorRuntimePreviewSession();

    void rebuild(const EditorSceneDocument& document, ContentRegistry& content);
    void clear();

    entt::registry& registry() { return registry_; }
    const entt::registry& registry() const { return registry_; }
    MeshLibrary& meshLibrary() { return meshLibrary_; }
    const MeshLibrary& meshLibrary() const { return meshLibrary_; }
    DebugParams& debugParams() { return debugParams_; }
    const DebugParams& debugParams() const { return debugParams_; }

private:
    void clearEntities();

    entt::registry registry_;
    MeshLibrary meshLibrary_;
    std::vector<entt::entity> entities_;
    RunSession session_;
    DebugParams debugParams_;
};
