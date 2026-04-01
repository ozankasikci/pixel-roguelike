---
gsd_state_version: 1.0
milestone: v1.1
milestone_name: Editor UX
status: executing
stopped_at: Completed 12-04-PLAN.md
last_updated: "2026-04-01T18:30:05.059Z"
last_activity: 2026-04-01
progress:
  total_phases: 14
  completed_phases: 13
  total_plans: 35
  completed_plans: 35
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-04-01)

**Core value:** The Stanley Parable-inspired art style — clean, minimalist environments with warm soft lighting, muted color palette, and stylized realism
**Current focus:** Phase 12 — engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens

## Current Position

Phase: 12
Plan: Not started
Status: Ready to execute
Last activity: 2026-04-01

Progress: [░░░░░░░░░░] 0% (v1.1 milestone)

## Performance Metrics

**Velocity (v1.1):**

- Total plans completed: 0
- Average duration: —
- Total execution time: 0 hours

**By Phase (v1.1):**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| 9 - Selection Depth Fix | TBD | - | - |
| 10 - Global Shortcuts + Hover | TBD | - | - |
| 11 - Add Mesh Discoverability | TBD | - | - |

*Updated after each plan completion*
| Phase 09 P01 | 5min | 2 tasks | 1 files |
| Phase 10 P01 | 2 | 2 tasks | 3 files |
| Phase 10 P02 | 30min | 2 tasks | 3 files |
| Phase 11-add-mesh-discoverability P01 | 2 | 2 tasks | 3 files |
| Phase 12 P02 | 15 | 2 tasks | 6 files |
| Phase 12 P03 | 12 | 2 tasks | 8 files |
| Phase 12 P04 | 15 | 2 tasks | 6 files |

## Accumulated Context

### Decisions

Recent decisions affecting current work:

- [Phase 08]: Metal and chained doors from buildScriptedGeometry (not .scene file) so InteractableComponent can be attached
- [Phase 08]: Chain padlock links constructed from cylinder segments; 4 cylinders per link in rectangular loop
- [v1.1 research]: All document mutation APIs complete — `eraseObjects`, `duplicateObject`, `addMesh`, `applyWorldTransform` need no changes
- [v1.1 research]: Selection depth fix is a single-file change in `EditorScenePreviewRenderer.cpp` — remove `ignoreDepth=true` from primary selection overlay pass
- [v1.1 research]: Global shortcuts (Delete, Ctrl+D, Escape, F) must live in `main.cpp` as single handlers with `ImGuiInputFlags_RouteGlobal`; panel-side duplicates removed
- [v1.1 research]: Every new mutation entry point requires explicit capture-before/push-after to `EditorCommandStack` — not enforced by the type system
- [v1.1 research]: `pruneSelection` must be called after every undo/redo call to avoid inspector null-dereference on stale selected IDs
- [v1.1 research]: `duplicateObject()` copies nodeId verbatim — `ensureObjectNodeId()` must be called on duplicate to avoid serialization collision
- [Phase 09]: Two-pass selection overlay: ghost wireframe (ignoreDepth=true, 20% tint) + depth-tested primary wireframe (ignoreDepth=false, full tint)
- [Phase 10]: Camera animation uses ease-out cubic (1-(1-t)^3) for natural deceleration; user input (RMB/MMB/alt+LMB/scroll) cancels in-progress framing animation
- [Phase 10]: Duplicate offset is world-space translation (0.5,0,0) via applyWorldTransform, preserving rotation and scale of duplicated object
- [Phase 10]: Escape guard uses !io.WantTextInput so text field Escape deactivates field first; second Escape clears selectedIds+selectionPicker+inspector context
- [Phase 10]: Hover color (0.55, 0.85, 1.00) produces cool blue-white visually distinct from selection gold without a separate alpha channel
- [Phase 10]: appendHoverOverlay self-guards against selected objects; ignoreDepth=false for depth-tested single pass only (no ghost through geometry)
- [Phase 10]: Selection picker popup overlay removed during verification — hover highlight provides equivalent pre-click affordance without intrusive UI
- [Phase 11]: commitPlacement returns std::optional<uint64_t>: result variable pattern with Mesh case setting it, all others returning nullopt
- [Phase 11]: Add Mesh button placed immediately after Add button with SameLine; filter cleared on each popup open
- [Phase 12]: CameraDebugInfo lives in engine/rendering (camera display data is engine-layer concern, not game-layer)
- [Phase 12]: RuntimeLightingOverride lives in game/rendering (lighting overrides are game-layer concern)
- [Phase 12]: DebugParams retains only UI overlay state plus two embedded sub-structs (CameraDebugInfo camera, RuntimeLightingOverride lighting)
- [Phase 12]: TextureUnits namespace (not enum class) allows direct int use in GL calls without casting
- [Phase 12]: LevelLoadArgs uses pointers for optional levelDef and designated initializer compatibility
- [Phase 12]: GenericFileScene scripted geometry uses static registry pattern — register via registerScriptedGeometry(), not if-chain in onEnter()
- [Phase 12]: File-alias registrations removed from ProceduralGameAssets (pillar, arch, hand, wood_door, etc.) — will be auto-discovered in Plan 04; loadFromFileMulti kept as multi-submesh exception
- [Phase 12]: EventBus [[nodiscard]] subscribe() returns RAII SubscriptionToken; store token in member to keep subscription alive
- [Phase 12]: culledInput pattern: copy SceneRenderInput, swap objects pointer to culledObjects vector — cleanest way to thread culled list through sub-passes without signature changes
- [Phase 12]: FrustumCulling uses Gribb-Hartmann VP matrix extraction; isAabbInsideFrustum transforms 8 local AABB corners to world space before plane test

