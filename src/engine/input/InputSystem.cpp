#include "engine/input/InputSystem.h"
#include "engine/core/Application.h"
#include "engine/core/Window.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <cstring>

InputSystem* InputSystem::instance_ = nullptr;

void InputSystem::init(Application& app) {
    window_ = app.window().handle();
    instance_ = this;

    // Only mouse position and scroll need callbacks (can't poll deltas)
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetCharCallback(window_, charCallback);

    // Lock cursor for FPS mode
    lockCursor();
}

void InputSystem::update(Application& app, float deltaTime) {
    (void)app;
    (void)deltaTime;

    state_.beginFrame();

    for (int i = 0; i < RuntimeInputState::MaxKeys; ++i) {
        state_.setKeyPressed(i, glfwGetKey(window_, i) == GLFW_PRESS);
    }

    for (int i = 0; i < RuntimeInputState::MaxButtons; ++i) {
        state_.setMouseButtonPressed(i, glfwGetMouseButton(window_, i) == GLFW_PRESS);
    }

    state_.setMousePosition(mousePos_);
    state_.setMouseDelta(mouseDeltaAccum_);
    mouseDeltaAccum_ = glm::vec2(0.0f);
    state_.setScrollDelta(scrollAccum_);
    scrollAccum_ = 0.0f;
    for (const auto& event : keyPressEventsAccum_) {
        state_.addKeyPressEvent(event.key, event.scancode);
    }
    keyPressEventsAccum_.clear();
    for (unsigned int codepoint : typedCharactersAccum_) {
        state_.addTypedCharacter(codepoint);
    }
    typedCharactersAccum_.clear();
    state_.setWantsCaptureMouse(ImGui::GetIO().WantCaptureMouse);

    // Toggle cursor lock with Escape (for debug tools)
    if (isKeyJustPressed(GLFW_KEY_ESCAPE)) {
        if (state_.isCursorLocked()) {
            unlockCursor();
        } else {
            lockCursor();
        }
    }
}

void InputSystem::shutdown() {
    if (window_) {
        glfwSetCursorPosCallback(window_, nullptr);
        glfwSetScrollCallback(window_, nullptr);
        glfwSetKeyCallback(window_, nullptr);
        glfwSetCharCallback(window_, nullptr);
    }
    instance_ = nullptr;
}

// --- Keyboard ---

bool InputSystem::isKeyPressed(int key) const {
    return state_.isKeyPressed(key);
}

bool InputSystem::isKeyJustPressed(int key) const {
    return state_.isKeyJustPressed(key);
}

bool InputSystem::isKeyJustReleased(int key) const {
    return state_.isKeyJustReleased(key);
}

bool InputSystem::isKeyJustPressedByName(std::string_view keyName) const {
    return state_.isKeyJustPressedByName(keyName);
}

bool InputSystem::wasCharacterTyped(unsigned int codepoint) const {
    return state_.wasCharacterTyped(codepoint);
}

// --- Mouse buttons ---

bool InputSystem::isMouseButtonPressed(int button) const {
    return state_.isMouseButtonPressed(button);
}

bool InputSystem::isMouseButtonJustPressed(int button) const {
    return state_.isMouseButtonJustPressed(button);
}

bool InputSystem::isMouseButtonJustReleased(int button) const {
    return state_.isMouseButtonJustReleased(button);
}

// --- Mouse movement ---

glm::vec2 InputSystem::mouseDelta() const {
    return state_.mouseDelta();
}

glm::vec2 InputSystem::mousePosition() const {
    return state_.mousePosition();
}

float InputSystem::scrollDelta() const {
    return state_.scrollDelta();
}

// --- Cursor control ---

void InputSystem::lockCursor() {
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window_, &cursorX, &cursorY);
    mousePos_ = glm::vec2(static_cast<float>(cursorX), static_cast<float>(cursorY));
    mouseDeltaAccum_ = glm::vec2(0.0f);
    firstMouse_ = false;
    state_.setMousePosition(mousePos_);
    state_.setMouseDelta(glm::vec2(0.0f));
    state_.setCursorLocked(true);
}

void InputSystem::unlockCursor() {
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }

    mouseDeltaAccum_ = glm::vec2(0.0f);
    firstMouse_ = true;
    state_.setMouseDelta(glm::vec2(0.0f));
    state_.setCursorLocked(false);
}

bool InputSystem::isCursorLocked() const {
    return state_.isCursorLocked();
}

bool InputSystem::wantsCaptureMouse() const {
    return state_.wantsCaptureMouse();
}

// --- GLFW Callbacks ---

void InputSystem::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    if (!instance_) return;

    glm::vec2 pos(static_cast<float>(xpos), static_cast<float>(ypos));

    if (instance_->firstMouse_) {
        instance_->mousePos_ = pos;
        instance_->firstMouse_ = false;
    } else {
        instance_->mouseDeltaAccum_ += pos - instance_->mousePos_;
        instance_->mousePos_ = pos;
    }
}

void InputSystem::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window;
    (void)xoffset;
    if (!instance_) return;

    instance_->scrollAccum_ += static_cast<float>(yoffset);
}

void InputSystem::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)window;
    (void)mods;
    if (!instance_ || action != GLFW_PRESS) return;

    instance_->keyPressEventsAccum_.push_back({key, scancode});
}

void InputSystem::charCallback(GLFWwindow* window, unsigned int codepoint) {
    (void)window;
    if (!instance_) return;

    instance_->typedCharactersAccum_.push_back(codepoint);
}
