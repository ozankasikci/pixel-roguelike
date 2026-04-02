#include "engine/rendering/geometry/Renderer.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/TextureUnits.h"

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
    shader_->setInt("uEnableShadows", lighting.enableShadows ? 1 : 0);
    shader_->setFloat("uShadowBias", lighting.shadowBias);
    shader_->setFloat("uShadowNormalBias", lighting.shadowNormalBias);

    int numLights = static_cast<int>(std::min(lights.size(), static_cast<size_t>(kMaxRenderLights)));
    shader_->setInt("uNumLights", numLights);

    shader_->setInt("uShadowCount", shadowData.shadowCount);
    for (int i = 0; i < kMaxShadowedSpotLights; ++i) {
        shader_->setMat4("uShadowMatrices[" + std::to_string(i) + "]", shadowData.matrices[static_cast<std::size_t>(i)]);
        const int textureUnit = TextureUnits::kShadowMap0 + i;
        shader_->setInt("uShadowMaps[" + std::to_string(i) + "]", textureUnit);
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_2D, shadowData.textures[static_cast<std::size_t>(i)]);
    }
    glActiveTexture(GL_TEXTURE0);
    shader_->setInt("uAlbedoMap", TextureUnits::kAlbedo);
    shader_->setInt("uNormalMap", TextureUnits::kNormalMap);
    shader_->setInt("uRoughnessMap", TextureUnits::kRoughnessMap);
    shader_->setInt("uAoMap", TextureUnits::kAoMap);

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
        shader_->setVec3(base + "right", light.right);
        shader_->setVec3(base + "up", light.up);
        shader_->setFloat(base + "width", light.width);
        shader_->setFloat(base + "height", light.height);
        shader_->setInt(base + "doubleSided", light.doubleSided ? 1 : 0);
    }

    // Track last-bound material to skip redundant uniform and texture calls.
    // Objects are sorted by material ID upstream (SceneRenderPipeline::render).
    std::string lastMaterialId;

    for (const auto& obj : objects) {
        if (obj.ignoreDepth) {
            glDisable(GL_DEPTH_TEST);
        } else {
            glEnable(GL_DEPTH_TEST);
        }
        if (obj.wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(std::max(1.0f, obj.lineWidth));
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        const RenderMaterialData& material = obj.material;

        // Skip redundant material uniforms and texture binds when consecutive
        // objects share the same material (saves ~30 uniform + 4 texture calls)
        const bool sameMaterial = (!obj.material.id.empty() && obj.material.id == lastMaterialId);
        if (!sameMaterial) {
            lastMaterialId = obj.material.id;
            shader_->setInt("uUseMaterialMaps", material.useMaterialMaps ? 1 : 0);
            shader_->setInt("uUseProceduralDetail", material.useProceduralDetail ? 1 : 0);
            shader_->setInt("uMaterialUvMode", material.uvMode);
            shader_->setVec2("uMaterialUvScale", material.uvScale);
            shader_->setFloat("uMaterialNormalStrength", material.normalStrength);
            shader_->setFloat("uMaterialRoughnessScale", material.roughnessScale);
            shader_->setFloat("uMaterialRoughnessBias", material.roughnessBias);
            shader_->setFloat("uMaterialMetalness", material.metalness);
            shader_->setFloat("uMaterialAoStrength", material.aoStrength);
            shader_->setFloat("uMaterialLightTintResponse", material.lightTintResponse);
            shader_->setFloat("uEmissiveStrength", material.emissiveStrength);
            glActiveTexture(GL_TEXTURE0 + TextureUnits::kAlbedo);
            glBindTexture(GL_TEXTURE_2D, material.albedoTexture);
            glActiveTexture(GL_TEXTURE0 + TextureUnits::kNormalMap);
            glBindTexture(GL_TEXTURE_2D, material.normalTexture);
            glActiveTexture(GL_TEXTURE0 + TextureUnits::kRoughnessMap);
            glBindTexture(GL_TEXTURE_2D, material.roughnessTexture);
            glActiveTexture(GL_TEXTURE0 + TextureUnits::kAoMap);
            glBindTexture(GL_TEXTURE_2D, material.aoTexture);
            glActiveTexture(GL_TEXTURE0);
            shader_->setFloat("uMaterialSpecularLevel", material.specularLevel);
            shader_->setInt("uMaterialAnimated", material.animated ? 1 : 0);
            shader_->setInt("uMaterialSubsurface", material.subsurface ? 1 : 0);
            shader_->setInt("uMaterialBrickDetail", material.detailBrick ? 1 : 0);
            shader_->setInt("uMaterialWoodDetail", material.detailWood ? 1 : 0);
            shader_->setInt("uMaterialStoneDetail", material.detailStone ? 1 : 0);
            shader_->setInt("uMaterialFloorDetail", material.detailFloor ? 1 : 0);
            shader_->setInt("uNormalMapFlipY", material.normalMapFlipY ? 1 : 0);
            shader_->setInt("uAlphaTest", material.alphaTest ? 1 : 0);
            shader_->setFloat("uAlphaCutoff", material.alphaCutoff);
        }

        // Per-object uniforms: always set (model matrix, base color with tint, unlit flag)
        shader_->setInt("uUnlit", obj.unlit ? 1 : 0);
        shader_->setMat4("uModel", obj.modelMatrix);
        shader_->setVec3("uBaseColor", obj.tint * material.baseColor);
        obj.mesh->draw();
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
    glEnable(GL_DEPTH_TEST);
}
