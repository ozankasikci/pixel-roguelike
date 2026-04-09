#include "engine/core/Application.h"
#include "engine/core/System.h"
#include "engine/scene/SceneManager.h"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

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
    int frameCount = 0;
    double frameTotalMs = 0.0;
    double sceneTotalMs = 0.0;
    double systemsTotalMs = 0.0;
    double swapTotalMs = 0.0;
    double maxFrameMs = 0.0;
    constexpr int kLogInterval = 300; // log every 300 frames (~5s at 60fps)

    while (running_ && !window_.shouldClose()) {
        const double frameStart = glfwGetTime();

        window_.pollEvents();
        time_.update();

        float dt = time_.deltaTime();

        const double sceneStart = glfwGetTime();
        if (sceneManager_ != nullptr) {
            sceneManager_->updateActive(*this, dt);
        }
        const double sceneEnd = glfwGetTime();

        const double systemsStart = glfwGetTime();
        static int systemLogCounter = 0;
        bool logSystems = (++systemLogCounter >= kLogInterval);
        if (logSystems) systemLogCounter = 0;
        for (std::size_t phase = 0; phase < systemsByPhase_.size(); ++phase) {
            for (auto& system : systemsByPhase_[phase]) {
                const double sysStart = glfwGetTime();
                system->update(*this, dt);
                if (logSystems) {
                    const double sysMs = (glfwGetTime() - sysStart) * 1000.0;
                    if (sysMs > 0.1) {
                        spdlog::info("[Perf] phase {} system: {:.1f}ms", phase, sysMs);
                    }
                }
            }
        }
        const double systemsEnd = glfwGetTime();

        const double swapStart = glfwGetTime();
        window_.swapBuffers();
        const double swapEnd = glfwGetTime();

        const double frameMs = (swapEnd - frameStart) * 1000.0;
        frameTotalMs += frameMs;
        sceneTotalMs += (sceneEnd - sceneStart) * 1000.0;
        systemsTotalMs += (systemsEnd - systemsStart) * 1000.0;
        swapTotalMs += (swapEnd - swapStart) * 1000.0;
        if (frameMs > maxFrameMs) maxFrameMs = frameMs;
        ++frameCount;

        if (frameCount >= kLogInterval) {
            const double avgFrame = frameTotalMs / frameCount;
            const double avgScene = sceneTotalMs / frameCount;
            const double avgSystems = systemsTotalMs / frameCount;
            const double avgSwap = swapTotalMs / frameCount;
            spdlog::info("[Perf] avg {:.1f}ms/frame ({:.0f} FPS) | scene {:.1f}ms | systems {:.1f}ms | swap {:.1f}ms | max {:.1f}ms",
                         avgFrame, 1000.0 / avgFrame, avgScene, avgSystems, avgSwap, maxFrameMs);
            frameCount = 0;
            frameTotalMs = 0.0;
            sceneTotalMs = 0.0;
            systemsTotalMs = 0.0;
            swapTotalMs = 0.0;
            maxFrameMs = 0.0;
        }
    }

    for (auto phaseIt = systemsByPhase_.rbegin(); phaseIt != systemsByPhase_.rend(); ++phaseIt) {
        for (auto systemIt = phaseIt->rbegin(); systemIt != phaseIt->rend(); ++systemIt) {
            (*systemIt)->shutdown();
        }
    }
}
