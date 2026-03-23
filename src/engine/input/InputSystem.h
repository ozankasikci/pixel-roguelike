#pragma once
#include "engine/core/System.h"

struct GLFWwindow;

class InputSystem : public System {
public:
    void init(Application& app) override;
    void update(Application& app, float deltaTime) override;
    void shutdown() override;

    bool isKeyPressed(int key) const;
    bool wantsCaptureMouse() const;  // wraps ImGui::GetIO().WantCaptureMouse

private:
    GLFWwindow* window_ = nullptr;
};
