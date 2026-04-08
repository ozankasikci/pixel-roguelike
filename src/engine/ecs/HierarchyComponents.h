#pragma once

#include <entt/entt.hpp>
#include <vector>

// Runtime parent reference. Built during level loading from parentNodeId data.
struct ParentComponent {
    entt::entity parent = entt::null;
};

// Runtime children list. Built during level loading alongside ParentComponent.
struct ChildrenComponent {
    std::vector<entt::entity> children;
};
