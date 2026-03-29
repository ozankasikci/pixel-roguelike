#include "editor/viewport/EditorViewportController.h"

#include <GLFW/glfw3.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace {

float safeDelta(float value) {
    return std::max(value, 0.0001f);
}

constexpr float kScrollZoomSpeed = 2.5f;
constexpr float kFastMoveMultiplier = 3.0f;
constexpr float kOrbitSensitivity = 0.20f;
constexpr float kPanSensitivity = 0.0025f;
constexpr float kDollySensitivity = 0.02f;
constexpr float kMinOrbitDistance = 0.5f;
constexpr float kDefaultOrbitDistance = 6.0f;

bool altPressed(GLFWwindow* window, const ImGuiIO& io) {
    return io.KeyAlt
        || glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS
        || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
}

bool shiftPressed(GLFWwindow* window) {
    return glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
        || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
}

void ensureOrbitPivot(EditorCamera& camera) {
    if (!camera.orbitPivotValid) {
        const float distance = std::max(camera.orbitDistance, kDefaultOrbitDistance);
        camera.orbitPivot = camera.position + editorCameraForward(camera) * distance;
        camera.orbitDistance = distance;
        camera.orbitPivotValid = true;
        return;
    }

    camera.orbitDistance = std::max(glm::length(camera.orbitPivot - camera.position), kMinOrbitDistance);
}

void orbitCamera(EditorCamera& camera, const ImVec2& mouseDelta) {
    ensureOrbitPivot(camera);
    camera.yawDegrees += mouseDelta.x * kOrbitSensitivity;
    camera.pitchDegrees = glm::clamp(camera.pitchDegrees - mouseDelta.y * kOrbitSensitivity, -89.0f, 89.0f);
    camera.position = camera.orbitPivot - editorCameraForward(camera) * camera.orbitDistance;
}

void panCamera(EditorCamera& camera, const ImVec2& mouseDelta, bool fastPan) {
    ensureOrbitPivot(camera);

    float panScale = std::max(camera.orbitDistance, 1.0f) * kPanSensitivity;
    if (fastPan) {
        panScale *= kFastMoveMultiplier;
    }

    const glm::vec3 delta = (-editorCameraRight(camera) * mouseDelta.x + editorCameraUp(camera) * mouseDelta.y) * panScale;
    camera.position += delta;
    camera.orbitPivot += delta;
}

void dollyCamera(EditorCamera& camera, const ImVec2& mouseDelta) {
    ensureOrbitPivot(camera);
    camera.orbitDistance = std::max(camera.orbitDistance + mouseDelta.y * std::max(camera.orbitDistance, 1.0f) * kDollySensitivity,
                                    kMinOrbitDistance);
    camera.position = camera.orbitPivot - editorCameraForward(camera) * camera.orbitDistance;
}

} // namespace

glm::vec3 editorCameraForward(const EditorCamera& camera) {
    const float yaw = glm::radians(camera.yawDegrees);
    const float pitch = glm::radians(camera.pitchDegrees);
    return glm::normalize(glm::vec3(
        std::cos(pitch) * std::cos(yaw),
        std::sin(pitch),
        std::cos(pitch) * std::sin(yaw)
    ));
}

glm::vec3 editorCameraRight(const EditorCamera& camera) {
    return glm::normalize(glm::cross(editorCameraForward(camera), glm::vec3(0.0f, 1.0f, 0.0f)));
}

glm::vec3 editorCameraUp(const EditorCamera& camera) {
    return glm::normalize(glm::cross(editorCameraRight(camera), editorCameraForward(camera)));
}

