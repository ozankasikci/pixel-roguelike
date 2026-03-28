#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <vector>

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
    std::vector<std::filesystem::path> takeDroppedPaths();

private:
    static void dropCallback(GLFWwindow* window, int pathCount, const char* paths[]);

    GLFWwindow* window_ = nullptr;
    std::vector<std::filesystem::path> droppedPaths_;
};
