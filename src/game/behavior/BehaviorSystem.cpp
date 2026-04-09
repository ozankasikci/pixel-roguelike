#include "game/behavior/BehaviorSystem.h"

#include <unordered_map>

namespace {

std::unordered_map<ActionType, BehaviorActionHandler>& actionHandlerRegistry() {
    static std::unordered_map<ActionType, BehaviorActionHandler> registry;
    return registry;
}

} // namespace

void registerBehaviorActionHandler(ActionType type, BehaviorActionHandler handler) {
    actionHandlerRegistry()[type] = std::move(handler);
}

BehaviorActionHandler* findBehaviorActionHandler(ActionType type) {
    auto& reg = actionHandlerRegistry();
    auto it = reg.find(type);
    return it != reg.end() ? &it->second : nullptr;
}
