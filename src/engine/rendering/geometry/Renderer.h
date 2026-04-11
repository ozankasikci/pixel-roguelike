#pragma once

#include "engine/rendering/lighting/RenderLight.h"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <array>
#include <string>
#include <vector>

class Mesh;
class Shader;

struct RenderMaterialData {
    std::string id;
    float specularLevel = 0.20f;
    bool animated = false;
    bool subsurface = false;
    bool detailBrick = false;
    bool detailWood = false;
    bool detailStone = false;
    bool detailFloor = false;
    bool normalMapFlipY = false;
    bool alphaTest = false;
    float alphaCutoff = 0.5f;
    glm::vec3 baseColor{1.0f};
    bool useMaterialMaps = false;
    bool useProceduralDetail = false;
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
    float emissiveStrength = 0.0f;
    bool weatheringEnabled = false;
    float weatheringDirtStrength = 0.0f;
    glm::vec3 weatheringDirtColor{0.2f, 0.16f, 0.12f};
    float weatheringEdgeWearStrength = 0.0f;
    float weatheringDustStrength = 0.0f;
    float weatheringDampStrength = 0.0f;
    float weatheringNoiseScale = 0.3f;
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
    std::array<glm::mat4, kMaxShadowedSpotLights> matrices{};
    std::array<GLuint, kMaxShadowedSpotLights> textures{};
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
