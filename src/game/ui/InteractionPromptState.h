#pragma once

#include <string>

struct InteractionPromptState {
    bool visible = false;
    bool busy = false;
    std::string text;
};
