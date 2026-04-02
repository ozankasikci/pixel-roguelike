#pragma once

#include "editor/debug/CommandRegistry.h"

#include <chrono>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class SessionPlayer {
public:
    SessionPlayer() = default;
    SessionPlayer(const SessionPlayer&) = delete;
    SessionPlayer& operator=(const SessionPlayer&) = delete;

    nlohmann::json load(const std::string& file);
    nlohmann::json play();
    nlohmann::json pause();
    nlohmann::json step(CommandRegistry& registry);

    void tick(CommandRegistry& registry);

    bool isPlaying() const { return playing_ && !paused_; }

private:
    std::vector<nlohmann::json> events_;
    std::size_t playIndex_ = 0;
    bool playing_ = false;
    bool paused_ = false;
    std::chrono::steady_clock::time_point playStartTime_;
};
