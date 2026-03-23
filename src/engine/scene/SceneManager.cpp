#include "engine/scene/SceneManager.h"
#include "engine/core/Application.h"

void SceneManager::pushScene(std::unique_ptr<Scene> scene, Application& app) {
    scene->onEnter(app);
    scenes_.push_back(std::move(scene));
}

void SceneManager::popScene(Application& app) {
    if (!scenes_.empty()) {
        scenes_.back()->onExit(app);
        scenes_.pop_back();
    }
}

Scene* SceneManager::activeScene() const {
    if (scenes_.empty()) return nullptr;
    return scenes_.back().get();
}

void SceneManager::updateActive(Application& app, float deltaTime) {
    Scene* active = activeScene();
    if (active) {
        active->onUpdate(app, deltaTime);
    }
}
