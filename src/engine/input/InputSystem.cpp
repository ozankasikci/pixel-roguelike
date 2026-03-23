#include "engine/input/InputSystem.h"
#include "engine/core/Application.h"
#include "engine/core/Window.h"
#include <GLFW/glfw3.h>
#include <imgui.h>

void InputSystem::init(Application& app) {
    window_ = app.window().handle();
}

void InputSystem::update(Application& app, float deltaTime) {
    // no-op: event processing happens in Window::pollEvents()
    (void)app;
    (void)deltaTime;
}

void InputSystem::shutdown() {
    // no-op
}

bool InputSystem::isKeyPressed(int key) const {
    return glfwGetKey(window_, key) == GLFW_PRESS;
}

bool InputSystem::wantsCaptureMouse() const {
    return ImGui::GetIO().WantCaptureMouse;
}
