#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <array>
#include <string>
#include <vector>

#include "game/rendering/MaterialKind.h"

class Mesh;
class Shader;

struct RenderMaterialData {
    std::string id;
    MaterialKind shadingModel = MaterialKind::Stone;
    glm::vec3 baseColor{1.0f};
    bool useMaterialMaps = false;
    GLuint albedoTexture = 0;
    GLuint normalTexture = 0;
    GLuint roughnessTexture = 0;
    GLuint aoTexture = 0;
    int uvMode = 0;
    glm::vec2 uvScale{1.0f, 1.0f};
    float normalStrength = 1.0f;
    float roughnessScale = 1.0f;
    float roughnessBias = 0.0f;
    float metalness = 0.0f;
    float aoStrength = 1.0f;
    float lightTintResponse = 0.18f;
};

struct RenderObject {
    Mesh* mesh;
    glm::mat4 modelMatrix;
    glm::vec3 tint{1.0f};
    RenderMaterialData material;
    bool wireframe = false;
    bool ignoreDepth = false;
    bool unlit = false;
    float lineWidth = 1.0f;
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
