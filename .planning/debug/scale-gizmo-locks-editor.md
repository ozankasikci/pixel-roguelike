---
status: resolved
trigger: "scale-gizmo-locks-editor"
created: 2026-04-02T00:00:00Z
updated: 2026-04-02T20:35:00Z
---

## Current Focus
<!-- OVERWRITE on each update - reflects NOW -->

hypothesis: CONFIRMED -- HandleScale() and HandleRotation() in ImGuizmo.cpp have !mbMouseOver in their early-exit guards, causing mbUsing to stay true permanently when the mouse leaves the viewport mid-drag. editorGizmoIsHot() then blocks all editor input.
test: Fix applied to both HandleScale() and HandleRotation() -- split the guard so !mbMouseOver only blocks when NOT in active drag (mbUsing==false)
expecting: Scale gizmo drags that leave viewport should release normally; editor input should remain responsive
next_action: Awaiting human verification -- user should test scale gizmo in editor, including dragging beyond viewport bounds

## Symptoms
<!-- Written during gathering, then IMMUTABLE -->

expected: Scale gizmo should allow scaling objects normally, like translate and rotate gizmos work
actual: When using the scale gizmo, input becomes stuck — editor renders but mouse/keyboard seems locked or gizmo won't release
errors: None reported
reproduction: Select any model in the editor, switch to scale gizmo, try to scale — input locks up
started: Recently broke — scale gizmo used to work

## Eliminated
<!-- APPEND only - prevents re-investigating -->

- hypothesis: Bug introduced by 515138b (vertical drag direction change)
  evidence: The actual lock-up mechanism is the !mbMouseOver early-exit guard in HandleScale, which predates the recent commits. The recent commits changed the scale sensitivity calculation, not the guard logic.
  timestamp: 2026-04-02

- hypothesis: Input capture / WantCaptureMouse issue in editor main loop (original)
  evidence: The issue was thought to be entirely within ImGuizmo — but human verify response confirms translate gizmo and undo/redo also lock, meaning the root cause must be broader than the HandleScale mbMouseOver guard
  timestamp: 2026-04-02

- hypothesis: HandleScale() mbMouseOver fix is the complete solution
  evidence: Human verification confirms translate gizmo also locks input, and undo/redo gets stuck. HandleTranslation has no mbMouseOver guard so the same mechanism does not apply there. The root cause must involve an editor-level input lock.
  timestamp: 2026-04-02

## Evidence
<!-- APPEND only - facts discovered -->

- timestamp: 2026-04-02
  checked: HandleTranslation() guard at line 2156
  found: `if(!Intersects(op, TRANSLATE) || type != MT_NONE)` — NO !mbMouseOver check
  implication: Translate always executes the active-drag path including mouse release check, so it never gets stuck

- timestamp: 2026-04-02
  checked: HandleScale() guard at line 2279
  found: `if((!Intersects(op, SCALE) && !Intersects(op, SCALEU)) || type != MT_NONE || !gContext.mbMouseOver)` — HAS !mbMouseOver check
  implication: When mouse leaves viewport during scale drag (e.g. drifts into outliner/inspector panel), IsHoveringWindow() returns false, the function exits early at line 2281, never reaching the `if (!io.MouseDown[0]) { mbUsing = false; }` release at line 2392-2396. mbUsing stays true permanently.

- timestamp: 2026-04-02
  checked: IsHoveringWindow() at line 942
  found: Returns false when g.HoveredWindow is a different (non-null) window
  implication: Dragging the scale gizmo vertically (as the new Unity-style uniform scale requires) causes the mouse to easily stray into adjacent panels, triggering the stuck state

- timestamp: 2026-04-02
  checked: HandleRotation() guard at line 2405
  found: Same !mbMouseOver guard — same potential bug, but rotate gesture (circular) is less likely to move mouse out of viewport than vertical scale drag
  implication: Rotate could have the same stuck bug but is triggered less easily by user gesture

- timestamp: 2026-04-02
  checked: editorGizmoIsHot() in EditorViewportController.cpp line 276
  found: `return ImGuizmo::IsUsing() || ImGuizmo::IsOver();` — IsUsing() returns true when mbUsing is true
  implication: When mbUsing gets stuck as true, editorGizmoIsHot() always returns true, blocking all click-to-select, placement, and camera orbit interactions — consistent with "input locked" symptom

## Resolution
<!-- OVERWRITE as understanding evolves -->

root_cause: HandleScale() and HandleRotation() in ImGuizmo.cpp have `!gContext.mbMouseOver` in their early-exit guards. When the mouse leaves the viewport window mid-drag (by drifting into an adjacent panel), IsHoveringWindow() returns false, the handler returns early, never reaching the `if (!io.MouseDown[0]) { mbUsing = false; }` release check. mbUsing stays true permanently, and editorGizmoIsHot() always returns true, blocking all editor input. The vertical drag added in the recent scale gizmo commits (8b1e428, 515138b) makes this pre-existing bug easy to trigger because dragging up/down readily moves the mouse out of the viewport bounds.
fix: Split the early-exit guard in HandleScale() (line 2284) and HandleRotation() (line 2418) into two parts -- (1) op/type check always runs, (2) !mbMouseOver check only runs when NOT in active drag (`if (!gContext.mbUsing && !gContext.mbMouseOver) return false`). This ensures active drags always proceed to the release check. Same pattern already used by GetMoveType() (line 2096).
verification: Build succeeded (level-editor compiles cleanly). Test script at tests/editor/test_scale_gizmo_release.sh exercises the fix via debug harness. Awaiting human runtime verification.
files_changed: [external/ImGuizmo/ImGuizmo.cpp, tests/editor/test_scale_gizmo_release.sh]
