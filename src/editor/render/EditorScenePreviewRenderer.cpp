#include "editor/render/EditorScenePreviewRenderer.h"

#include "editor/scene/EditorPreviewWorld.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/viewport/EditorViewportInteraction.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/rendering/lighting/ShadowMap.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/MaterialTextureLibrary.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr int kMaxShadowedSpotLightsLocal = 2;

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

glm::mat4 makeModelMatrix(const glm::vec3& position,
                          const glm::vec3& scale,
                          const glm::vec3& rotation = glm::vec3(0.0f)) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}

RenderMaterialData resolveHelperMaterial(const MaterialTextureLibrary& materials,
                                         MaterialKind kind,
                                         const std::string& materialId = {}) {
    return materials.resolve(materialId.empty() ? std::string(defaultMaterialIdForKind(kind)) : materialId, kind);
}

void appendDirectionalLight(std::vector<RenderLight>& lights,
                            const DirectionalLightSlot& slot,
                            const glm::vec3& fallbackDirection) {
    if (!slot.enabled || slot.intensity <= 0.0001f) {
        return;
    }

    RenderLight light;
    light.type = LightType::Directional;
    light.direction = safeNormalize(slot.direction, fallbackDirection);
    light.color = glm::max(slot.color, glm::vec3(0.0f));
    light.intensity = slot.intensity;
    lights.push_back(light);
}

void assignShadowSlots(std::vector<RenderLight>& lights, bool enableShadows) {
    for (auto& light : lights) {
        light.shadowIndex = -1;
    }

    if (!enableShadows) {
        return;
    }

    int nextShadowIndex = 0;
    for (auto& light : lights) {
        if (nextShadowIndex >= kMaxShadowedSpotLightsLocal) {
            break;
        }
        if (light.type != LightType::Spot || !light.castsShadows) {
            continue;
        }
        light.shadowIndex = nextShadowIndex++;
    }
}

