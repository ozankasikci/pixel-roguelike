# Quick Task 260404-p8q Summary

**Task:** Safe cleanup of environment shader properties: remove redundant sky sun fields, unify sky enable toggle, stop persisting depth preview scale
**Date:** 2026-04-04
**Code Commit:** `5bad39e`
**Status:** Complete

## What changed

- Removed redundant environment-authored sky sun direction/color fields and replaced them with runtime-only post-process sun inputs synced from `lighting.sun`.
- Collapsed the duplicated sky visibility controls to a single `post.enableSky` property.
- Stopped writing debug-only `depth_view_scale` into `.environment` assets and removed it from environment editing UI.
- Kept legacy parser aliases for `sky_enabled`, `sky_sun_direction`, `sky_sun_color`, and `depth_view_scale` so older environment files still load cleanly.

## Verification

- `cmake --build build --target level-editor test_content_registry test_environment_profiles`
- `./build/tests/game/test_content_registry`
- `./build/tests/game/test_environment_profiles`

## Outcome

- The rendered look is unchanged, but the environment schema is smaller and less misleading.
- Environment assets now serialize only authored look properties, while debug depth preview tuning stays runtime-local.
