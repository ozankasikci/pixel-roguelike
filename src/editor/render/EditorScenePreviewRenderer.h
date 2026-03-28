#pragma once

#include "engine/rendering/geometry/Renderer.h"

#include <entt/entt.hpp>

#include <array>
#include <cstdint>
#include <vector>

class Shader;
class ShadowMap;
class MaterialTextureLibrary;
class EditorPreviewWorld;
class EditorSceneDocument;
class ContentRegistry;

struct EnvironmentDefinition;

std::vector<RenderLight> collectLights(const entt::registry& registry,
                                       const EnvironmentDefinition& environment);

std::vector<RenderObject> collectRenderObjects(const EditorPreviewWorld& world,
                                               const MaterialTextureLibrary& materials,
                                               const std::vector<std::uint64_t>& selectedIds);

void appendHelperObjects(std::vector<RenderObject>& objects,
                         const EditorPreviewWorld& world,
                         const EditorSceneDocument& document,
                         const MaterialTextureLibrary& materials,
                         const std::vector<std::uint64_t>& selectedIds,
                         bool showColliders,
                         bool showLights,
                         bool showSpawn);

void appendSelectionOverlays(std::vector<RenderObject>& objects,
                             const EditorPreviewWorld& world,
                             const MaterialTextureLibrary& materials,
                             const std::vector<std::uint64_t>& selectedIds);

void renderShadowPass(const std::vector<RenderObject>& objects,
                      const std::vector<RenderLight>& lights,
                      const Shader& shadowShader,
                      std::array<ShadowMap, 2>& shadowMaps,
                      int shadowResolution,
                      ShadowRenderData& shadowData);