glm::mat4 buildShadowMatrix(const RenderLight& light) {
    const glm::vec3 direction = safeNormalize(light.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    const glm::vec3 up = std::abs(glm::dot(direction, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.97f
        ? glm::vec3(0.0f, 0.0f, 1.0f)
        : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::mat4 lightView = glm::lookAt(light.position, light.position + direction, up);
    const float coneDegrees = std::max(light.outerConeDegrees, light.innerConeDegrees + 1.0f) * 2.0f + 4.0f;
    const float farPlane = std::max(light.radius, 0.5f);
    const glm::mat4 lightProjection = glm::perspective(glm::radians(coneDegrees), 1.0f, 0.05f, farPlane);
    return lightProjection * lightView;
}

} // namespace

std::vector<RenderLight> collectLights(const entt::registry& registry,
                                       const EnvironmentDefinition& environment) {
    std::vector<RenderLight> lights;
    appendDirectionalLight(lights, environment.lighting.sun, glm::vec3(0.0f, -1.0f, 0.0f));
    appendDirectionalLight(lights, environment.lighting.fill, glm::vec3(0.0f, -1.0f, 0.0f));

    auto lightView = registry.view<TransformComponent, LightComponent>();
    for (auto [entity, transform, light] : lightView.each()) {
        RenderLight renderLight;
        renderLight.type = light.type;
        renderLight.position = transform.position;
        renderLight.direction = safeNormalize(light.direction, glm::vec3(0.0f, -1.0f, 0.0f));
        renderLight.color = light.color;
        renderLight.radius = light.radius;
        renderLight.intensity = light.intensity;
        renderLight.innerConeDegrees = light.innerConeDegrees;
        renderLight.outerConeDegrees = light.outerConeDegrees;
        renderLight.castsShadows = light.type == LightType::Spot && light.castsShadows;
        lights.push_back(renderLight);
    }
    assignShadowSlots(lights, environment.lighting.enableShadows);
    return lights;
}

std::vector<RenderObject> collectRenderObjects(const EditorPreviewWorld& world,
                                               const MaterialTextureLibrary& materials,
                                               const std::vector<std::uint64_t>& selectedIds) {
    std::vector<RenderObject> objects;
    auto meshView = world.registry().view<TransformComponent, MeshComponent>();
    for (auto [entity, transform, mesh] : meshView.each()) {
        if (mesh.mesh == nullptr) {
            continue;
        }
        glm::vec3 tint = mesh.tint;
        auto ownerIt = world.ownerMap().find(entity);
        if (ownerIt != world.ownerMap().end() && isSelected(selectedIds, ownerIt->second)) {
            tint = glm::min(tint * 1.20f + glm::vec3(0.08f, 0.08f, 0.02f), glm::vec3(1.4f));
        }
        objects.push_back(RenderObject{
            mesh.mesh,
            mesh.useModelOverride ? mesh.modelOverride : transform.modelMatrix(),
            tint,
            materials.resolve(mesh.materialId, mesh.material)
        });
    }
    return objects;
}

void appendHelperObjects(std::vector<RenderObject>& objects,
                         const EditorPreviewWorld& world,
                         const EditorSceneDocument& document,
                         const MaterialTextureLibrary& materials,
                         const std::vector<std::uint64_t>& selectedIds,
                         bool showColliders,
                         bool showLights,
                         bool showSpawn) {
    Mesh* cube = world.meshLibrary().get("cube");
    Mesh* cylinder = world.meshLibrary().get("cylinder");
    if (showColliders) {
        auto colliderView = world.registry().view<StaticColliderComponent>();
        for (auto [entity, collider] : colliderView.each()) {
            const std::uint64_t ownerId = world.ownerMap().contains(entity) ? world.ownerMap().at(entity) : 0;
            const bool selected = ownerId != 0 && isSelected(selectedIds, ownerId);
            const glm::vec3 tint = selected ? glm::vec3(0.48f, 1.00f, 0.58f) : glm::vec3(0.20f, 0.92f, 0.28f);
            if (collider.shape == ColliderShape::Box && cube != nullptr) {
                objects.push_back(RenderObject{
                    cube,
                    makeModelMatrix(collider.position, collider.halfExtents * 2.0f, collider.rotation),
                    tint,
                    resolveHelperMaterial(materials, MaterialKind::Metal, "metal_default"),
                    true,
                    true,
                    true,
                    selected ? 3.0f : 2.0f
                });
            } else if (collider.shape == ColliderShape::Cylinder && cylinder != nullptr) {
                objects.push_back(RenderObject{
                    cylinder,
                    makeModelMatrix(collider.position, glm::vec3(collider.radius, collider.halfHeight * 2.0f, collider.radius), collider.rotation),
                    tint,
                    resolveHelperMaterial(materials, MaterialKind::Metal, "metal_default"),
                    true,
                    true,
                    true,
                    selected ? 3.0f : 2.0f
                });
            }
        }
    }

    if (showLights && cube != nullptr) {
        auto lightView = world.registry().view<TransformComponent, LightComponent>();
        for (auto [entity, transform, light] : lightView.each()) {
            const std::uint64_t ownerId = world.ownerMap().contains(entity) ? world.ownerMap().at(entity) : 0;
            const bool selected = ownerId != 0 && isSelected(selectedIds, ownerId);
            const glm::vec3 tint = selected ? glm::vec3(1.30f, 1.20f, 0.46f) : glm::vec3(1.00f, 0.88f, 0.22f);
            objects.push_back(RenderObject{
                cube,
                makeModelMatrix(transform.position, glm::vec3(0.14f)),
                tint,
                resolveHelperMaterial(materials, MaterialKind::Wax, "wax_default")
            });
        }
    }

    if (showSpawn && cube != nullptr) {
        for (const auto& object : document.objects()) {
            if (object.kind != EditorSceneObjectKind::PlayerSpawn) {
                continue;
            }
            const bool selected = isSelected(selectedIds, object.id);
            const glm::vec3 tint = selected ? glm::vec3(0.40f, 1.10f, 0.44f) : glm::vec3(0.18f, 0.92f, 0.28f);
            objects.push_back(RenderObject{
                cube,
                makeModelMatrix(std::get<LevelPlayerSpawn>(object.payload).position, glm::vec3(0.22f, 0.80f, 0.22f)),
                tint,
                resolveHelperMaterial(materials, MaterialKind::Moss, "moss_default")
            });
        }
    }
}

void appendSelectionOverlays(std::vector<RenderObject>& objects,
                             const EditorPreviewWorld& world,
                             const MaterialTextureLibrary& materials,
                             const std::vector<std::uint64_t>& selectedIds) {
    if (selectedIds.empty()) {
        return;
    }

    Mesh* cube = world.meshLibrary().get("cube");
    if (cube == nullptr) {
        return;
    }

    for (std::size_t index = 0; index < selectedIds.size(); ++index) {
        const std::uint64_t objectId = selectedIds[index];
        const EditorObjectBounds* bounds = world.findObjectBounds(objectId);
        if (bounds == nullptr || !bounds->valid) {
            continue;
        }

        const glm::vec3 center = bounds->center();
        glm::vec3 size = glm::max(bounds->max - bounds->min, glm::vec3(0.12f));
        size *= 1.035f;

        const bool primary = (index + 1 == selectedIds.size());
        const glm::vec3 tint = primary
            ? glm::vec3(1.30f, 0.92f, 0.24f)
            : glm::vec3(0.50f, 1.00f, 0.62f);

        objects.push_back(RenderObject{
            cube,
            makeModelMatrix(center, size),
            tint,
            resolveHelperMaterial(materials, MaterialKind::Metal, "metal_default"),
            true,
            true,
            true,
            primary ? 4.0f : 2.5f
        });
    }
}

void renderShadowPass(const std::vector<RenderObject>& objects,
                      const std::vector<RenderLight>& lights,
                      const Shader& shadowShader,
                      std::array<ShadowMap, 2>& shadowMaps,
                      int shadowResolution,
                      ShadowRenderData& shadowData) {
    shadowData.shadowCount = 0;
    shadowData.matrices = {glm::mat4(1.0f), glm::mat4(1.0f)};
    shadowData.textures = {shadowMaps[0].texture(), shadowMaps[1].texture()};

    for (auto& shadowMap : shadowMaps) {
        if (shadowMap.texture() == 0) {
            shadowMap.create(shadowResolution);
        } else {
            shadowMap.resize(shadowResolution);
        }
    }

    glEnable(GL_DEPTH_TEST);
    shadowShader.use();

    for (const auto& light : lights) {
        if (light.shadowIndex < 0 || light.shadowIndex >= kMaxShadowedSpotLightsLocal) {
            continue;
        }

        ShadowMap& shadowMap = shadowMaps[static_cast<std::size_t>(light.shadowIndex)];
        const glm::mat4 lightMatrix = buildShadowMatrix(light);
        shadowData.matrices[static_cast<std::size_t>(light.shadowIndex)] = lightMatrix;
        shadowData.textures[static_cast<std::size_t>(light.shadowIndex)] = shadowMap.texture();
        shadowData.shadowCount = std::max(shadowData.shadowCount, light.shadowIndex + 1);

        shadowMap.bind();
        glViewport(0, 0, shadowResolution, shadowResolution);
        glClear(GL_DEPTH_BUFFER_BIT);
        shadowShader.setMat4("uLightViewProjection", lightMatrix);

        for (const auto& object : objects) {
            shadowShader.setMat4("uModel", object.modelMatrix);
            object.mesh->draw();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
