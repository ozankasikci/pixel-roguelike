#include "engine/core/Application.h"
#include "engine/core/System.h"

Application::Application(int width, int height, const char* title)
    : window_(width, height, title)
{
}

Application::~Application() = default;

void Application::run() {
    // Init all systems
    for (auto& system : systems_) {
        system->init(*this);
    }

    running_ = true;
    while (running_ && !window_.shouldClose()) {
        window_.pollEvents();
        time_.update();

        float dt = time_.deltaTime();
        for (auto& system : systems_) {
            system->update(*this, dt);
        }

        window_.swapBuffers();
    }

    // Shutdown in reverse order
    for (int i = static_cast<int>(systems_.size()) - 1; i >= 0; --i) {
        systems_[i]->shutdown();
    }
}
