---
phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity
plan: "04"
subsystem: editor
tags: [cpp, std-visit, variant, compile-time, exhaustiveness, editor-scene]

# Dependency graph
requires:
  - phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity
    provides: EditorSceneDocument with EditorSceneObjectPayload variant type
provides:
  - EditorSceneDocument.cpp with std::visit replacing 5 switch-on-kind statements
  - Compile-time exhaustiveness guards via static_assert(sizeof(T) == 0) in 3 visitors
affects:
  - any future addition of a new type to EditorSceneObjectPayload (compiler will guide)

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "std::visit with if constexpr for variant dispatch instead of switch-on-kind"
    - "static_assert(sizeof(T) == 0, ...) for compile-time exhaustiveness in C++20"
    - "Generic lambda for uniform-field visitors (all types have same field)"

key-files:
  created: []
  modified:
    - src/editor/scene/EditorSceneDocument.cpp

key-decisions:
  - "Use sizeof(T) == 0 static_assert (not static_assert(false)) for C++20 compatibility in uninstantiated branches"
  - "editorSceneObjectKindName() kept as enum switch — dispatches on enum, not variant, std::visit not applicable"
  - "editorSceneObjectLabel LevelPlayerSpawn falls through without suffix (correct — matches original default: break)"
  - "editorSceneObjectAnchor uses generic lambda without if constexpr — all 7 types have position field"

patterns-established:
  - "Variant dispatch pattern: std::visit with if constexpr chains + static_assert exhaustiveness guard"

requirements-completed: []

# Metrics
duration: 3min
completed: 2026-04-05
---

# Phase 18 Plan 04: Visitor Pattern Refactoring Summary

**5 switch-on-kind statements in EditorSceneDocument replaced with std::visit and if constexpr chains, making EditorSceneObjectPayload extension a compile-error-guided process**

## Performance

- **Duration:** 3 min
- **Started:** 2026-04-05T02:15:53Z
- **Completed:** 2026-04-05T02:18:51Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments

- Replaced `applyWorldTransform` switch with `std::visit` and full if constexpr chain with static_assert guard
- Replaced `toLevelDef` switch with `std::visit` and full if constexpr chain with static_assert guard
- Replaced `localTransformMatrix` switch with `std::visit` and full if constexpr chain with static_assert guard
- Replaced `editorSceneObjectLabel` switch with `std::visit` and if constexpr chain (PlayerSpawn falls through — no suffix)
- Replaced `editorSceneObjectAnchor` switch with a single-line generic lambda (all types share `.position` field)
- Added `<type_traits>` include for `std::decay_t` and `std::is_same_v`
- Retained `editorSceneObjectKindName` as enum switch (dispatches on `EditorSceneObjectKind` enum, not the variant)

## Task Commits

1. **Task 1: Replace switch statements with std::visit in EditorSceneDocument.cpp** - `e30269e` (refactor)

**Plan metadata:** (to be committed with docs)

## Files Created/Modified

- `src/editor/scene/EditorSceneDocument.cpp` - 5 switch-on-kind statements replaced with std::visit visitors

## Decisions Made

- `sizeof(T) == 0` used in `static_assert` rather than `static_assert(false)` — the latter is ill-formed even in uninstantiated branches before C++23
- `editorSceneObjectKindName` kept as-is: it dispatches on `EditorSceneObjectKind` enum not on `EditorSceneObjectPayload` variant, so `std::visit` is inapplicable
- `editorSceneObjectAnchor` uses a generic lambda with no `if constexpr` — all 7 payload types have a `.position` field, so a single `return p.position` suffices and adding a static_assert would be unnecessarily verbose
- `editorSceneObjectLabel` has no static_assert — `LevelPlayerSpawn` produces no label suffix intentionally, matching original `default: break` behavior

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None. The `procedural-model-viewer` target had a pre-existing build failure (`game/levels/GameAssets.h` not found) unrelated to this plan — not addressed.

## Known Stubs

None.

## Next Phase Readiness

Adding a new type to `EditorSceneObjectPayload` in `EditorSceneDocument.h` will now produce compile errors in `applyWorldTransform`, `toLevelDef`, and `localTransformMatrix`, guiding the developer to update all handlers. The `editorSceneObjectLabel` and `editorSceneObjectAnchor` visitors will silently accept new types (both have sensible fallback behavior — no suffix and `.position` respectively).

---
*Phase: 18-maintainability-refactoring-for-editor-ui-serialization-and-round-trip-fidelity*
*Completed: 2026-04-05*

## Self-Check: PASSED

- `src/editor/scene/EditorSceneDocument.cpp` modified: FOUND
- Commit `e30269e`: FOUND
- `grep -c "std::visit" src/editor/scene/EditorSceneDocument.cpp` = 9 (4 existing + 5 new)
- `editorSceneObjectKindName` still uses switch: CONFIRMED
- `level-editor` builds: CONFIRMED
- `pixel-roguelike` builds: CONFIRMED
- `test_editor_scene_document` passes (exit 0): CONFIRMED
