#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <array>
#include <vector>

#include "game/rendering/MaterialKind.h"

class Mesh;
class Shader;

struct RenderObject {
    Mesh* mesh;
    glm::mat4 modelMatrix;
    glm::vec3 tint{1.0f};
    MaterialKind material = MaterialKind::Stone;
};

struct ShadowRenderData {
    int shadowCount = 0;
    std::array<glm::mat4, kMaxShadowedSpotLights> matrices{
        glm::mat4(1.0f),
        glm::mat4(1.0f),
    };
    std::array<GLuint, kMaxShadowedSpotLights> textures{0, 0};
};

class Renderer {
public:
    explicit Renderer(Shader* shader);
    ~Renderer() = default;

    // Non-copyable
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void drawScene(const std::vector<RenderObject>& objects,
                   const std::vector<RenderLight>& lights,
                   const LightingEnvironment& lighting,
                   const ShadowRenderData& shadowData,
                   const glm::mat4& view,
                   const glm::mat4& projection,
                   const glm::vec3& cameraPos);

private:
    Shader* shader_;
};
