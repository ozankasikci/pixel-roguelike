#include "editor/ui/inspectors/MaterialInspector.h"

#include "editor/ui/LevelEditorUi.h"
#include "game/content/ContentRegistry.h"
#include "game/rendering/MaterialDefinition.h"
#include "engine/rendering/geometry/Renderer.h"

#include <glm/common.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <string>

namespace {

void ensureMaterialLibraryReady(AssetInspectorSession& session, const ContentRegistry& content) {
    if (!session.materialLibraryReady || session.materialLibraryDirty) {
        session.materialLibrary.init(content);
        session.materialLibraryReady = true;
        session.materialLibraryDirty = false;
    }
}

RenderMaterialData buildDraftMaterialPreview(const MaterialDefinition& draft,
                                             AssetInspectorSession& session,
                                             const ContentRegistry& content) {
    ensureMaterialLibraryReady(session, content);

    std::string previewMaterialId = draft.id;
    if (content.findMaterial(previewMaterialId) == nullptr) {
        if (draft.parent.has_value() && content.findMaterial(*draft.parent) != nullptr) {
            previewMaterialId = *draft.parent;
        } else {
            previewMaterialId = "stone_default";
        }
    }

    RenderMaterialData material = session.materialLibrary.resolve(previewMaterialId);
    material.id = draft.id;
    if (draft.baseColor.has_value()) {
        material.baseColor = *draft.baseColor;
    }
    if (draft.uvMode.has_value()) {
        material.uvMode = static_cast<int>(*draft.uvMode);
    }
    if (draft.uvScale.has_value()) {
        material.uvScale = *draft.uvScale;
    }
    if (draft.normalStrength.has_value()) {
        material.normalStrength = *draft.normalStrength;
    }
    if (draft.roughnessScale.has_value()) {
        material.roughnessScale = *draft.roughnessScale;
    }
    if (draft.roughnessBias.has_value()) {
        material.roughnessBias = *draft.roughnessBias;
    }
    if (draft.metalness.has_value()) {
        material.metalness = *draft.metalness;
    }
    if (draft.aoStrength.has_value()) {
        material.aoStrength = *draft.aoStrength;
    }
    if (draft.lightTintResponse.has_value()) {
        material.lightTintResponse = *draft.lightTintResponse;
    }
    return material;
}

void renderMaterialDraftFields(MaterialDefinition& draft, bool& dirty) {
    if (!beginInspectorPropertyTable("MaterialDraftFields")) {
        return;
    }

    dirty |= renderInspectorPropertyRow("Id", [&]() { return editString("##value", draft.id); });
    std::string parent = draft.parent.value_or("");
    if (renderInspectorPropertyRow("Parent", [&]() { return editString("##value", parent, "optional"); })) {
        draft.parent = parent.empty() ? std::optional<std::string>{} : std::optional<std::string>{parent};
        dirty = true;
    }

    auto editOptionalPath = [&](const char* label, std::optional<std::string>& value, const char* hint) {
        std::string working = value.value_or("");
        if (renderInspectorPropertyRow(label, [&]() { return editString("##value", working, hint); })) {
            value = working.empty() ? std::optional<std::string>{} : std::optional<std::string>{working};
            dirty = true;
        }
    };

    editOptionalPath("Albedo Map", draft.albedoMapPath, "assets/materials/.../albedo.png");
    editOptionalPath("Normal Map", draft.normalMapPath, "assets/materials/.../normal.png");
    editOptionalPath("Roughness Map", draft.roughnessMapPath, "assets/materials/.../roughness.png");
    editOptionalPath("AO Map", draft.aoMapPath, "assets/materials/.../ao.png");

    glm::vec3 baseColor = draft.baseColor.value_or(glm::vec3(1.0f));
    bool useBaseColor = draft.baseColor.has_value();
    if (renderInspectorPropertyRow("Use Base Color", [&]() { return ImGui::Checkbox("##value", &useBaseColor); })) {
        draft.baseColor = useBaseColor ? std::optional<glm::vec3>{baseColor} : std::nullopt;
        dirty = true;
    }
    if (useBaseColor && renderInspectorPropertyRow("Base Color", [&]() { return editColor("##value", baseColor); })) {
        draft.baseColor = baseColor;
        dirty = true;
    }

    static constexpr const char* kUvModes[] = {"mesh", "world_projected"};
    int uvMode = draft.uvMode.has_value() && *draft.uvMode == MaterialUvMode::WorldProjected ? 1 : 0;
    if (renderInspectorPropertyRow("UV Mode", [&]() { return ImGui::Combo("##value", &uvMode, kUvModes, 2); },
                                  EditorInspectorFieldKind::Enum)) {
        draft.uvMode = uvMode == 0 ? MaterialUvMode::Mesh : MaterialUvMode::WorldProjected;
        dirty = true;
    }

    glm::vec2 uvScale = draft.uvScale.value_or(glm::vec2(1.0f));
    if (renderInspectorPropertyRow("UV Scale", [&]() { return ImGui::DragFloat2("##value", &uvScale.x, 0.01f, 0.01f, 16.0f, "%.2f"); })) {
        draft.uvScale = glm::max(uvScale, glm::vec2(0.01f));
        dirty = true;
    }

    auto dragOptionalFloat = [&](const char* label, std::optional<float>& value, float defaultValue, float speed, float min, float max, const char* format) {
        float working = value.value_or(defaultValue);
        if (renderInspectorPropertyRow(label, [&]() { return ImGui::DragFloat("##value", &working, speed, min, max, format); })) {
            value = working;
            dirty = true;
        }
    };

    dragOptionalFloat("Normal Strength", draft.normalStrength, 1.0f, 0.01f, 0.0f, 4.0f, "%.2f");
    dragOptionalFloat("Roughness Scale", draft.roughnessScale, 1.0f, 0.01f, 0.0f, 4.0f, "%.2f");
    dragOptionalFloat("Roughness Bias", draft.roughnessBias, 0.82f, 0.01f, 0.0f, 1.0f, "%.2f");
    dragOptionalFloat("Metalness", draft.metalness, 0.0f, 0.01f, 0.0f, 1.0f, "%.2f");
    dragOptionalFloat("Specular Level", draft.specularLevel, 0.20f, 0.01f, 0.0f, 2.0f, "%.2f");
    dragOptionalFloat("AO Strength", draft.aoStrength, 1.0f, 0.01f, 0.0f, 2.0f, "%.2f");
    dragOptionalFloat("Light Tint Response", draft.lightTintResponse, 0.18f, 0.01f, 0.0f, 1.0f, "%.2f");
    dragOptionalFloat("Emissive Strength", draft.emissiveStrength, 0.0f, 0.05f, 0.0f, 10.0f, "%.2f");

    static constexpr const char* kProceduralSources[] = {
        "none", "generated_brick", "generated_stone", "generated_smooth", "generated_floor", "generated_ceiling"
    };
    int proceduralIndex = 0;
    if (draft.proceduralSource.has_value()) {
        switch (*draft.proceduralSource) {
        case MaterialProceduralSource::None: proceduralIndex = 0; break;
        case MaterialProceduralSource::GeneratedBrick: proceduralIndex = 1; break;
        case MaterialProceduralSource::GeneratedStone: proceduralIndex = 2; break;
        case MaterialProceduralSource::GeneratedSmooth: proceduralIndex = 3; break;
        case MaterialProceduralSource::GeneratedFloor: proceduralIndex = 4; break;
        case MaterialProceduralSource::GeneratedCeiling: proceduralIndex = 5; break;
        }
    }
    if (renderInspectorPropertyRow("Procedural Source", [&]() { return ImGui::Combo("##value", &proceduralIndex, kProceduralSources, 6); },
                                  EditorInspectorFieldKind::Enum)) {
        draft.proceduralSource = static_cast<MaterialProceduralSource>(proceduralIndex);
        dirty = true;
    }

    endInspectorPropertyTable();

    // Feature Flags
    if (ImGui::CollapsingHeader("Feature Flags", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!beginInspectorPropertyTable("MaterialFeatureFlags")) {
            return;
        }
        auto editOptionalBool = [&](const char* label, std::optional<bool>& value) {
            bool working = value.value_or(false);
            if (renderInspectorPropertyRow(label, [&]() { return ImGui::Checkbox("##value", &working); })) {
                value = working;
                dirty = true;
            }
        };
        editOptionalBool("Animated (wax flame flicker)", draft.animated);
        editOptionalBool("Subsurface (moss SSS)", draft.subsurface);
        editOptionalBool("Detail: Brick", draft.detailBrick);
        editOptionalBool("Detail: Wood", draft.detailWood);
        editOptionalBool("Detail: Stone", draft.detailStone);
        editOptionalBool("Detail: Floor", draft.detailFloor);
        endInspectorPropertyTable();
    }
}

} // namespace

