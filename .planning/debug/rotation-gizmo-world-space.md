---
status: awaiting_human_verify
trigger: "rotation-gizmo-rotates-with-object"
created: 2026-04-01T00:00:00Z
updated: 2026-04-01T00:00:00Z
---

## Current Focus

hypothesis: CONFIRMED — ImGuizmo::Manipulate() is hardcoded to LOCAL mode, causing the rotation gizmo to follow the object's local axes instead of world axes.
test: Verified by reading EditorViewportController.cpp lines 259-266
expecting: Fix: pass WORLD mode for rotate, LOCAL for translate/scale
next_action: Apply fix in EditorViewportController.cpp

## Symptoms

expected: The rotation gizmo should always display in world-space orientation — its rings represent world X, Y, Z axes, not the object's local axes. This is how Unity's default rotation gizmo behavior works.
actual: The rotation gizmo rotates with the object, so its rings follow the object's local axes instead of staying world-aligned.
errors: No errors — visual/UX issue.
reproduction: Select an object in the editor, switch to rotation gizmo, rotate the object — the gizmo rings rotate with it instead of staying world-aligned.
started: Likely always been this way.

## Eliminated

(none yet)

## Evidence

- timestamp: 2026-04-01T00:00:00Z
  checked: src/editor/viewport/EditorViewportController.cpp lines 259-266
  found: ImGuizmo::Manipulate() is called with ImGuizmo::LOCAL hardcoded as the mode argument regardless of which tool (Translate/Rotate/Scale) is active.
  implication: The rotation gizmo uses LOCAL space, so its rings track the object's local axes. Fix is to select the mode based on the operation — WORLD for rotate, LOCAL for translate/scale.

## Resolution

root_cause: ImGuizmo::Manipulate() in EditorViewportController.cpp was hardcoded to ImGuizmo::LOCAL mode for all gizmo operations. For the rotation gizmo this makes the rings track the object's local axes, so after any rotation the ring orientations change to match the new local frame instead of staying world-aligned.
fix: Added a gizmoMode variable that selects ImGuizmo::WORLD when the active tool is EditorTransformTool::Rotate, and ImGuizmo::LOCAL otherwise (Translate and Scale). The WORLD mode causes ImGuizmo to draw the rotation rings aligned to world X/Y/Z axes regardless of the object's orientation. The delta rotation written back into modelMatrix is still correct — ImGuizmo handles the WORLD-to-LOCAL conversion internally.
verification: Code compiles cleanly (level-editor target built without errors). Visual verification needed in the running editor.
files_changed: ["src/editor/viewport/EditorViewportController.cpp"]
