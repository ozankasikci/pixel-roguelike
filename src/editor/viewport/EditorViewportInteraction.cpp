#include "editor/viewport/EditorViewportInteraction.h"

#include "editor/scene/EditorPreviewWorld.h"
#include "editor/viewport/EditorViewportController.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/MaterialTextureLibrary.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <ImGuizmo.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

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

bool isViewportSelectableKind(const EditorSelectionHandle& handle, const EditorUiState& ui) {
    switch (handle.objectKind) {
    case EditorSceneObjectKind::BoxCollider:
    case EditorSceneObjectKind::CylinderCollider:
        return ui.showColliders;
    case EditorSceneObjectKind::Light:
        return ui.showLightHelpers;
    case EditorSceneObjectKind::PlayerSpawn:
        return ui.showSpawnMarker;
    case EditorSceneObjectKind::Mesh:
    case EditorSceneObjectKind::Archetype:
        return true;
    }
    return true;
}

bool sameHitOrder(const std::vector<EditorHitResult>& lhs, const std::vector<EditorHitResult>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lhs[index].objectId != rhs[index].objectId) {
            return false;
        }
    }
    return true;
}

float snappedValue(float value, float snap) {
    if (snap <= 0.0001f) {
        return value;
    }
    return std::round(value / snap) * snap;
}

glm::vec3 snappedVec3(const glm::vec3& value, float snap) {
    if (snap <= 0.0001f) {
        return value;
    }
    return glm::vec3(
        snappedValue(value.x, snap),
        snappedValue(value.y, snap),
        snappedValue(value.z, snap)
    );
}

bool decomposeModelMatrix(const glm::mat4& matrix,
                          glm::vec3& position,
                          glm::vec3& rotationDegrees,
                          glm::vec3& scale) {
    glm::vec3 skew(0.0f);
    glm::vec4 perspective(0.0f);
    glm::quat orientation;
    if (!glm::decompose(matrix, scale, orientation, position, skew, perspective)) {
        return false;
    }
    rotationDegrees = glm::degrees(glm::eulerAngles(orientation));
    return true;
}

std::optional<glm::vec3> intersectHorizontalPlacementPlane(const EditorRay& ray,
                                                           const EditorCamera& camera) {
    const glm::vec3 planePoint = camera.position + editorCameraForward(camera) * 4.0f;
    constexpr glm::vec3 planeNormal(0.0f, 1.0f, 0.0f);
    const float denom = glm::dot(ray.direction, planeNormal);
    if (std::abs(denom) < 0.0001f) {
        return std::nullopt;
    }
    const float t = glm::dot(planePoint - ray.origin, planeNormal) / denom;
    if (t < 0.0f) {
        return std::nullopt;
    }
    return ray.origin + ray.direction * t;
}

} // namespace

bool pointInViewport(const EditorViewportState& viewport, const ImVec2& mousePos) {
    return mousePos.x >= viewport.origin.x
        && mousePos.y >= viewport.origin.y
        && mousePos.x <= viewport.origin.x + viewport.size.x
        && mousePos.y <= viewport.origin.y + viewport.size.y;
}

bool isSelected(const std::vector<std::uint64_t>& selectedIds, std::uint64_t id) {
    return std::find(selectedIds.begin(), selectedIds.end(), id) != selectedIds.end();
}

EditorPlacementState makePlacementState(const EditorDragPayload& payload) {
    EditorPlacementState state;
    state.kind = payload.kind;
    state.meshId = payload.primaryId;
    state.materialId = payload.secondaryId;
    state.archetypeId = payload.primaryId;

    switch (payload.kind) {
    case EditorPlacementKind::Mesh:
        state.archetypeId.clear();
        break;
    case EditorPlacementKind::Archetype:
        state.meshId.clear();
        state.materialId.clear();
        break;
    case EditorPlacementKind::PointLight:
    case EditorPlacementKind::SpotLight:
    case EditorPlacementKind::DirectionalLight:
    case EditorPlacementKind::BoxCollider:
    case EditorPlacementKind::CylinderCollider:
    case EditorPlacementKind::PlayerSpawn:
    case EditorPlacementKind::None:
        state.meshId.clear();
        state.materialId.clear();
        state.archetypeId.clear();
        break;
    }

    return state;
}

std::vector<EditorSelectionHandle> buildViewportSelectionHandles(const std::vector<EditorSelectionHandle>& handles,
                                                                 const EditorUiState& ui) {
    std::vector<EditorSelectionHandle> filtered;
    filtered.reserve(handles.size());
    for (const auto& handle : handles) {
        if (isViewportSelectableKind(handle, ui)) {
            filtered.push_back(handle);
        }
    }
    return filtered;
}

