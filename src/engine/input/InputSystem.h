#pragma once
#include "engine/core/System.h"
#include "engine/input/ActionMap.h"
#include <glm/glm.hpp>
#include <array>
#include <string_view>
#include <vector>

struct GLFWwindow;

class InputSystem : public System {
public:
    struct KeyPressEvent {
        int key = -1;
        int scancode = -1;
    };

    static constexpr int kMaxKeys = 512;
    static constexpr int kMaxButtons = 8;

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

    // Action map (D-03)
    ActionMap& actionMap() { return actionMap_; }
    const ActionMap& actionMap() const { return actionMap_; }

    // Manual state injection -- used by editor preview (not GLFW callback path)
    void beginFrame();
    void reset();
    void setKeyPressed(int key, bool pressed);
    void setMouseButtonPressed(int button, bool pressed);
    void setMousePosition(const glm::vec2& position);
    void setMouseDelta(const glm::vec2& delta);
    void setScrollDelta(float delta);
    void setCursorLocked(bool locked);
    void setWantsCaptureMouse(bool wantsCaptureMouse);
    void addKeyPressEvent(int key, int scancode);
    void addTypedCharacter(unsigned int codepoint);

    const std::vector<KeyPressEvent>& keyPressEvents() const { return keyPressEvents_; }
    const std::vector<unsigned int>& typedCharacters() const { return typedCharacters_; }

private:
    GLFWwindow* window_ = nullptr;
    glm::vec2 mousePos_{0.0f};
    glm::vec2 mouseDeltaAccum_{0.0f};
    bool firstMouse_ = true;
    float scrollAccum_ = 0.0f;

    // Raw input state -- managed directly, no game-layer dependency
    std::array<bool, kMaxKeys> currentKeys_{};
    std::array<bool, kMaxKeys> previousKeys_{};
    std::array<bool, kMaxButtons> currentButtons_{};
    std::array<bool, kMaxButtons> previousButtons_{};
    glm::vec2 mouseDelta_{0.0f};
    float scrollDelta_ = 0.0f;
    bool cursorLocked_ = false;
    bool wantsCaptureMouse_ = false;
    std::vector<KeyPressEvent> keyPressEventsAccum_;
    std::vector<KeyPressEvent> keyPressEvents_;
    std::vector<unsigned int> typedCharactersAccum_;
    std::vector<unsigned int> typedCharacters_;

    ActionMap actionMap_;

    // Static instance for GLFW callbacks
    static InputSystem* instance_;
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void charCallback(GLFWwindow* window, unsigned int codepoint);
};
