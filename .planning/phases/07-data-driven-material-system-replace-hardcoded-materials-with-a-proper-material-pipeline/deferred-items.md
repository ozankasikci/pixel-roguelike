# Deferred Items

## Pre-existing Issues (not introduced by this plan)

### test_content_registry failure — missing environment definitions

**Status:** Pre-existing failure, confirmed failing before Plan 01 changes.
**Test:** `tests/game/test_content_registry.cpp:87` — `registry.findEnvironment("neutral") != nullptr`
**Root cause:** The test expects environment definition files (`neutral.environment`, `cloister_daylight.environment`, `game_ready_neutral.environment`) in `assets/defs/environments/` but only `default.environment` exists on disk.
**Impact:** `test_content_registry` does not pass. This is out of scope for Plan 01.
**Suggested fix:** Either create the missing environment files, or update the test to reflect what environments actually exist.
