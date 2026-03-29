---
phase: 04-make-engine-fully-generic
verified: 2026-03-29T18:30:00Z
status: gaps_found
score: 5/6 requirements verified
gaps:
  - truth: "REQUIREMENTS.md accurately reflects the implementation state of ENG-BOUNDARY-03 and ENG-BOUNDARY-04"
    status: failed
    reason: "Both requirements are implemented in code but REQUIREMENTS.md still marks them [ ] (Pending). This is a documentation gap, not a code gap."
    artifacts:
      - path: ".planning/REQUIREMENTS.md"
        issue: "Lines 55-56 show [ ] for ENG-BOUNDARY-03 and ENG-BOUNDARY-04; code has both fully implemented"
    missing:
      - "Update REQUIREMENTS.md lines 55-56 from [ ] to [x]"
      - "Update REQUIREMENTS.md traceability table lines 133-134 from Pending to Complete"
human_verification:
  - test: "Confirm torch sliders in debug overlay are acceptable engine-generic scope"
    expected: "DebugParams.playerTorchInnerConeDegrees / playerTorchOuterConeDegrees in ImGuiLayer.h are either removed or moved to game layer"
    why_human: "The SUMMARY acknowledged these as orphaned sliders deferred to a future plan. Whether the deferral is acceptable or must be resolved now is a product decision."
---

# Phase 04: Make Engine Fully Generic — Verification Report

**Phase Goal:** The engine layer compiles with zero game-layer imports — Renderer.h has no MaterialKind, InputSystem.h has no RuntimeInputState, RuntimeSceneRenderer has no hardcoded torch constants, and ImGuiLayer.h has no game-specific overlay methods. The engine is reusable for any 3D project.
**Verified:** 2026-03-29T18:30:00Z
**Status:** gaps_found
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | Renderer.h has no MaterialKind include and no MaterialKind field | VERIFIED | `int shadingModelIndex = 0` confirmed; zero `game/` includes in engine rendering headers |
| 2 | RenderMaterialData uses opaque int — game layer casts MaterialKind to int at boundary | VERIFIED | `MaterialTextureLibrary.cpp:139: renderMaterial.shadingModelIndex = static_cast<int>(resolved.shadingModel)` |
| 3 | InputSystem.h has no RuntimeInputState include and no RuntimeInputState member | VERIFIED | No `game/` includes in any engine input file; InputSystem.h has `ActionMap& actionMap()` and raw state arrays |
| 4 | engine_input CMake target has zero game-layer source files | VERIFIED | engine_input = `InputSystem.cpp` + `ActionMap.cpp` only; `RuntimeInputState.cpp` in gameplay target |
| 5 | ActionMap with bind/isPressed/isJustPressed API exists in engine layer | VERIFIED | `src/engine/input/ActionMap.h` and `.cpp` fully implemented; `ActionMap::update` called from `InputSystem::update` each frame |
| 6 | Game registers named actions via actionMap().bind() | VERIFIED | `RuntimeGameSession.cpp:67-79`: `registerGameActions()` called in constructor; binds move_forward, move_backward, strafe_left, strafe_right, sprint, jump, interact, inventory, attack, screenshot |
| 7 | RuntimeSceneRenderer::collectLights has no hardcoded player torch constants | VERIFIED | No `kPlayerTorch*` in RuntimeSceneRenderer.cpp; collectLights reads `PlayerTorchComponent.computedLights` |
| 8 | ImGuiLayer.h has no game-specific forward declarations or static overlay methods | MOSTLY VERIFIED | No game-type forward decls, no `renderMovementOverlay`/`renderInventory` methods — BUT `DebugParams` struct retains `playerTorchInnerConeDegrees` / `playerTorchOuterConeDegrees` fields (see Anti-Patterns) |
| 9 | REQUIREMENTS.md reflects completed state for all 6 ENG-BOUNDARY requirements | FAILED | ENG-BOUNDARY-03 and ENG-BOUNDARY-04 still marked `[ ]` (Pending) in REQUIREMENTS.md despite both being implemented |

