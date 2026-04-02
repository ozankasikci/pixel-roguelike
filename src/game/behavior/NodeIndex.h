#pragma once

#include <entt/entt.hpp>
#include <string>
#include <unordered_map>

struct NodeIndex {
    std::unordered_map<std::string, entt::entity> byNodeId;

    void add(const std::string& nodeId, entt::entity entity) {
        if (!nodeId.empty()) {
            byNodeId[nodeId] = entity;
        }
    }

    entt::entity resolve(const std::string& nodeId) const {
        auto it = byNodeId.find(nodeId);
        return it != byNodeId.end() ? it->second : entt::null;
    }

    entt::entity resolve(const std::string& nodeId, entt::entity self) const {
        if (nodeId == "self" || nodeId.empty()) return self;
        return resolve(nodeId);
    }
};
