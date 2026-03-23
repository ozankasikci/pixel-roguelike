#include "engine/core/Time.h"
#include <GLFW/glfw3.h>

void Time::update() {
    float currentTime = static_cast<float>(glfwGetTime());
    if (firstFrame_) {
        lastTime_ = currentTime;
        deltaTime_ = 0.0001f;
        firstFrame_ = false;
    } else {
        deltaTime_ = currentTime - lastTime_;
        if (deltaTime_ < 0.0001f) deltaTime_ = 0.0001f;
        lastTime_ = currentTime;
    }
    elapsed_ += deltaTime_;
}
