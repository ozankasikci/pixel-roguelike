#include "engine/core/Application.h"
#include "engine/core/System.h"
#include "engine/scene/SceneManager.h"

Application::Application(int width, int height, const char* title)
    : window_(width, height, title)
{
}

Application::~Application() = default;

void Application::run() {
    for (auto& systems : systemsByPhase_) {
        for (auto& system : systems) {
            system->init(*this);
        }
    }

    running_ = true;
    while (running_ && !window_.shouldClose()) {
        window_.pollEvents();
        time_.update();

        float dt = time_.deltaTime();
        if (sceneManager_ != nullptr) {
            sceneManager_->updateActive(*this, dt);
        }
        for (auto& systems : systemsByPhase_) {
            for (auto& system : systems) {
                system->update(*this, dt);
            }
        }

        window_.swapBuffers();
    }

    for (auto phaseIt = systemsByPhase_.rbegin(); phaseIt != systemsByPhase_.rend(); ++phaseIt) {
        for (auto systemIt = phaseIt->rbegin(); systemIt != phaseIt->rend(); ++systemIt) {
            (*systemIt)->shutdown();
        }
    }
}
