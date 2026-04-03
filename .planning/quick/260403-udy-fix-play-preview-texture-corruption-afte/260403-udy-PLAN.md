---
phase: quick
plan: 260403-udy
type: execute
wave: 1
depends_on: []
files_modified:
  - apps/level_editor/main.cpp
  - tests/editor/test_editor_runtime_preview.cpp
autonomous: true
requirements: []
---

<objective>
Fix play preview texture corruption that appears after moving a scene mesh with the gizmo.

Purpose: The likely bug is in the editor's scene-revision sync path. Gizmo moves increment `document.sceneRevision()`, but the current branch only updates `previewWorld` for the edit viewport and leaves `runtimePreviewSession` reusable. Entering play preview can then render stale runtime scene state after the document changed.

Output: Moving a mesh in edit mode no longer leaves play preview in a corrupted or stale render state.
</objective>

<context>
@apps/level_editor/main.cpp
@tests/editor/test_editor_runtime_preview.cpp
</context>

<tasks>

<task type="auto">
  <name>Task 1: Invalidate and rebuild play preview after gizmo-driven scene revisions</name>
  <files>apps/level_editor/main.cpp, tests/editor/test_editor_runtime_preview.cpp</files>
  <action>
Keep the fix narrow.

In `apps/level_editor/main.cpp`, update the `previewSceneRevision != document.sceneRevision()` path so gizmo-driven scene changes do two things:
1. Keep the current edit-preview sync (`previewWorld.syncTransforms/syncMaterials/syncLights`) so the editor viewport stays live.
2. Also mark the runtime play-preview session dirty for a full rebuild and refresh the debounce timestamp, so the next idle rebuild or play-preview entry reconstructs the runtime world from the latest `EditorSceneDocument` instead of reusing stale state.

Do not redesign the preview system or add partial runtime transform sync for this quick fix. Prefer correctness: any scene-object transform change from the gizmo should force the same full runtime rebuild path already used for structural scene changes.

In `tests/editor/test_editor_runtime_preview.cpp`, add a focused regression around the moved-mesh case: rebuild a preview session, mutate a mesh transform in the document, take the same rebuild path the editor now uses, and confirm a subsequent render completes cleanly.
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor test_editor_runtime_preview && ctest --test-dir build --output-on-failure -R test_editor_runtime_preview</automated>
  </verify>
  <done>
    After moving a scene mesh with the gizmo, play preview rebuilds from the updated document before rendering, and the corruption/stale-state bug is gone.
  </done>
</task>

</tasks>

<verification>
- Build `level-editor` and `test_editor_runtime_preview`.
- Run `test_editor_runtime_preview`.
- Manual sanity check: open a scene, move any mesh with the translate gizmo, toggle Play Preview, and confirm the viewport renders normally with no corrupted textures or stale mesh state.
</verification>

<success_criteria>
Play Preview no longer reuses stale runtime scene state after a gizmo mesh move, and the moved-mesh workflow renders cleanly.
</success_criteria>

<output>
After completion, commit changes directly (quick task, no SUMMARY needed).
</output>
