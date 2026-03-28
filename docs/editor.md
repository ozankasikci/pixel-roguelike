# Editor

The level editor is the `level-editor` desktop app built from [apps/level_editor](../apps/level_editor). It edits the same scene and content formats the runtime uses rather than maintaining a separate project database or binary editor format.

## What the Editor Owns

The editor library lives under [`src/editor/`](../src/editor) and is split internally into:

- `assets/`: asset-browser tree building and asset classification
- `core/`: command stack, layout presets, runtime preview session, scene-loading helpers
- `render/`: editor-specific rendering and preview support
- `scene/`: editable scene document and selection system
- `ui/`: outliner, inspector, environment, and asset-browser panels
- `viewport/`: viewport camera, gizmos, and placement interaction

## Main Concepts

### `EditorSceneDocument`

`EditorSceneDocument` is the editable in-memory model of a scene. It stores:

- meshes
- lights
- box and cylinder colliders
- player spawn
- gameplay archetype placements
- environment data and dirty state

Saving the document writes back to the shared `.scene` format.

### Preview World

`EditorPreviewWorld` provides the edit-time scene preview used while manipulating content in the viewport.

### Runtime Preview Session

`EditorRuntimePreviewSession` rebuilds a real `RuntimeGameSession` from the current document when play preview is enabled. This is the bridge that lets the editor test gameplay behavior against the same systems used by the runtime executable.

### Command Stack

The editor tracks undo/redo through `EditorCommandStack` and document-state snapshots. If you add new editor operations, they should participate in that command flow so editing remains undoable.

## Main UI Areas

The default dock layout uses these windows:

- `Viewport`
- `Outliner`
- `Inspector`
- `Asset Browser`
- `Environment`

The editor UI state also tracks:

- transform tool
- preview mode
- play-preview toggle
- visibility toggles for helpers such as colliders and light markers
- placement state for meshes, lights, colliders, player spawn, and archetypes
- selected mesh/material/archetype IDs

## Asset Discovery Inside the Editor

- Scene choices come from `assets/scenes/*.scene`.
- The asset browser scans the `assets/` tree recursively.
- Mesh IDs come from mesh filenames.
- Materials, environments, and prefabs expose their declared `id` in the asset browser when the file format includes one.

This means the editor is sensitive to both directory location and file naming conventions.

## Layout Presets

Editor layout presets are stored outside `assets/` under:

- `editor_layouts/*.layout`

A layout preset stores:

- panel visibility
- saved ImGui dock/layout state

This state is editor-local and should not be confused with gameplay content.

## Practical Workflow

1. Open a scene from `assets/scenes/`.
2. Use the outliner and viewport to select existing objects.
3. Use the inspector and environment panels to modify properties.
4. Use placement tools to add meshes, lights, colliders, player spawn, or gameplay archetypes.
5. Toggle play preview to rebuild a runtime session from the current document.
6. Save the scene or related environment asset back to `assets/`.

## When Editing Code

- Scene-object shape changes usually touch both `EditorSceneDocument` and `LevelDef`.
- New inspector-editable asset types usually need both UI work and content-registry support.
- New viewport manipulation behavior usually belongs under `src/editor/viewport/`.
- If you add new persistent UI state, decide whether it belongs in scene content, editor layout presets, or transient session-only state.

## Related Docs

- [content-pipeline.md](content-pipeline.md)
- [architecture.md](architecture.md)
- [building.md](building.md)
