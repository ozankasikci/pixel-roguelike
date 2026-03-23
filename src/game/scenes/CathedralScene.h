#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>

#include "engine/rendering/Renderer.h"
#include "engine/rendering/Mesh.h"

class CathedralScene {
public:
    CathedralScene() = default;
    ~CathedralScene() = default;

    // Non-copyable
    CathedralScene(const CathedralScene&) = delete;
    CathedralScene& operator=(const CathedralScene&) = delete;

    void build();

    const std::vector<RenderObject>& objects() const { return objects_; }
    std::vector<PointLight>& lights() { return lights_; }
    glm::vec3& cameraPos() { return cameraPos_; }

private:
    std::vector<std::unique_ptr<Mesh>> meshes_;  // owns mesh memory
    std::vector<RenderObject> objects_;
    std::vector<PointLight> lights_;
    glm::vec3 cameraPos_{0.0f, 2.0f, 5.0f};
};
