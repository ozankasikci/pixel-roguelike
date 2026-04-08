#include "engine/ecs/EntityActiveState.h"
#include "engine/ecs/HierarchyComponents.h"
#include "engine/ecs/ActiveStateComponent.h"

#include <cassert>

struct Renderable {};

// Helper: link parent -> child in hierarchy components
void linkParentChild(GameRegistry& reg, entt::entity parent, entt::entity child) {
    reg.emplace<ParentComponent>(child, ParentComponent{parent});
    auto& children = reg.get_or_emplace<ChildrenComponent>(parent);
    children.children.push_back(child);
}

int main() {
    // --- Disable single entity (no children) ---
    {
        GameRegistry reg;
        auto e = reg.create();
        reg.emplace<Renderable>(e);

        assert(isActiveSelf(reg, e));
        assert(isActiveInHierarchy(reg, e));

        setEntityActive(reg, e, false);
        assert(!isActiveSelf(reg, e));
        assert(!isActiveInHierarchy(reg, e));

        // view() should exclude it
        int count = 0;
        for (auto entity : reg.view<Renderable>()) { (void)entity; count++; }
        assert(count == 0);

        setEntityActive(reg, e, true);
        assert(isActiveSelf(reg, e));
        assert(isActiveInHierarchy(reg, e));
    }

    // --- Disable parent propagates to children ---
    {
        GameRegistry reg;
        auto room = reg.create();
        auto table = reg.create();
        auto lamp = reg.create();
        auto chair = reg.create();

        reg.emplace<Renderable>(room);
        reg.emplace<Renderable>(table);
        reg.emplace<Renderable>(lamp);
        reg.emplace<Renderable>(chair);

        linkParentChild(reg, room, table);
        linkParentChild(reg, room, chair);
        linkParentChild(reg, table, lamp);

        setEntityActive(reg, room, false);

        assert(!isActiveInHierarchy(reg, room));
        assert(!isActiveInHierarchy(reg, table));
        assert(!isActiveInHierarchy(reg, lamp));
        assert(!isActiveInHierarchy(reg, chair));

        // All still have activeSelf true except room
        assert(!isActiveSelf(reg, room));
        assert(isActiveSelf(reg, table));
        assert(isActiveSelf(reg, lamp));
        assert(isActiveSelf(reg, chair));
    }

    // --- Re-enable parent, but child with activeSelf=false stays disabled ---
    {
        GameRegistry reg;
        auto room = reg.create();
        auto table = reg.create();
        auto lamp = reg.create();
        auto chair = reg.create();

        reg.emplace<Renderable>(room);
        reg.emplace<Renderable>(table);
        reg.emplace<Renderable>(lamp);
        reg.emplace<Renderable>(chair);

        linkParentChild(reg, room, table);
        linkParentChild(reg, room, chair);
        linkParentChild(reg, table, lamp);

        // Explicitly disable table first
        setEntityActive(reg, table, false);
        assert(!isActiveInHierarchy(reg, table));
        assert(!isActiveInHierarchy(reg, lamp));  // child of disabled table

        // Now disable room
        setEntityActive(reg, room, false);

        // Re-enable room
        setEntityActive(reg, room, true);

        // room and chair should be active
        assert(isActiveInHierarchy(reg, room));
        assert(isActiveInHierarchy(reg, chair));

        // table was explicitly disabled -- stays disabled
        assert(!isActiveInHierarchy(reg, table));
        assert(!isActiveSelf(reg, table));

        // lamp is child of disabled table -- stays disabled
        assert(!isActiveInHierarchy(reg, lamp));
    }

    // --- Enable entity with disabled ancestor -> stays disabled ---
    {
        GameRegistry reg;
        auto parent = reg.create();
        auto child = reg.create();
        linkParentChild(reg, parent, child);

        setEntityActive(reg, parent, false);
        setEntityActive(reg, child, false);

        // Try to re-enable child while parent is disabled
        setEntityActive(reg, child, true);
        assert(isActiveSelf(reg, child));          // activeSelf is true
        assert(!isActiveInHierarchy(reg, child));  // but parent is disabled
    }

    return 0;
}
