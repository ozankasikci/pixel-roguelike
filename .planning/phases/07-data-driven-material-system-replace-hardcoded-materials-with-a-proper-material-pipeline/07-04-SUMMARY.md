---
phase: 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline
plan: 04
subsystem: content
tags: [hot-reload, material-system, validation, crud, file-watcher, content-registry]

# Dependency graph
requires:
  - phase: 07-02
    provides: ContentRegistry with loadMaterialsFromDirectory, MaterialTextureLibrary with magenta fallback

provides:
  - ContentRegistry::pollMaterialHotReload — polls assets/materials/ every 500ms and reloads changed .material files
  - ContentRegistry::validateMaterialDefinition — validates roughness/metalness ranges, empty ID, missing parent
  - ContentRegistry::addMaterial / removeMaterial — CRUD mutation API for editor asset browser
  - MaterialTextureLibrary::reloadMaterial — per-material cache invalidation with immediate re-resolution
  - Editor main loop polls hot-reload per-frame (level-editor only)

affects:
  - editor asset browser (Plan 03 uses addMaterial/removeMaterial)
  - any code that calls ContentRegistry::validateMaterialInheritance

# Tech tracking
tech-stack:
  added: []
  patterns:
    - Polling file watcher using std::filesystem::last_write_time with 500ms debounce
    - Per-material cache invalidation: erase old texture key, re-resolve definition, lazy texture re-generation
    - CRUD mutation pair (addMaterial/removeMaterial) tracks file path alongside in-memory definition for watcher state

key-files:
  created: []
  modified:
    - src/game/content/ContentRegistry.h
    - src/game/content/ContentRegistry.cpp
    - src/game/rendering/MaterialTextureLibrary.h
    - src/game/rendering/MaterialTextureLibrary.cpp
    - apps/level_editor/main.cpp
    - tests/game/CMakeLists.txt

key-decisions:
  - "pollMaterialHotReload takes MaterialTextureLibrary& by reference — editor owns both and passes them; runtime game never calls this"
  - "reloadMaterial takes the materials map so it can immediately re-resolve the updated definition — avoids one-frame magenta flash"
  - "test_content_registry links game_rendering (not just game_content) because ContentRegistry.cpp now calls MaterialTextureLibrary methods"
  - "validateMaterialDefinition is public so Plan 03 asset browser can validate before saving new materials"

patterns-established:
  - "Hot-reload poll: check elapsed time guard → iterate file times map → compare last_write_time → reload + validate + invalidate cache"
  - "CRUD mutation: addMaterial/removeMaterial keep materialFilePathById_ and materialFileTimes_ in sync so hot-reload watches added materials too"

requirements-completed:
  - MAT-HOT-RELOAD
  - MAT-VALIDATION
  - MAT-MAGENTA-FALLBACK-VISUAL

# Metrics
duration: 8min
completed: 2026-03-30
---

# Phase 07 Plan 04: Hot-Reload, Validation, and CRUD Mutation API Summary

**Polling file watcher in ContentRegistry hot-reloads modified .material files every 500ms with per-material texture cache invalidation and load-time validation of roughness/metalness ranges**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-03-30T19:49:00Z
- **Completed:** 2026-03-30T19:57:03Z
- **Tasks:** 2
- **Files modified:** 6

## Accomplishments

- ContentRegistry gains `pollMaterialHotReload`, `validateMaterialDefinition`, `addMaterial`, `removeMaterial`
- MaterialTextureLibrary gains `reloadMaterial` which clears old texture set by texture key, erases resolved definition, and immediately re-resolves from updated materials map
- Editor main loop calls `pollMaterialHotReload` once per frame — editing a .material file externally triggers reload within 500ms

## Task Commits

1. **Task 1: File watcher, CRUD, validation, per-material cache invalidation** - `446da25` (feat)
2. **Task 2: Wire hot-reload polling into editor main loop** - `e0eac3d` (feat)

## Files Created/Modified

