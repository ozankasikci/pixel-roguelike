#include "engine/core/PathUtils.h"
#include "engine/core/Window.h"
#include "engine/rendering/core/Framebuffer.h"
#include "engine/rendering/core/Shader.h"
#include "engine/rendering/geometry/Mesh.h"
#include "engine/rendering/geometry/MeshLibrary.h"
#include "engine/rendering/geometry/Renderer.h"
#include "engine/rendering/lighting/ShadowMap.h"
#include "engine/rendering/post/CompositePass.h"
#include "engine/rendering/post/PostProcessParams.h"
#include "engine/rendering/post/StylizePass.h"
#include "engine/ui/ImGuiLayer.h"
#include "editor/EditorCommand.h"
#include "editor/EditorLayoutPreset.h"
#include "editor/EditorPreviewWorld.h"
#include "editor/EditorSceneDocument.h"
#include "editor/EditorSelectionSystem.h"
#include "editor/EditorViewportController.h"
#include "game/components/LightComponent.h"
#include "game/components/MeshComponent.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/TransformComponent.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MaterialDefinition.h"
#include "game/rendering/MaterialTextureLibrary.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <cstring>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

constexpr int kMaxShadowedSpotLightsLocal = 2;
constexpr int kShadowResolutions[] = {512, 1024, 2048};

enum class EditorPreviewMode {
    Final,
    LightingOnly,
    SkyOnly,
};

enum class EditorPlacementKind {
    None,
    Mesh,
    PointLight,
    SpotLight,
    DirectionalLight,
    BoxCollider,
    CylinderCollider,
    PlayerSpawn,
    Archetype,
};

struct EditorUiState {
    EditorTransformTool tool = EditorTransformTool::Translate;
    EditorPreviewMode previewMode = EditorPreviewMode::Final;
    bool playPreview = false;
    bool showOutliner = true;
    bool showInspector = true;
    bool showAssetBrowser = true;
    bool showEnvironment = true;
    bool showViewport = true;
    bool showColliders = true;
    bool showLightHelpers = true;
    bool showSpawnMarker = true;
    bool snappingEnabled = true;
    float moveSnap = 0.5f;
    float rotateSnap = 15.0f;
    float scaleSnap = 0.1f;
    int shadowResolutionIndex = 1;
    std::string selectedMeshId = "cube";
    std::string selectedMaterialId = "stone_default";
    std::string selectedArchetypeId = "checkpoint_shrine";
    std::string pendingScenePath = "assets/scenes/silos_cloister.scene";
    std::string activeLayoutPreset = "default";
    char layoutNameBuffer[64] = "default";
};

struct EditorPlacementState {
    EditorPlacementKind kind = EditorPlacementKind::None;
    std::string meshId;
    std::string materialId;
    std::string archetypeId;

    bool active() const { return kind != EditorPlacementKind::None; }
    void clear() {
        kind = EditorPlacementKind::None;
        meshId.clear();
        materialId.clear();
        archetypeId.clear();
    }
};

struct EditorDragPayload {
    EditorPlacementKind kind = EditorPlacementKind::None;
    char primaryId[64]{};
    char secondaryId[64]{};
};

struct EditorPendingCommand {
    bool active = false;
    std::string label;
    EditorSceneDocumentState beforeState;

    void clear() {
        active = false;
        label.clear();
        beforeState = EditorSceneDocumentState{};
    }
};

MaterialKind resolvePlacementMaterialKind(const std::string& materialId, const ContentRegistry& content);

struct EditorSelectionPickerState {
    std::vector<EditorHitResult> hits;
    ImVec2 anchorScreen{0.0f, 0.0f};
    std::size_t currentIndex = 0;
    double visibleUntilSeconds = 0.0;

    bool active(double nowSeconds) const {
        return !hits.empty() && nowSeconds <= visibleUntilSeconds;
    }

    void clear() {
        hits.clear();
        anchorScreen = ImVec2(0.0f, 0.0f);
        currentIndex = 0;
        visibleUntilSeconds = 0.0;
    }
};

EditorLayoutVisibility captureLayoutVisibility(const EditorUiState& ui) {
    return EditorLayoutVisibility{
        ui.showOutliner,
        ui.showInspector,
        ui.showAssetBrowser,
        ui.showEnvironment,
        ui.showViewport,
    };
}

void applyLayoutVisibility(EditorUiState& ui, const EditorLayoutVisibility& visibility) {
    ui.showOutliner = visibility.showOutliner;
    ui.showInspector = visibility.showInspector;
    ui.showAssetBrowser = visibility.showAssetBrowser;
    ui.showEnvironment = visibility.showEnvironment;
    ui.showViewport = visibility.showViewport;
}

bool saveLayoutPresetFromUi(const EditorUiState& ui, const std::string& layoutName) {
    const std::string trimmedName = layoutName;
    if (trimmedName.empty()) {
        return false;
    }

    const char* iniText = ImGui::SaveIniSettingsToMemory();
    EditorLayoutPreset preset;
    preset.name = trimmedName;
    preset.visibility = captureLayoutVisibility(ui);
    preset.imguiIni = iniText != nullptr ? std::string(iniText) : std::string();
    saveEditorLayoutPreset(editorLayoutPresetPath(trimmedName), preset);
    return true;
}

bool loadLayoutPresetIntoUi(EditorUiState& ui, const std::string& layoutName) {
    if (layoutName.empty()) {
        return false;
    }

    const EditorLayoutPreset preset = loadEditorLayoutPreset(editorLayoutPresetPath(layoutName));
    applyLayoutVisibility(ui, preset.visibility);
    ImGui::LoadIniSettingsFromMemory(preset.imguiIni.c_str(), preset.imguiIni.size());
    const std::string storedName = std::filesystem::path(editorLayoutPresetPath(layoutName)).stem().string();
    ui.activeLayoutPreset = storedName;
    std::snprintf(ui.layoutNameBuffer, sizeof(ui.layoutNameBuffer), "%s", preset.name.c_str());
    return true;
}

void copyPayloadString(char (&dst)[64], const std::string& src) {
    std::snprintf(dst, sizeof(dst), "%s", src.c_str());
}

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