void toggleSelection(std::vector<std::uint64_t>& selectedIds, std::uint64_t id, bool additive) {
    if (!additive) {
        selectedIds = {id};
        return;
    }
    auto it = std::find(selectedIds.begin(), selectedIds.end(), id);
    if (it == selectedIds.end()) {
        selectedIds.push_back(id);
    } else {
        selectedIds.erase(it);
    }
}

void pruneSelection(const EditorSceneDocument& document, std::vector<std::uint64_t>& selectedIds) {
    selectedIds.erase(
        std::remove_if(selectedIds.begin(), selectedIds.end(), [&](std::uint64_t id) {
            return document.findObject(id) == nullptr;
        }),
        selectedIds.end());
}

void refreshSelectionPicker(EditorSelectionPickerState& picker,
                            const std::vector<EditorHitResult>& hits,
                            const ImVec2& mousePos,
                            double nowSeconds,
                            bool advanceCycle) {
    if (hits.empty()) {
        picker.clear();
        return;
    }

    const bool sameHits = picker.active(nowSeconds) && sameHitOrder(picker.hits, hits);
    if (!sameHits) {
        picker.hits = hits;
        picker.currentIndex = 0;
    } else if (!hits.empty() && advanceCycle) {
        picker.currentIndex = (picker.currentIndex + 1) % hits.size();
    }

    picker.anchorScreen = mousePos;
    picker.visibleUntilSeconds = nowSeconds + 2.75;
}

void applySelectionHit(std::vector<std::uint64_t>& selectedIds,
                       const EditorSelectionPickerState& picker,
                       bool additive) {
    if (picker.hits.empty() || picker.currentIndex >= picker.hits.size()) {
        return;
    }
    toggleSelection(selectedIds, picker.hits[picker.currentIndex].objectId, additive);
}