- `src/game/content/ContentRegistry.h` — Added `<chrono>`, `<filesystem>`, forward decl for MaterialTextureLibrary, new public methods (pollMaterialHotReload, validateMaterialDefinition, addMaterial, removeMaterial), new private members (materialFileTimes_, materialFilePathById_, lastMaterialPoll_)
- `src/game/content/ContentRegistry.cpp` — Added MaterialTextureLibrary include; updated loadMaterialsFromDirectory to track file paths/times; implemented pollMaterialHotReload, validateMaterialDefinition, addMaterial, removeMaterial; updated validateMaterialInheritance to call validateMaterialDefinition per material
- `src/game/rendering/MaterialTextureLibrary.h` — Added `reloadMaterial(const std::string& materialId, const std::unordered_map<std::string, MaterialDefinition>& materials)` declaration
- `src/game/rendering/MaterialTextureLibrary.cpp` — Implemented reloadMaterial: erase old texture set by computed key, erase resolved definition, re-resolve from updated materials map
- `apps/level_editor/main.cpp` — Added `content.pollMaterialHotReload(materialTextures)` call at start of renderFrame lambda
- `tests/game/CMakeLists.txt` — Changed test_content_registry link library from game_content to game_rendering (now required for reloadMaterial symbol)

## Decisions Made

- `reloadMaterial` takes the materials map as a second parameter (not just materialId) so it can immediately re-resolve after clearing the cache, avoiding a one-frame magenta flash on hot-reload.
- `test_content_registry` now links `game_rendering` instead of `game_content` because ContentRegistry.cpp includes MaterialTextureLibrary.h. Since `game_rendering` transitively links `game_content`, all test assertions still work.
- `pollMaterialHotReload` is not `const` — it modifies `materials_`, `materialFileTimes_`, and `lastMaterialPoll_`.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] Corrected textureSets_ erase key in reloadMaterial**
- **Found during:** Task 1 (reloadMaterial implementation)
- **Issue:** Plan's pseudocode called `textureSets_.erase(materialId)` but `textureSets_` is keyed by `textureKeyFor()` hash, not by materialId. Erasing by materialId would silently do nothing, leaving stale textures cached.
- **Fix:** Compute `textureKeyFor(resolvedIt->second)` before erasing the resolved definition, then erase that key from `textureSets_`.
- **Files modified:** src/game/rendering/MaterialTextureLibrary.cpp
- **Verification:** Build passes; logic confirmed by reading ensureTextureSet which uses textureKeyFor as map key.
- **Committed in:** 446da25 (Task 1 commit)

**2. [Rule 1 - Bug] Added re-resolution after cache clear in reloadMaterial**
- **Found during:** Task 1 (reloadMaterial design review)
- **Issue:** Plan said "next resolve() call re-resolves from updated ContentRegistry" but `resolve()` calls `definitionFor()` which looks up `resolvedDefinitions_`. After erasing from that map, it returns nullptr and shows magenta fallback until the next full `init()` rebuild.
- **Fix:** Extended `reloadMaterial` to accept the materials map and immediately call `resolveMaterialDefinition(materialId, materials)` to re-populate `resolvedDefinitions_` after clearing the stale entry.
- **Files modified:** src/game/rendering/MaterialTextureLibrary.h, src/game/rendering/MaterialTextureLibrary.cpp, src/game/content/ContentRegistry.cpp (updated call site)
- **Verification:** Signature matches design intent; build passes.
- **Committed in:** 446da25 (Task 1 commit)

**3. [Rule 3 - Blocking] Updated test_content_registry link library**
- **Found during:** Task 1 build (link error)
- **Issue:** test_content_registry linked only game_content but ContentRegistry.cpp now references MaterialTextureLibrary symbols in game_rendering, causing undefined symbol linker error.
- **Fix:** Changed LIBRARIES from `game_content` to `game_rendering` in tests/game/CMakeLists.txt. game_rendering PUBLIC-links game_content transitively.
- **Files modified:** tests/game/CMakeLists.txt
- **Verification:** cmake --build build succeeds with all targets including test_content_registry.
- **Committed in:** 446da25 (Task 1 commit)

---

**Total deviations:** 3 auto-fixed (2 bugs, 1 blocking)
**Impact on plan:** All three fixes were necessary for correctness. The texture key fix prevents silent cache-miss bugs. The re-resolution fix prevents the magenta flash. The CMake fix prevents a linker error. No scope creep.

## Issues Encountered

None beyond the deviations documented above.

## Next Phase Readiness

- Hot-reload is active in the editor — editing any .material file in assets/materials/ will reload within 500ms
- Plan 03 (asset browser) can now call `addMaterial()` and `removeMaterial()` for CRUD operations
- Validation runs at load time and hot-reload time — invalid materials emit errors but don't crash
- The magenta fallback path in `MaterialTextureLibrary::resolve()` is already wired from Plan 02

---
*Phase: 07-data-driven-material-system-replace-hardcoded-materials-with-a-proper-material-pipeline*
*Completed: 2026-03-30*