**Score:** 5/6 requirements verified in code. 1 documentation gap.

---

## Required Artifacts

### Plan 04-01 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/engine/rendering/geometry/Renderer.h` | `int shadingModelIndex` instead of `MaterialKind` | VERIFIED | Line 15: `int shadingModelIndex = 0;` — zero game-layer includes |
| `src/engine/rendering/geometry/Renderer.cpp` | `shader_->setInt("uMaterialKind", material.shadingModelIndex)` | VERIFIED | Line 101 confirmed |
| `src/game/rendering/MaterialTextureLibrary.cpp` | `shadingModelIndex = static_cast<int>(resolved.shadingModel)` | VERIFIED | Line 139 confirmed |
| `src/game/components/MeshComponent.h` | Only `materialId` string, no `MaterialKind` field | VERIFIED | 6-field struct with `std::string materialId;` — no MaterialKind import |

### Plan 04-02 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/engine/input/ActionMap.h` | `class ActionMap` with bind/isPressed/isJustPressed | VERIFIED | Full API: `bind`, `unbind`, `isPressed`, `isJustPressed`, `isJustReleased`, `hasAction`, `update` |
| `src/engine/input/ActionMap.cpp` | `ActionMap::update` implementation | VERIFIED | Correct update logic: swaps previousState_, iterates bindings, checks currentKeys/currentButtons |
| `src/engine/input/InputSystem.h` | No game-layer imports, `ActionMap& actionMap()` accessor | VERIFIED | Raw state arrays, `actionMap_` member, `ActionMap& actionMap()` — zero game-layer includes |
| `src/engine/input/InputSystem.cpp` | `actionMap_.update(...)` called each frame | VERIFIED | Wired via grepcount=1 |
| `src/engine/CMakeLists.txt` | engine_input = `InputSystem.cpp` + `ActionMap.cpp` only | VERIFIED | Lines 46-49 confirmed; no game source files |
| `src/game/CMakeLists.txt` | gameplay target contains `RuntimeInputState.cpp` | VERIFIED | Line 49 confirmed |

### Plan 04-03 Artifacts

| Artifact | Expected | Status | Details |
|----------|----------|--------|---------|
| `src/game/components/PlayerTorchComponent.h` | POD struct with torch visual params and `computedLights` | VERIFIED | Full struct with all k* constants as fields; `std::vector<RenderLight> computedLights` |
| `src/game/systems/PlayerTorchSystem.h` | `updatePlayerTorch` free function declaration | VERIFIED | Minimal header; follows free-function game system pattern |
| `src/game/systems/PlayerTorchSystem.cpp` | `playerTorchVisualFlicker` and 4-light computation | VERIFIED | Anonymous namespace with `playerTorchVisualFlicker`, `playerTorchLightFlicker`, `clampInnerCone`, `clampOuterCone` |
| `src/game/ui/GameOverlays.h` | `renderMovementOverlay` and other overlay decls in GameOverlays namespace | VERIFIED | All 4 functions declared in `namespace GameOverlays` |
| `src/game/ui/GameOverlays.cpp` | Overlay implementations | VERIFIED | Full implementations present with correct game-layer includes |
| `src/engine/ui/ImGuiLayer.h` | Clean engine-only lifecycle (no game forward decls, no game overlay methods) | PARTIAL | Game forward decls removed, overlay methods removed — BUT `DebugParams` has `playerTorchInnerConeDegrees` / `playerTorchOuterConeDegrees` (orphaned, non-functional) |

---

## Key Link Verification

### Plan 04-01 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `MaterialTextureLibrary.cpp` | `Renderer.h` | `shadingModelIndex = static_cast<int>` | WIRED | Line 139 confirmed |
| `Renderer.cpp` | `scene.frag` | `setInt("uMaterialKind", material.shadingModelIndex)` | WIRED | Line 101 confirmed |

