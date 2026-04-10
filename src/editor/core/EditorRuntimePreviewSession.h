#pragma once

#include "game/runtime/RuntimeGameSession.h"

#include <glm/vec2.hpp>

#include <string>

struct ImGuiIO;
struct GLFWwindow;
class ContentRegistry;
class EditorSceneDocument;

class EditorRuntimePreviewSession {
public:
    EditorRuntimePreviewSession() = default;

    void rebuild(const EditorSceneDocument& document, ContentRegistry& content, bool contentChanged = false);
    void syncMaterials(const EditorSceneDocument& document, const ContentRegistry& content);
    void resetForPlay();
    void syncEnvironment(const EditorSceneDocument& document);
    void clear();
    void tick(float deltaTime, float aspect);
    void prewarmRenderer(ContentRegistry& content);
    bool setPrimaryCameraView(const glm::vec3& position,
                              float yaw,
                              float pitch,
                              const std::optional<float>& fov = std::nullopt);
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

    GameRegistry& registry() { return session_.registry(); }
    const GameRegistry& registry() const { return session_.registry(); }
    MeshLibrary& meshLibrary() { return session_.meshLibrary(); }
    const MeshLibrary& meshLibrary() const { return session_.meshLibrary(); }
    DebugParams& debugParams() { return session_.debugParams(); }
    const DebugParams& debugParams() const { return session_.debugParams(); }
    InputSystem& input() { return session_.input(); }
    const InputSystem& input() const { return session_.input(); }
    RunSession& runSession() { return session_.runSession(); }
    const RunSession& runSession() const { return session_.runSession(); }
    const RuntimeSessionPerformanceStats& performanceStats() const { return session_.performanceStats(); }
    const SceneRenderPipelineStats& pipelineStats() const { return session_.pipelineStats(); }
    CameraManager& cameraManager() { return session_.cameraManager(); }
    const CameraManager& cameraManager() const { return session_.cameraManager(); }

private:
    RuntimeGameSession session_;
    bool captured_ = false;
    bool hasLastCursorPosition_ = false;
    glm::vec2 lastCursorPosition_{0.0f};
};