EditorPlacementState makePlacementState(const EditorDragPayload& payload) {
    EditorPlacementState state;
    state.kind = payload.kind;
    state.meshId = payload.primaryId;
    state.materialId = payload.secondaryId;
    state.archetypeId = payload.primaryId;
    return state;
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

std::vector<std::string> sortedScenePaths() {
    namespace fs = std::filesystem;
    std::vector<std::string> results;
    const fs::path sceneDir(resolveProjectPath("assets/scenes"));
    if (!fs::exists(sceneDir)) {
        return results;
    }
    for (const auto& entry : fs::directory_iterator(sceneDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".scene") {
            continue;
        }
        results.push_back(fs::relative(entry.path(), fs::current_path()).generic_string());
    }
    std::sort(results.begin(), results.end());
    return results;
}

std::vector<std::string> sortedMeshNames(const MeshLibrary& library) {
    auto names = library.names();
    std::sort(names.begin(), names.end());
    return names;
}

std::vector<std::string> sortedMaterialIds(const ContentRegistry& content) {
    std::vector<std::string> ids;
    ids.reserve(content.materials().size());
    for (const auto& [id, definition] : content.materials()) {
        (void)definition;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> sortedArchetypeIds(const ContentRegistry& content) {
    std::vector<std::string> ids;
    ids.reserve(content.archetypes().size());
    for (const auto& [id, definition] : content.archetypes()) {
        (void)definition;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> sortedEnvironmentIds(const ContentRegistry& content) {
    std::vector<std::string> ids;
    ids.reserve(content.environments().size());
    for (const auto& [id, definition] : content.environments()) {
        (void)definition;
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

glm::vec3 placementForward(const glm::vec3& direction) {
    return safeNormalize(direction, glm::vec3(0.0f, -1.0f, 0.0f));
}

bool pointInViewport(const EditorViewportState& viewport, const ImVec2& mousePos) {
    return mousePos.x >= viewport.origin.x
        && mousePos.y >= viewport.origin.y
        && mousePos.x <= viewport.origin.x + viewport.size.x
        && mousePos.y <= viewport.origin.y + viewport.size.y;
}

bool isSelected(const std::vector<std::uint64_t>& selectedIds, std::uint64_t id) {
    return std::find(selectedIds.begin(), selectedIds.end(), id) != selectedIds.end();
}

std::vector<EditorSceneObject*> selectedMeshObjects(EditorSceneDocument& document,
                                                    const std::vector<std::uint64_t>& selectedIds) {
    std::vector<EditorSceneObject*> meshes;
    meshes.reserve(selectedIds.size());
    for (const std::uint64_t id : selectedIds) {
        EditorSceneObject* object = document.findObject(id);
        if (object != nullptr && object->kind == EditorSceneObjectKind::Mesh) {
            meshes.push_back(object);
        }
    }
    return meshes;
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

bool applyMaterialToMeshes(const std::vector<EditorSceneObject*>& meshObjects,
                           const std::string& materialId,
                           const ContentRegistry& content,
                           EditorSceneDocument& document) {
    if (meshObjects.empty()) {
        return false;
    }

    const MaterialKind materialKind = resolvePlacementMaterialKind(materialId, content);
    bool changed = false;
    for (EditorSceneObject* object : meshObjects) {
        if (object == nullptr || object->kind != EditorSceneObjectKind::Mesh) {
            continue;
        }
        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        if (mesh.materialId == materialId && mesh.material == materialKind) {
            continue;
        }
        mesh.materialId = materialId;
        mesh.material = materialKind;
        changed = true;
    }

    if (changed) {
        document.markSceneDirty();
    }
    return changed;
}

std::string materialSelectionLabel(const std::vector<EditorSceneObject*>& meshObjects) {
    if (meshObjects.empty()) {
        return "No Mesh Selected";
    }

    const auto& firstMesh = std::get<LevelMeshPlacement>(meshObjects.front()->payload);
    const std::string firstMaterial = firstMesh.materialId.empty()
        ? std::string(defaultMaterialIdForKind(firstMesh.material.value_or(MaterialKind::Stone)))
        : firstMesh.materialId;

    for (std::size_t index = 1; index < meshObjects.size(); ++index) {
        const auto& mesh = std::get<LevelMeshPlacement>(meshObjects[index]->payload);
        const std::string materialId = mesh.materialId.empty()
            ? std::string(defaultMaterialIdForKind(mesh.material.value_or(MaterialKind::Stone)))
            : mesh.materialId;
        if (materialId != firstMaterial) {
            return "<mixed>";
        }
    }

    return firstMaterial;
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

void beginPendingCommand(EditorPendingCommand& pending,
                         const EditorSceneDocumentState& beforeState,
                         std::string label) {
    if (pending.active) {
        return;
    }
    pending.active = true;
    pending.label = std::move(label);
    pending.beforeState = beforeState;
}

bool finalizePendingCommand(EditorPendingCommand& pending,
                            EditorCommandStack& commandStack,
                            EditorSceneDocument& document) {
    if (!pending.active) {
        return false;
    }
    const bool pushed = commandStack.pushDocumentStateCommand(
        pending.label, pending.beforeState, document.captureState(), document);
    pending.clear();
    return pushed;
}

void trackLastItemCommand(const EditorSceneDocumentState& beforeState,
                          std::string label,
                          EditorPendingCommand& pending,
                          EditorCommandStack& commandStack,
                          EditorSceneDocument& document) {
    const bool itemActive = ImGui::IsItemActive();
    const bool itemActivated = ImGui::IsItemActivated();
    const bool itemEdited = ImGui::IsItemEdited();
    const bool itemDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();

    if (itemActivated && !pending.active) {
        beginPendingCommand(pending, beforeState, label);
    }

    if (itemDeactivatedAfterEdit) {
        if (!pending.active) {
            beginPendingCommand(pending, beforeState, label);
        }
        finalizePendingCommand(pending, commandStack, document);
        return;
    }

    if (itemEdited && !itemActive) {
        commandStack.pushDocumentStateCommand(
            std::move(label), beforeState, document.captureState(), document);
        return;
    }

    if (pending.active && pending.label == label && !itemActive && !itemEdited) {
        pending.clear();
    }
}

LightingEnvironment makeLightingEnvironment(const EnvironmentDefinition& environment) {
    LightingEnvironment lighting = environment.lighting;
    lighting.sun.direction = safeNormalize(lighting.sun.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    lighting.fill.direction = safeNormalize(lighting.fill.direction, glm::vec3(0.0f, -1.0f, 0.0f));
    return lighting;
}

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
                      std::array<ShadowMap, kMaxShadowedSpotLightsLocal>& shadowMaps,
                      int shadowResolution,
                      ShadowRenderData& shadowData) {
    shadowData.shadowCount = 0;
    shadowData.matrices = {glm::mat4(1.0f), glm::mat4(1.0f)};
    shadowData.textures = {
        shadowMaps[0].texture(),
        shadowMaps[1].texture(),
    };

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

bool editVec3(const char* label, glm::vec3& value, float speed = 0.1f) {
    return ImGui::DragFloat3(label, &value.x, speed, -1000.0f, 1000.0f, "%.2f");
}

bool editColor(const char* label, glm::vec3& value) {
    return ImGui::ColorEdit3(label, &value.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs);
}

bool editString(const char* label, std::string& value, const char* hint = nullptr) {
    char buffer[512]{};
    std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
    bool changed = false;
    if (hint != nullptr && hint[0] != '\0') {
        changed = ImGui::InputTextWithHint(label, hint, buffer, sizeof(buffer));
    } else {
        changed = ImGui::InputText(label, buffer, sizeof(buffer));
    }
    if (changed) {
        value = buffer;
    }
    return changed;
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
    bool supported = true;
    switch (object->kind) {
    case EditorSceneObjectKind::Mesh: {
        model = document.worldTransformMatrix(object->id);
        break;
    }
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
    case EditorSceneObjectKind::BoxCollider: {
        model = document.worldTransformMatrix(object->id);
        break;
    }
    case EditorSceneObjectKind::CylinderCollider: {
        model = document.worldTransformMatrix(object->id);
        break;
    }
    case EditorSceneObjectKind::PlayerSpawn: {
        glm::mat4 world = document.worldTransformMatrix(object->id);
        world = glm::scale(world, glm::vec3(0.5f, 1.4f, 0.5f));
        model = world;
        break;
    }
    case EditorSceneObjectKind::Archetype: {
        model = document.worldTransformMatrix(object->id);
        break;
    }
    }

    if (!supported) {
        return false;
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
    case EditorSceneObjectKind::Mesh: {
        if (!document.applyWorldTransform(object->id, model)) {
            return false;
        }
        break;
    }
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
    case EditorSceneObjectKind::BoxCollider: {
        if (!document.applyWorldTransform(object->id, model)) {
            return false;
        }
        break;
    }
    case EditorSceneObjectKind::CylinderCollider: {
        if (!document.applyWorldTransform(object->id, model)) {
            return false;
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
    case EditorSceneObjectKind::Archetype: {
        if (!document.applyWorldTransform(object->id, model)) {
            return false;
        }
        break;
    }
    }
    return true;
}

void beginPlacement(EditorPlacementState& state,
                    EditorPlacementKind kind,
                    const std::string& primaryId = {},
                    const std::string& secondaryId = {}) {
    state.clear();
    state.kind = kind;
    switch (kind) {
    case EditorPlacementKind::Mesh:
        state.meshId = primaryId;
        state.materialId = secondaryId;
        break;
    case EditorPlacementKind::Archetype:
        state.archetypeId = primaryId;
        break;
    default:
        break;
    }
}

void emitPlacementDragSource(EditorPlacementKind kind,
                             const std::string& primaryId = {},
                             const std::string& secondaryId = {}) {
    if (!ImGui::BeginDragDropSource()) {
        return;
    }
    EditorDragPayload payload;
    payload.kind = kind;
    copyPayloadString(payload.primaryId, primaryId);
    copyPayloadString(payload.secondaryId, secondaryId);
    ImGui::SetDragDropPayload("EDITOR_PLACE", &payload, sizeof(payload));
    ImGui::TextUnformatted("Place in viewport");
    ImGui::EndDragDropSource();
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

MaterialKind resolvePlacementMaterialKind(const std::string& materialId, const ContentRegistry& content) {
    if (const auto* material = content.findMaterial(materialId)) {
        return resolveMaterialDefinition(material->id, content.materials()).shadingModel;
    }
    return MaterialKind::Stone;
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

bool renderEnvironmentPanel(EditorSceneDocument& document,
                            ContentRegistry& content,
                            std::vector<std::string>& environmentIds,
                            bool* open,
                            EditorPendingCommand& pendingCommand,
                            EditorCommandStack& commandStack) {
    static bool showSaveAsProfile = false;
    static char saveAsProfileBuffer[256]{};
    bool contentReloaded = false;
    if (open != nullptr && !*open) {
        return contentReloaded;
    }
    EnvironmentDefinition& environment = document.environment();
    if (!ImGui::Begin("Environment", open)) {
        ImGui::End();
        return contentReloaded;
    }

    if (ImGui::BeginCombo("Environment", environment.id.c_str())) {
        for (const auto& id : environmentIds) {
            const bool selected = (id == environment.id);
            if (ImGui::Selectable(id.c_str(), selected)) {
                const EditorSceneDocumentState beforeState = document.captureState();
                document.setEnvironmentId(id, content);
                commandStack.pushDocumentStateCommand(
                    "Change Environment",
                    beforeState,
                    document.captureState(),
                    document);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Button("Save")) {
        try {
            saveEnvironmentDefinitionAsset(document.environmentAssetPath(content), environment);
            document.markEnvironmentSaved();
            content.loadDefaults();
            environmentIds = sortedEnvironmentIds(content);
            contentReloaded = true;
        } catch (const std::exception& ex) {
            spdlog::error("Environment save failed: {}", ex.what());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As")) {
        showSaveAsProfile = true;
        std::snprintf(saveAsProfileBuffer, sizeof(saveAsProfileBuffer), "%s", environment.id.c_str());
        ImGui::OpenPopup("Save Environment As");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        try {
            const EditorSceneDocumentState beforeState = document.captureState();
            content.loadDefaults();
            environmentIds = sortedEnvironmentIds(content);
            contentReloaded = true;
            if (document.reloadEnvironmentFromRegistry(content)) {
                commandStack.pushDocumentStateCommand(
                    "Reload Environment",
                    beforeState,
                    document.captureState(),
                    document);
            }
        } catch (const std::exception& ex) {
            spdlog::error("Environment reload failed: {}", ex.what());
        }
    }

    if (showSaveAsProfile && ImGui::BeginPopupModal("Save Environment As", &showSaveAsProfile, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Profile Id", saveAsProfileBuffer, sizeof(saveAsProfileBuffer));
        if (ImGui::Button("Create Profile")) {
            try {
                const EditorSceneDocumentState beforeState = document.captureState();
                document.renameEnvironmentId(saveAsProfileBuffer);
                saveEnvironmentDefinitionAsset(document.environmentAssetPath(content), document.environment());
                document.markEnvironmentSaved();
                content.loadDefaults();
                environmentIds = sortedEnvironmentIds(content);
                contentReloaded = true;
                commandStack.pushDocumentStateCommand(
                    "Save Environment As",
                    beforeState,
                    document.captureState(),
                    document);
                showSaveAsProfile = false;
                ImGui::CloseCurrentPopup();
            } catch (const std::exception& ex) {
                spdlog::error("Environment save-as failed: {}", ex.what());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            showSaveAsProfile = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::CollapsingHeader("Post", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto trackEnvItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
            if (changed) {
                document.markEnvironmentDirty();
            }
            trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
        };

        static constexpr const char* kPaletteVariants[] = {
            "Neutral",
            "Dungeon",
            "Meadow",
            "Dusk",
            "Arcane",
            "Cathedral",
        };
        static constexpr const char* kToneMapModes[] = {
            "Linear",
            "ACES Fitted",
        };

        auto beforeState = document.captureState();
        int paletteVariant = environment.post.paletteVariant;
        const bool paletteChanged = ImGui::Combo("Palette Variant", &paletteVariant, kPaletteVariants, 6);
        if (paletteChanged) {
            environment.post.paletteVariant = paletteVariant;
        }
        trackEnvItem(beforeState, "Change Palette Variant", paletteChanged);
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sky In Post", ImGui::Checkbox("Enable Sky", &environment.post.enableSky));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Dither", ImGui::Checkbox("Enable Dither", &environment.post.enableDither));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Edges", ImGui::Checkbox("Enable Edges", &environment.post.enableEdges));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Fog", ImGui::Checkbox("Enable Fog", &environment.post.enableFog));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Tone Map", ImGui::Checkbox("Enable Tone Map", &environment.post.enableToneMap));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Bloom", ImGui::Checkbox("Enable Bloom", &environment.post.enableBloom));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Vignette", ImGui::Checkbox("Enable Vignette", &environment.post.enableVignette));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Grain", ImGui::Checkbox("Enable Grain", &environment.post.enableGrain));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Scanlines", ImGui::Checkbox("Enable Scanlines", &environment.post.enableScanlines));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sharpen", ImGui::Checkbox("Enable Sharpen", &environment.post.enableSharpen));
        beforeState = document.captureState();
        int toneMapMode = environment.post.toneMapMode;
        const bool toneMapChanged = ImGui::Combo("Tone Mapper", &toneMapMode, kToneMapModes, 2);
        if (toneMapChanged) {
            environment.post.toneMapMode = toneMapMode;
        }
        trackEnvItem(beforeState, "Change Tone Mapper", toneMapChanged);
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Threshold Bias", ImGui::DragFloat("Threshold Bias", &environment.post.thresholdBias, 0.002f, -0.5f, 0.5f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Pattern Scale", ImGui::DragFloat("Pattern Scale", &environment.post.patternScale, 1.0f, 0.0f, 512.0f, "%.1f"));
        if (environment.post.patternScale <= 1.0f) {
            ImGui::TextDisabled("Pattern Scale: Auto");
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Edge Threshold", ImGui::DragFloat("Edge Threshold", &environment.post.edgeThreshold, 0.005f, 0.0f, 1.0f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Fog Density", ImGui::DragFloat("Fog Density", &environment.post.fogDensity, 0.001f, 0.0f, 0.5f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Fog Start", ImGui::DragFloat("Fog Start", &environment.post.fogStart, 0.1f, 0.0f, 80.0f, "%.1f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Depth View Scale", ImGui::DragFloat("Depth View Scale", &environment.post.depthViewScale, 0.001f, 0.01f, 0.30f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Exposure", ImGui::DragFloat("Exposure", &environment.post.exposure, 0.01f, 0.2f, 2.5f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Gamma", ImGui::DragFloat("Gamma", &environment.post.gamma, 0.01f, 0.5f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Contrast", ImGui::DragFloat("Contrast", &environment.post.contrast, 0.01f, 0.4f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Saturation", ImGui::DragFloat("Saturation", &environment.post.saturation, 0.01f, 0.0f, 1.5f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Bloom Threshold", ImGui::DragFloat("Bloom Threshold", &environment.post.bloomThreshold, 0.01f, 0.0f, 1.5f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Bloom Intensity", ImGui::DragFloat("Bloom Intensity", &environment.post.bloomIntensity, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Bloom Radius", ImGui::DragFloat("Bloom Radius", &environment.post.bloomRadius, 0.01f, 0.5f, 5.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Vignette Strength", ImGui::DragFloat("Vignette Strength", &environment.post.vignetteStrength, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Vignette Softness", ImGui::DragFloat("Vignette Softness", &environment.post.vignetteSoftness, 0.01f, 0.1f, 1.2f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Grain Amount", ImGui::DragFloat("Grain Amount", &environment.post.grainAmount, 0.001f, 0.0f, 0.2f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Scanline Amount", ImGui::DragFloat("Scanline Amount", &environment.post.scanlineAmount, 0.01f, 0.0f, 0.35f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Scanline Density", ImGui::DragFloat("Scanline Density", &environment.post.scanlineDensity, 0.01f, 0.5f, 3.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sharpen Amount", ImGui::DragFloat("Sharpen Amount", &environment.post.sharpenAmount, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Split Tone Strength", ImGui::DragFloat("Split Strength", &environment.post.splitToneStrength, 0.01f, 0.0f, 0.6f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Split Tone Balance", ImGui::DragFloat("Split Balance", &environment.post.splitToneBalance, 0.01f, 0.15f, 0.85f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Fog Near Color", editColor("Fog Near", environment.post.fogNearColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Fog Far Color", editColor("Fog Far", environment.post.fogFarColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Shadow Tint", editColor("Shadow Tint", environment.post.shadowTint));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Highlight Tint", editColor("Highlight Tint", environment.post.highlightTint));
    }

    if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto trackEnvItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
            if (changed) {
                document.markEnvironmentDirty();
            }
            trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
        };

        auto beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Sky System", ImGui::Checkbox("Sky Enabled", &environment.sky.enabled));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Zenith Color", editColor("Zenith", environment.sky.zenithColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Horizon Color", editColor("Horizon", environment.sky.horizonColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Ground Haze Color", editColor("Ground Haze", environment.sky.groundHazeColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Direction", editVec3("Sun Direction", environment.sky.sunDirection, 0.01f));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Sky Sun Color", editColor("Sun Color", environment.sky.sunColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Size", ImGui::DragFloat("Sun Size", &environment.sky.sunSize, 0.001f, 0.001f, 0.10f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Sky Sun Glow", ImGui::DragFloat("Sun Glow", &environment.sky.sunGlow, 0.01f, 0.0f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Set Panorama Path", editString("Panorama Path", environment.sky.panoramaPath, "assets/skies/sky.jpg"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Panorama Tint", editColor("Panorama Tint", environment.sky.panoramaTint));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Panorama Strength", ImGui::DragFloat("Panorama Strength", &environment.sky.panoramaStrength, 0.01f, 0.0f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Panorama Yaw", ImGui::DragFloat("Panorama Yaw", &environment.sky.panoramaYawOffset, 0.5f, -360.0f, 360.0f, "%.1f"));
        if (ImGui::TreeNodeEx("Cubemap", ImGuiTreeNodeFlags_DefaultOpen)) {
            static constexpr std::array<const char*, 6> kCubemapLabels{
                "Face +X",
                "Face -X",
                "Face +Y",
                "Face -Y",
                "Face +Z",
                "Face -Z",
            };
            static constexpr std::array<const char*, 6> kCubemapCommandLabels{
                "Set Cubemap Face +X",
                "Set Cubemap Face -X",
                "Set Cubemap Face +Y",
                "Set Cubemap Face -Y",
                "Set Cubemap Face +Z",
                "Set Cubemap Face -Z",
            };
            for (std::size_t i = 0; i < kCubemapLabels.size(); ++i) {
                beforeState = document.captureState();
                trackEnvItem(beforeState,
                             kCubemapCommandLabels[i],
                             editString(kCubemapLabels[i],
                                        environment.sky.cubemapFacePaths[i],
                                        "assets/skies/cubemap_face.png"));
            }
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Cubemap Tint", editColor("Cubemap Tint", environment.sky.cubemapTint));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Cubemap Strength", ImGui::DragFloat("Cubemap Strength", &environment.sky.cubemapStrength, 0.01f, 0.0f, 2.0f, "%.2f"));
            ImGui::TreePop();
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Moon", ImGui::Checkbox("Moon Enabled", &environment.sky.moonEnabled));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Moon Direction", editVec3("Moon Direction", environment.sky.moonDirection, 0.01f));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Moon Color", editColor("Moon Color", environment.sky.moonColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Moon Size", ImGui::DragFloat("Moon Size", &environment.sky.moonSize, 0.001f, 0.001f, 0.10f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Moon Glow", ImGui::DragFloat("Moon Glow", &environment.sky.moonGlow, 0.01f, 0.0f, 1.0f, "%.2f"));
        if (ImGui::TreeNodeEx("Sky Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Set Cloud Layer A", editString("Cloud Layer A", environment.sky.cloudLayerAPath, "assets/skies/clouds_a.png"));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Set Cloud Layer B", editString("Cloud Layer B", environment.sky.cloudLayerBPath, "assets/skies/clouds_b.png"));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Set Horizon Layer", editString("Horizon Layer", environment.sky.horizonLayerPath, "assets/skies/horizon.png"));
            ImGui::TreePop();
        }
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Cloud Tint", editColor("Cloud Tint", environment.sky.cloudTint));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Scale", ImGui::DragFloat("Cloud Scale", &environment.sky.cloudScale, 0.01f, 0.1f, 4.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Speed", ImGui::DragFloat("Cloud Speed", &environment.sky.cloudSpeed, 0.0002f, 0.0f, 0.05f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Coverage", ImGui::DragFloat("Cloud Coverage", &environment.sky.cloudCoverage, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Cloud Parallax", ImGui::DragFloat("Cloud Parallax", &environment.sky.cloudParallax, 0.001f, 0.0f, 0.20f, "%.3f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Horizon Tint", editColor("Horizon Tint", environment.sky.horizonTint));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Horizon Height", ImGui::DragFloat("Horizon Height", &environment.sky.horizonHeight, 0.01f, 0.0f, 1.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Horizon Fade", ImGui::DragFloat("Horizon Fade", &environment.sky.horizonFade, 0.01f, 0.0f, 1.0f, "%.2f"));
    }

    if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto trackEnvItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
            if (changed) {
                document.markEnvironmentDirty();
            }
            trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
        };

        auto beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Hemisphere Sky Color", editColor("Hemi Sky", environment.lighting.hemisphereSkyColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Change Hemisphere Ground Color", editColor("Hemi Ground", environment.lighting.hemisphereGroundColor));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Hemisphere Strength", ImGui::DragFloat("Hemi Strength", &environment.lighting.hemisphereStrength, 0.01f, 0.0f, 2.0f, "%.2f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Directional Lighting", ImGui::Checkbox("Enable Directional", &environment.lighting.enableDirectionalLights));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Toggle Environment Shadows", ImGui::Checkbox("Enable Shadows", &environment.lighting.enableShadows));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Shadow Bias", ImGui::DragFloat("Shadow Bias", &environment.lighting.shadowBias, 0.0001f, 0.0001f, 0.02f, "%.4f"));
        beforeState = document.captureState();
        trackEnvItem(beforeState, "Adjust Shadow Normal Bias", ImGui::DragFloat("Shadow Normal Bias", &environment.lighting.shadowNormalBias, 0.001f, 0.0f, 0.20f, "%.3f"));
        if (ImGui::TreeNodeEx("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Toggle Sun Light", ImGui::Checkbox("Sun Enabled", &environment.lighting.sun.enabled));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Sun Direction", editVec3("Sun Dir##slot", environment.lighting.sun.direction, 0.01f));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Sun Color", editColor("Sun Color##slot", environment.lighting.sun.color));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Sun Intensity", ImGui::DragFloat("Sun Intensity##slot", &environment.lighting.sun.intensity, 0.01f, 0.0f, 4.0f, "%.2f"));
            ImGui::TreePop();
        }
        if (ImGui::TreeNodeEx("Fill", ImGuiTreeNodeFlags_DefaultOpen)) {
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Toggle Fill Light", ImGui::Checkbox("Fill Enabled", &environment.lighting.fill.enabled));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Fill Direction", editVec3("Fill Dir##slot", environment.lighting.fill.direction, 0.01f));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Change Fill Color", editColor("Fill Color##slot", environment.lighting.fill.color));
            beforeState = document.captureState();
            trackEnvItem(beforeState, "Adjust Fill Intensity", ImGui::DragFloat("Fill Intensity##slot", &environment.lighting.fill.intensity, 0.01f, 0.0f, 4.0f, "%.2f"));
            ImGui::TreePop();
        }
    }

    ImGui::End();
    return contentReloaded;
}

std::vector<std::uint64_t> renderOutliner(EditorSceneDocument& document,
                                          std::vector<std::uint64_t>& selectedIds,
                                          bool* open,
                                          EditorCommandStack& commandStack) {
    std::vector<std::uint64_t> deleteRequests;
    if (open != nullptr && !*open) {
        return deleteRequests;
    }
    if (!ImGui::Begin("Outliner", open)) {
        ImGui::End();
        return deleteRequests;
    }
    const bool additive = ImGui::GetIO().KeyShift;

    std::function<void(std::uint64_t)> renderNode = [&](std::uint64_t objectId) {
        const EditorSceneObject* object = document.findObject(objectId);
        if (object == nullptr) {
            return;
        }

        const auto children = document.childObjectIds(objectId);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
        if (children.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }
        if (isSelected(selectedIds, objectId)) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        const bool openNode = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(objectId)),
                                                flags,
                                                "%s",
                                                editorSceneObjectLabel(*object).c_str());
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            toggleSelection(selectedIds, objectId, additive);
        }
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !isSelected(selectedIds, objectId)) {
            selectedIds = {objectId};
        }

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("EDITOR_OUTLINER_OBJECT", &objectId, sizeof(objectId));
            ImGui::TextUnformatted(editorSceneObjectLabel(*object).c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_OUTLINER_OBJECT")) {
                if (payload->DataSize == sizeof(std::uint64_t)) {
                    const std::uint64_t draggedId = *static_cast<const std::uint64_t*>(payload->Data);
                    if (draggedId != objectId && document.canSetParent(draggedId, objectId)) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        if (document.setParent(draggedId, objectId)) {
                            commandStack.pushDocumentStateCommand(
                                "Parent Object",
                                beforeState,
                                document.captureState(),
                                document);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("OutlinerContext")) {
            if (ImGui::MenuItem("Delete")) {
                deleteRequests.push_back(objectId);
            }
            if (document.parentObjectId(objectId) != 0 && ImGui::MenuItem("Clear Parent")) {
                const EditorSceneDocumentState beforeState = document.captureState();
                if (document.clearParent(objectId)) {
                    commandStack.pushDocumentStateCommand(
                        "Clear Parent",
                        beforeState,
                        document.captureState(),
                        document);
                }
            }
            ImGui::EndPopup();
        }

        if (openNode) {
            for (const auto childId : children) {
                renderNode(childId);
            }
            ImGui::TreePop();
        }
    };

    for (const auto rootId : document.rootObjectIds()) {
        renderNode(rootId);
    }

    ImGui::End();
    return deleteRequests;
}

void renderAssetBrowser(EditorUiState& ui,
                        EditorPlacementState& placementState,
                        EditorSceneDocument& document,
                        const std::vector<std::uint64_t>& selectedIds,
                        const ContentRegistry& content,
                        const std::vector<std::string>& meshIds,
                        const std::vector<std::string>& materialIds,
                        const std::vector<std::string>& archetypeIds,
                        bool* open,
                        EditorCommandStack& commandStack) {
    if (open != nullptr && !*open) {
        return;
    }
    if (!ImGui::Begin("Asset Browser", open)) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Add Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo("Mesh", ui.selectedMeshId.c_str())) {
            for (const auto& meshId : meshIds) {
                const bool selected = meshId == ui.selectedMeshId;
                if (ImGui::Selectable(meshId.c_str(), selected)) {
                    ui.selectedMeshId = meshId;
                }
                emitPlacementDragSource(EditorPlacementKind::Mesh, meshId, ui.selectedMaterialId);
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("Material", ui.selectedMaterialId.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == ui.selectedMaterialId;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    ui.selectedMaterialId = materialId;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Place Mesh")) {
            beginPlacement(placementState, EditorPlacementKind::Mesh, ui.selectedMeshId, ui.selectedMaterialId);
        }
    }

    if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo("Active Material", ui.selectedMaterialId.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == ui.selectedMaterialId;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    ui.selectedMaterialId = materialId;
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    ui.selectedMaterialId = materialId;
                    const auto meshObjects = selectedMeshObjects(document, selectedIds);
                    const EditorSceneDocumentState beforeState = document.captureState();
                    if (applyMaterialToMeshes(meshObjects, materialId, content, document)) {
                        commandStack.pushDocumentStateCommand(
                            meshObjects.size() == 1 ? "Assign Mesh Material" : "Assign Mesh Materials",
                            beforeState,
                            document.captureState(),
                            document);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        const auto meshObjects = selectedMeshObjects(document, selectedIds);
        if (meshObjects.empty()) {
            ImGui::TextDisabled("Select one or more mesh placements to assign a material.");
        } else {
            ImGui::Text("%zu mesh%s selected", meshObjects.size(), meshObjects.size() == 1 ? "" : "es");
            ImGui::TextDisabled("Current: %s", materialSelectionLabel(meshObjects).c_str());
            if (ImGui::Button(meshObjects.size() == 1 ? "Apply To Selected Mesh" : "Apply To Selected Meshes")) {
                const EditorSceneDocumentState beforeState = document.captureState();
                if (applyMaterialToMeshes(meshObjects, ui.selectedMaterialId, content, document)) {
                    commandStack.pushDocumentStateCommand(
                        meshObjects.size() == 1 ? "Assign Mesh Material" : "Assign Mesh Materials",
                        beforeState,
                        document.captureState(),
                        document);
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Add Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Place Point Light")) {
            beginPlacement(placementState, EditorPlacementKind::PointLight);
        }
        if (ImGui::Button("Place Spot Light")) {
            beginPlacement(placementState, EditorPlacementKind::SpotLight);
        }
        if (ImGui::Button("Add Directional Light")) {
            beginPlacement(placementState, EditorPlacementKind::DirectionalLight);
        }
    }

    if (ImGui::CollapsingHeader("Add Colliders", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Place Box Collider")) {
            beginPlacement(placementState, EditorPlacementKind::BoxCollider);
        }
        if (ImGui::Button("Place Cylinder Collider")) {
            beginPlacement(placementState, EditorPlacementKind::CylinderCollider);
        }
        if (ImGui::Button("Place Player Spawn")) {
            beginPlacement(placementState, EditorPlacementKind::PlayerSpawn);
        }
    }

    if (ImGui::CollapsingHeader("Add Archetype", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginCombo("Archetype", ui.selectedArchetypeId.c_str())) {
            for (const auto& archetypeId : archetypeIds) {
                const bool selected = archetypeId == ui.selectedArchetypeId;
                if (ImGui::Selectable(archetypeId.c_str(), selected)) {
                    ui.selectedArchetypeId = archetypeId;
                }
                emitPlacementDragSource(EditorPlacementKind::Archetype, archetypeId);
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Place Archetype")) {
            beginPlacement(placementState, EditorPlacementKind::Archetype, ui.selectedArchetypeId);
        }
    }

    if (placementState.active()) {
        ImGui::Separator();
        ImGui::Text("Placement: active");
        if (ImGui::Button("Cancel Placement")) {
            placementState.clear();
        }
    }

    ImGui::End();
}

void renderInspector(EditorSceneDocument& document,
                     const std::vector<std::uint64_t>& selectedIds,
                     const ContentRegistry& content,
                     const std::vector<std::string>& meshIds,
                     const std::vector<std::string>& materialIds,
                     const std::vector<std::string>& archetypeIds,
                     bool* open,
                     EditorPendingCommand& pendingCommand,
                     EditorCommandStack& commandStack) {
    if (open != nullptr && !*open) {
        return;
    }
    if (!ImGui::Begin("Inspector", open)) {
        ImGui::End();
        return;
    }
    if (selectedIds.empty()) {
        ImGui::TextUnformatted("No selection");
        ImGui::End();
        return;
    }

    const auto trackSceneItem = [&](const EditorSceneDocumentState& beforeState, const std::string& label, bool changed) {
        if (changed) {
            document.markSceneDirty();
        }
        trackLastItemCommand(beforeState, label, pendingCommand, commandStack, document);
    };

    if (selectedIds.size() > 1) {
        ImGui::Text("%zu objects selected", selectedIds.size());
        const auto meshObjects = selectedMeshObjects(document, selectedIds);
        if (!meshObjects.empty()) {
            ImGui::Separator();
            ImGui::Text("Mesh Materials");
            const std::string currentMaterialLabel = materialSelectionLabel(meshObjects);
            if (ImGui::BeginCombo("Material Id", currentMaterialLabel.c_str())) {
                for (const auto& materialId : materialIds) {
                    const bool selected = materialId == currentMaterialLabel;
                    if (ImGui::Selectable(materialId.c_str(), selected)) {
                        const EditorSceneDocumentState beforeState = document.captureState();
                        if (applyMaterialToMeshes(meshObjects, materialId, content, document)) {
                            commandStack.pushDocumentStateCommand(
                                meshObjects.size() == 1 ? "Assign Mesh Material" : "Assign Mesh Materials",
                                beforeState,
                                document.captureState(),
                                document);
                        }
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("Applies to %zu selected mesh%s", meshObjects.size(), meshObjects.size() == 1 ? "" : "es");
        }
        ImGui::End();
        return;
    }

    EditorSceneObject* object = document.findObject(selectedIds.front());
    if (object == nullptr) {
        ImGui::TextUnformatted("Selection missing");
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(editorSceneObjectKindName(object->kind));
    ImGui::TextDisabled("Id: %llu", static_cast<unsigned long long>(object->id));
    ImGui::Separator();

    const std::uint64_t currentParentId = document.parentObjectId(object->id);
    if (document.supportsParenting(object->id) || currentParentId != 0) {
        std::string parentLabel = "<None>";
        if (currentParentId != 0) {
            if (const EditorSceneObject* parentObject = document.findObject(currentParentId)) {
                parentLabel = editorSceneObjectLabel(*parentObject);
            }
        }

        if (ImGui::BeginCombo("Parent", parentLabel.c_str())) {
            const bool noParentSelected = (currentParentId == 0);
            if (ImGui::Selectable("<None>", noParentSelected)) {
                const EditorSceneDocumentState beforeState = document.captureState();
                if (document.clearParent(object->id)) {
                    commandStack.pushDocumentStateCommand(
                        "Clear Parent",
                        beforeState,
                        document.captureState(),
                        document);
                }
            }
            if (noParentSelected) {
                ImGui::SetItemDefaultFocus();
            }

            for (const auto& candidate : document.objects()) {
                if (candidate.id == object->id) {
                    continue;
                }
                if (!document.canSetParent(object->id, candidate.id) && candidate.id != currentParentId) {
                    continue;
                }
                const bool selected = (candidate.id == currentParentId);
                if (ImGui::Selectable(editorSceneObjectLabel(candidate).c_str(), selected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    if (document.setParent(object->id, candidate.id)) {
                        commandStack.pushDocumentStateCommand(
                            "Set Parent",
                            beforeState,
                            document.captureState(),
                            document);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    switch (object->kind) {
    case EditorSceneObjectKind::Mesh: {
        auto& mesh = std::get<LevelMeshPlacement>(object->payload);
        const std::string currentMaterialLabel = mesh.materialId.empty()
            ? std::string(defaultMaterialIdForKind(mesh.material.value_or(MaterialKind::Stone)))
            : mesh.materialId;
        if (ImGui::BeginCombo("Mesh Id", mesh.meshId.c_str())) {
            for (const auto& meshId : meshIds) {
                const bool selected = meshId == mesh.meshId;
                if (ImGui::Selectable(meshId.c_str(), selected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    mesh.meshId = meshId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand(
                        "Change Mesh Asset",
                        beforeState,
                        document.captureState(),
                        document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("Material Id", currentMaterialLabel.c_str())) {
            for (const auto& materialId : materialIds) {
                const bool selected = materialId == currentMaterialLabel;
                if (ImGui::Selectable(materialId.c_str(), selected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    if (applyMaterialToMeshes({object}, materialId, content, document)) {
                        commandStack.pushDocumentStateCommand(
                            "Change Mesh Material",
                            beforeState,
                            document.captureState(),
                            document);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Mesh", editVec3("Position", mesh.position));
        beforeState = document.captureState();
        const bool scaleChanged = editVec3("Scale", mesh.scale, 0.02f);
        if (scaleChanged) {
            mesh.scale = glm::max(mesh.scale, glm::vec3(0.01f));
        }
        trackSceneItem(beforeState, "Scale Mesh", scaleChanged);
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Rotate Mesh", editVec3("Rotation", mesh.rotation, 0.5f));
        bool hasTint = mesh.tint.has_value();
        beforeState = document.captureState();
        const bool tintToggleChanged = ImGui::Checkbox("Use Tint", &hasTint);
        if (tintToggleChanged) {
            mesh.tint = hasTint ? std::optional<glm::vec3>{mesh.tint.value_or(glm::vec3(1.0f))} : std::nullopt;
        }
        trackSceneItem(beforeState, "Toggle Mesh Tint", tintToggleChanged);
        if (mesh.tint.has_value()) {
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Change Mesh Tint", editColor("Tint", *mesh.tint));
        }
        break;
    }
    case EditorSceneObjectKind::Light: {
        auto& light = std::get<LevelLightPlacement>(object->payload);
        int typeIndex = static_cast<int>(light.type);
        const char* lightTypes[] = {"Point", "Spot", "Directional"};
        if (ImGui::Combo("Light Type", &typeIndex, lightTypes, 3)) {
            const EditorSceneDocumentState beforeState = document.captureState();
            light.type = static_cast<LightType>(typeIndex);
            document.markSceneDirty();
            commandStack.pushDocumentStateCommand(
                "Change Light Type",
                beforeState,
                document.captureState(),
                document);
        }
        if (light.type != LightType::Directional) {
            const auto beforeState = document.captureState();
            trackSceneItem(beforeState, "Move Light", editVec3("Position", light.position));
        }
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Light Direction", editVec3("Direction", light.direction, 0.01f));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Change Light Color", editColor("Color", light.color));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Light Intensity", ImGui::DragFloat("Intensity", &light.intensity, 0.01f, 0.0f, 10.0f, "%.2f"));
        if (light.type != LightType::Directional) {
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Adjust Light Radius", ImGui::DragFloat("Radius", &light.radius, 0.05f, 0.1f, 40.0f, "%.2f"));
        }
        if (light.type == LightType::Spot) {
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Adjust Spot Inner Cone", ImGui::DragFloat("Inner Cone", &light.innerConeDegrees, 0.5f, 1.0f, 85.0f, "%.1f"));
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Adjust Spot Outer Cone", ImGui::DragFloat("Outer Cone", &light.outerConeDegrees, 0.5f, 1.0f, 89.0f, "%.1f"));
            beforeState = document.captureState();
            trackSceneItem(beforeState, "Toggle Spot Shadows", ImGui::Checkbox("Casts Shadows", &light.castsShadows));
        }
        break;
    }
    case EditorSceneObjectKind::BoxCollider: {
        auto& box = std::get<LevelBoxColliderPlacement>(object->payload);
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Box Collider", editVec3("Position", box.position));
        beforeState = document.captureState();
        const bool extentsChanged = editVec3("Half Extents", box.halfExtents, 0.02f);
        if (extentsChanged) {
            box.halfExtents = glm::max(box.halfExtents, glm::vec3(0.01f));
        }
        trackSceneItem(beforeState, "Resize Box Collider", extentsChanged);
        break;
    }
    case EditorSceneObjectKind::CylinderCollider: {
        auto& cylinder = std::get<LevelCylinderColliderPlacement>(object->payload);
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Cylinder Collider", editVec3("Position", cylinder.position));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Cylinder Radius", ImGui::DragFloat("Radius", &cylinder.radius, 0.02f, 0.05f, 20.0f, "%.2f"));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Cylinder Height", ImGui::DragFloat("Half Height", &cylinder.halfHeight, 0.02f, 0.05f, 20.0f, "%.2f"));
        break;
    }
    case EditorSceneObjectKind::PlayerSpawn: {
        auto& spawn = std::get<LevelPlayerSpawn>(object->payload);
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Player Spawn", editVec3("Position", spawn.position));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Adjust Fall Respawn Height", ImGui::DragFloat("Fall Respawn Y", &spawn.fallRespawnY, 0.1f, -100.0f, 100.0f, "%.2f"));
        break;
    }
    case EditorSceneObjectKind::Archetype: {
        auto& archetype = std::get<LevelArchetypePlacement>(object->payload);
        if (ImGui::BeginCombo("Archetype Id", archetype.archetypeId.c_str())) {
            for (const auto& archetypeId : archetypeIds) {
                const bool selected = archetypeId == archetype.archetypeId;
                if (ImGui::Selectable(archetypeId.c_str(), selected)) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    archetype.archetypeId = archetypeId;
                    document.markSceneDirty();
                    commandStack.pushDocumentStateCommand(
                        "Change Archetype",
                        beforeState,
                        document.captureState(),
                        document);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        auto beforeState = document.captureState();
        trackSceneItem(beforeState, "Move Archetype", editVec3("Position", archetype.position));
        beforeState = document.captureState();
        trackSceneItem(beforeState, "Rotate Archetype", ImGui::DragFloat("Yaw", &archetype.yawDegrees, 0.5f, -360.0f, 360.0f, "%.1f"));
        break;
    }
    }

    ImGui::End();
}

} // namespace

int main(int argc, char* argv[]) {
    spdlog::set_level(spdlog::level::info);

    const std::string initialScene = argc > 1 ? argv[1] : "assets/scenes/silos_cloister.scene";

    Window window(1600, 960, "Level Editor");
    glfwSwapInterval(1);

    ImGuiLayer imgui;
    imgui.init(window.handle());
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

    ContentRegistry content;
    content.loadDefaults();

    MaterialTextureLibrary materialTextures;
    materialTextures.init(content);

    EditorSceneDocument document;
    document.loadFromSceneFile(initialScene, content);
    EditorCommandStack commandStack;
    commandStack.reset(document);

    EditorPreviewWorld previewWorld;
    previewWorld.rebuild(document, content);

    std::unique_ptr<Shader> sceneShader = std::make_unique<Shader>(
        "assets/shaders/game/scene.vert",
        "assets/shaders/game/scene.frag"
    );
    std::unique_ptr<Shader> shadowShader = std::make_unique<Shader>(
        "assets/shaders/engine/shadow_depth.vert",
        "assets/shaders/engine/shadow_depth.frag"
    );
    Renderer renderer(sceneShader.get());
    CompositePass compositePass;
    StylizePass stylizePass;

    Framebuffer sceneFbo;
    Framebuffer compositeFbo;
    Framebuffer finalFbo;
    sceneFbo.create(1280, 720);
    compositeFbo.create(1280, 720);
    finalFbo.create(1280, 720);

    std::array<ShadowMap, kMaxShadowedSpotLightsLocal> shadowMaps;

    EditorCamera editCamera;
    EditorCamera playCamera;
    if (previewWorld.sceneBounds().valid) {
        focusEditorCameraOnBounds(editCamera, previewWorld.sceneBounds().min, previewWorld.sceneBounds().max);
    }
    EditorUiState ui;
    ui.pendingScenePath = initialScene;
    EditorPlacementState placementState;
    std::vector<std::uint64_t> selectedIds;
    EditorSelectionPickerState selectionPicker;

    auto scenePaths = sortedScenePaths();
    auto meshIds = sortedMeshNames(previewWorld.meshLibrary());
    auto materialIds = sortedMaterialIds(content);
    auto archetypeIds = sortedArchetypeIds(content);
    auto environmentIds = sortedEnvironmentIds(content);
    auto layoutPresetNames = listEditorLayoutPresetNames();

    if (!materialIds.empty()) {
        ui.selectedMaterialId = materialIds.front();
    }
    if (!meshIds.empty()) {
        ui.selectedMeshId = meshIds.front();
    }
    if (!archetypeIds.empty()) {
        ui.selectedArchetypeId = archetypeIds.front();
    }
    if (std::find(layoutPresetNames.begin(), layoutPresetNames.end(), ui.activeLayoutPreset) != layoutPresetNames.end()) {
        try {
            loadLayoutPresetIntoUi(ui, ui.activeLayoutPreset);
        } catch (const std::exception& ex) {
            spdlog::warn("Failed to load editor layout '{}': {}", ui.activeLayoutPreset, ex.what());
        }
    }

    bool previewDirty = true;
    bool savePressed = false;
    bool focusPressed = false;
    bool duplicatePressed = false;
    bool deletePressed = false;
    bool undoPressed = false;
    bool redoPressed = false;
    bool playTogglePressed = false;
    std::uint64_t previewSceneRevision = document.sceneRevision();
    EditorPendingCommand widgetCommand;
    EditorPendingCommand gizmoCommand;

    while (!window.shouldClose()) {
        window.pollEvents();
        imgui.beginFrame();
        ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
        ImGuizmo::BeginFrame();

        const float deltaTime = 1.0f / std::max(ImGui::GetIO().Framerate, 1.0f);
        ImGuiIO& io = ImGui::GetIO();

        if (ImGui::IsKeyPressed(ImGuiKey_F)) focusPressed = true;
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deletePressed = true;
        if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_D)) duplicatePressed = true;
        if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Z)) {
            if (io.KeyShift) {
                redoPressed = true;
            } else {
                undoPressed = true;
            }
        }
        if ((io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Y)) {
            redoPressed = true;
        }
        if (!io.WantTextInput && glfwGetMouseButton(window.handle(), GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) ui.tool = EditorTransformTool::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) ui.tool = EditorTransformTool::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) ui.tool = EditorTransformTool::Scale;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            placementState.clear();
            widgetCommand.clear();
            gizmoCommand.clear();
        }

        ImGuiViewport* mainViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainViewport->WorkPos);
        ImGui::SetNextWindowSize(mainViewport->WorkSize);
        ImGui::Begin("Level Editor Root", nullptr,
                     ImGuiWindowFlags_NoDecoration
                     | ImGuiWindowFlags_NoMove
                     | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_NoBringToFrontOnFocus
                     | ImGuiWindowFlags_NoNavFocus);

        if (ImGui::Button("Save")) {
            savePressed = true;
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!commandStack.canUndo());
        if (ImGui::Button("Undo")) {
            undoPressed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!commandStack.canRedo());
        if (ImGui::Button("Redo")) {
            redoPressed = true;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(ui.playPreview ? "Stop Preview" : "Play Preview")) {
            playTogglePressed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Focus")) {
            focusPressed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            duplicatePressed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            deletePressed = true;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(document.dirty() ? "* Unsaved" : "Saved");
        ImGui::SameLine();
        if (ImGui::BeginCombo("Layout", ui.activeLayoutPreset.c_str())) {
            for (const auto& layoutName : layoutPresetNames) {
                const bool selected = (layoutName == ui.activeLayoutPreset);
                if (ImGui::Selectable(layoutName.c_str(), selected)) {
                    try {
                        loadLayoutPresetIntoUi(ui, layoutName);
                    } catch (const std::exception& ex) {
                        spdlog::error("Layout load failed: {}", ex.what());
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("Layout Name", ui.layoutNameBuffer, sizeof(ui.layoutNameBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Save Layout")) {
            try {
                const std::string layoutName(ui.layoutNameBuffer);
                if (saveLayoutPresetFromUi(ui, layoutName)) {
                    ui.activeLayoutPreset = std::filesystem::path(editorLayoutPresetPath(layoutName)).stem().string();
                    layoutPresetNames = listEditorLayoutPresetNames();
                }
            } catch (const std::exception& ex) {
                spdlog::error("Layout save failed: {}", ex.what());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload Layout")) {
            try {
                loadLayoutPresetIntoUi(ui, ui.activeLayoutPreset);
            } catch (const std::exception& ex) {
                spdlog::error("Layout reload failed: {}", ex.what());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Panels")) {
            ImGui::OpenPopup("PanelVisibility");
        }
        if (ImGui::BeginPopup("PanelVisibility")) {
            ImGui::Checkbox("Outliner", &ui.showOutliner);
            ImGui::Checkbox("Inspector", &ui.showInspector);
            ImGui::Checkbox("Asset Browser", &ui.showAssetBrowser);
            ImGui::Checkbox("Environment", &ui.showEnvironment);
            ImGui::Checkbox("Viewport", &ui.showViewport);
            ImGui::EndPopup();
        }

        if (ImGui::BeginCombo("Scene", ui.pendingScenePath.c_str())) {
            for (const auto& scenePath : scenePaths) {
                const bool selected = (scenePath == ui.pendingScenePath);
                if (ImGui::Selectable(scenePath.c_str(), selected)) {
                    ui.pendingScenePath = scenePath;
                    document.loadFromSceneFile(scenePath, content);
                    commandStack.reset(document);
                    selectedIds.clear();
                    selectionPicker.clear();
                    placementState.clear();
                    widgetCommand.clear();
                    gizmoCommand.clear();
                    previewDirty = true;
                    playCamera = editCamera;
                    previewWorld.rebuild(document, content);
                    previewDirty = false;
                    previewSceneRevision = document.sceneRevision();
                    if (previewWorld.sceneBounds().valid) {
                        focusEditorCameraOnBounds(editCamera, previewWorld.sceneBounds().min, previewWorld.sceneBounds().max);
                    }
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SameLine();
        const char* toolLabel = ui.tool == EditorTransformTool::Translate ? "Translate"
            : ui.tool == EditorTransformTool::Rotate ? "Rotate"
            : "Scale";
        ImGui::Text("Tool: %s", toolLabel);
        ImGui::SameLine();
        if (ImGui::Button("W")) ui.tool = EditorTransformTool::Translate;
        ImGui::SameLine();
        if (ImGui::Button("E")) ui.tool = EditorTransformTool::Rotate;
        ImGui::SameLine();
        if (ImGui::Button("R")) ui.tool = EditorTransformTool::Scale;
        ImGui::SameLine();
        const char* previewModes[] = {"Final", "Lighting", "Sky"};
        int previewModeIndex = static_cast<int>(ui.previewMode);
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##previewmode", &previewModeIndex, previewModes, 3)) {
            ui.previewMode = static_cast<EditorPreviewMode>(previewModeIndex);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &ui.snappingEnabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::DragFloat("Move", &ui.moveSnap, 0.05f, 0.05f, 10.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::DragFloat("Rot", &ui.rotateSnap, 1.0f, 1.0f, 90.0f, "%.1f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        ImGui::DragFloat("Scale", &ui.scaleSnap, 0.01f, 0.01f, 2.0f, "%.2f");
        ImGui::SameLine();
        ImGui::Checkbox("Colliders", &ui.showColliders);
        ImGui::SameLine();
        ImGui::Checkbox("Light Helpers", &ui.showLightHelpers);
        ImGui::SameLine();
        ImGui::Checkbox("Spawn Marker", &ui.showSpawnMarker);
        ImGui::SameLine();
        if (ImGui::Button("Mesh")) beginPlacement(placementState, EditorPlacementKind::Mesh, ui.selectedMeshId, ui.selectedMaterialId);
        ImGui::SameLine();
        if (ImGui::Button("Point")) beginPlacement(placementState, EditorPlacementKind::PointLight);
        ImGui::SameLine();
        if (ImGui::Button("Spot")) beginPlacement(placementState, EditorPlacementKind::SpotLight);
        ImGui::SameLine();
        if (ImGui::Button("Dir")) beginPlacement(placementState, EditorPlacementKind::DirectionalLight);
        ImGui::SameLine();
        if (ImGui::Button("Box")) beginPlacement(placementState, EditorPlacementKind::BoxCollider);
        ImGui::SameLine();
        if (ImGui::Button("Cyl")) beginPlacement(placementState, EditorPlacementKind::CylinderCollider);
        ImGui::SameLine();
        if (ImGui::Button("Spawn")) beginPlacement(placementState, EditorPlacementKind::PlayerSpawn);
        ImGui::SameLine();
        if (ImGui::Button("Archetype")) beginPlacement(placementState, EditorPlacementKind::Archetype, ui.selectedArchetypeId);

        ImGui::Separator();
        ImGui::End();

        const std::vector<std::uint64_t> outlinerDeleteRequests = renderOutliner(document, selectedIds, &ui.showOutliner, commandStack);
        renderInspector(document, selectedIds, content, meshIds, materialIds, archetypeIds, &ui.showInspector, widgetCommand, commandStack);
        if (renderEnvironmentPanel(document, content, environmentIds, &ui.showEnvironment, widgetCommand, commandStack)) {
            previewDirty = true;
        }
        renderAssetBrowser(ui,
                           placementState,
                           document,
                           selectedIds,
                           content,
                           meshIds,
                           materialIds,
                           archetypeIds,
                           &ui.showAssetBrowser,
                           commandStack);

        EditorViewportState viewportState;
        bool viewportWindowBegun = false;
        bool viewportWindowVisible = false;
        if (ui.showViewport) {
            viewportWindowBegun = true;
            viewportWindowVisible = ImGui::Begin("Viewport",
                                                 &ui.showViewport,
                                                 ImGuiWindowFlags_NoScrollbar
                                                 | ImGuiWindowFlags_NoScrollWithMouse);
            if (viewportWindowVisible) {
                viewportState.focused = ImGui::IsWindowFocused();
                const ImVec2 avail = ImGui::GetContentRegionAvail();
                viewportState.size = ImVec2(std::max(avail.x, 64.0f), std::max(avail.y, 64.0f));
                viewportState.origin = ImGui::GetCursorScreenPos();
                viewportState.hovered = pointInViewport(viewportState, io.MousePos);
            }
        }

        const int targetW = std::max(1, static_cast<int>(std::max(viewportState.size.x, 64.0f)));
        const int targetH = std::max(1, static_cast<int>(std::max(viewportState.size.y, 64.0f)));
        if (sceneFbo.width() != targetW || sceneFbo.height() != targetH) {
            sceneFbo.resize(targetW, targetH);
            compositeFbo.resize(targetW, targetH);
            finalFbo.resize(targetW, targetH);
        }

        if (previewDirty || previewSceneRevision != document.sceneRevision()) {
            previewWorld.rebuild(document, content);
            previewDirty = false;
            previewSceneRevision = document.sceneRevision();
        }

        EditorCamera& activeCamera = ui.playPreview ? playCamera : editCamera;
        updateEditorFlyCamera(activeCamera, window.handle(), viewportState, deltaTime);

        const glm::mat4 view = editorCameraView(activeCamera);
        const glm::mat4 projection = editorCameraProjection(activeCamera, static_cast<float>(targetW) / static_cast<float>(targetH));
        const glm::mat4 inverseViewProjection = glm::inverse(projection * view);
        const auto selectionHandles = buildEditorSelectionHandles(document, previewWorld);
        const auto viewportSelectionHandles = buildViewportSelectionHandles(selectionHandles, ui);

        std::vector<RenderObject> objects = collectRenderObjects(previewWorld, materialTextures, selectedIds);
        if (!ui.playPreview) {
            appendHelperObjects(objects, previewWorld, document, materialTextures, selectedIds,
                                ui.showColliders, ui.showLightHelpers, ui.showSpawnMarker);
            appendSelectionOverlays(objects, previewWorld, materialTextures, selectedIds);
        }

        EditorPlacementState dragPlacement;
        if (!ui.playPreview && viewportState.hovered) {
            if (const ImGuiPayload* payload = ImGui::GetDragDropPayload();
                payload != nullptr && payload->IsDataType("EDITOR_PLACE") && payload->DataSize == sizeof(EditorDragPayload)) {
                dragPlacement = makePlacementState(*static_cast<const EditorDragPayload*>(payload->Data));
            }
        }

        const EditorPlacementState effectivePlacement = dragPlacement.active() ? dragPlacement : placementState;
        std::optional<glm::vec3> placementPoint;
        if (!ui.playPreview && effectivePlacement.active() && pointInViewport(viewportState, io.MousePos)) {
            const EditorRay ray = buildEditorRay(inverseViewProjection,
                                                 glm::vec2(viewportState.origin.x, viewportState.origin.y),
                                                 glm::vec2(viewportState.size.x, viewportState.size.y),
                                                 glm::vec2(io.MousePos.x, io.MousePos.y));
            placementPoint = computePlacementPoint(selectionHandles, ray, activeCamera, ui.snappingEnabled, ui.moveSnap);
            if (placementPoint.has_value()) {
                appendPlacementGhost(objects, effectivePlacement, *placementPoint, previewWorld, materialTextures, content);
            }
        }

        EnvironmentDefinition previewEnvironment = document.environment();
        previewEnvironment.post.timeSeconds = static_cast<float>(glfwGetTime());
        previewEnvironment.post.nearPlane = 0.1f;
        previewEnvironment.post.farPlane = 150.0f;
        previewEnvironment.post.inverseViewProjection = inverseViewProjection;
        previewEnvironment.post.sky.sunDirection = safeNormalize(previewEnvironment.lighting.sun.direction, previewEnvironment.post.sky.sunDirection);
        previewEnvironment.post.sky.sunColor = previewEnvironment.lighting.sun.color;
        switch (ui.previewMode) {
        case EditorPreviewMode::Final:
            previewEnvironment.post.debugViewMode = 0;
            break;
        case EditorPreviewMode::LightingOnly:
            previewEnvironment.post.debugViewMode = 1;
            break;
        case EditorPreviewMode::SkyOnly:
            previewEnvironment.post.debugViewMode = 4;
            break;
        }

        std::vector<RenderLight> lights = collectLights(previewWorld.registry(), previewEnvironment);
        ShadowRenderData shadowData;
        if (previewEnvironment.lighting.enableShadows) {
            renderShadowPass(objects, lights, *shadowShader, shadowMaps, kShadowResolutions[ui.shadowResolutionIndex], shadowData);
        } else {
            shadowData.shadowCount = 0;
            shadowData.matrices = {glm::mat4(1.0f), glm::mat4(1.0f)};
            shadowData.textures = {0, 0};
        }

        sceneFbo.bind();
        glViewport(0, 0, targetW, targetH);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.04f, 0.04f, 0.045f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderer.drawScene(objects,
                           lights,
                           makeLightingEnvironment(previewEnvironment),
                           shadowData,
                           view,
                           projection,
                           activeCamera.position);
        sceneFbo.unbind();
        glDisable(GL_DEPTH_TEST);

        compositePass.apply(sceneFbo.colorTexture(),
                            sceneFbo.depthTexture(),
                            sceneFbo.normalTexture(),
                            compositeFbo.framebuffer(),
                            previewEnvironment.post,
                            targetW,
                            targetH);
        stylizePass.apply(compositeFbo.colorTexture(),
                          sceneFbo.colorTexture(),
                          sceneFbo.depthTexture(),
                          sceneFbo.normalTexture(),
                          previewEnvironment.post,
                          targetW,
                          targetH,
                          finalFbo.framebuffer());

        if (viewportWindowVisible) {
            ImGui::SetNextItemAllowOverlap();
            ImGui::Image(static_cast<ImTextureID>(static_cast<uintptr_t>(finalFbo.colorTexture())),
                         viewportState.size,
                         ImVec2(0.0f, 1.0f),
                         ImVec2(1.0f, 0.0f));

            if (!selectedIds.empty()) {
                const std::uint64_t activeId = selectedIds.back();
                std::string label = selectedIds.size() == 1
                    ? "Selected: "
                    : (std::to_string(selectedIds.size()) + " selected | Active: ");
                if (const EditorSceneObject* selectedObject = document.findObject(activeId)) {
                    label += editorSceneObjectLabel(*selectedObject);
                } else {
                    label += "Object #" + std::to_string(activeId);
                }

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 textPos(viewportState.origin.x + 12.0f, viewportState.origin.y + 12.0f);
                const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
                const ImVec2 padding(8.0f, 6.0f);
                drawList->AddRectFilled(ImVec2(textPos.x - padding.x, textPos.y - padding.y),
                                        ImVec2(textPos.x + textSize.x + padding.x, textPos.y + textSize.y + padding.y),
                                        IM_COL32(18, 20, 24, 215),
                                        4.0f);
                drawList->AddText(textPos, IM_COL32(255, 236, 168, 255), label.c_str());
            }

            const EditorSceneDocumentState gizmoBeforeState = document.captureState();
            if (!ui.playPreview) {
                if (applyGizmoToSelectedObject(document, selectedIds, viewportState, view, projection, ui, previewWorld)) {
                    previewDirty = true;
                }
                if (editorGizmoIsHot() && !gizmoCommand.active) {
                    beginPendingCommand(gizmoCommand, gizmoBeforeState, "Transform Object");
                } else if (gizmoCommand.active && !editorGizmoIsHot()) {
                    finalizePendingCommand(gizmoCommand, commandStack, document);
                }
            }

            if (!ui.playPreview) {
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_PLACE", ImGuiDragDropFlags_AcceptBeforeDelivery)) {
                        if (payload->Delivery && placementPoint.has_value() && payload->DataSize == sizeof(EditorDragPayload)) {
                            const EditorPlacementState droppedState = makePlacementState(*static_cast<const EditorDragPayload*>(payload->Data));
                            const EditorSceneDocumentState beforeState = document.captureState();
                            commitPlacement(document, droppedState, *placementPoint, content, activeCamera);
                            commandStack.pushDocumentStateCommand(
                                "Place Object",
                                beforeState,
                                document.captureState(),
                                document);
                            selectionPicker.clear();
                            previewDirty = true;
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (placementState.active() && placementPoint.has_value() && viewportState.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !editorGizmoIsHot()) {
                    const EditorSceneDocumentState beforeState = document.captureState();
                    commitPlacement(document, placementState, *placementPoint, content, activeCamera);
                    commandStack.pushDocumentStateCommand(
                        "Place Object",
                        beforeState,
                        document.captureState(),
                        document);
                    placementState.clear();
                    selectionPicker.clear();
                    previewDirty = true;
                } else if (viewportState.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !editorGizmoIsHot()) {
                    const EditorRay ray = buildEditorRay(inverseViewProjection,
                                                         glm::vec2(viewportState.origin.x, viewportState.origin.y),
                                                         glm::vec2(viewportState.size.x, viewportState.size.y),
                                                         glm::vec2(io.MousePos.x, io.MousePos.y));
                    const std::vector<EditorHitResult> hits = pickEditorObjects(viewportSelectionHandles, ray);
                    if (!hits.empty()) {
                        refreshSelectionPicker(selectionPicker,
                                               hits,
                                               io.MousePos,
                                               glfwGetTime(),
                                               hits.size() > 1);
                        applySelectionHit(selectedIds, selectionPicker, io.KeyShift);
                    } else {
                        selectionPicker.clear();
                        if (!io.KeyShift) {
                            selectedIds.clear();
                        }
                    }
                }
            }

            if (!ui.playPreview) {
                renderSelectionPicker(selectionPicker, document, selectedIds, glfwGetTime());
            }
        }

        if (viewportWindowBegun) {
            ImGui::End();
        }

        if (playTogglePressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            ui.playPreview = !ui.playPreview;
            if (ui.playPreview) {
                playCamera = editCamera;
                for (const auto& object : document.objects()) {
                    if (object.kind == EditorSceneObjectKind::PlayerSpawn) {
                        playCamera.position = std::get<LevelPlayerSpawn>(object.payload).position;
                        break;
                    }
                }
            }
            playTogglePressed = false;
        }

        if (focusPressed && selectedIds.size() == 1) {
            if (const EditorObjectBounds* bounds = previewWorld.findObjectBounds(selectedIds.front())) {
                focusEditorCameraOnBounds(editCamera, bounds->min, bounds->max);
            } else if (const EditorSceneObject* object = document.findObject(selectedIds.front())) {
                focusEditorCameraOnPoint(editCamera, editorSceneObjectAnchor(*object));
            }
            focusPressed = false;
        } else {
            focusPressed = false;
        }

        if (undoPressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            if (commandStack.undo(document)) {
                pruneSelection(document, selectedIds);
                selectionPicker.clear();
                previewDirty = true;
            }
            undoPressed = false;
        }

        if (redoPressed) {
            widgetCommand.clear();
            gizmoCommand.clear();
            if (commandStack.redo(document)) {
                pruneSelection(document, selectedIds);
                selectionPicker.clear();
                previewDirty = true;
            }
            redoPressed = false;
        }

        if (duplicatePressed) {
            const EditorSceneDocumentState beforeState = document.captureState();
            std::vector<std::uint64_t> duplicated;
            for (auto id : selectedIds) {
                const std::uint64_t newId = document.duplicateObject(id);
                if (newId != 0) {
                    duplicated.push_back(newId);
                }
            }
            if (!duplicated.empty()) {
                selectedIds = duplicated;
                selectionPicker.clear();
                commandStack.pushDocumentStateCommand(
                    "Duplicate Selection",
                    beforeState,
                    document.captureState(),
                    document);
                previewDirty = true;
            }
            duplicatePressed = false;
        }

        if (deletePressed) {
            if (!selectedIds.empty()) {
                const EditorSceneDocumentState beforeState = document.captureState();
                document.eraseObjects(selectedIds);
                commandStack.pushDocumentStateCommand(
                    "Delete Selection",
                    beforeState,
                    document.captureState(),
                    document);
                previewDirty = true;
            }
            selectedIds.clear();
            selectionPicker.clear();
            deletePressed = false;
        }

        if (!outlinerDeleteRequests.empty()) {
            const EditorSceneDocumentState beforeState = document.captureState();
            document.eraseObjects(outlinerDeleteRequests);
            commandStack.pushDocumentStateCommand(
                outlinerDeleteRequests.size() == 1 ? "Delete Object" : "Delete Objects",
                beforeState,
                document.captureState(),
                document);
            pruneSelection(document, selectedIds);
            selectionPicker.clear();
            previewDirty = true;
        }

        if (savePressed) {
            try {
                document.save(content);
                content.loadDefaults();
                commandStack.markSaved(document);
                scenePaths = sortedScenePaths();
                materialIds = sortedMaterialIds(content);
                archetypeIds = sortedArchetypeIds(content);
                environmentIds = sortedEnvironmentIds(content);
                previewDirty = true;
            } catch (const std::exception& ex) {
                spdlog::error("Save failed: {}", ex.what());
            }
            savePressed = false;
        }

        imgui.endFrame();
        window.swapBuffers();
    }

    imgui.shutdown();
    return 0;
}