void renderSelectionPicker(EditorSelectionPickerState& picker,
                           EditorSceneDocument& document,
                           std::vector<std::uint64_t>& selectedIds,
                           double nowSeconds) {
    if (!picker.active(nowSeconds) || picker.hits.size() <= 1) {
        if (!picker.hits.empty() && nowSeconds > picker.visibleUntilSeconds) {
            picker.clear();
        }
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.94f);
    ImGui::SetNextWindowPos(ImVec2(picker.anchorScreen.x + 14.0f, picker.anchorScreen.y + 14.0f), ImGuiCond_Always);
    if (!ImGui::Begin("##SelectionPicker",
                      nullptr,
                      ImGuiWindowFlags_NoDecoration
                          | ImGuiWindowFlags_AlwaysAutoResize
                          | ImGuiWindowFlags_NoSavedSettings
                          | ImGuiWindowFlags_NoNav
                          | ImGuiWindowFlags_NoFocusOnAppearing)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Pick object");
    ImGui::Separator();
    ImGui::TextDisabled("Click again to cycle");

    const std::size_t maxVisible = std::min<std::size_t>(picker.hits.size(), 8);
    for (std::size_t index = 0; index < maxVisible; ++index) {
        const EditorHitResult& hit = picker.hits[index];
        const EditorSceneObject* object = document.findObject(hit.objectId);
        std::string label = object != nullptr
            ? editorSceneObjectLabel(*object)
            : ("Object #" + std::to_string(hit.objectId));
        label += "  ";
        label += std::to_string(static_cast<int>(std::round(hit.distance * 10.0f)) / 10.0f);
        label += "m";

        if (ImGui::Selectable(label.c_str(), picker.currentIndex == index)) {
            picker.currentIndex = index;
            picker.visibleUntilSeconds = nowSeconds + 2.75;
            applySelectionHit(selectedIds, picker, false);
        }
    }
    if (picker.hits.size() > maxVisible) {
        ImGui::TextDisabled("+%zu more", picker.hits.size() - maxVisible);
    }

    ImGui::End();
}

bool applyGizmoToSelectedObject(EditorSceneDocument& document,
                                const std::vector<std::uint64_t>& selectedIds,
                                const EditorViewportState& viewport,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                const EditorUiState& ui,
                                const EditorPreviewWorld& previewWorld) {
    if (selectedIds.size() != 1 || ui.playPreview) {
        return false;
    }

    EditorSceneObject* object = document.findObject(selectedIds.front());
    if (object == nullptr) {
        return false;
    }

    glm::mat4 model(1.0f);
    switch (object->kind) {
    case EditorSceneObjectKind::Mesh:
    case EditorSceneObjectKind::BoxCollider:
    case EditorSceneObjectKind::CylinderCollider:
    case EditorSceneObjectKind::Archetype:
        model = document.worldTransformMatrix(object->id);
        break;
    case EditorSceneObjectKind::Light: {
        const auto& light = std::get<LevelLightPlacement>(object->payload);
        if (light.type == LightType::Directional) {
            const glm::vec3 anchor = previewWorld.sceneBounds().valid
                ? previewWorld.sceneBounds().center() - safeNormalize(light.direction, glm::vec3(-0.3f, -0.9f, -0.1f)) * 4.0f
                : -safeNormalize(light.direction, glm::vec3(-0.3f, -0.9f, -0.1f)) * 4.0f;
            model = makeModelMatrix(anchor, glm::vec3(0.6f));
        } else {
            model = makeModelMatrix(light.position, glm::vec3(0.6f));
        }
        break;
    }
    case EditorSceneObjectKind::PlayerSpawn: {
        glm::mat4 world = document.worldTransformMatrix(object->id);
        world = glm::scale(world, glm::vec3(0.5f, 1.4f, 0.5f));
        model = world;
        break;
    }
    }

    if (!manipulateEditorGizmo(viewport,
                               view,
                               projection,
                               ui.tool,
                               ui.snappingEnabled,
                               ui.moveSnap,
                               ui.rotateSnap,
                               ui.scaleSnap,
                               model)) {
        return false;
    }

    glm::vec3 position(0.0f);
    glm::vec3 rotationDegrees(0.0f);
    glm::vec3 scale(1.0f);
    if (!decomposeModelMatrix(model, position, rotationDegrees, scale)) {
        return false;
    }

    if (ui.snappingEnabled) {
        position = snappedVec3(position, ui.moveSnap);
        rotationDegrees = snappedVec3(rotationDegrees, ui.rotateSnap);
    }

    switch (object->kind) {
    case EditorSceneObjectKind::Mesh:
    case EditorSceneObjectKind::BoxCollider:
    case EditorSceneObjectKind::CylinderCollider:
    case EditorSceneObjectKind::Archetype:
        if (!document.applyWorldTransform(object->id, model)) {
            return false;
        }
        break;
    case EditorSceneObjectKind::Light: {
        auto& light = std::get<LevelLightPlacement>(object->payload);
        if (light.type == LightType::Directional) {
            const glm::mat4 rotationMatrix = glm::yawPitchRoll(glm::radians(rotationDegrees.y),
                                                               glm::radians(rotationDegrees.x),
                                                               glm::radians(rotationDegrees.z));
            light.direction = safeNormalize(glm::vec3(rotationMatrix * glm::vec4(0.0f, -1.0f, 0.0f, 0.0f)),
                                            glm::vec3(0.0f, -1.0f, 0.0f));
        } else {
            light.position = position;
            if (light.type == LightType::Spot) {
                const glm::mat4 rotationMatrix = glm::yawPitchRoll(glm::radians(rotationDegrees.y),
                                                                   glm::radians(rotationDegrees.x),
                                                                   glm::radians(rotationDegrees.z));
                light.direction = safeNormalize(glm::vec3(rotationMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)),
                                                glm::vec3(0.0f, -1.0f, 0.0f));
            }
        }
        break;
    }
    case EditorSceneObjectKind::PlayerSpawn: {
        glm::mat4 spawnWorld = model;
        spawnWorld[0] = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
        spawnWorld[1] = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
        spawnWorld[2] = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
        if (!document.applyWorldTransform(object->id, spawnWorld)) {
            return false;
        }
        break;
    }
    }
    return true;
}

std::optional<glm::vec3> computePlacementPoint(const std::vector<EditorSelectionHandle>& handles,
                                               const EditorRay& ray,
                                               const EditorCamera& camera,
                                               bool snappingEnabled,
                                               float moveSnap) {
    if (const auto hit = pickEditorPlacementSurface(handles, ray)) {
        return snappingEnabled ? snappedVec3(hit->position, moveSnap) : hit->position;
    }
    const auto planeHit = intersectHorizontalPlacementPlane(ray, camera);
    if (!planeHit.has_value()) {
        return std::nullopt;
    }
    return snappingEnabled ? snappedVec3(*planeHit, moveSnap) : *planeHit;
}

