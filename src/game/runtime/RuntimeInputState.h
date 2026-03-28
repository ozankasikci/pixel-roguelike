#pragma once

#include <array>
#include <glm/vec2.hpp>
#include <string_view>
#include <vector>

struct RuntimeInputState {
    struct KeyPressEvent {
        int key = -1;
        int scancode = -1;
    };

    static constexpr int MaxKeys = 512;
    static constexpr int MaxButtons = 8;

    void beginFrame();
    void reset();

    void setKeyPressed(int key, bool pressed);
    void setMouseButtonPressed(int button, bool pressed);
    void setMousePosition(const glm::vec2& position);
    void setMouseDelta(const glm::vec2& delta);
    void setScrollDelta(float delta);
    void setWantsCaptureMouse(bool wantsCaptureMouse);
    void setCursorLocked(bool cursorLocked);
    void addKeyPressEvent(int key, int scancode);
    void addTypedCharacter(unsigned int codepoint);

    bool isKeyPressed(int key) const;
    bool isKeyJustPressed(int key) const;
    bool isKeyJustReleased(int key) const;
    bool isKeyJustPressedByName(std::string_view keyName) const;
    bool wasCharacterTyped(unsigned int codepoint) const;

    bool isMouseButtonPressed(int button) const;
    bool isMouseButtonJustPressed(int button) const;
    bool isMouseButtonJustReleased(int button) const;

    glm::vec2 mouseDelta() const;
    glm::vec2 mousePosition() const;
    float scrollDelta() const;

    bool isCursorLocked() const;
    bool wantsCaptureMouse() const;

private:
    std::array<bool, MaxKeys> currentKeys_{};
    std::array<bool, MaxKeys> previousKeys_{};
    std::array<bool, MaxButtons> currentButtons_{};
    std::array<bool, MaxButtons> previousButtons_{};
    glm::vec2 mousePos_{0.0f};
    glm::vec2 mouseDelta_{0.0f};
    float scrollDelta_ = 0.0f;
    bool cursorLocked_ = false;
    bool wantsCaptureMouse_ = false;
    std::vector<unsigned int> typedCharacters_;
    std::vector<KeyPressEvent> keyPressEvents_;
};
