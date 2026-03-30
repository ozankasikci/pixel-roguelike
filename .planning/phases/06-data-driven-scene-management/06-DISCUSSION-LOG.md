# Phase 6: Data-Driven Scene Management - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-03-30
**Phase:** 06-data-driven-scene-management
**Areas discussed:** New Scene defaults, Default scene config, Scene browser UX, Asset registration

---

## New Scene Defaults

| Option | Description | Selected |
|--------|-------------|----------|
| Bare minimum | Player spawn at origin + default environment profile only. Clean slate. | ✓ |
| Starter template | Player spawn + one warm ceiling light + a floor plane. | |
| Multiple templates | Let user pick from templates: Empty, Basic Room, Corridor. | |

**User's choice:** Bare minimum
**Notes:** Matches handcrafted design philosophy — user adds everything intentionally.

### Trigger mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| File > New Scene | Standard File menu entry with name prompt. | |
| Asset Browser + button | '+' button in scenes section of asset browser. | |
| Both | File menu + asset browser button. | ✓ |

**User's choice:** Both
**Notes:** File menu for discoverability, asset browser for quick access.

### Auto-open behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Auto-open | Create and immediately load into editor. | |
| Create only | Just create the file, user opens manually. | ✓ |

**User's choice:** Create only

### File location

| Option | Description | Selected |
|--------|-------------|----------|
| Always assets/scenes/ | Flat directory, no subfolders. | ✓ |
| User picks location | File dialog for custom path. | |

**User's choice:** Always assets/scenes/

---

## Default Scene Config

### Default scene mechanism

| Option | Description | Selected |
|--------|-------------|----------|
| Project config file | project.cfg with manually set default_scene. | |
| Editor's last-opened scene | Runtime reads editor's last-opened scene. | |
| First .scene file found | Alphabetically first scene. | |

**User's choice:** Other — "the project should start with latest opened scene automatically"
**Notes:** User wants the runtime to automatically load whatever scene was last opened in the editor. This led to the auto-tracking approach: editor writes last-opened scene to project.cfg, runtime reads it.

### Auto-track confirmation

| Option | Description | Selected |
|--------|-------------|----------|
| Auto-track only | Editor always writes last-opened scene to project.cfg. No manual override. | ✓ |
| Auto-track + manual override | Auto-tracks but user can pin a specific default. | |

**User's choice:** Auto-track only

### First-launch fallback

| Option | Description | Selected |
|--------|-------------|----------|
| Fallback to warden_office.scene | Hardcoded fallback. | |
| Fallback to first .scene found | Scan and pick first alphabetically. | |
| Show scene picker | Simple scene selection dialog. | ✓ |

**User's choice:** Show scene picker

### Scene picker style

| Option | Description | Selected |
|--------|-------------|----------|
| Simple ImGui list | Plain list of filenames with Launch button. | ✓ |
| Visual with previews | Thumbnails or mini-previews. | |

**User's choice:** Simple ImGui list

---

## Scene Browser UX

### Opening scenes

| Option | Description | Selected |
|--------|-------------|----------|
| Asset browser scenes tab | Scenes category in asset browser, double-click to open. | ✓ |
| File > Open Scene dialog | Standard OS file dialog. | |
| Both | Asset browser + file dialog. | |

**User's choice:** Asset browser scenes tab

### Delete behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Right-click > Delete in asset browser | Context menu with confirmation dialog. | ✓ |
| File menu only | File > Delete Current Scene. | |
| No delete from editor | User deletes from filesystem. | |

**User's choice:** Right-click > Delete in asset browser

### Delete active scene behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Close and show empty editor | Clear editor, show create/open prompt. | ✓ |
| Block deletion | Prevent deleting open scene. | |
| Auto-open another scene | Load next available scene. | |

**User's choice:** Close and show empty editor

### Editor startup behavior

| Option | Description | Selected |
|--------|-------------|----------|
| Auto-load last scene | Read project.cfg and load last-opened scene. | ✓ |
| Start empty | No scene loaded on startup. | |

**User's choice:** Auto-load last scene

### Active scene indicator

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, mark active scene | Visual highlight in asset browser. | ✓ |
| No indicator needed | Scene name shown elsewhere. | |

**User's choice:** Yes, mark active scene

### Unsaved changes on switch

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, prompt if dirty | Save / Don't Save / Cancel dialog. | ✓ |
| Auto-save before switch | Automatic save. | |
| No prompt | Just switch, lose changes. | |

**User's choice:** Yes, prompt if dirty

### Rename support

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, add rename | Right-click > Rename in asset browser. | ✓ |
| No, not this phase | Defer to later. | |

**User's choice:** Yes, add rename

### Duplicate support

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, add duplicate | Right-click > Duplicate with _copy suffix. | |
| No, skip for now | Defer to later. | ✓ |

**User's choice:** No, skip for now

---

## Asset Registration

### Registration strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Register all assets always | Unconditional registration. GenericFileScene already does this. | ✓ |
| Per-scene asset declaration | .scene file declares needed asset sets. | |
| Auto-detect from content | Scan scene for mesh names. | |

**User's choice:** Register all assets always

### Consolidation

| Option | Description | Selected |
|--------|-------------|----------|
| Consolidate into one | Single ContentRegistry::registerAll(). | ✓ |
| Keep separate | Keep registerCathedralAssets/registerPrisonAssets as organizational units. | |

**User's choice:** Consolidate into ContentRegistry::registerAll()

### Legacy file handling

| Option | Description | Selected |
|--------|-------------|----------|
| Delete entirely | Remove all legacy scene .h/.cpp files. Git preserves history. | ✓ |
| Keep as comments | Comment out with reference note. | |

**User's choice:** Delete entirely

### Registration function handling

| Option | Description | Selected |
|--------|-------------|----------|
| Inline into registerAll | Merge registrations into single method, delete original functions. | ✓ |
| Keep as private helpers | Keep as private methods in ContentRegistry. | |

**User's choice:** Inline into registerAll

---

## Claude's Discretion

- project.cfg parsing implementation details
- Empty editor state UI design
- Asset browser scenes tab implementation (refresh, sorting)
- Scene picker dialog integration into runtime main loop
- CMake changes for legacy file removal

## Deferred Ideas

- Duplicate Scene feature (right-click > Duplicate)
- Per-scene asset declarations
- Scene thumbnails/previews in asset browser
- Scene templates for New Scene
- Subdirectory organization for scenes
