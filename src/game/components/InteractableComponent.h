#pragma once

#include <string>

struct InteractableComponent {
    std::string promptText = "E  INTERACT";
    std::string busyText = "INTERACTING";
    float interactDistance = 2.0f;
    float interactDotThreshold = 0.55f;
    bool enabled = true;
    bool busy = false;
};
