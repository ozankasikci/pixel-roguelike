#include "editor/debug/SessionRecorder.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

nlohmann::json SessionRecorder::start(const std::string& file, const std::string& scenePath) {
    if (recording_) {
        return {{"ok", false}, {"error", "Already recording"}};
    }

    if (file.empty()) {
        // Generate default path with timestamp
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        char tsBuf[32];
        std::strftime(tsBuf, sizeof(tsBuf), "%Y%m%d_%H%M%S", std::localtime(&t));
        filePath_ = std::string("assets/debug-sessions/") + tsBuf + ".editsession";
    } else {
        filePath_ = file;
    }

    // Ensure directory exists
    std::filesystem::path dir = std::filesystem::path(filePath_).parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
    }

    scenePath_ = scenePath;
    events_.clear();
    recording_ = true;
    startTime_ = std::chrono::steady_clock::now();

    // Record initial snapshot at t=0
    nlohmann::json initEvent = {
        {"t", 0},
        {"type", "snapshot"},
        {"data", {{"scene", scenePath_}}}
    };
    events_.push_back(std::move(initEvent));

    spdlog::info("SessionRecorder: started recording to {}", filePath_);
    return {{"ok", true}, {"file", filePath_}};
}

nlohmann::json SessionRecorder::stop() {
    if (!recording_) {
        return {{"ok", false}, {"error", "Not recording"}};
    }

    long long durationMs = elapsedMs();
    recording_ = false;

    nlohmann::json doc = {
        {"version",     1},
        {"scene",       scenePath_},
        {"duration_ms", durationMs},
        {"events",      events_}
    };

    try {
        std::ofstream out(filePath_);
        if (!out.is_open()) {
            return {{"ok", false}, {"error", "Failed to open file for writing: " + filePath_}};
        }
        out << doc.dump(2);
        out.close();
    } catch (const std::exception& e) {
        return {{"ok", false}, {"error", std::string("Write error: ") + e.what()}};
    }

    std::size_t eventCount = events_.size();
    events_.clear();

    spdlog::info("SessionRecorder: saved {} events ({} ms) to {}", eventCount, durationMs, filePath_);
    return {
        {"ok",         true},
        {"file",       filePath_},
        {"events",     static_cast<int>(eventCount)},
        {"duration_ms", durationMs}
    };
}

nlohmann::json SessionRecorder::status() const {
    if (!recording_) {
        return {
            {"ok", true},
            {"data", {
                {"recording",   false},
                {"duration_ms", 0},
                {"events",      0}
            }}
        };
    }

    return {
        {"ok", true},
        {"data", {
            {"recording",   true},
            {"duration_ms", elapsedMs()},
            {"events",      static_cast<int>(events_.size())},
            {"file",        filePath_}
        }}
    };
}

void SessionRecorder::recordCommand(const std::string& cmd, const nlohmann::json& args) {
    if (!recording_) {
        return;
    }
    events_.push_back({
        {"t",    elapsedMs()},
        {"type", "command"},
        {"cmd",  cmd},
        {"args", args}
    });
}

void SessionRecorder::recordSnapshot(const nlohmann::json& stateData) {
    if (!recording_) {
        return;
    }
    events_.push_back({
        {"t",    elapsedMs()},
        {"type", "snapshot"},
        {"data", stateData}
    });
}

long long SessionRecorder::elapsedMs() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime_).count();
}
