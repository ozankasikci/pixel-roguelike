---
phase: quick
plan: 260402-3at
subsystem: editor
tags: [editor, hierarchy, grouping, scene-document, serialization]
dependency_graph:
  requires: []
  provides: [EditorSceneObjectKind::Group, LevelGroupNode, group serialization, outliner Add Group button]
  affects: [EditorSceneDocument, LevelDef, EditorOutlinerPanel, EditorInspectorPanel, EditorPreviewWorld, EditorViewportInteraction]
tech_stack:
  added: []
  patterns: [EditorSceneObjectPayload variant extension, switch case consistency, editor-only nodes that burn away at runtime]
key_files:
  created: []
  modified:
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/editor/scene/EditorSceneDocument.h
    - src/editor/scene/EditorSceneDocument.cpp
    - src/editor/scene/EditorPreviewWorld.cpp
    - src/editor/scene/EditorSelectionSystem.cpp
    - src/editor/ui/EditorOutlinerPanel.cpp
    - src/editor/ui/EditorInspectorPanel.cpp
    - src/editor/viewport/EditorViewportInteraction.cpp
    - src/editor/core/EditorCommand.cpp
decisions:
  - "Groups produce no ECS entities at runtime — resolveLevelHierarchy processes their transform for child resolution then they disappear"
  - "Groups are loaded first in loadFromSceneFile so parent nodeIds exist before child objects are added"
  - "Group name InputText uses char buffer pattern (imgui_stdlib not used in codebase)"
  - "Group selection in viewport uses sphere handle (0.4f radius) via default case in buildEditorSelectionHandles"
  - "EditorViewportInteraction Group cases fall through with Mesh/BoxCollider/Archetype for gizmo transforms"
metrics:
  duration: "~25 minutes"
  completed: "2026-04-02"
  tasks: 2
  files_modified: 10
---

# Quick Task 260402-3at: Add EditorSceneObjectKind::Group with hierarchy grouping

**One-liner:** Editor group nodes with LevelGroupNode payload carrying transform for child inheritance — serialize as `group <name> <pos> <scale> <rot>`, burn away at runtime in resolveLevelHierarchy.

## What Was Built

Full round-trip support for group nodes in the level editor:

- `LevelGroupNode` struct (name + position/scale/rotation + nodeId/parentNodeId) in `LevelDef.h`
- `groups` vector added to `LevelDef`
- `loadLevelDef` parses `group` keyword; `serializeLevelDef` outputs groups; `resolveLevelHierarchy` processes groups as transform-only nodes (resolves world transform for child nodes to look up, writes back decomposed transform, spawns no entity)
- `EditorSceneObjectKind::Group` added to enum; `LevelGroupNode` added to `EditorSceneObjectPayload` variant
- `addGroup()` method on `EditorSceneDocument`
- All switch statements updated: `supportsParenting`, `localTransformMatrix`, `applyWorldTransform`, `toLevelDef`, `editorSceneObjectKindName`, `editorSceneObjectLabel`, `editorSceneObjectAnchor`
- Groups loaded first in `loadFromSceneFile()` (before meshes) so nodeIds exist for child resolution
- `EditorPreviewWorld::rebuild()` and `syncTransforms()` treat Group as no-op
- "Add Group" button in outliner toolbar (after Delete Selected, with SameLine)
- Group inspector: Name (char buffer InputText), Position/Scale/Rotation DragFloat3
- `EditorViewportInteraction` gizmo switches include Group alongside Mesh/Archetype cases
- `EditorCommand.cpp::makeLevelDefFromState` and `EditorSelectionSystem::selectionPriority` both handle Group

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing critical functionality] Added Group cases to EditorViewportInteraction, EditorCommand, EditorSelectionSystem**

- **Found during:** Build after Task 2 (compiler -Wswitch warnings)
- **Issue:** Three switch statements in `EditorViewportInteraction.cpp`, one in `EditorCommand.cpp`, and two in `EditorSelectionSystem.cpp` did not handle the new `Group` enum value
- **Fix:** Added `case EditorSceneObjectKind::Group:` to each switch: viewport gizmo model selection, viewport gizmo apply, viewport multi-gizmo apply, `makeLevelDefFromState`, and `selectionPriority`
- **Files modified:** `src/editor/viewport/EditorViewportInteraction.cpp`, `src/editor/core/EditorCommand.cpp`, `src/editor/scene/EditorSelectionSystem.cpp`
- **Commit:** ed27e6e

## Known Stubs

None — all Group node functionality is fully wired. Groups create no stubs; they have no renderable presence intentionally.

## Self-Check: PASSED

- `src/game/level/LevelDef.h` contains `LevelGroupNode` and `groups` in `LevelDef`: FOUND
- `src/editor/scene/EditorSceneDocument.h` contains `EditorSceneObjectKind::Group` and `LevelGroupNode` in variant: FOUND
- Build succeeds for both `pixel-roguelike` and `level-editor` targets: PASSED
- Task 1 commit `117785d`: FOUND
- Task 2 commit `ed27e6e`: FOUND
