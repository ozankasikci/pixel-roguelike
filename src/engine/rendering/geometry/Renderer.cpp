#include "engine/rendering/geometry/Renderer.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Mesh.h"

#include <algorithm>
#include <cmath>
#include <string>

Renderer::Renderer(Shader* shader)
    : shader_(shader)
{}

void Renderer::drawScene(const std::vector<RenderObject>& objects,
                         const std::vector<RenderLight>& lights,
                         const LightingEnvironment& lighting,
                         const ShadowRenderData& shadowData,
                         const glm::mat4& view,
                         const glm::mat4& projection,
                         const glm::vec3& cameraPos)
{
    shader_->use();

    shader_->setMat4("uView", view);
    shader_->setMat4("uProjection", projection);
    shader_->setVec3("uCameraPos", cameraPos);
    shader_->setVec3("uHemisphereSkyColor", lighting.hemisphereSkyColor);
    shader_->setVec3("uHemisphereGroundColor", lighting.hemisphereGroundColor);
    shader_->setFloat("uHemisphereStrength", lighting.hemisphereStrength);
    shader_->setInt("uEnableDirectionalLights", lighting.enableDirectionalLights ? 1 : 0);
    shader_->setFloat("uDirectionalLightIntensityScale", lighting.directionalIntensityScale);
    shader_->setVec3("uDirectionalLightTint", lighting.directionalLightTint);
    shader_->setInt("uEnableShadows", lighting.enableShadows ? 1 : 0);
    shader_->setFloat("uShadowBias", lighting.shadowBias);
    shader_->setFloat("uShadowNormalBias", lighting.shadowNormalBias);

    int numLights = static_cast<int>(std::min(lights.size(), static_cast<size_t>(kMaxRenderLights)));
    shader_->setInt("uNumLights", numLights);

    shader_->setInt("uShadowCount", shadowData.shadowCount);
    for (int i = 0; i < kMaxShadowedSpotLights; ++i) {
        shader_->setMat4("uShadowMatrices[" + std::to_string(i) + "]", shadowData.matrices[static_cast<std::size_t>(i)]);
        const int textureUnit = 8 + i;
        shader_->setInt("uShadowMaps[" + std::to_string(i) + "]", textureUnit);
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, shadowData.textures[static_cast<std::size_t>(i)]);
    }
    glActiveTexture(GL_TEXTURE0);
    shader_->setInt("uAlbedoMap", 12);
    shader_->setInt("uNormalMap", 13);
    shader_->setInt("uRoughnessMap", 14);
    shader_->setInt("uAoMap", 15);

    for (int i = 0; i < numLights; ++i) {
        const RenderLight& light = lights[static_cast<std::size_t>(i)];
        std::string base = "uLights[" + std::to_string(i) + "].";
        shader_->setInt(base + "type", static_cast<int>(light.type));
        shader_->setVec3(base + "position", light.position);
        shader_->setVec3(base + "direction", light.direction);
        shader_->setVec3(base + "color", light.color);
        shader_->setFloat(base + "radius", light.radius);
        shader_->setFloat(base + "intensity", light.intensity);
        shader_->setFloat(base + "innerConeCos", std::cos(glm::radians(light.innerConeDegrees)));
        shader_->setFloat(base + "outerConeCos", std::cos(glm::radians(light.outerConeDegrees)));
        shader_->setInt(base + "castsShadows", light.castsShadows ? 1 : 0);
        shader_->setInt(base + "shadowIndex", light.shadowIndex);
    }

    for (const auto& obj : objects) {
        const RenderMaterialData& material = obj.material;
        shader_->setInt("uUseMaterialMaps", material.useMaterialMaps ? 1 : 0);
        shader_->setInt("uMaterialUvMode", material.uvMode);
        shader_->setVec2("uMaterialUvScale", material.uvScale);
        shader_->setFloat("uMaterialNormalStrength", material.normalStrength);
        shader_->setFloat("uMaterialRoughnessScale", material.roughnessScale);
        shader_->setFloat("uMaterialRoughnessBias", material.roughnessBias);
        shader_->setFloat("uMaterialMetalness", material.metalness);
        shader_->setFloat("uMaterialAoStrength", material.aoStrength);
        shader_->setFloat("uMaterialLightTintResponse", material.lightTintResponse);
        glActiveTexture(GL_TEXTURE12);
        glBindTexture(GL_TEXTURE_2D, material.albedoTexture);
        glActiveTexture(GL_TEXTURE13);
        glBindTexture(GL_TEXTURE_2D, material.normalTexture);
        glActiveTexture(GL_TEXTURE14);
        glBindTexture(GL_TEXTURE_2D, material.roughnessTexture);
        glActiveTexture(GL_TEXTURE15);
        glBindTexture(GL_TEXTURE_2D, material.aoTexture);
        glActiveTexture(GL_TEXTURE0);
        shader_->setMat4("uModel", obj.modelMatrix);
        shader_->setVec3("uBaseColor", obj.tint * material.baseColor);
        shader_->setInt("uMaterialKind", static_cast<int>(material.shadingModel));
        obj.mesh->draw();
    }
}
