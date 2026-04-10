# Player Control Module Design

**Date:** 2026-04-09
**Goal:** Extract player movement + camera into a self-contained module, simplifying RuntimeGameSession orchestration.

## Module Boundary

**Location:** `src/game/modules/player_control/`

### Inside the module
- **Components**: `PlayerMovementComponent`, `CharacterControllerComponent`, `CameraComponent`, `PlayerSpawnComponent`, `PlayerInteractionLockComponent`
- **Tick functions**: `tickPlayerMovement(registry, input, physics, dt)`, `tickPlayerCamera(registry, input, aspect, dt)`
- **Action handlers**: `handleLockPlayer`, `handleUnlockPlayer`, `handleTeleportPlayer` (with physics sync fix)
- **Math**: camera math (buildForward, updateRuntimeCameraComponent, capturePrimaryRuntimeCamera)
- **Spawner**: `spawnPlayerEntity()` — extracted from LevelLoader's hardcoded player creation
- **Serializer**: `player_spawn` keyword parsing/serialization for `.scene` files
- **Registration**: `registerPlayerControlModule()` — registers keyword + action handlers

### Outside the module
- Tags (`PlayerTag`, `ControllableTag`, `PrimaryCameraTag`) → shared `Tags.h`
- `ViewmodelComponent` → stays in components (inventory/render concern)
- `InputSystem`, `PhysicsSystem` → engine services, passed as parameters
- `RunSession` → session persistence, passed to spawner

## File Structure

```
src/game/modules/player_control/
├── PlayerControlModule.h/cpp        — registerPlayerControlModule()
├── PlayerControlComponents.h        — all 5 components in one header
├── PlayerControlActionTypes.h       — PlayerLockParams, TeleportPlayerParams
├── PlayerControlActionHandler.h/cpp — handleLockPlayer, handleUnlockPlayer, handleTeleportPlayer
├── PlayerControlSerializer.h/cpp    — parse/serialize player_spawn keyword
├── PlayerControlSpawner.h/cpp       — spawnPlayerEntity()
├── PlayerControlMovement.h/cpp      — tickPlayerMovement()
├── PlayerControlCamera.h/cpp        — tickPlayerCamera(), camera math
```

## Registration

```cpp
void registerPlayerControlModule() {
    registerLevelDefKeyword("player_spawn", parsePlayerSpawn, serializePlayerSpawn);
    registerBehaviorActionHandler(ActionType::LockPlayer, handleLockPlayer);
    registerBehaviorActionHandler(ActionType::UnlockPlayer, handleUnlockPlayer);
    registerBehaviorActionHandler(ActionType::TeleportPlayer, handleTeleportPlayer);
}
```

## RuntimeGameSession Changes

Call sites change from:
```cpp
updateRuntimePlayerMovement(registry_, inputSystem_, physics_, deltaTime);
updateRuntimeCamera(registry_, inputSystem_, aspect, deltaTime);
```

To:
```cpp
tickPlayerMovement(registry_, inputSystem_, physics_, deltaTime);
tickPlayerCamera(registry_, inputSystem_, aspect, deltaTime);
```

## Deletions and Migrations

### Files deleted
- `src/game/systems/PlayerMovementSystem.h/cpp` — facade, logic moves to module
- `src/game/systems/CameraSystem.h/cpp` — facade, logic moves to module
- `src/game/rendering/RuntimeCameraMath.h/cpp` — absorbed into PlayerControlCamera

### Files modified
- `src/game/components/` — remove individual component files (PlayerMovementComponent.h, CharacterControllerComponent.h, CameraComponent.h, PlayerSpawnComponent.h, PlayerInteractionLockComponent.h); definitions move to PlayerControlComponents.h
- `src/game/components/Tags.h` — new shared header consolidating PlayerTag, ControllableTag, PrimaryCameraTag, AudioListenerTag
- `src/game/runtime/RuntimeGameplay.h/cpp` — remove updateRuntimePlayerMovement() and updateRuntimeCamera()
- `src/game/runtime/RuntimeGameSession.cpp` — change includes, swap function names
- `src/game/level/LevelLoader.cpp` — replace inline player creation with spawnPlayerEntity()
- `src/game/level/LevelDef.cpp` — remove hardcoded player_spawn parser (registered by module)
- `src/game/behavior/BehaviorSystem.cpp` — remove inline LockPlayer/UnlockPlayer/TeleportPlayer cases (registered by module)

## TeleportPlayer Physics Fix

Current code sets `transform.position` but doesn't update the physics character. The module version will sync both:

```cpp
void handleTeleportPlayer(registry, source, target, action) {
    transform.position = params->position;
    physics.setCharacterPosition(entity, params->position - eyeOffset);
    movement.velocity = {0, 0, 0};
}
```

PhysicsSystem accessed via `registry.ctx()` where it's already stored by RuntimeGameSession.

## Build Order

### Step 1: Create shared Tags.h
Consolidate the 4 tag structs (PlayerTag, ControllableTag, PrimaryCameraTag, AudioListenerTag) into `src/game/components/Tags.h`. Update all includes across the codebase. Build + run tests.

### Step 2: Create module skeleton
Create `PlayerControlModule.h/cpp` with empty `registerPlayerControlModule()`. Create `PlayerControlComponents.h` with the 5 components moved over. Update includes across codebase. Build to verify.

### Step 3: Extract tick functions
Move `updateRuntimePlayerMovement` → `tickPlayerMovement` in `PlayerControlMovement.cpp`. Move `updateRuntimeCamera` → `tickPlayerCamera` in `PlayerControlCamera.cpp` (absorbing RuntimeCameraMath). Update RuntimeGameSession and RuntimeGameplay. Delete old System facades. Build + run.

### Step 4: Extract action handlers + spawner + serializer
Move LockPlayer/UnlockPlayer/TeleportPlayer out of BehaviorSystem into PlayerControlActionHandler. Fix TeleportPlayer physics desync. Move player entity creation from LevelLoader into PlayerControlSpawner. Move player_spawn parsing from LevelDef into PlayerControlSerializer. Wire up registerPlayerControlModule(). Build + run.

### Step 5: Clean up
Delete empty/orphaned files. Verify no stale includes. Run full test suite.

## Risk

The biggest risk is **include chain breakage** — many files include PlayerMovementComponent.h or CameraComponent.h directly. Step 2 handles this by updating every include before moving logic.