### Roadmap Evolution

- Phase 12 added: Engine quality — frustum culling, texture unit enum, generic asset system, EventBus RAII tokens

### Pending Todos

None yet.

### Blockers/Concerns

None for v1.1. All APIs exist; work is wiring and one renderer fix.

### Quick Tasks Completed

| # | Description | Date | Commit | Directory |
|---|-------------|------|--------|-----------|
| 260329-t50 | Fix lighting attenuation and intensity | 2026-03-29 | 459e34f | [260329-t50-fix-lighting-attenuation-and-intensity](./quick/260329-t50-fix-lighting-attenuation-and-intensity/) |
| 260329-uom | Raise warden office ceiling from 2.5m to 3.5m | 2026-03-29 | 97c3880 | [260329-uom-fix-warden-office-room-being-too-small-i](./quick/260329-uom-fix-warden-office-room-being-too-small-i/) |
| 260329-uy6 | Match Stanley Parable lighting and color palette | 2026-03-29 | 0a1f494 | [260329-uy6-match-stanley-parable-lighting-and-color](./quick/260329-uy6-match-stanley-parable-lighting-and-color/) |
| 260329-x0q | Fix pixelated rendering and clean up old dither artifacts | 2026-03-29 | a5fa67a | [260329-x0q-fix-pixelated-low-resolution-rendering-i](./quick/260329-x0q-fix-pixelated-low-resolution-rendering-i/) |
| 260330-0fz | Implement disk-based asset cache for meshes and procedural textures | 2026-03-30 | a2b1c1e | [260330-0fz-implement-disk-based-asset-cache-for-mes](./quick/260330-0fz-implement-disk-based-asset-cache-for-mes/) |
| 260330-171 | Comprehensive AssetCache test suite for invalidation, binary format, and edge cases | 2026-03-30 | 0ed7241 | [260330-171-comprehensive-assetcache-test-suite-for-](./quick/260330-171-comprehensive-assetcache-test-suite-for-/) |
| 260330-222 | Port AudioSystem, ActionMap/InputSystem, GameOverlays, EditorConsoleSink from codex/scripting-v1 | 2026-03-30 | ecc270a | [260330-222-port-audiosystem-actionmap-inputsystem-g](./quick/260330-222-port-audiosystem-actionmap-inputsystem-g/) |
| 260330-321 | Add concrete wall texture material to warden office | 2026-03-30 | 3187a34 | [260330-321-add-concrete-wall-texture-material-to-wa](./quick/260330-321-add-concrete-wall-texture-material-to-wa/) |
| 260330-mzu | Restore post-processing flags disabled by SSAO commit | 2026-03-30 | 19068c7 | [260330-mzu-fix-editor-scene-objects-not-visible-aft](./quick/260330-mzu-fix-editor-scene-objects-not-visible-aft/) |
| 260330-rwe | Create a Claude Code skill for procedural texture generation | 2026-03-30 | e976302 | [260330-rwe-create-a-claude-code-skill-for-procedura](./quick/260330-rwe-create-a-claude-code-skill-for-procedura/) |
| 260330-wc2 | Fix too-fast scrollbar scrolling in the editor asset browser panel | 2026-03-30 | 156472e | [260330-wc2-fix-too-fast-scrollbar-scrolling-in-the-](./quick/260330-wc2-fix-too-fast-scrollbar-scrolling-in-the-/) |
| 260330-wt0 | Reorganize Environment inspector panel into logical sub-groups | 2026-03-30 | 1824913 | [260330-wt0-improve-environment-inspector-panel-ux-b](./quick/260330-wt0-improve-environment-inspector-panel-ux-b/) |
| 260401-qau | Fix delete key on macOS: add ImGuiKey_Backspace as alternative trigger | 2026-04-01 | 698ccf3 | [260401-qau-fix-delete-key-on-macos-add-imguikey-bac](./quick/260401-qau-fix-delete-key-on-macos-add-imguikey-bac/) |
| 260401-rt2 | Cache shader uniform locations, guard Jolt body ID, deduplicate MathUtils, reuse renderer vectors | 2026-04-01 | 639ae43 | [260401-rt2-codebase-cleanup-cache-uniform-locations](./quick/260401-rt2-codebase-cleanup-cache-uniform-locations/) |

## Session Continuity

Last activity: 2026-04-01 - Completed quick task 260401-rt2: Codebase cleanup — cache uniform locations
Last session: 2026-04-01T18:23:39.214Z
Stopped at: Completed 12-04-PLAN.md
Resume file: None
