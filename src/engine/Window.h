#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>

class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();

    // Non-copyable
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();

    GLFWwindow* handle() const { return window_; }

    int width() const;
    int height() const;

private:
    GLFWwindow* window_ = nullptr;
};
