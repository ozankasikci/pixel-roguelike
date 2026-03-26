#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>

#include "game/rendering/MaterialKind.h"

class Mesh;
class Shader;

struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float radius;
    float intensity;
};

struct RenderObject {
    Mesh* mesh;
    glm::mat4 modelMatrix;
    glm::vec3 tint{1.0f};
    MaterialKind material = MaterialKind::Stone;
};

class Renderer {
public:
    explicit Renderer(Shader* shader);
    ~Renderer() = default;

    // Non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void drawScene(const std::vector<RenderObject>& objects,
                   const std::vector<PointLight>& lights,
                   const glm::mat4& view,
                   const glm::mat4& projection,
                   const glm::vec3& cameraPos);

private:
    Shader* shader_;
};