### Plan 04-02 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `InputSystem.cpp` | `ActionMap.h` | `actionMap_.update(...)` each frame | WIRED | grep count=1 confirmed |
| `RuntimeGameSession.cpp` | `ActionMap.h` | `input.actionMap().bind(...)` in constructor | WIRED | Lines 67-79, all 10 actions registered |
| Game systems | `InputSystem.h` | Direct `input_.isKey*()` / `input_.mouse*()` calls | WIRED | Zero `.state()` calls in `src/game/systems/` |

### Plan 04-03 Key Links

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| `PlayerTorchSystem.cpp` | `PlayerTorchComponent.h` | `updatePlayerTorch` reads/writes `PlayerTorchComponent` | WIRED | Included and queried |
| `RuntimeSceneRenderer.cpp` | `PlayerTorchComponent.h` | `collectLights` reads `PlayerTorchComponent.computedLights` | WIRED | Lines 9, 176 confirmed |
| `RuntimeGameSession.cpp` | `PlayerTorchSystem.h` | `tick()` calls `updatePlayerTorch` after `updateRuntimeCamera` | WIRED | Line 218 confirmed; execution order correct |
| `RenderSystem.cpp` | `GameOverlays.h` | `GameOverlays::render*` instead of `ImGuiLayer::render*` | WIRED | Only `ImGuiLayer::renderOverlay` (engine-generic debug) remains |

---

## Data-Flow Trace (Level 4)

| Artifact | Data Variable | Source | Produces Real Data | Status |
|----------|---------------|--------|--------------------|--------|
| `RuntimeSceneRenderer.cpp::collectLights` | `torch.computedLights` | `PlayerTorchSystem.cpp::updatePlayerTorch` — writes 4 `RenderLight` objects per frame based on camera transform | Yes — 4-light computation with flicker math | FLOWING |
| `Renderer.cpp::drawScene` | `material.shadingModelIndex` | `MaterialTextureLibrary.cpp::resolve()` — casts `MaterialKind` enum to int | Yes — enum cast produces valid int 0-7 | FLOWING |
| `InputSystem` action state | `actionMap_.currentState_` | GLFW key/button state arrays polled each frame in `InputSystem::update()` | Yes — real hardware polling | FLOWING |

---

## Behavioral Spot-Checks

Step 7b SKIPPED — no runnable server entry point can be invoked without starting the application. The engine targets are static libraries; behavior is verified structurally above.

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|---------|
| ENG-BOUNDARY-01 | 04-01 | Engine rendering layer compiles with zero game-layer includes | SATISFIED | `grep -r "game/" src/engine/rendering/` returns zero results |
| ENG-BOUNDARY-02 | 04-01 | RenderMaterialData uses opaque int instead of MaterialKind enum | SATISFIED | `int shadingModelIndex = 0` in Renderer.h; cast in MaterialTextureLibrary.cpp:139 |
| ENG-BOUNDARY-03 | 04-02 | Engine provides generic action mapping system for input bindings | SATISFIED (code) / DOCUMENTATION GAP | ActionMap.h/cpp exist; 10 named actions registered in RuntimeGameSession constructor — but REQUIREMENTS.md marks `[ ]` |
| ENG-BOUNDARY-04 | 04-02 | Engine input layer compiles with zero game-layer includes | SATISFIED (code) / DOCUMENTATION GAP | engine_input = InputSystem.cpp + ActionMap.cpp only; zero game/ imports in engine/input/ — but REQUIREMENTS.md marks `[ ]` |
| ENG-BOUNDARY-05 | 04-03 | RuntimeSceneRenderer has no hardcoded game-specific constants | SATISFIED | Zero `kPlayerTorch*` in RuntimeSceneRenderer.cpp; all torch logic in PlayerTorchComponent + PlayerTorchSystem |
| ENG-BOUNDARY-06 | 04-03 | ImGuiLayer has no game-specific forward declarations or methods | PARTIALLY SATISFIED | Forward decls and overlay methods removed — but `DebugParams` retains `playerTorchInnerConeDegrees` / `playerTorchOuterConeDegrees` fields rendered by ImGuiLayer.cpp (orphaned, acknowledged deferred) |

