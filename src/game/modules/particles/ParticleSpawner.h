#pragma once

#include <glm/glm.hpp>

#include <string>

class LevelBuilder;
namespace entt { enum class entity : unsigned int; }

entt::entity spawnParticleEmitter(LevelBuilder& builder,
                                  const std::string& emitterId,
                                  const glm::vec3& position,
                                  const std::string& nodeId = {},
                                  const std::string& parentNodeId = {});
