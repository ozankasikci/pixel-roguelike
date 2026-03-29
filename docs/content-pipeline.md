# Content Pipeline

The project stores most gameplay content as text assets under [`assets/`](../assets) and loads them into runtime/editor structures through `ContentRegistry`, `LevelDef`, and the editor scene document.

## Asset Layout

- `assets/scenes/`: playable scene layouts stored as `.scene`
- `assets/prefabs/gameplay/`: gameplay archetypes stored as `.prefab`
- `assets/defs/materials/`: material definitions stored as `.material`
- `assets/defs/environments/`: environment definitions stored as `.environment`
- `assets/defs/weapons/`: weapon definitions stored as `.weapon`
- `assets/defs/enemies/`: enemy definitions stored as `.enemy`
- `assets/defs/items/`: item definitions stored as `.item`
- `assets/defs/skills/`: skill definitions stored as `.skill`
- `assets/meshes/`: `.glb` mesh assets used by runtime, editor, and tools
- `assets/shaders/`: engine/game shader sources
- `assets/skies/`: sky panoramas and cubemap faces

## Discovery Rules

Not every asset type is discovered the same way today.

### Automatically discovered

- Scene files are discovered for editor selection by enumerating `assets/scenes/*.scene`.
- The editor asset browser scans the full `assets/` tree recursively and classifies assets by extension.
- Environment definitions are loaded by scanning `assets/defs/environments/*.environment`.
- Mesh IDs are derived from the mesh filename stem, so `assets/meshes/pillar.glb` becomes mesh id `pillar`.

### Explicitly registered today

`ContentRegistry::loadDefaults()` currently lists these explicitly in code:

- weapons
- enemies
- items
- skills
- gameplay prefabs
- materials

That means adding a new file in those directories is not enough by itself yet. You also need to update [src/game/content/ContentRegistry.cpp](../src/game/content/ContentRegistry.cpp) so the new asset is loaded into the registry.

## Core File Types

### Scenes: `.scene`

Scene files are parsed into `LevelDef` and may contain:

- meshes
- lights
- box colliders
- cylinder colliders
- player spawn
- gameplay archetype instances
- environment selection

They are line-oriented text files. Parent/child hierarchy is represented through `nodeId` and `parentNodeId`, even though many existing scenes still use mostly flat placement data.

Conventions:

- keep paths under `assets/scenes/`
- use stable `nodeId` values when hierarchy matters
- prefer material IDs over only legacy material enums when authoring new content
- use archetype instances for reusable gameplay setups such as checkpoints and doors

### Gameplay prefabs: `.prefab`

Gameplay prefabs define reusable archetypes such as checkpoints and double doors. Scenes reference them by `archetype_id`.

Conventions:

- store gameplay prefabs under `assets/prefabs/gameplay/`
- start the file with an `id`
- declare the prefab `type`
- keep local-space placement/orientation data in the prefab and scene-space placement in the scene

### Materials: `.material`

Material definition files describe shading model, inheritance, UV behavior, and procedural-material settings.

Conventions:

- place material files under `assets/defs/materials/`
- give every material a unique `id`
- use `parent` for inheritance rather than duplicating large blocks of data

### Environments: `.environment`

Environment files define sky, fog, lighting, post-process, and palette behavior.

Conventions:

- place environment files under `assets/defs/environments/`
- give each file an `id` that matches how scenes and the editor refer to it
- keep reusable lighting/post-process profiles here instead of baking them into scenes
- prefer `game_ready_neutral` as the clean daylight starting point for new reusable environment presets; keep `neutral` as the compatibility baseline

### Weapons, enemies, items, skills

These definition files live under their matching `assets/defs/*` directory and are parsed as small line-oriented text records.

Conventions:

- keep one gameplay definition per file
- give every file an `id`
- if you add a new definition today, also register it in `ContentRegistry::loadDefaults()`

## Contributor Workflows

### Add a new scene

1. Create a new `.scene` file under `assets/scenes/`.
2. Reference existing mesh IDs, material IDs, environment IDs, and archetype IDs.
3. Open it in the editor or pass its path to `level-editor` to validate it.

Because scenes are directory-discovered, a new scene file should appear in the editor without additional registry work.

### Add a new environment

1. Create a new `.environment` file under `assets/defs/environments/`.
2. Give it a unique `id`.
3. Reload content in the editor or restart the app.

Environment files are directory-scanned today, so no code registration step is needed.

### Add a new material, prefab, or gameplay definition

1. Create the new asset file under the appropriate directory.
2. Give it a unique `id`.
3. Update [src/game/content/ContentRegistry.cpp](../src/game/content/ContentRegistry.cpp) so `loadDefaults()` loads it.
4. Rebuild and run tests.

### Add a new mesh

1. Add a `.glb` file under `assets/meshes/`, or generate it with a tool from [`tools/`](../tools).
2. Reference it by filename stem in scenes, prefabs, or viewer presets.
3. If the mesh should be part of a built-in bundle such as cathedral content, update the relevant asset-registration code under [`src/game/levels/`](../src/game/levels).

## Editor Relationship

The editor works directly against these asset formats:

- scenes load into `EditorSceneDocument`
- environment assets can be inspected and saved from the editor
- materials and prefabs are visible in the asset browser
- scene saves write back to the same `.scene` text format the runtime consumes

That shared format is intentional: the editor is not using a private project file format separate from the runtime.

## Related Docs

- [editor.md](editor.md)
- [tooling.md](tooling.md)
- [architecture.md](architecture.md)
