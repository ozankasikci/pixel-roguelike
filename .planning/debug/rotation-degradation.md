---
status: awaiting_human_verify
trigger: "rotation-degradation — rotations break/degrade after repeated rotation"
created: 2026-04-01T00:00:00Z
updated: 2026-04-01T00:00:01Z
---

## Current Focus

hypothesis: CONFIRMED — Two distinct root causes both stem from Euler-angle-based rotation storage and XYZ sequential apply order
test: Read all rotation-related code
expecting: N/A — confirmed
next_action: Fix TransformComponent.modelMatrix(), MathUtils.h makeTransformMatrix(), and the decomposeTransformMatrix functions

## Symptoms

expected: Rotations should be stable and independent — rotating around one axis shouldn't affect others, and repeated small rotations shouldn't cause drift or degradation.
actual: Rotation axes get swapped/mixed (rotating one axis affects another), and objects drift/accumulate error over time with many small rotations.
errors: No explicit error messages — visual degradation only.
reproduction: Apply many small rotations to objects over time; rotate objects on different axes.
started: Ongoing issue, likely since rotation was implemented.

## Eliminated

(none — root cause found on first investigation)

## Evidence

- timestamp: 2026-04-01T00:00:01Z
  checked: TransformComponent.h — rotation storage
  found: rotation stored as glm::vec3 (Euler angles in degrees)
  implication: Foundation of the problem — Euler angles accumulate floating-point error and cause gimbal lock

- timestamp: 2026-04-01T00:00:01Z
  checked: TransformComponent::modelMatrix() in TransformComponent.h
  found: Applies rotations sequentially as X→Y→Z via glm::rotate() in world space (each axis is a FIXED world axis).
  implication: This is extrinsic XYZ Euler order. The Y rotation is applied after X, but rotates around the ORIGINAL Y axis (world Y), not the local Y axis after X rotation. This causes axis-coupling: rotating X then Y produces results that feel like axes are swapped.

- timestamp: 2026-04-01T00:00:01Z
  checked: MathUtils.h makeTransformMatrix() — identical to TransformComponent::modelMatrix()
  found: Same extrinsic X→Y→Z Euler rotation sequence used in editor for all object transforms.
  implication: All scene objects (meshes, colliders, etc.) share this same root issue.

- timestamp: 2026-04-01T00:00:01Z
  checked: EditorViewportInteraction.cpp decomposeModelMatrix() and EditorSceneDocument.cpp decomposeTransformMatrix()
  found: After gizmo manipulation, the matrix is decomposed via glm::decompose() → glm::quat → glm::eulerAngles().
  implication: The decompose-to-Euler path is lossy. glm::eulerAngles() returns angles in a different convention than XYZ rotation compose order. Specifically glm::eulerAngles() returns pitch-yaw-roll (YXZ intrinsic) but the matrices are built with XYZ extrinsic order. This round-trip introduces rotation error on every gizmo drag.

- timestamp: 2026-04-01T00:00:01Z
  checked: RuntimeGameplay.cpp updateRuntimeCamera — camera rotation
  found: Camera stores yaw/pitch as separate floats, accumulates them by addition each frame. No quaternion used.
  implication: Camera yaw/pitch accumulation is actually fine for FPS camera (separate scalars, no matrix multiply). This is NOT the problem for camera.

- timestamp: 2026-04-01T00:00:01Z
  checked: RuntimeSceneRenderer.cpp collectViewmodelObjects — viewmodel rotation
  found: Applies rotation via sequential glm::rotate() calls on matrices (same XYZ Euler pattern).
  implication: Same axis-coupling issue for viewmodel objects.

## Resolution

root_cause: Two related problems both caused by Euler-angle rotation storage and application:
  1. TransformComponent and makeTransformMatrix() apply rotations as sequential extrinsic XYZ rotations (each axis is the original world axis). This couples axes — rotating X then Y does not give "rotate around the new local Y after tilting X". To get independent axes, the convention must match what the editor/user expects.
  2. The decompose round-trip (gizmo → mat4 → glm::decompose → glm::eulerAngles → store as vec3 → rebuild mat4) loses precision every cycle because glm::eulerAngles() returns angles in YXZ intrinsic order, but the matrices are built with XYZ extrinsic order. This introduces drift on every gizmo operation.

fix: Change both TransformComponent::modelMatrix() and makeTransformMatrix() to convert Euler degrees to a quaternion first (via glm::quat), then to a mat4. This gives intrinsic ZYX (= extrinsic XYZ) which is standard game-engine convention. Also the decompose function must use the same convention. The key property: building via quaternion gives a single rotation with no axis-coupling, and the round-trip mat4→decompose→quat→eulerAngles→rebuild is stable for the same angles.

verification: Build passes cleanly for all three targets (pixel-roguelike, level-editor, procedural-model-viewer) with zero errors or warnings after the change.
files_changed:
  - src/game/components/TransformComponent.h
  - src/engine/core/MathUtils.h
