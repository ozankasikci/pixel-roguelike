#include "engine/input/ActionMap.h"

void ActionMap::bind(std::string_view action, ActionBinding binding) {
    bindings_[std::string(action)] = std::move(binding);
}

void ActionMap::unbind(std::string_view action) {
    bindings_.erase(std::string(action));
}

bool ActionMap::isPressed(std::string_view action) const {
    const auto it = currentState_.find(std::string(action));
    return it != currentState_.end() && it->second;
}

bool ActionMap::isJustPressed(std::string_view action) const {
    const std::string key(action);
    const auto cur = currentState_.find(key);
    const auto prev = previousState_.find(key);
    const bool curPressed = (cur != currentState_.end() && cur->second);
    const bool prevPressed = (prev != previousState_.end() && prev->second);
    return curPressed && !prevPressed;
}

bool ActionMap::isJustReleased(std::string_view action) const {
    const std::string key(action);
    const auto cur = currentState_.find(key);
    const auto prev = previousState_.find(key);
    const bool curPressed = (cur != currentState_.end() && cur->second);
    const bool prevPressed = (prev != previousState_.end() && prev->second);
    return !curPressed && prevPressed;
}

bool ActionMap::hasAction(std::string_view action) const {
    return bindings_.count(std::string(action)) > 0;
}

void ActionMap::update(const std::array<bool, 512>& currentKeys,
                       const std::array<bool, 512>& previousKeys,
                       const std::array<bool, 8>& currentButtons,
                       const std::array<bool, 8>& previousButtons) {
    (void)previousKeys;
    (void)previousButtons;

    previousState_ = currentState_;

    for (const auto& [name, binding] : bindings_) {
        bool pressed = false;

        for (int key : binding.keys) {
            if (key >= 0 && key < 512 && currentKeys[static_cast<std::size_t>(key)]) {
                pressed = true;
                break;
            }
        }

        if (!pressed) {
            for (int btn : binding.mouseButtons) {
                if (btn >= 0 && btn < 8 && currentButtons[static_cast<std::size_t>(btn)]) {
                    pressed = true;
                    break;
                }
            }
        }

        currentState_[name] = pressed;
    }
}
