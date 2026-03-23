#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <any>

class EventBus {
public:
    template<typename EventType>
    void subscribe(std::function<void(const EventType&)> handler) {
        auto key = std::type_index(typeid(EventType));
        subscribers_[key].push_back([handler](const std::any& event) {
            handler(std::any_cast<const EventType&>(event));
        });
    }

    template<typename EventType>
    void publish(const EventType& event) {
        auto key = std::type_index(typeid(EventType));
        auto it = subscribers_.find(key);
        if (it != subscribers_.end()) {
            for (auto& handler : it->second) {
                handler(event);
            }
        }
    }

private:
    std::unordered_map<std::type_index, std::vector<std::function<void(const std::any&)>>> subscribers_;
};