glm::mat4 editorCameraView(const EditorCamera& camera) {
    const glm::vec3 forward = editorCameraForward(camera);
    return glm::lookAt(camera.position, camera.position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 editorCameraProjection(const EditorCamera& camera, float aspect) {
    return glm::perspective(glm::radians(camera.fovDegrees), aspect, camera.nearPlane, camera.farPlane);
}

void updateEditorFlyCamera(EditorCamera& camera,
                           GLFWwindow* window,
                           const EditorViewportState& viewport,
                           float deltaTime) {
    ImGuiIO& io = ImGui::GetIO();
    if (!viewport.hovered) {
        return;
    }

    if (std::abs(io.MouseWheel) > 0.0001f) {
        const float zoomDistance = io.MouseWheel * camera.moveSpeed * kScrollZoomSpeed * safeDelta(deltaTime);
        if (camera.orbitPivotValid) {
            camera.orbitDistance = std::max(camera.orbitDistance - zoomDistance, kMinOrbitDistance);
            camera.position = camera.orbitPivot - editorCameraForward(camera) * camera.orbitDistance;
        } else {
            camera.position += editorCameraForward(camera) * zoomDistance;
        }
    }

    const bool altDown = altPressed(window, io);
    const bool middleMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    const bool rightMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (altDown && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        orbitCamera(camera, io.MouseDelta);
        return;
    }
    if (middleMouseDown) {
        panCamera(camera, io.MouseDelta, shiftPressed(window));
        return;
    }
    if (altDown && rightMouseDown) {
        dollyCamera(camera, io.MouseDelta);
        return;
    }

    if (!rightMouseDown) {
        return;
    }

    float moveSpeed = camera.moveSpeed;
    if (shiftPressed(window)) {
        moveSpeed *= kFastMoveMultiplier;
    }

    camera.orbitPivotValid = false;
    camera.yawDegrees += io.MouseDelta.x * kOrbitSensitivity;
    camera.pitchDegrees = glm::clamp(camera.pitchDegrees - io.MouseDelta.y * kOrbitSensitivity, -89.0f, 89.0f);

    const glm::vec3 forward = editorCameraForward(camera);
    const glm::vec3 right = editorCameraRight(camera);
    const glm::vec3 up(0.0f, 1.0f, 0.0f);
    glm::vec3 velocity(0.0f);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) velocity += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) velocity -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) velocity -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) velocity += right;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) velocity += up;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) velocity -= up;
    if (glm::dot(velocity, velocity) > 0.0f) {
        camera.position += glm::normalize(velocity) * moveSpeed * safeDelta(deltaTime);
    }
}

void focusEditorCameraOnPoint(EditorCamera& camera, const glm::vec3& point) {
    const glm::vec3 toTarget = point - camera.position;
    if (glm::dot(toTarget, toTarget) <= 0.0001f) {
        camera.orbitPivot = point;
        camera.orbitDistance = kMinOrbitDistance;
        camera.orbitPivotValid = true;
        return;
    }
    const glm::vec3 dir = glm::normalize(toTarget);
    camera.pitchDegrees = glm::degrees(std::asin(glm::clamp(dir.y, -1.0f, 1.0f)));
    camera.yawDegrees = glm::degrees(std::atan2(dir.z, dir.x));
    camera.orbitPivot = point;
    camera.orbitDistance = std::max(glm::length(toTarget), kMinOrbitDistance);
    camera.orbitPivotValid = true;
}

void focusEditorCameraOnBounds(EditorCamera& camera,
                               const glm::vec3& boundsMin,
                               const glm::vec3& boundsMax) {
    const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    const glm::vec3 extents = glm::max((boundsMax - boundsMin) * 0.5f, glm::vec3(0.25f));
    const float radius = glm::length(extents);
    const glm::vec3 forward = editorCameraForward(camera);
    camera.position = center - forward * std::max(radius * 2.4f, 3.0f);
    focusEditorCameraOnPoint(camera, center);
}

bool manipulateEditorGizmo(const EditorViewportState& viewport,
                           const glm::mat4& view,
                           const glm::mat4& projection,
                           EditorTransformTool tool,
                           bool snappingEnabled,
                           float moveSnap,
                           float rotateSnap,
                           float scaleSnap,
                           glm::mat4& modelMatrix) {
    if (viewport.size.x <= 1.0f || viewport.size.y <= 1.0f) {
        return false;
    }

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewport.origin.x, viewport.origin.y, viewport.size.x, viewport.size.y);

    ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
    float snap[3]{moveSnap, moveSnap, moveSnap};
    switch (tool) {
    case EditorTransformTool::Translate:
        operation = ImGuizmo::TRANSLATE;
        snap[0] = snap[1] = snap[2] = moveSnap;
        break;
    case EditorTransformTool::Rotate:
        operation = ImGuizmo::ROTATE;
        snap[0] = rotateSnap;
        snap[1] = rotateSnap;
        snap[2] = rotateSnap;
        break;
    case EditorTransformTool::Scale:
        operation = ImGuizmo::SCALE;
        snap[0] = snap[1] = snap[2] = scaleSnap;
        break;
    }

    ImGuizmo::Manipulate(&view[0][0],
                         &projection[0][0],
                         operation,
                         ImGuizmo::LOCAL,
                         &modelMatrix[0][0],
                         nullptr,
                         snappingEnabled ? snap : nullptr);
    return ImGuizmo::IsUsing();
}

bool editorGizmoIsHot() {
    return ImGuizmo::IsUsing() || ImGuizmo::IsOver();
}
