#pragma once

class Time {
public:
    void update();       // call once per frame
    float deltaTime() const { return deltaTime_; }
    float elapsed() const { return elapsed_; }

private:
    float deltaTime_ = 0.0f;
    float elapsed_ = 0.0f;
    float lastTime_ = 0.0f;
    bool firstFrame_ = true;
};
