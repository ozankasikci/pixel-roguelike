#pragma once

#include "game/runtime/RuntimeGameSession.h"

#include <entt/entt.hpp>
#include <glm/vec2.hpp>

#include <string>

struct ImGuiIO;
struct GLFWwindow;
class ContentRegistry;
class EditorSceneDocument;

class EditorRuntimePreviewSession {
public:
    EditorRuntimePreviewSession() = default;

    void rebuild(const EditorSceneDocument& document, ContentRegistry& content);
    void resetForPlay();
    void syncEnvironment(const EditorSceneDocument& document);
    void clear();
    void tick(float deltaTime, float aspect);
    void prewarmRenderer(ContentRegistry& content);
    RuntimeSceneRenderOutput render(float deltaTime,
                                    int internalWidth,
                                    int internalHeight,
                                    int outputWidth,
                                    int outputHeight,
                                    GLuint targetFramebuffer = 0);

    void updateInput(GLFWwindow* window, const ImGuiIO& io);
    void beginCapture(GLFWwindow* window);
    void endCapture(GLFWwindow* window);
    void syncCursor(GLFWwindow* window);
    bool captured() const { return captured_; }

    entt::registry& registry() { return session_.registry(); }
    const entt::registry& registry() const { return session_.registry(); }
    MeshLibrary& meshLibrary() { return session_.meshLibrary(); }
    const MeshLibrary& meshLibrary() const { return session_.meshLibrary(); }
    DebugParams& debugParams() { return session_.debugParams(); }
    const DebugParams& debugParams() const { return session_.debugParams(); }
    RuntimeInputState& input() { return session_.input(); }
    const RuntimeInputState& input() const { return session_.input(); }
    RunSession& runSession() { return session_.runSession(); }
    const RunSession& runSession() const { return session_.runSession(); }
    const RuntimeSessionPerformanceStats& performanceStats() const { return session_.performanceStats(); }

private:
    RuntimeGameSession session_;
    bool captured_ = false;
    bool hasLastCursorPosition_ = false;
    glm::vec2 lastCursorPosition_{0.0f};
};
