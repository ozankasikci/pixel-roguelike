#include "editor/debug/SessionPlayer.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <fstream>
#include <stdexcept>

nlohmann::json SessionPlayer::load(const std::string& file) {
    try {
        std::ifstream in(file);
        if (!in.is_open()) {
            return {{"ok", false}, {"error", "File not found: " + file}};
        }
        nlohmann::json doc = nlohmann::json::parse(in);
        events_ = doc.value("events", nlohmann::json::array()).get<std::vector<nlohmann::json>>();
        playIndex_ = 0;
        playing_ = false;
        paused_ = false;
        spdlog::info("SessionPlayer: loaded {} events from {}", events_.size(), file);
        return {{"ok", true}, {"events", static_cast<int>(events_.size())}};
    } catch (const std::exception& e) {
        return {{"ok", false}, {"error", std::string("Load error: ") + e.what()}};
    }
}

nlohmann::json SessionPlayer::play() {
    if (events_.empty()) {
        return {{"ok", false}, {"error", "No session loaded"}};
    }
    playing_ = true;
    paused_ = false;
    playIndex_ = 0;
    playStartTime_ = std::chrono::steady_clock::now();
    spdlog::info("SessionPlayer: playing back {} events", events_.size());
    return {{"ok", true}};
}

nlohmann::json SessionPlayer::pause() {
    if (!playing_) {
        return {{"ok", false}, {"error", "Not playing"}};
    }
    paused_ = !paused_;
    return {{"ok", true}, {"paused", paused_}};
}

nlohmann::json SessionPlayer::step(CommandRegistry& registry) {
    if (playIndex_ >= events_.size()) {
        return {{"ok", false}, {"error", "No more events"}};
    }
    const nlohmann::json& event = events_[playIndex_];
    ++playIndex_;

    std::string type = event.value("type", "");
    if (type == "command") {
        std::string cmd = event.value("cmd", "");
        nlohmann::json args = event.contains("args") ? event["args"] : nlohmann::json::object();
        registry.dispatch(cmd, args);
    } else if (type == "snapshot") {
        // Log snapshot but don't enforce yet
        spdlog::debug("SessionPlayer: snapshot event at index {}", playIndex_ - 1);
    }

    return {{"ok", true}, {"index", static_cast<int>(playIndex_)}};
}

void SessionPlayer::tick(CommandRegistry& registry) {
    if (!playing_ || paused_) {
        return;
    }
    if (playIndex_ >= events_.size()) {
        playing_ = false;
        spdlog::info("SessionPlayer: playback complete");
        return;
    }

    auto now = std::chrono::steady_clock::now();
    long long elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - playStartTime_).count();

    while (playIndex_ < events_.size()) {
        const nlohmann::json& event = events_[playIndex_];
        long long eventT = event.value("t", 0LL);
        if (eventT > elapsedMs) {
            break;
        }

        std::string type = event.value("type", "");
        if (type == "command") {
            std::string cmd = event.value("cmd", "");
            nlohmann::json args = event.contains("args") ? event["args"] : nlohmann::json::object();
            registry.dispatch(cmd, args);
        } else if (type == "snapshot") {
            spdlog::debug("SessionPlayer: snapshot at t={}ms", eventT);
        }
        ++playIndex_;
    }

    if (playIndex_ >= events_.size()) {
        playing_ = false;
        spdlog::info("SessionPlayer: playback complete");
    }
}
