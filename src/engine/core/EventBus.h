#pragma once
#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <any>
#include <algorithm>
#include <cstdint>

// Main-thread-only. Not thread-safe.
class EventBus {
public:
    class SubscriptionToken {
    public:
        SubscriptionToken() = default;
        ~SubscriptionToken() { reset(); }

        SubscriptionToken(const SubscriptionToken&) = delete;
        SubscriptionToken& operator=(const SubscriptionToken&) = delete;

        SubscriptionToken(SubscriptionToken&& other) noexcept
            : bus_(other.bus_), key_(other.key_), id_(other.id_) {
            other.bus_ = nullptr;
        }

        SubscriptionToken& operator=(SubscriptionToken&& other) noexcept {
            if (this != &other) {
                reset();
                bus_ = other.bus_;
                key_ = other.key_;
                id_ = other.id_;
                other.bus_ = nullptr;
            }
            return *this;
        }

    private:
        friend class EventBus;
        SubscriptionToken(EventBus* bus, std::type_index key, uint64_t id)
            : bus_(bus), key_(key), id_(id) {}

        void reset() {
            if (bus_) {
                bus_->unsubscribe(key_, id_);
                bus_ = nullptr;
            }
        }

        EventBus* bus_ = nullptr;
        std::type_index key_{typeid(void)};
        uint64_t id_ = 0;
    };

    template<typename EventType>
    [[nodiscard]] SubscriptionToken subscribe(std::function<void(const EventType&)> handler) {
        auto key = std::type_index(typeid(EventType));
        uint64_t id = nextId_++;
        subscribers_[key].push_back({id, [handler](const std::any& event) {
            handler(std::any_cast<const EventType&>(event));
        }});
        return SubscriptionToken(this, key, id);
    }

    template<typename EventType>
    void publish(const EventType& event) {
        auto key = std::type_index(typeid(EventType));
        auto it = subscribers_.find(key);
        if (it != subscribers_.end()) {
            for (auto& [subId, handler] : it->second) {
                handler(event);
            }
        }
    }

private:
    void unsubscribe(std::type_index key, uint64_t id) {
        auto it = subscribers_.find(key);
        if (it != subscribers_.end()) {
            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [id](const auto& pair) { return pair.first == id; }),
                vec.end());
        }
    }

    uint64_t nextId_ = 0;
    std::unordered_map<std::type_index,
        std::vector<std::pair<uint64_t, std::function<void(const std::any&)>>>> subscribers_;
};
