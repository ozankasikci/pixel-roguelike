#pragma once

#include "editor/scene/EditorSceneDocument.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>
#include <vector>

class EditorPreviewWorld;

struct EditorRay {
    glm::vec3 origin{0.0f};
    glm::vec3 direction{0.0f, 0.0f, -1.0f};
};

struct EditorHitResult {
    std::uint64_t objectId = 0;
    EditorSceneObjectKind objectKind = EditorSceneObjectKind::Mesh;
    float distance = 0.0f;
    glm::vec3 position{0.0f};
};

enum class EditorSelectionShape {
    WorldAabb,
    OrientedBox,
    Cylinder,
    Sphere,
};

struct EditorSelectionHandle {
    std::uint64_t objectId = 0;
    EditorSceneObjectKind objectKind = EditorSceneObjectKind::Mesh;
    EditorSelectionShape shape = EditorSelectionShape::Sphere;
    bool placementSurface = false;
    glm::vec3 anchor{0.0f};
    glm::vec3 worldMin{0.0f};
    glm::vec3 worldMax{0.0f};
    glm::mat4 localToWorld{1.0f};
    glm::vec3 halfExtents{0.5f};
    float radius = 0.5f;
    float halfHeight = 0.5f;
};

EditorRay buildEditorRay(const glm::mat4& inverseViewProjection,
                         const glm::vec2& viewportOrigin,
                         const glm::vec2& viewportSize,
                         const glm::vec2& screenPosition);

std::vector<EditorSelectionHandle> buildEditorSelectionHandles(const EditorSceneDocument& document,
                                                               const EditorPreviewWorld& previewWorld);

std::vector<EditorHitResult> pickEditorObjects(const std::vector<EditorSelectionHandle>& handles,
                                               const EditorRay& ray);

std::optional<EditorHitResult> pickEditorObject(const std::vector<EditorSelectionHandle>& handles,
                                                const EditorRay& ray);

std::optional<EditorHitResult> pickEditorPlacementSurface(const std::vector<EditorSelectionHandle>& handles,
                                                          const EditorRay& ray);
