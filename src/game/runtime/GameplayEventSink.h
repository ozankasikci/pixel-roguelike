#pragma once

#include <string>
#include <vector>

struct GameplayEvent {
    enum class Kind { PlaySound, ShowMessage, Custom };
    Kind kind;
    std::string id;       // soundId for PlaySound, text for ShowMessage, name for Custom
    float value = 0.0f;   // volume for PlaySound, duration for ShowMessage
};

class GameplayEventSink {
public:
    void emit(GameplayEvent event) { events_.push_back(std::move(event)); }
    const std::vector<GameplayEvent>& events() const { return events_; }
    void drain() { events_.clear(); }

private:
    std::vector<GameplayEvent> events_;
};
