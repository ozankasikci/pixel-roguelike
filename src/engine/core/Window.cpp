#include "Window.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

Window::Window(int width, int height, const char* title) {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // OpenGL 4.1 Core Profile - required on macOS
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // REQUIRED on macOS

    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetDropCallback(window_, &Window::dropCallback);

    glfwMakeContextCurrent(window_);

    // Load OpenGL functions via GLAD 2
    if (!gladLoadGL(glfwGetProcAddress)) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("Failed to load OpenGL functions via GLAD");
    }

    // Validate OpenGL version
    int major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    if (major < 4 || (major == 4 && minor < 1)) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error("OpenGL 4.1 or higher required");
    }

    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    spdlog::info("OpenGL {}.{} Core Profile", major, minor);
    spdlog::info("Renderer: {}", renderer ? renderer : "unknown");
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_) != 0;
}

void Window::swapBuffers() {
    glfwSwapBuffers(window_);
}

void Window::pollEvents() {
    glfwPollEvents();
}

int Window::width() const {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    return w;
}

int Window::height() const {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    return h;
}

std::vector<std::filesystem::path> Window::takeDroppedPaths() {
    std::vector<std::filesystem::path> result = std::move(droppedPaths_);
    droppedPaths_.clear();
    return result;
}

void Window::dropCallback(GLFWwindow* window, int pathCount, const char* paths[]) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self == nullptr || paths == nullptr || pathCount <= 0) {
        return;
    }

    self->droppedPaths_.reserve(self->droppedPaths_.size() + static_cast<std::size_t>(pathCount));
    for (int index = 0; index < pathCount; ++index) {
        if (paths[index] != nullptr) {
            self->droppedPaths_.emplace_back(paths[index]);
        }
    }
}
