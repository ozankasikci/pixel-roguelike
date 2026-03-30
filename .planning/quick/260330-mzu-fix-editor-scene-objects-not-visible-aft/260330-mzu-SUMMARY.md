---
phase: quick
plan: 260330-mzu
subsystem: rendering/post-processing
tags: [environment, post-processing, editor, tone-mapping, bloom, fog, grain]
dependency_graph:
  requires: []
  provides: [default-environment-post-processing]
  affects: [editor-viewport, compositor-pipeline]
tech_stack:
  added: []
  patterns: [environment-definition-flags]
key_files:
  created: []
  modified:
    - assets/defs/environments/default.environment
decisions:
  - "Restore only the four boolean flags; leave all numeric tuning parameters (exposure, bloom_threshold, fog_density, etc.) unchanged as they were intentional adjustments"
metrics:
  duration: "3 minutes"
  completed: "2026-03-30T13:50:37Z"
  tasks_completed: 2
  files_modified: 1
---

# Quick Task 260330-mzu: Fix Editor Scene Objects Not Visible After SSAO Commit Summary

**One-liner:** Restored four post-processing flags (fog, tone_map, bloom, grain) disabled in the SSAO pipeline commit, which caused editor scenes to appear as uniform gray due to untonemapped linear HDR values.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Restore post-processing flags in default.environment | 19068c7 | assets/defs/environments/default.environment |
| 2 | Build and verify editor compiles | (no code change) | — |

## What Was Done

Commit `772df83` (SSAO pipeline wiring) accidentally disabled four post-processing flags in `assets/defs/environments/default.environment`:

- `enable_fog false` → restored to `enable_fog true`
- `enable_tone_map false` → restored to `enable_tone_map true`
- `enable_bloom false` → restored to `enable_bloom true`
- `enable_grain false` → restored to `enable_grain true`

The editor has no player torch lights and relies on hemisphere ambient + directional sun + fill light, producing very dim linear HDR colors (~0.03–0.07). Without tone mapping, these values pass through the composite pipeline nearly unchanged, making everything appear as near-black uniform gray. With tone mapping restored, the HDR values are properly remapped to a visible range.

All numeric tuning parameters (exposure, contrast, saturation, bloom_threshold, bloom_intensity, fog_density, fog_start, grain_amount, vignette_strength, etc.) were left unchanged as they represent intentional visual calibration.

## Verification

```
enable_fog true
enable_tone_map true
enable_bloom true
enable_grain true
```

All 4 flags confirmed `true`. `level-editor` builds cleanly with `[100%] Built target level-editor`.

## Deviations from Plan

None — plan executed exactly as written.

## Self-Check: PASSED

- File modified: `assets/defs/environments/default.environment` — confirmed updated
- Commit `19068c7` — confirmed in git log
- Build: `[100%] Built target level-editor` — no errors
