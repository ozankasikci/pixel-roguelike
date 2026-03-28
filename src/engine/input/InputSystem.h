#pragma once
#include "engine/core/System.h"
#include "game/runtime/RuntimeInputState.h"
#include <glm/glm.hpp>
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
    RuntimeInputState& state() { return state_; }
    const RuntimeInputState& state() const { return state_; }

private:
    GLFWwindow* window_ = nullptr;
    glm::vec2 mousePos_{0.0f};
    glm::vec2 mouseDeltaAccum_{0.0f};
    bool firstMouse_ = true;
    float scrollAccum_ = 0.0f;
    std::vector<unsigned int> typedCharactersAccum_;
    std::vector<RuntimeInputState::KeyPressEvent> keyPressEventsAccum_;
    RuntimeInputState state_;

    // Static instance for GLFW callbacks
    static InputSystem* instance_;
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
};