void commitPlacement(EditorSceneDocument& document,
                     const EditorPlacementState& state,
                     const glm::vec3& position,
                     const ContentRegistry& content,
                     const EditorCamera& camera) {
    switch (state.kind) {
    case EditorPlacementKind::Mesh: {
        LevelMeshPlacement placement;
        placement.meshId = state.meshId;
        placement.position = position;
        placement.scale = glm::vec3(1.0f);
        placement.rotation = glm::vec3(0.0f);
        placement.materialId = state.materialId;
        placement.material = resolvePlacementMaterialKind(state.materialId, content);
        document.addMesh(placement);
        break;
    }
    case EditorPlacementKind::PointLight: {
        LevelLightPlacement light;
        light.position = position;
        light.color = glm::vec3(1.0f, 0.88f, 0.72f);
        light.radius = 6.0f;
        light.intensity = 0.8f;
        document.addLight(light);
        break;
    }
    case EditorPlacementKind::SpotLight: {
        LevelLightPlacement light;
        light.type = LightType::Spot;
        light.position = position;
        light.direction = editorCameraForward(camera);
        light.color = glm::vec3(1.0f, 0.92f, 0.82f);
        light.radius = 10.0f;
        light.intensity = 1.0f;
        light.innerConeDegrees = 24.0f;
        light.outerConeDegrees = 36.0f;
        light.castsShadows = true;
        document.addLight(light);
        break;
    }
    case EditorPlacementKind::DirectionalLight: {
        LevelLightPlacement light;
        light.type = LightType::Directional;
        light.direction = glm::vec3(-0.3f, -0.9f, -0.1f);
        light.color = glm::vec3(1.0f);
        light.intensity = 0.8f;
        document.addLight(light);
        break;
    }
    case EditorPlacementKind::BoxCollider:
        document.addBoxCollider(LevelBoxColliderPlacement{
            .position = position,
            .halfExtents = glm::vec3(0.5f),
        });
        break;
    case EditorPlacementKind::CylinderCollider:
        document.addCylinderCollider(LevelCylinderColliderPlacement{
            .position = position,
            .radius = 0.5f,
            .halfHeight = 0.9f,
        });
        break;
    case EditorPlacementKind::PlayerSpawn:
        document.setPlayerSpawn(LevelPlayerSpawn{
            .position = position,
            .fallRespawnY = -8.0f,
        });
        break;
    case EditorPlacementKind::Archetype:
        document.addArchetype(LevelArchetypePlacement{
            .archetypeId = state.archetypeId,
            .position = position,
            .yawDegrees = 0.0f,
        });
        break;
    case EditorPlacementKind::None:
        break;
    }
}

void appendPlacementGhost(std::vector<RenderObject>& objects,
                          const EditorPlacementState& state,
                          const glm::vec3& position,
                          const EditorPreviewWorld& previewWorld,
                          const MaterialTextureLibrary& materials,
                          const ContentRegistry& content) {
    Mesh* cube = previewWorld.meshLibrary().get("cube");
    Mesh* cylinder = previewWorld.meshLibrary().get("cylinder");
    const glm::vec3 tint(0.55f, 1.15f, 0.65f);

    switch (state.kind) {
    case EditorPlacementKind::Mesh: {
        Mesh* mesh = previewWorld.meshLibrary().get(state.meshId);
        if (mesh == nullptr) {
            break;
        }
        objects.push_back(RenderObject{
            mesh,
            makeModelMatrix(position, glm::vec3(1.0f)),
            tint,
            materials.resolve(state.materialId, resolvePlacementMaterialKind(state.materialId, content))
        });
        break;
    }
    case EditorPlacementKind::PointLight:
    case EditorPlacementKind::SpotLight:
    case EditorPlacementKind::DirectionalLight:
        if (cube != nullptr) {
            objects.push_back(RenderObject{
                cube,
                makeModelMatrix(position, glm::vec3(0.18f)),
                tint,
                resolveHelperMaterial(materials, MaterialKind::Wax, "wax_default")
            });
        }
        break;
    case EditorPlacementKind::BoxCollider:
        if (cube != nullptr) {
            objects.push_back(RenderObject{
                cube,
                makeModelMatrix(position, glm::vec3(1.0f)),
                tint,
                resolveHelperMaterial(materials, MaterialKind::Metal, "metal_default")
            });
        }
        break;
    case EditorPlacementKind::CylinderCollider:
        if (cylinder != nullptr) {
            objects.push_back(RenderObject{
                cylinder,
                makeModelMatrix(position, glm::vec3(1.0f)),
                tint,
                resolveHelperMaterial(materials, MaterialKind::Metal, "metal_default")
            });
        }
        break;
    case EditorPlacementKind::PlayerSpawn:
        if (cube != nullptr) {
            objects.push_back(RenderObject{
                cube,
                makeModelMatrix(position, glm::vec3(0.22f, 0.80f, 0.22f)),
                tint,
                resolveHelperMaterial(materials, MaterialKind::Moss, "moss_default")
            });
        }
        break;
    case EditorPlacementKind::Archetype:
        if (cube != nullptr) {
            objects.push_back(RenderObject{
                cube,
                makeModelMatrix(position, glm::vec3(1.0f)),
                tint,
                resolveHelperMaterial(materials, MaterialKind::Metal, "metal_default")
            });
        }
        break;
    case EditorPlacementKind::None:
        break;
    }
}
