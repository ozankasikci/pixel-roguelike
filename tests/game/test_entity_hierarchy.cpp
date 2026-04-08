#include "engine/ecs/EntityActiveState.h"
#include "engine/ecs/GameRegistry.h"
#include "engine/ecs/HierarchyComponents.h"
#include "game/behavior/NodeIdComponent.h"
#include "game/behavior/NodeIndex.h"
#include "game/components/TransformComponent.h"

#include <cassert>

// Simulate what LevelLoader does: create entities with nodeIds and parentNodeIds,
// build NodeIndex, then build hierarchy.
int main() {
    GameRegistry reg;

    // Create a scene: room -> table -> lamp
    auto room = reg.create();
    reg.emplace<NodeIdComponent>(room, NodeIdComponent{"room_1"});
    reg.emplace<TransformComponent>(room);

    auto table = reg.create();
    reg.emplace<NodeIdComponent>(table, NodeIdComponent{"table_1"});
    reg.emplace<TransformComponent>(table);

    auto lamp = reg.create();
    reg.emplace<NodeIdComponent>(lamp, NodeIdComponent{"lamp_1"});
    reg.emplace<TransformComponent>(lamp);

    // Build NodeIndex (same as LevelLoader does)
    NodeIndex nodeIndex;
    nodeIndex.add("room_1", room);
    nodeIndex.add("table_1", table);
    nodeIndex.add("lamp_1", lamp);

    // Simulate parentNodeId linking (room is parent of table, table is parent of lamp)
    auto linkParent = [&](entt::entity child, const std::string& parentNodeId) {
        entt::entity parent = nodeIndex.resolve(parentNodeId);
        if (parent != entt::null) {
            reg.emplace<ParentComponent>(child, ParentComponent{parent});
            auto& children = reg.get_or_emplace<ChildrenComponent>(parent);
            children.children.push_back(child);
        }
    };
    linkParent(table, "room_1");
    linkParent(lamp, "table_1");

    // --- Verify hierarchy was built correctly ---
    assert(reg.all_of<ParentComponent>(table));
    assert(reg.get<ParentComponent>(table).parent == room);
    assert(reg.all_of<ParentComponent>(lamp));
    assert(reg.get<ParentComponent>(lamp).parent == table);

    assert(reg.all_of<ChildrenComponent>(room));
    assert(reg.get<ChildrenComponent>(room).children.size() == 1);
    assert(reg.get<ChildrenComponent>(room).children[0] == table);

    assert(reg.all_of<ChildrenComponent>(table));
    assert(reg.get<ChildrenComponent>(table).children.size() == 1);
    assert(reg.get<ChildrenComponent>(table).children[0] == lamp);

    // Room has no parent
    assert(!reg.all_of<ParentComponent>(room));
    // Lamp has no children
    assert(!reg.all_of<ChildrenComponent>(lamp));

    // --- All entities start active ---
    assert(isActiveInHierarchy(reg, room));
    assert(isActiveInHierarchy(reg, table));
    assert(isActiveInHierarchy(reg, lamp));

    // --- Disable room -> table and lamp should also be disabled ---
    setEntityActive(reg, room, false);
    assert(!isActiveInHierarchy(reg, room));
    assert(!isActiveInHierarchy(reg, table));
    assert(!isActiveInHierarchy(reg, lamp));

    // activeSelf should only be false for room (explicitly disabled)
    assert(!isActiveSelf(reg, room));
    assert(isActiveSelf(reg, table));
    assert(isActiveSelf(reg, lamp));

    // --- view() should return nothing (all disabled) ---
    {
        int count = 0;
        for (auto entity : reg.view<TransformComponent>()) {
            (void)entity;
            count++;
        }
        assert(count == 0);
    }

    // --- viewAll() should still return all 3 entities ---
    {
        int count = 0;
        for (auto entity : reg.viewAll<TransformComponent>()) {
            (void)entity;
            count++;
        }
        assert(count == 3);
    }

    // --- Re-enable room -> everything comes back ---
    setEntityActive(reg, room, true);
    assert(isActiveInHierarchy(reg, room));
    assert(isActiveInHierarchy(reg, table));
    assert(isActiveInHierarchy(reg, lamp));

    {
        int count = 0;
        for (auto entity : reg.view<TransformComponent>()) {
            (void)entity;
            count++;
        }
        assert(count == 3);
    }

    // --- Disable table explicitly, then disable room, then re-enable room ---
    // Table should stay disabled because its activeSelf is false
    setEntityActive(reg, table, false);
    assert(!isActiveInHierarchy(reg, table));
    assert(!isActiveInHierarchy(reg, lamp));  // child of disabled table
    assert(isActiveInHierarchy(reg, room));   // room is still active

    setEntityActive(reg, room, false);
    assert(!isActiveInHierarchy(reg, room));

    setEntityActive(reg, room, true);
    assert(isActiveInHierarchy(reg, room));
    assert(!isActiveInHierarchy(reg, table));  // table was explicitly disabled
    assert(!isActiveSelf(reg, table));
    assert(!isActiveInHierarchy(reg, lamp));   // lamp is child of disabled table

    // view() should only return room
    {
        int count = 0;
        for (auto entity : reg.view<TransformComponent>()) {
            assert(entity == room);
            count++;
        }
        assert(count == 1);
    }

    // --- Re-enable table, everything comes back ---
    setEntityActive(reg, table, true);
    assert(isActiveInHierarchy(reg, table));
    assert(isActiveInHierarchy(reg, lamp));

    {
        int count = 0;
        for (auto entity : reg.view<TransformComponent>()) {
            (void)entity;
            count++;
        }
        assert(count == 3);
    }

    return 0;
}
