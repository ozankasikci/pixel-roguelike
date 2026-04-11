#pragma once

#include "engine/rendering/geometry/Renderer.h"
#include "game/level/LevelDef.h"

#include "engine/ecs/GameRegistry.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

class Shader;
class ShadowMap;
class MaterialTextureLibrary;
class EditorPreviewWorld;
class EditorSceneDocument;
class ContentRegistry;

struct EnvironmentDefinition;

std::vector<RenderLight> collectLights(const GameRegistry& registry,
                                       const EnvironmentDefinition& environment);

struct MaterialDragPreview {
    std::uint64_t objectId = 0;
    std::string_view materialId;
};

std::vector<RenderObject> collectRenderObjects(const EditorPreviewWorld& world,
                                               const MaterialTextureLibrary& materials,
                                               const std::vector<std::uint64_t>& selectedIds,
                                               const MaterialDragPreview& dragPreview = {});

void appendHelperObjects(std::vector<RenderObject>& objects,
                         const EditorPreviewWorld& world,
                         const EditorSceneDocument& document,
                         const MaterialTextureLibrary& materials,
                         const std::vector<std::uint64_t>& selectedIds,
                         bool showColliders,
                         bool showLights,
                         bool showSpawn,
                         bool showTriggers = true);

void appendSelectionOverlays(std::vector<RenderObject>& objects,
                             const EditorPreviewWorld& world,
                             const MaterialTextureLibrary& materials,
                             const std::vector<std::uint64_t>& selectedIds);

void appendHoverOverlay(std::vector<RenderObject>& objects,
                        const EditorPreviewWorld& world,
                        const MaterialTextureLibrary& materials,
                        std::uint64_t hoveredId,
                        const std::vector<std::uint64_t>& selectedIds);

void renderShadowPass(const std::vector<RenderObject>& objects,
                      const std::vector<RenderLight>& lights,
                      const Shader& shadowShader,
                      std::array<ShadowMap, kMaxShadowedSpotLights>& shadowMaps,
                      int shadowResolution,
                      ShadowRenderData& shadowData);
