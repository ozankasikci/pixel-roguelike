#pragma once

#include <glm/glm.hpp>
#include <imgui.h>

struct GLFWwindow;

enum class EditorTransformTool {
    Translate,
    Rotate,
    Scale,
};

struct EditorCamera {
    glm::vec3 position{0.0f, 1.8f, 8.0f};
    float yawDegrees = -90.0f;
    float pitchDegrees = -8.0f;
    float fovDegrees = 70.0f;
    float moveSpeed = 8.0f;
};

struct EditorViewportState {
    bool hovered = false;
    bool focused = false;
    ImVec2 origin{0.0f, 0.0f};
    ImVec2 size{0.0f, 0.0f};
};

glm::vec3 editorCameraForward(const EditorCamera& camera);
glm::vec3 editorCameraRight(const EditorCamera& camera);
glm::vec3 editorCameraUp(const EditorCamera& camera);
glm::mat4 editorCameraView(const EditorCamera& camera);
glm::mat4 editorCameraProjection(const EditorCamera& camera, float aspect);

void updateEditorFlyCamera(EditorCamera& camera,
                           GLFWwindow* window,
                           const EditorViewportState& viewport,
                           float deltaTime);

void focusEditorCameraOnPoint(EditorCamera& camera, const glm::vec3& point);
void focusEditorCameraOnBounds(EditorCamera& camera,
                               const glm::vec3& boundsMin,
                               const glm::vec3& boundsMax);

bool manipulateEditorGizmo(const EditorViewportState& viewport,
                           const glm::mat4& view,
                           const glm::mat4& projection,
                           EditorTransformTool tool,
                           bool snappingEnabled,
                           float moveSnap,
                           float rotateSnap,
                           float scaleSnap,
                           glm::mat4& modelMatrix);

bool editorGizmoIsHot();
