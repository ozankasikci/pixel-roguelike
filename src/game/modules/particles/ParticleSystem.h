#pragma once

#include "engine/particles/ParticleEmitter.h"
#include "engine/particles/ParticleTypes.h"
#include "engine/ecs/GameRegistry.h"

#include <memory>
#include <unordered_map>
#include <vector>

class ContentRegistry;

class ParticleUpdateSystem {
public:
    void init(const ContentRegistry& content);
    void update(GameRegistry& registry, float deltaTime, const glm::vec3& cameraPos);
    const std::vector<particles::ParticleRenderBatch>& renderBatches() const {
        return renderBatches_;
    }

private:
    void ensureEmitter(entt::entity entity, const std::string& emitterId);

    const ContentRegistry* content_ = nullptr;
    std::unordered_map<entt::entity, std::unique_ptr<particles::ParticleEmitter>> emitters_;
    std::vector<particles::ParticleRenderBatch> renderBatches_;
};
