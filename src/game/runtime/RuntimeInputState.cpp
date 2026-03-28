#include "game/runtime/RuntimeInputState.h"

#include <GLFW/glfw3.h>

#include <algorithm>

void RuntimeInputState::beginFrame() {
    previousKeys_ = currentKeys_;
    previousButtons_ = currentButtons_;
    mouseDelta_ = glm::vec2(0.0f);
    scrollDelta_ = 0.0f;
    typedCharacters_.clear();
    keyPressEvents_.clear();
}

void RuntimeInputState::reset() {
    currentKeys_.fill(false);
    previousKeys_.fill(false);
    currentButtons_.fill(false);
    previousButtons_.fill(false);
    mousePos_ = glm::vec2(0.0f);
    mouseDelta_ = glm::vec2(0.0f);
    scrollDelta_ = 0.0f;
    cursorLocked_ = false;
    wantsCaptureMouse_ = false;
    typedCharacters_.clear();
    keyPressEvents_.clear();
}

void RuntimeInputState::setKeyPressed(int key, bool pressed) {
    if (key < 0 || key >= MaxKeys) {
        return;
    }
    currentKeys_[static_cast<std::size_t>(key)] = pressed;
}

void RuntimeInputState::setMouseButtonPressed(int button, bool pressed) {
    if (button < 0 || button >= MaxButtons) {
        return;
    }
    currentButtons_[static_cast<std::size_t>(button)] = pressed;
}

void RuntimeInputState::setMousePosition(const glm::vec2& position) {
    mousePos_ = position;
}

void RuntimeInputState::setMouseDelta(const glm::vec2& delta) {
    mouseDelta_ = delta;
}

void RuntimeInputState::setScrollDelta(float delta) {
    scrollDelta_ = delta;
}

void RuntimeInputState::setWantsCaptureMouse(bool wantsCaptureMouse) {
    wantsCaptureMouse_ = wantsCaptureMouse;
}

void RuntimeInputState::setCursorLocked(bool cursorLocked) {
    cursorLocked_ = cursorLocked;
}

void RuntimeInputState::addKeyPressEvent(int key, int scancode) {
    keyPressEvents_.push_back(KeyPressEvent{key, scancode});
}

void RuntimeInputState::addTypedCharacter(unsigned int codepoint) {
    typedCharacters_.push_back(codepoint);
}

bool RuntimeInputState::isKeyPressed(int key) const {
    if (key < 0 || key >= MaxKeys) {
        return false;
    }
    return currentKeys_[static_cast<std::size_t>(key)];
}

bool RuntimeInputState::isKeyJustPressed(int key) const {
    if (key < 0 || key >= MaxKeys) {
        return false;
    }

    const std::size_t index = static_cast<std::size_t>(key);
    if (currentKeys_[index] && !previousKeys_[index]) {
        return true;
    }

    return std::any_of(keyPressEvents_.begin(), keyPressEvents_.end(), [key](const KeyPressEvent& event) {
        return event.key == key;
    });
}

bool RuntimeInputState::isKeyJustReleased(int key) const {
    if (key < 0 || key >= MaxKeys) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(key);
    return !currentKeys_[index] && previousKeys_[index];
}

bool RuntimeInputState::isKeyJustPressedByName(std::string_view keyName) const {
    for (const KeyPressEvent& event : keyPressEvents_) {
        const char* localizedName = glfwGetKeyName(event.key, event.scancode);
        if (localizedName != nullptr && keyName == localizedName) {
            return true;
        }
    }

    for (int key = 0; key < MaxKeys; ++key) {
        if (!(currentKeys_[static_cast<std::size_t>(key)] && !previousKeys_[static_cast<std::size_t>(key)])) {
            continue;
        }
        const char* localizedName = glfwGetKeyName(key, glfwGetKeyScancode(key));
        if (localizedName != nullptr && keyName == localizedName) {
            return true;
        }
    }

    return false;
}

bool RuntimeInputState::wasCharacterTyped(unsigned int codepoint) const {
    return std::find(typedCharacters_.begin(), typedCharacters_.end(), codepoint) != typedCharacters_.end();
}

bool RuntimeInputState::isMouseButtonPressed(int button) const {
    if (button < 0 || button >= MaxButtons) {
        return false;
    }
    return currentButtons_[static_cast<std::size_t>(button)];
}

bool RuntimeInputState::isMouseButtonJustPressed(int button) const {
    if (button < 0 || button >= MaxButtons) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(button);
    return currentButtons_[index] && !previousButtons_[index];
}

bool RuntimeInputState::isMouseButtonJustReleased(int button) const {
    if (button < 0 || button >= MaxButtons) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(button);
    return !currentButtons_[index] && previousButtons_[index];
}

glm::vec2 RuntimeInputState::mouseDelta() const {
    return mouseDelta_;
}

glm::vec2 RuntimeInputState::mousePosition() const {
    return mousePos_;
}

float RuntimeInputState::scrollDelta() const {
    return scrollDelta_;
}

bool RuntimeInputState::isCursorLocked() const {
    return cursorLocked_;
}

bool RuntimeInputState::wantsCaptureMouse() const {
    return wantsCaptureMouse_;
}
