#pragma once
#include <memory>
#include <vector>
#include "engine/scene/Scene.h"

class Application;

class SceneManager {
public:
    void pushScene(std::unique_ptr<Scene> scene, Application& app);
    void popScene(Application& app);
    Scene* activeScene() const;
    bool empty() const { return scenes_.empty(); }
    void updateActive(Application& app, float deltaTime);

private:
    std::vector<std::unique_ptr<Scene>> scenes_;
};
