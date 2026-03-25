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

    // Swap key state: previous = last frame, current = poll GLFW
    std::memcpy(previousKeys_, currentKeys_, sizeof(currentKeys_));
    for (int i = 0; i < MAX_KEYS; ++i) {
        currentKeys_[i] = glfwGetKey(window_, i) == GLFW_PRESS;
    }

    // Swap button state: previous = last frame, current = poll GLFW
    std::memcpy(previousButtons_, currentButtons_, sizeof(currentButtons_));
    for (int i = 0; i < MAX_BUTTONS; ++i) {
        currentButtons_[i] = glfwGetMouseButton(window_, i) == GLFW_PRESS;
    }

    // Transfer accumulated mouse delta from callbacks
    mouseDelta_ = mouseDeltaAccum_;
    mouseDeltaAccum_ = glm::vec2(0.0f);

    // Transfer accumulated scroll delta from callbacks
    scrollDelta_ = scrollAccum_;
    scrollAccum_ = 0.0f;

    keyPressEvents_.swap(keyPressEventsAccum_);
    keyPressEventsAccum_.clear();
    typedCharacters_.swap(typedCharactersAccum_);
    typedCharactersAccum_.clear();

    // Toggle cursor lock with Escape (for debug tools)
    if (isKeyJustPressed(GLFW_KEY_ESCAPE)) {
        if (cursorLocked_) {
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
    if (key < 0 || key >= MAX_KEYS) return false;
    return currentKeys_[key];
}

bool InputSystem::isKeyJustPressed(int key) const {
    if (key < 0 || key >= MAX_KEYS) return false;
    if (currentKeys_[key] && !previousKeys_[key]) {
        return true;
    }

    for (const KeyPressEvent& event : keyPressEvents_) {
        if (event.key == key) {
            return true;
        }
    }

    return false;
}

bool InputSystem::isKeyJustReleased(int key) const {
    if (key < 0 || key >= MAX_KEYS) return false;
    return !currentKeys_[key] && previousKeys_[key];
}

bool InputSystem::isKeyJustPressedByName(std::string_view keyName) const {
    if (window_ == nullptr) {
        return false;
    }

    for (const KeyPressEvent& event : keyPressEvents_) {
        const char* localizedName = glfwGetKeyName(event.key, event.scancode);
        if (localizedName != nullptr && keyName == localizedName) {
            return true;
        }
    }

    for (int key = 0; key < MAX_KEYS; ++key) {
        if (!(currentKeys_[key] && !previousKeys_[key])) {
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
    for (unsigned int typed : typedCharacters_) {
        if (typed == codepoint) {
            return true;
        }
    }
    return false;
}

// --- Mouse buttons ---

bool InputSystem::isMouseButtonPressed(int button) const {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return currentButtons_[button];
}

bool InputSystem::isMouseButtonJustPressed(int button) const {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return currentButtons_[button] && !previousButtons_[button];
}

bool InputSystem::isMouseButtonJustReleased(int button) const {
    if (button < 0 || button >= MAX_BUTTONS) return false;
    return !currentButtons_[button] && previousButtons_[button];
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
    mouseDelta_ = glm::vec2(0.0f);
    mouseDeltaAccum_ = glm::vec2(0.0f);
    cursorLocked_ = true;
    firstMouse_ = false;
}

void InputSystem::unlockCursor() {
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
    }

    mouseDelta_ = glm::vec2(0.0f);
    mouseDeltaAccum_ = glm::vec2(0.0f);
    firstMouse_ = true;
    cursorLocked_ = false;
}

bool InputSystem::isCursorLocked() const {
    return cursorLocked_;
}

bool InputSystem::wantsCaptureMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
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
