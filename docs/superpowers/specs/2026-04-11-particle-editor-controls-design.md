# Particle Editor Controls Design Spec

## Overview

Add full editor controls for particle emitters: viewport gizmo for moving them, "Place Particle Emitter" in the create menu, and live parameter tweaking in the inspector with hot-reload. All three features follow existing editor patterns.

## Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Gizmo | Position-only (translate) | Emitters are point sources, no rotation/scale needed |
| Create menu | Placement flow via EditorPlacementKind | Same pattern as lights, checkpoints, colliders |
| Inspector params | Show all parameters always | Small param count, conditional hiding adds complexity without benefit |
| Color stops UI | Simple list with DragFloat + ColorEdit per row | Matches existing inspector list patterns; visual gradient bar is a future feature |
| Edit target | Inspector edits the `.particle` definition file | Multiple emitters sharing the same ID all update together |
| Hot-reload | Automatic via file modification polling | Follows the existing material hot-reload pattern in ContentRegistry |

## 1. Viewport Gizmo

Wire `ParticleEmitter` into the existing ImGuizmo system in `EditorViewportInteraction.cpp`.

**Single-object gizmo switch** — add case for `EditorSceneObjectKind::ParticleEmitter`:
- Build model matrix from `placement.position` with a small display scale (0.5) for the gizmo handle
- On gizmo drag, call `document.applyWorldTransform()` which already handles `LevelParticleEmitterPlacement` (extracts position from matrix column 3)

**Multi-gizmo** — already works through the existing `applyWorldTransform()` visitor.

### Files Modified
- `src/editor/viewport/EditorViewportInteraction.cpp` — two switch cases (gizmo display + position update)

## 2. Create Menu

Add "Place Particle Emitter" to the editor's create context menu.

### Changes
- `src/editor/ui/LevelEditorUi.h` — add `ParticleEmitter` to `EditorPlacementKind` enum
- `src/editor/ui/EditorAssetBrowserPanel.cpp` — add menu item calling `beginPlacement()` with `EditorPlacementKind::ParticleEmitter`
- `src/editor/ui/EditorPanelUtils.cpp` — handle new kind in `beginPlacement()` (set placement properties) and `commitPlacement()` (create `LevelParticleEmitterPlacement` with default emitterId `"dust_motes"` and committed position, call `document.addParticleEmitter()`)

## 3. Inspector Parameter Editing

Expand `ParticleEmitterInspector` to show all definition parameters with live editing.

### Inspector Layout

```
--- Emitter Id ---
[combo dropdown from ContentRegistry particle emitter IDs]

--- Position ---
[x] [y] [z]

--- Emission ---
Emission Rate      [DragFloat, 0.1-1000]
Looping            [Checkbox]
Duration           [DragFloat, 0-60]
Warm Up            [Checkbox]

--- Particles ---
Lifetime           [DragFloat min] [DragFloat max]
Initial Speed      [DragFloat min] [DragFloat max]
Rotation Speed     [DragFloat min] [DragFloat max]

--- Shape ---
Shape Type         [Combo: point/sphere/cone]
Param 0            [DragFloat] (label: "Radius" for sphere, "Angle" for cone)
Param 1            [DragFloat] (label: "Base Radius", only for cone)

--- Rendering ---
Blend Mode         [Combo: additive/alpha]
Simulation Space   [Combo: world/local]
Emissive Strength  [DragFloat, 0.1-10]
Soft Particle Fade [DragFloat, 0-5]

--- Forces ---
[Combo type] [params per type]  [X remove button]
...
[+ Add Force button]

--- Color Over Lifetime ---
[DragFloat t] [ColorEdit4 RGBA]  [X remove button]
...
[+ Add Stop button]

--- Size Over Lifetime ---
[DragFloat t] [DragFloat value]  [X remove button]
...
[+ Add Keyframe button]
```

### Data Flow

1. Inspector reads `placement.emitterId` and looks up `ParticleEmitterDefinition` from ContentRegistry
2. All definition fields are presented as editable widgets
3. On any field change:
   a. Update the in-memory `ParticleEmitterDefinition` in ContentRegistry
   b. Call `saveParticleEmitterDefinition()` to write the `.particle` file
   c. Mark the document dirty for undo support
4. Hot-reload detects the file change and reloads (see section 4)
5. ParticleUpdateSystem picks up the updated definition and reconstructs affected emitters

### Undo/Redo

Parameter edits use the same `captureState()` / `trackLastItemCommand()` pattern as other inspectors. The undo captures the full document state before the edit. On undo, the previous state is restored and the `.particle` file is re-saved.

### Files Modified
- `src/editor/ui/inspectors/ParticleEmitterInspector.cpp` — expanded from 2 fields to full parameter editing

### Files Accessed (read-only)
- `src/game/content/ContentRegistry.h` — `findParticleEmitter()`, `particleEmitters()`
- `src/game/particles/ParticleEmitterDefinition.h` — `saveParticleEmitterDefinition()`

## 4. Hot-Reload

Follow the material hot-reload pattern in ContentRegistry.

### Changes to ContentRegistry
- Add `particleEmitterPaths_` map (id → file path) populated during `loadParticleEmittersFromDirectory()`
- Add `particleEmitterTimestamps_` map (path → last modification time)
- Add `pollParticleHotReload()` method — checks file timestamps each frame, reloads changed definitions
- Increment a `particleDefinitionGeneration_` counter on any reload

### Changes to ParticleUpdateSystem
- Track the generation counter from ContentRegistry
- When generation changes, destroy cached emitters for affected IDs so they get reconstructed from the new definition on next update

### Integration
- Call `pollParticleHotReload()` from the same place `pollMaterialHotReload()` is called (editor per-frame update)

### Files Modified
- `src/game/content/ContentRegistry.h` — add path/timestamp maps, poll method, generation counter
- `src/game/content/ContentRegistry.cpp` — implement hot-reload polling
- `src/game/modules/particles/ParticleSystem.h` — add generation tracking
- `src/game/modules/particles/ParticleSystem.cpp` — check generation, invalidate stale emitters

## Scope

### Included
- Viewport translate gizmo for particle emitters
- "Place Particle Emitter" in create menu with placement flow
- Full parameter inspector for all `.particle` definition fields
- Add/remove for forces, color stops, size keyframes
- Emitter ID dropdown from ContentRegistry
- Auto-save to `.particle` file on edit
- Hot-reload with live preview update

### Excluded (future)
- Visual gradient bar editor for color stops
- Rotation/scale gizmo (emitters are point sources)
- Emitter shape wireframe gizmo overlay in viewport
- Creating new `.particle` definition files from the inspector (must exist on disk first)
- Burst event editing in inspector