void drawMaterialAssetInspector(EditorUiState& ui,
                                const EditorInspectedAsset& asset,
                                AssetInspectorSession& session,
                                ContentRegistry& content,
                                InspectorActionResult& result) {
    if (!session.materialDraft.has_value()) {
        ImGui::TextUnformatted("Failed to load material asset.");
        return;
    }

    MaterialDefinition& material = *session.materialDraft;
    bool dirty = false;

    // Validation error state (persists per frame)
    static std::string validationError;

    if (ImGui::Button("Save Material")) {
        std::string errorOut;
        if (!content.validateMaterialDefinition(material, errorOut)) {
            validationError = errorOut;
        } else {
            validationError.clear();
            try {
                saveMaterialDefinitionAsset(asset.absolutePath, material);
                content.addMaterial(material, asset.absolutePath);
                session.materialLibraryDirty = true;
                session.materialDirty = false;
                ui.inspectedAsset.declaredId = material.id;
                result.contentReloaded = true;
                result.previewDirty = true;
            } catch (const std::exception& ex) {
                spdlog::error("Material save failed: {}", ex.what());
                validationError = ex.what();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Material")) {
        try {
            material = loadMaterialDefinitionAsset(asset.absolutePath);
            session.materialLibraryDirty = true;
            session.materialDirty = false;
            validationError.clear();
        } catch (const std::exception& ex) {
            spdlog::error("Material reload failed: {}", ex.what());
        }
    }

    if (!validationError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Validation error: %s", validationError.c_str());
    }

    // Preview sphere at top of inspector (before property groups)
    RenderMaterialData previewMaterial = buildDraftMaterialPreview(material, session, content);
    session.previewRenderer.drawMaterialPreview(previewMaterial, glm::vec3(0.08f, 0.09f, 0.10f), "material_asset_preview");
    if (session.materialDirty) {
        ImGui::TextDisabled("Preview uses current draft values; save to persist.");
    }

    ImGui::Separator();
    renderMaterialDraftFields(material, dirty);
    if (dirty) {
        session.materialDirty = true;
    }
    if (session.materialDirty) {
        ImGui::TextDisabled("Unsaved changes");
    }
}
