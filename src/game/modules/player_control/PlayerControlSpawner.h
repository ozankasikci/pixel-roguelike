#pragma once

#include <string>

#include <entt/entity/fwd.hpp>

struct LevelDef;
struct RunSession;
class LevelBuilder;

void syncPlayerSpawnState(const LevelDef& level,
                          const std::string& levelId,
                          RunSession& session);

entt::entity spawnPlayerEntity(LevelBuilder& builder,
                               const LevelDef& level,
                               const RunSession& session);
