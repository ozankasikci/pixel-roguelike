#pragma once

#include "game/behavior/ActionTypes.h"

#include <vector>

struct BehaviorComponent {
    std::vector<ActionEntry> onActivate;
    std::vector<ActionEntry> onEnter;
    std::vector<ActionEntry> onExit;
    std::vector<ActionEntry> onTimer;
    bool enabled = true;
};
