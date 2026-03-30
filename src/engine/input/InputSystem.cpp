#include "engine/input/InputSystem.h"
#include "engine/core/Application.h"
#include "engine/core/Window.h"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <algorithm>
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

    beginFrame();

    for (int i = 0; i < kMaxKeys; ++i) {
        currentKeys_[static_cast<std::size_t>(i)] = (glfwGetKey(window_, i) == GLFW_PRESS);
    }

    for (int i = 0; i < kMaxButtons; ++i) {
        currentButtons_[static_cast<std::size_t>(i)] = (glfwGetMouseButton(window_, i) == GLFW_PRESS);
    }

    mouseDelta_ = mouseDeltaAccum_;
    mouseDeltaAccum_ = glm::vec2(0.0f);
    scrollDelta_ = scrollAccum_;
    scrollAccum_ = 0.0f;

    keyPressEvents_ = keyPressEventsAccum_;
    keyPressEventsAccum_.clear();
    typedCharacters_ = typedCharactersAccum_;
    typedCharactersAccum_.clear();

    wantsCaptureMouse_ = ImGui::GetIO().WantCaptureMouse;

    // Toggle cursor lock with Escape (for debug tools)
    if (isKeyJustPressed(GLFW_KEY_ESCAPE)) {
        if (cursorLocked_) {
            unlockCursor();
        } else {
            lockCursor();
        }
    }

    actionMap_.update(currentKeys_, previousKeys_, currentButtons_, previousButtons_);
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

// --- Manual state injection (for editor preview) ---

void InputSystem::beginFrame() {
    previousKeys_ = currentKeys_;
    previousButtons_ = currentButtons_;
    mouseDelta_ = glm::vec2(0.0f);
    scrollDelta_ = 0.0f;
    keyPressEvents_.clear();
    typedCharacters_.clear();
}

void InputSystem::reset() {
    currentKeys_.fill(false);
    previousKeys_.fill(false);
    currentButtons_.fill(false);
    previousButtons_.fill(false);
    mousePos_ = glm::vec2(0.0f);
    mouseDelta_ = glm::vec2(0.0f);
    scrollDelta_ = 0.0f;
    cursorLocked_ = false;
    wantsCaptureMouse_ = false;
    keyPressEvents_.clear();
    keyPressEventsAccum_.clear();
    typedCharacters_.clear();
    typedCharactersAccum_.clear();
    mouseDeltaAccum_ = glm::vec2(0.0f);
    scrollAccum_ = 0.0f;
}

void InputSystem::setKeyPressed(int key, bool pressed) {
    if (key >= 0 && key < kMaxKeys) {
        currentKeys_[static_cast<std::size_t>(key)] = pressed;
    }
}

void InputSystem::setMouseButtonPressed(int button, bool pressed) {
    if (button >= 0 && button < kMaxButtons) {
        currentButtons_[static_cast<std::size_t>(button)] = pressed;
    }
}

void InputSystem::setMousePosition(const glm::vec2& position) {
    mousePos_ = position;
}

void InputSystem::setMouseDelta(const glm::vec2& delta) {
    mouseDelta_ = delta;
}

void InputSystem::setScrollDelta(float delta) {
    scrollDelta_ = delta;
}

void InputSystem::setCursorLocked(bool locked) {
    cursorLocked_ = locked;
}

void InputSystem::setWantsCaptureMouse(bool wantsCaptureMouse) {
    wantsCaptureMouse_ = wantsCaptureMouse;
}

void InputSystem::addKeyPressEvent(int key, int scancode) {
    keyPressEvents_.push_back(KeyPressEvent{key, scancode});
}

void InputSystem::addTypedCharacter(unsigned int codepoint) {
    typedCharacters_.push_back(codepoint);
}

// --- Keyboard ---

bool InputSystem::isKeyPressed(int key) const {
    if (key < 0 || key >= kMaxKeys) return false;
    return currentKeys_[static_cast<std::size_t>(key)];
}

bool InputSystem::isKeyJustPressed(int key) const {
    if (key < 0 || key >= kMaxKeys) return false;
    const std::size_t index = static_cast<std::size_t>(key);

    if (currentKeys_[index] && !previousKeys_[index]) {
        return true;
    }

    return std::any_of(keyPressEvents_.begin(), keyPressEvents_.end(), [key](const KeyPressEvent& event) {
        return event.key == key;
    });
}

bool InputSystem::isKeyJustReleased(int key) const {
    if (key < 0 || key >= kMaxKeys) return false;
    const std::size_t index = static_cast<std::size_t>(key);
    return !currentKeys_[index] && previousKeys_[index];
}

bool InputSystem::isKeyJustPressedByName(std::string_view keyName) const {
    for (const KeyPressEvent& event : keyPressEvents_) {
        const char* localizedName = glfwGetKeyName(event.key, event.scancode);
        if (localizedName != nullptr && keyName == localizedName) {
            return true;
        }
    }

    for (int key = 0; key < kMaxKeys; ++key) {
        const std::size_t index = static_cast<std::size_t>(key);
        if (!(currentKeys_[index] && !previousKeys_[index])) {
            continue;
        }
        const char* localizedName = glfwGetKeyName(key, glfwGetKeyScancode(key));
        if (localizedName != nullptr && keyName == localizedName) {
            return true;
        }
    }

    return false;
}

bool InputSystem::wasCharacterTyped(unsigned int codepoint) const {
    return std::find(typedCharacters_.begin(), typedCharacters_.end(), codepoint) != typedCharacters_.end();
}

// --- Mouse buttons ---

bool InputSystem::isMouseButtonPressed(int button) const {
    if (button < 0 || button >= kMaxButtons) return false;
    return currentButtons_[static_cast<std::size_t>(button)];
}

bool InputSystem::isMouseButtonJustPressed(int button) const {
    if (button < 0 || button >= kMaxButtons) return false;
    const std::size_t index = static_cast<std::size_t>(button);
    return currentButtons_[index] && !previousButtons_[index];
}

bool InputSystem::isMouseButtonJustReleased(int button) const {
    if (button < 0 || button >= kMaxButtons) return false;
    const std::size_t index = static_cast<std::size_t>(button);
    return !currentButtons_[index] && previousButtons_[index];
}

// --- Mouse movement ---

glm::vec2 InputSystem::mouseDelta() const {
    return mouseDelta_;
}

glm::vec2 InputSystem::mousePosition() const {
    return mousePos_;
}

float InputSystem::scrollDelta() const {
    return scrollDelta_;
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
    mouseDelta_ = glm::vec2(0.0f);
    firstMouse_ = false;
    cursorLocked_ = true;
}

void InputSystem::unlockCursor() {
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }

    mouseDeltaAccum_ = glm::vec2(0.0f);
    mouseDelta_ = glm::vec2(0.0f);
    firstMouse_ = true;
    cursorLocked_ = false;
}

bool InputSystem::isCursorLocked() const {
    return cursorLocked_;
}

bool InputSystem::wantsCaptureMouse() const {
    return wantsCaptureMouse_;
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
    if (!instance_) return;

    constexpr double kScrollScale = 0.2;
    double scaledY = yoffset * kScrollScale;
    double scaledX = xoffset * kScrollScale;

    instance_->scrollAccum_ += static_cast<float>(scaledY);

    // Forward scaled scroll to ImGui (its own callback was overridden by ours)
    ImGui::GetIO().AddMouseWheelEvent(static_cast<float>(scaledX), static_cast<float>(scaledY));
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
