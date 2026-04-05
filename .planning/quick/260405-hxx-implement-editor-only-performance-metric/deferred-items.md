# Deferred Items — 260405-hxx

## Pre-existing issues discovered but out of scope

### procedural-model-viewer build failure
- **File:** apps/model_viewer/main.cpp:6
- **Error:** `fatal error: 'game/levels/GameAssets.h' file not found`
- **Status:** Pre-existing before this plan — verified by stashing changes and re-running the build
- **Impact:** procedural-model-viewer cannot be built; pixel-roguelike and level-editor are unaffected
- **Action needed:** Find or restore game/levels/GameAssets.h or update the include path in model_viewer/main.cpp
