#pragma once
#include "engine/core/System.h"
#include <glm/glm.hpp>
#include <string_view>
#include <vector>

struct GLFWwindow;

class InputSystem : public System {
public:
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

    // Keyboard
    bool isKeyPressed(int key) const;
    bool isKeyJustPressed(int key) const;
    bool isKeyJustReleased(int key) const;
    bool isKeyJustPressedByName(std::string_view keyName) const;
    bool wasCharacterTyped(unsigned int codepoint) const;

    // Mouse buttons
    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonJustPressed(int button) const;
    bool isMouseButtonJustReleased(int button) const;

    // Mouse movement
    glm::vec2 mouseDelta() const;
    glm::vec2 mousePosition() const;
    float scrollDelta() const;

    // Cursor control
    void lockCursor();
    void unlockCursor();
    bool isCursorLocked() const;

    // ImGui integration
    bool wantsCaptureMouse() const;

private:
    struct KeyPressEvent {
        int key = -1;
        int scancode = -1;
    };

    GLFWwindow* window_ = nullptr;

    // Key state arrays (polled each frame via glfwGetKey)
    static constexpr int MAX_KEYS = 512;
    bool currentKeys_[MAX_KEYS] = {};
    bool previousKeys_[MAX_KEYS] = {};

    // Mouse button state (polled each frame via glfwGetMouseButton)
    static constexpr int MAX_BUTTONS = 8;
    bool currentButtons_[MAX_BUTTONS] = {};
    bool previousButtons_[MAX_BUTTONS] = {};

    // Mouse position & delta (callback-driven, can't poll deltas)
    glm::vec2 mousePos_{0.0f};
    glm::vec2 mouseDelta_{0.0f};
    glm::vec2 mouseDeltaAccum_{0.0f};
    bool firstMouse_ = true;

    // Scroll (callback-driven, can't poll)
    float scrollDelta_ = 0.0f;
    float scrollAccum_ = 0.0f;

    // Printable characters typed this frame, independent of keyboard layout.
    std::vector<unsigned int> typedCharacters_;
    std::vector<unsigned int> typedCharactersAccum_;
    std::vector<KeyPressEvent> keyPressEvents_;
    std::vector<KeyPressEvent> keyPressEventsAccum_;

    // Cursor lock state
    bool cursorLocked_ = false;

    // Static instance for GLFW callbacks
    static InputSystem* instance_;
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
};
