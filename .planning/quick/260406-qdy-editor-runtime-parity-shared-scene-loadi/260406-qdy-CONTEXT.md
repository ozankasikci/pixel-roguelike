# Quick Task 260406-qdy: Editor-Runtime Parity — Context

**Gathered:** 2026-04-06
**Status:** Ready for planning

<domain>
## Task Boundary

Make the editor run the same scene loading pipeline as the game runtime, including scripted geometry. The editor must always show exactly what the game shows — no divergence.

Currently `InitialSceneScripted.cpp` registers scripted geometry that only the runtime executes via `GenericFileScene::registerScriptedGeometry()`. Doors appear in-game but not in the editor because:
1. `EditorSceneDocument::loadFromSceneFile()` never calls scripted geometry
2. `EditorRuntimePreviewSession::rebuild()` skips the `buildScriptedGeometry` callback

</domain>

<decisions>
## Implementation Decisions

### Integration Point
- Scripted geometry runs in the **preview session only**, not the outliner/scene document
- When user hits Play/Preview, the full runtime pipeline executes including scripted geometry
- The outliner remains file-only (shows what's in the .scene file)

### Editability
- Scripted geometry entities ARE editable in the editor during a preview session (can move, modify, etc.)
- Changes are NOT persistent — stopping and restarting the preview resets to code-defined state
- This is expected behavior since entities are code-generated

### Parity Scope
- **Full runtime parity** — editor preview runs ALL systems: physics, doors, interactions, gameplay
- The preview session should be indistinguishable from running the game
- This means `EditorRuntimePreviewSession` must populate `LevelLoadRequest.buildScriptedGeometry` from the scripted geometry registry

</decisions>

<specifics>
## Specific Ideas

- The key fix point is `EditorRuntimePreviewSession::rebuild()` (src/editor/core/EditorRuntimePreviewSession.cpp:25-29) — it needs to look up and invoke the scripted geometry callback from `GenericFileScene`'s static registry
- `registerInitialSceneScripted()` must also be called in the editor's main.cpp so the registry is populated
- The scripted geometry registry is in `GenericFileScene::scriptedGeometryRegistry()` — a static map keyed by levelId

</specifics>
