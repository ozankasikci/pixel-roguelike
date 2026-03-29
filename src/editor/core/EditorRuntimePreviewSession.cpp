#include "editor/core/EditorRuntimePreviewSession.h"

#include "editor/scene/EditorSceneDocument.h"
#include "game/components/MeshComponent.h"
#include "game/rendering/MaterialDefinition.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace {

void setCursorCapture(GLFWwindow* window, bool captured) {
    if (window == nullptr) {
        return;
    }

    glfwSetInputMode(window, GLFW_CURSOR, captured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, captured ? GLFW_TRUE : GLFW_FALSE);
    }
}

} // namespace

void EditorRuntimePreviewSession::rebuild(const EditorSceneDocument& document, ContentRegistry& content) {
    LevelLoadRequest request;
    request.levelId = document.scenePath().empty() ? "editor_runtime_preview" : document.scenePath();
    request.levelPath = document.scenePath();
    session_.rebuild(document.toLevelDef(), request.levelId, request.levelPath, content, request);
    syncEnvironment(document);
}

void EditorRuntimePreviewSession::syncMaterials(const EditorSceneDocument& document,
                                                 const ContentRegistry& content) {
    const LevelDef level = document.toLevelDef();
    auto meshView = session_.registry().view<MeshComponent>();
    std::size_t meshIndex = 0;
    for (auto [entity, mesh] : meshView.each()) {
        if (meshIndex < level.meshes.size()) {
            const auto& placement = level.meshes[meshIndex];
            mesh.material = placement.material.value_or(MaterialKind::Stone);
            mesh.materialId = placement.materialId;
            if (mesh.materialId.empty()) {
                mesh.materialId = std::string(defaultMaterialIdForKind(mesh.material));
            }
            mesh.tint = placement.tint.value_or(glm::vec3(1.0f));
        }
        ++meshIndex;
    }
}

void EditorRuntimePreviewSession::resetForPlay() {
    session_.resetForPlay();
}

void EditorRuntimePreviewSession::syncEnvironment(const EditorSceneDocument& document) {
    session_.setEnvironmentOverride(document.environment());
}

void EditorRuntimePreviewSession::clear() {
    captured_ = false;
    hasLastCursorPosition_ = false;
    session_.clear();
}

void EditorRuntimePreviewSession::tick(float deltaTime, float aspect) {
    session_.tick(deltaTime, aspect);
}

void EditorRuntimePreviewSession::prewarmRenderer(ContentRegistry& content) {
    session_.prewarmRenderer(content);
}

RuntimeSceneRenderOutput EditorRuntimePreviewSession::render(float deltaTime,
                                                            int internalWidth,
                                                            int internalHeight,
                                                            int outputWidth,
                                                            int outputHeight,
                                                            GLuint targetFramebuffer) {
    return session_.render(deltaTime, internalWidth, internalHeight, outputWidth, outputHeight, targetFramebuffer);
}

void EditorRuntimePreviewSession::updateInput(GLFWwindow* window, const ImGuiIO& io) {
    RuntimeInputState& state = session_.input();
    state.beginFrame();
    state.setWantsCaptureMouse(false);

    if (!captured_ || window == nullptr) {
        state.setCursorLocked(false);
        for (int key = 0; key < RuntimeInputState::MaxKeys; ++key) {
            state.setKeyPressed(key, false);
        }
        for (int button = 0; button < RuntimeInputState::MaxButtons; ++button) {
            state.setMouseButtonPressed(button, false);
        }
        state.setMousePosition(glm::vec2(io.MousePos.x, io.MousePos.y));
        state.setMouseDelta(glm::vec2(0.0f));
        state.setScrollDelta(0.0f);
        return;
    }

    for (int key = 0; key < RuntimeInputState::MaxKeys; ++key) {
        const bool pressed = glfwGetKey(window, key) == GLFW_PRESS;
        state.setKeyPressed(key, pressed);
        if (state.isKeyJustPressed(key)) {
            state.addKeyPressEvent(key, glfwGetKeyScancode(key));
        }
    }

    for (int button = 0; button < RuntimeInputState::MaxButtons; ++button) {
        state.setMouseButtonPressed(button, glfwGetMouseButton(window, button) == GLFW_PRESS);
    }

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    const glm::vec2 mousePosition(static_cast<float>(mouseX), static_cast<float>(mouseY));
    glm::vec2 mouseDelta(0.0f);
    if (hasLastCursorPosition_) {
        mouseDelta = mousePosition - lastCursorPosition_;
    }
    lastCursorPosition_ = mousePosition;
    hasLastCursorPosition_ = true;

    state.setMousePosition(mousePosition);
    state.setMouseDelta(mouseDelta);
    state.setScrollDelta(io.MouseWheel);
    for (ImWchar character : io.InputQueueCharacters) {
        state.addTypedCharacter(static_cast<unsigned int>(character));
    }
}

void EditorRuntimePreviewSession::beginCapture(GLFWwindow* window) {
    if (captured_) {
        return;
    }
    captured_ = true;
    hasLastCursorPosition_ = false;
    session_.input().reset();
    session_.input().setCursorLocked(true);
    setCursorCapture(window, true);
}

void EditorRuntimePreviewSession::endCapture(GLFWwindow* window) {
    if (!captured_ && window == nullptr) {
        session_.input().reset();
        return;
    }
    captured_ = false;
    hasLastCursorPosition_ = false;
    session_.input().reset();
    session_.input().setCursorLocked(false);
    setCursorCapture(window, false);
}

void EditorRuntimePreviewSession::syncCursor(GLFWwindow* window) {
    setCursorCapture(window, captured_ && session_.input().isCursorLocked());
}
