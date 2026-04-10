#pragma once

#include <entt/entt.hpp>

struct InteractionFocusState {
    entt::entity actor = entt::null;
    entt::entity focused = entt::null;
    bool activationRequested = false;
    bool activationConsumed = false;
};