**Note on REQUIREMENTS.md state:** The SUMMARY for plan 04-02 claimed completion but the REQUIREMENTS.md file was not updated. The code satisfies both ENG-BOUNDARY-03 and ENG-BOUNDARY-04 fully. The `[ ]` markers in REQUIREMENTS.md are stale documentation.

---

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `src/engine/ui/ImGuiLayer.h` | 65-66 | `playerTorchInnerConeDegrees` / `playerTorchOuterConeDegrees` fields in engine `DebugParams` struct — game-specific torch cone concepts in engine header | Warning | Fields are rendered by `ImGuiLayer.cpp:367-368` as "Torch Inner Cone" / "Torch Outer Cone" sliders but have no consumers — `collectLights` no longer reads them (disconnected). The engine header carries game-specific nomenclature. SUMMARY acknowledged these as "orphaned sliders, will be cleaned up in a future plan." |
| `src/engine/physics/PhysicsSystem.cpp` | 6-8 | Includes `game/components/StaticColliderComponent.h`, `CharacterControllerComponent.h`, `TransformComponent.h` | Info | Pre-existing, explicitly called out as "Clean (no changes needed)" in 04-CONTEXT.md. Out of scope for this phase. |
| `.planning/REQUIREMENTS.md` | 55-56, 133-134 | ENG-BOUNDARY-03 and ENG-BOUNDARY-04 marked `[ ]` (Pending) despite both being fully implemented | Warning | Stale documentation creates false impression that core deliverables of this phase remain unfinished. |

---

## Human Verification Required

### 1. Torch Cone Sliders in DebugParams

**Test:** Open the debug overlay in-game. Navigate to the Lighting section. Look for "Torch Inner Cone" and "Torch Outer Cone" sliders.
**Expected:** Sliders appear but adjusting them has no visible effect (they are disconnected from `PlayerTorchComponent.innerConeDegrees`). Confirm whether these should be wired to the component or removed from the engine header.
**Why human:** The functional impact (disconnected vs wired) and whether to resolve now vs defer requires a product decision. The code compiles and the engine goal is substantially achieved.

---

## Gaps Summary

The phase goal is achieved at the code level: all four named violations in the goal statement are resolved. Renderer.h has no MaterialKind, InputSystem.h has no RuntimeInputState, RuntimeSceneRenderer has no hardcoded torch constants, and ImGuiLayer.h has no game-specific overlay methods.

Two documentation gaps were found:

1. **REQUIREMENTS.md not updated for ENG-BOUNDARY-03 and ENG-BOUNDARY-04.** Both requirements are fully implemented in code — ActionMap exists, named actions are registered, engine_input has zero game-layer sources — but REQUIREMENTS.md lines 55-56 still show `[ ]` and the traceability table shows "Pending." This is a one-line fix per requirement (change `[ ]` to `[x]` and "Pending" to "Complete").

2. **Orphaned torch cone fields in engine DebugParams.** `playerTorchInnerConeDegrees` and `playerTorchOuterConeDegrees` remain in `ImGuiLayer.h`'s `DebugParams` struct and are rendered as debug sliders in `ImGuiLayer.cpp`, but they are disconnected from `PlayerTorchComponent` — adjusting them has no effect. The SUMMARY for 04-03 acknowledged this as a deferred cleanup. This does not block the phase goal per the acceptance criteria (which required removing forward declarations and static methods, not DebugParams fields).

Neither gap blocks the phase goal at the code level. The phase objective — engine is reusable for any 3D project with the four named boundary violations resolved — is achieved.

---

_Verified: 2026-03-29T18:30:00Z_
_Verifier: Claude (gsd-verifier)_
