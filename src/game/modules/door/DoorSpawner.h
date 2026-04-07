#pragma once

#include <entt/entt.hpp>

class LevelBuilder;
struct LevelDoorPlacement;
struct LevelDef;

// Spawn a door group entity hierarchy (root + leaf meshes with DoorConfigComponent,
// DoorStateComponent, DoorLeafComponent, InteractableComponent, BehaviorComponent).
// Extracted from LevelBuilder::addDoorGroup().
entt::entity spawnDoorGroup(LevelBuilder& builder,
                             const LevelDoorPlacement& group,
                             const LevelDef& level);
