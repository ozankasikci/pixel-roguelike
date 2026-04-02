#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

class SessionRecorder {
public:
    SessionRecorder() = default;
    SessionRecorder(const SessionRecorder&) = delete;
    SessionRecorder& operator=(const SessionRecorder&) = delete;

    nlohmann::json start(const std::string& file, const std::string& scenePath);
    nlohmann::json stop();
    nlohmann::json status() const;

    void recordCommand(const std::string& cmd, const nlohmann::json& args);
    void recordSnapshot(const nlohmann::json& stateData);

    bool isRecording() const { return recording_; }

private:
    long long elapsedMs() const;

    bool recording_ = false;
    std::string filePath_;
    std::string scenePath_;
    std::vector<nlohmann::json> events_;
    std::chrono::steady_clock::time_point startTime_;
};
