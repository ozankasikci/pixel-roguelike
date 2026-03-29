#pragma once

#include <array>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct ActionBinding {
    std::vector<int> keys;          // GLFW_KEY_* constants
    std::vector<int> mouseButtons;  // GLFW_MOUSE_BUTTON_* constants
};

class ActionMap {
public:
    void bind(std::string_view action, ActionBinding binding);
    void unbind(std::string_view action);
    bool isPressed(std::string_view action) const;
    bool isJustPressed(std::string_view action) const;
    bool isJustReleased(std::string_view action) const;
    bool hasAction(std::string_view action) const;

    // Called by InputSystem each frame -- not for game code
    void update(const std::array<bool, 512>& currentKeys,
                const std::array<bool, 512>& previousKeys,
                const std::array<bool, 8>& currentButtons,
                const std::array<bool, 8>& previousButtons);

private:
    std::unordered_map<std::string, ActionBinding> bindings_;
    std::unordered_map<std::string, bool> currentState_;
    std::unordered_map<std::string, bool> previousState_;
};
