# Phase 04: Make Engine Fully Generic — Research

**Researched:** 2026-03-29
**Domain:** C++ engine/game boundary decoupling, action mapping, shader material indirection
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Engine uses string-based material IDs instead of the game's MaterialKind enum. The engine's Renderer passes material ID strings — game registers material shaders/params by ID. No engine changes needed when adding new material types.
- **D-02:** Remove MaterialKind import from `src/engine/rendering/geometry/Renderer.h`. Replace RenderMaterialData's MaterialKind field with a std::string materialId.
- **D-03:** Engine provides an action mapping system (like Unity's Input System / Godot's InputMap). Game registers named actions ("move_forward", "attack", "interact") bound to keys/buttons. Engine delivers action state.
- **D-04:** Remove RuntimeInputState import from `src/engine/input/InputSystem.h`. InputSystem exposes generic action state, game builds its own RuntimeInputState from action queries.
- **D-05:** ContentRegistry is a game-only concern. It stays in `src/game/` and is NOT part of the engine.
- **D-06:** No generic asset registry in the engine.
- **D-07:** Editor stays game-aware — linked directly to the gameplay library. No plugin system needed.
- **D-08:** No structural change to editor's game-awareness.

### Claude's Discretion

- Move hardcoded player torch logic from RuntimeSceneRenderer to a game system
- Move CathedralAssets procedural geometry to game layer if not already there
- Clean up any other game-specific forward declarations that leaked into engine headers
- Determine the exact API surface for the action mapping system

### Deferred Ideas (OUT OF SCOPE)

- Generic editor plugin system (if engine is ever reused for a different project)
- Engine-level typed asset registry (if ContentRegistry pattern proves insufficient)
- Data-driven material shader pipeline (beyond string IDs)
</user_constraints>

---

## Summary

Phase 4 is a surgical refactor of two engine headers and one game renderer, plus a structural cleanup of one CMakeLists.txt. The engine is already ~80% clean — only four concrete violations exist and all are well-understood from the codebase scout. The scripting pipeline (TypeScript → QuickJS) is already fully implemented on the `codex/scripting-v1` branch; this phase is about completing the C++ boundary cleanup that makes the scripting work coherent.

The two hardest sub-problems are: (1) replacing `MaterialKind` (an enum) with a `std::string` field in `RenderMaterialData`, which ripples through ~21 files; and (2) introducing a generic `ActionMap` inside `InputSystem` that replaces the direct `RuntimeInputState` dependency, which touches ~24 files. Both are tractable because the changes are mechanical substitutions, not architectural rewrites.

The player torch logic in `RuntimeSceneRenderer::collectLights` is a pure move — it belongs in a game system and the game already has `LightComponent` + the ECS pattern to support it. Once moved, `RuntimeSceneRenderer` becomes fully generic.

**Primary recommendation:** Execute as three sequential waves: (1) material string ID decoupling, (2) action mapping and input decoupling, (3) torch extraction + ImGuiLayer forward declaration cleanup. Commit each wave separately so failures are isolated.

---

## Standard Stack

### Core (Already Present — No New Installs)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++20 STL (`std::string`, `std::unordered_map`) | C++20 | Replace enum with string-keyed material ID | No dependency; C++20 required by EnTT anyway |
| EnTT v3.16.0 | v3.16.0 | ECS component queries for torch system extraction | Already in project |
| GLFW 3.4 | 3.4 | Key/mouse constants for action binding (`GLFW_KEY_W`, `GLFW_MOUSE_BUTTON_LEFT`) | Already in project |
| QuickJS | vendored | JS runtime driving the scripting system (already wired) | Already in project |
| esbuild + TypeScript | esbuild ^0.25, TS ^5.9 | Script build pipeline (already in `tools/script_pipeline/`) | Already in project |

No new packages to install. This is a refactor phase — all dependencies already exist.

---

## Architecture Patterns

### Pattern 1: String Material ID in RenderMaterialData

**What:** Replace `MaterialKind shadingModel` field with `std::string materialId` in `RenderMaterialData` (engine header). The shader uniform `uMaterialKind` receives an integer — the game resolves the string to an integer index before passing to `Renderer`.

**The key insight:** `Renderer.cpp` line 101 calls `shader_->setInt("uMaterialKind", static_cast<int>(material.shadingModel))`. The shader expects an int. After decoupling, the game-layer `MaterialTextureLibrary::resolve()` maps `materialId` string → `MaterialKind` integer, packs the integer into `RenderMaterialData` under a new `int shadingModelIndex` field (or keeps the integer as an opaque int). The engine never sees `MaterialKind`.

**Migration path for `RenderMaterialData`:**

```cpp
// BEFORE (engine/rendering/geometry/Renderer.h):
#include "game/rendering/MaterialKind.h"
struct RenderMaterialData {
    std::string id;
    MaterialKind shadingModel = MaterialKind::Stone;
    // ...
};

// AFTER (engine/rendering/geometry/Renderer.h):
// No MaterialKind include
struct RenderMaterialData {
    std::string id;
    int shadingModelIndex = 0;  // opaque int — game sets this from its own MaterialKind
    // ...
};
```

**All callers that construct `RenderMaterialData` live in the game layer** (`MaterialTextureLibrary::resolve()`), so the integer assignment stays in game code. The engine only sees the `int`.

**Impact files (confirmed):** `src/engine/rendering/geometry/Renderer.h`, `src/engine/rendering/geometry/Renderer.cpp`, and every file that constructs `RenderMaterialData` — primarily `src/game/rendering/MaterialTextureLibrary.cpp`.

### Pattern 2: Action Mapping in InputSystem

**What:** Add an `ActionMap` (name → key/button binding) to `InputSystem`. Expose `isActionPressed(std::string_view)` / `isActionJustPressed(std::string_view)` / `getActionAxis(std::string_view)` on `InputSystem`. Remove the `state_` member of type `RuntimeInputState` from `InputSystem`.

**Current situation:** `InputSystem` owns a `RuntimeInputState state_` member and directly compiles `RuntimeInputState.cpp` as part of `engine_input` (visible in `src/engine/CMakeLists.txt` line 48: `${CMAKE_SOURCE_DIR}/src/game/runtime/RuntimeInputState.cpp`). This is the core violation — the build system exposes it clearly.

**Recommended ActionMap API surface:**

```cpp
// engine/input/ActionMap.h  (new file)
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct ActionBinding {
    std::vector<int> keys;           // GLFW_KEY_* constants
    std::vector<int> mouseButtons;   // GLFW_MOUSE_BUTTON_* constants
};

class ActionMap {
public:
    void bind(std::string_view action, ActionBinding binding);
    bool isPressed(std::string_view action) const;
    bool isJustPressed(std::string_view action) const;
    // internal: updated by InputSystem each frame
    void update(/* raw key/button state */);
private:
    std::unordered_map<std::string, ActionBinding> bindings_;
    std::unordered_map<std::string, bool> currentState_;
    std::unordered_map<std::string, bool> previousState_;
};
```

**InputSystem after decoupling:**
- Removes `#include "game/runtime/RuntimeInputState.h"` from `InputSystem.h`
- Removes `state_` member; `RuntimeInputState` moves back to game-only territory
- Adds `ActionMap& actionMap()` accessor
- Game layer registers actions in `RuntimeGameSession::init()` or a dedicated `InputBindingRegistry`

**CMakeLists.txt fix:** Remove line `${CMAKE_SOURCE_DIR}/src/game/runtime/RuntimeInputState.cpp` from `engine_input` sources. `RuntimeInputState` compiles as part of `gameplay` only.

**RuntimeInputState adoption:** All game systems that currently take `RuntimeInputState&` (CameraSystem, PlayerMovementSystem, RenderSystem, InteractionSystem, InventorySystem, DoorSystem, CheckpointSystem) continue to use `RuntimeInputState` — but the game layer constructs it from `ActionMap` queries each frame rather than the `InputSystem` owning it directly.

### Pattern 3: Player Torch Extraction

**What:** The torch constants and flicker logic in `RuntimeSceneRenderer::collectLights()` (lines 24–273 of `RuntimeSceneRenderer.cpp`) belong in a game system. Move to a dedicated `PlayerTorchSystem` or integrate into `PlayerMovementSystem`.

**Recommended approach:** Add a `PlayerTorchComponent` POD struct to `src/game/components/` with fields for all current `k*` constants and flicker state. A `PlayerTorchSystem` runs each frame, computes the 4 `RenderLight` objects and stores them in the component. `RuntimeSceneRenderer::collectLights()` then reads `PlayerTorchComponent` the same way it reads `LightComponent` — no special-casing.

This is the cleanest because:
1. Torch parameters become inspector-editable in the editor
2. `RuntimeSceneRenderer` becomes a generic "collect all lights from ECS" loop
3. Torch flicker logic is testable independently

**Alternative (simpler):** Move the torch logic to `RuntimeGameplay` as a free function called from `RenderSystem` before `collectLights`. Less clean but fewer files touched.

### Pattern 4: ImGuiLayer Forward Declaration Cleanup

**What:** `src/engine/ui/ImGuiLayer.h` has game-specific forward declarations for `PlayerMovementComponent`, `ViewmodelComponent`, `ContentRegistry`, `EffectiveEquipmentView`, `InventoryMenuState`, `RunSession` and exposes game-specific static methods (`renderMovementOverlay`, `renderViewmodelOverlay`, `renderInventory`).

**Decision D-07 says:** Editor stays game-aware. This means the game-specific methods on `ImGuiLayer` may be acceptable. However, the engine header is still polluted.

**Recommended approach:** Split `ImGuiLayer` into:
- `engine/ui/ImGuiLayer.h` — init/shutdown/beginFrame/endFrame only (engine layer)
- `game/ui/GameImGuiLayer.h` — game-specific overlay methods (game layer)

This is **Claude's discretion** territory. If the split creates too much churn, forward declarations in the header are acceptable per D-07 (editor is game-aware). The planner should assess effort vs benefit.

### Recommended Project Structure After Phase 4

```
src/engine/
├── input/
│   ├── InputSystem.h          # No RuntimeInputState import
│   ├── InputSystem.cpp        # No RuntimeInputState.cpp compilation
│   └── ActionMap.h            # NEW: generic action binding
├── rendering/geometry/
│   └── Renderer.h             # No MaterialKind import; shadingModelIndex: int
└── ui/
    └── ImGuiLayer.h           # Optionally: engine methods only

src/game/
├── components/
│   └── PlayerTorchComponent.h # NEW: torch constants + flicker state
├── rendering/
│   ├── MaterialKind.h         # Unchanged; game-only
│   └── RuntimeSceneRenderer   # collectLights reads PlayerTorchComponent
└── systems/
    └── PlayerTorchSystem.h    # NEW (or logic in PlayerMovementSystem)
```

### Anti-Patterns to Avoid

- **String materialId in MeshComponent but int in RenderMaterialData:** MeshComponent already has both `MaterialKind material` AND `std::string materialId`. The plan must consistently migrate MeshComponent to drop `MaterialKind material` field — otherwise two truth sources exist. Check all files that set `mesh.material` (the enum field).
- **Partial InputSystem decoupling:** Removing the `#include` but not removing `RuntimeInputState.cpp` from engine_input's CMakeLists.txt source list — the decoupling is incomplete until the CMakeLists change is made.
- **Circular compile order:** `engine_input` currently compiles game code. After the fix, `engine_input` must compile with zero game headers. Verify with a clean CMake configure after the change.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| String → int enum lookup | Custom hash map or if/else chain | `std::unordered_map<std::string, int>` in MaterialTextureLibrary | STL has this; the MaterialTextureLibrary already resolves strings |
| Action mapping storage | Bitmask-based action system | Simple `std::unordered_map<std::string, ActionBinding>` | The game has ~10 actions; no performance case for complex system |
| Torch flicker math | Custom per-game renderer hook | ECS PlayerTorchComponent + generic light collection | Already have LightComponent + collectLights pattern |

---

## Common Pitfalls

### Pitfall 1: MeshComponent has Two Material Fields

**What goes wrong:** `MeshComponent` has both `MaterialKind material = MaterialKind::Stone` AND `std::string materialId`. After removing `MaterialKind` from the engine, `MeshComponent` still imports `MaterialKind` from game — but `MeshComponent` is used throughout the engine/editor. This creates a new violation.

**Why it happens:** The migration removes the engine-level `MaterialKind` dependency but leaves `MeshComponent` polluted.

**How to avoid:** `MeshComponent` must also be decoupled — either drop the `MaterialKind material` field entirely (use `materialId` string only) or move `MeshComponent` to fully game-layer. Since `MeshComponent` is a game-layer component already (`src/game/components/MeshComponent.h`), dropping the `MaterialKind material` field is clean. Update all sites that set `mesh.material` to set `mesh.materialId` instead.

**Warning signs:** After the Renderer.h change, if `MeshComponent.h` still has `#include "game/rendering/MaterialKind.h"`, the boundary is still dirty.

### Pitfall 2: engine_input CMakeLists Compiles Game Code

**What goes wrong:** `src/engine/CMakeLists.txt` line 48 explicitly compiles `${CMAKE_SOURCE_DIR}/src/game/runtime/RuntimeInputState.cpp` as part of `engine_input`. This causes a linker error if `RuntimeInputState` is later compiled again in `gameplay`. The opposite risk: if it's only removed from engine_input without being added to gameplay, it goes uncompiled and causes link failures.

**How to avoid:** The CMakeLists change must be atomic — remove from engine_input, add to the appropriate game target (`gameplay` or `game_content`) in the same commit.

### Pitfall 3: Shader Uniform uMaterialKind

**What goes wrong:** `Renderer.cpp` calls `shader_->setInt("uMaterialKind", static_cast<int>(material.shadingModel))`. After renaming `shadingModel` to `shadingModelIndex`, the cast changes from `static_cast<int>(MaterialKind)` to just `material.shadingModelIndex` — a direct int. The shader itself (`assets/shaders/game/scene.frag`) uses `uMaterialKind` as an int; no shader changes are needed. But if the int values change between old `MaterialKind` enum and new game assignment, visual results change.

**How to avoid:** Preserve the same integer values in game code. `MaterialKind::Stone = 0`, `Wood = 1`, etc. — these are the same integers that `MaterialTextureLibrary::resolve()` must write into `shadingModelIndex`.

### Pitfall 4: RuntimeInputState Used in 24 Files

**What goes wrong:** The action mapping migration requires touching 24 files that currently import or reference `RuntimeInputState`. If done incrementally without a clear compile plan, the project won't build mid-migration.

**How to avoid:** Migrate in two stages within the same plan: (a) add `ActionMap` to `InputSystem` alongside `RuntimeInputState` (additive), build succeeds; (b) remove `RuntimeInputState` from `InputSystem`, update all callers. The planner should structure tasks as "add ActionMap" then "remove RuntimeInputState dependency."

### Pitfall 5: Player Torch Removal Breaks Lighting in Play Mode

**What goes wrong:** If the torch constants are removed from `RuntimeSceneRenderer` before `PlayerTorchSystem` writes to a `PlayerTorchComponent`, the player's torch lights simply disappear at runtime.

**How to avoid:** Wire `PlayerTorchSystem` and verify it produces lights before removing the old code. The task must include a runtime check — launch the game and confirm torches appear.

---

## Code Examples

### Example 1: RenderMaterialData After Decoupling

```cpp
// Source: codebase analysis (Renderer.h line 15-32)
// engine/rendering/geometry/Renderer.h — AFTER
struct RenderMaterialData {
    std::string id;
    int shadingModelIndex = 0;   // game sets this; 0 = Stone fallback
    glm::vec3 baseColor{1.0f};
    bool useMaterialMaps = false;
    GLuint albedoTexture = 0;
    GLuint normalTexture = 0;
    GLuint roughnessTexture = 0;
    GLuint aoTexture = 0;
    int uvMode = 0;
    glm::vec2 uvScale{1.0f, 1.0f};
    float normalStrength = 1.0f;
    float roughnessScale = 1.0f;
    float roughnessBias = 0.0f;
    float metalness = 0.0f;
    float aoStrength = 1.0f;
    float lightTintResponse = 0.18f;
};
```

### Example 2: engine_input CMakeLists After Fix

```cmake
# src/engine/CMakeLists.txt — engine_input target AFTER
add_library(engine_input STATIC
    input/InputSystem.cpp
    input/ActionMap.cpp          # new file
    # RuntimeInputState.cpp REMOVED — game layer owns it
)
target_include_directories(engine_input PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_input PUBLIC engine_core imgui glm::glm)
```

### Example 3: ActionMap Registration (Game Side)

```cpp
// game/runtime/RuntimeGameSession.cpp — in init()
// Game registers actions after InputSystem is initialized
auto& input = app.getService<InputSystem>();
input.actionMap().bind("move_forward",   { .keys = {GLFW_KEY_W} });
input.actionMap().bind("move_backward",  { .keys = {GLFW_KEY_S} });
input.actionMap().bind("strafe_left",    { .keys = {GLFW_KEY_A} });
input.actionMap().bind("strafe_right",   { .keys = {GLFW_KEY_D} });
input.actionMap().bind("attack",         { .mouseButtons = {GLFW_MOUSE_BUTTON_LEFT} });
input.actionMap().bind("interact",       { .keys = {GLFW_KEY_E} });
input.actionMap().bind("jump",           { .keys = {GLFW_KEY_SPACE} });
```

### Example 4: PlayerTorchComponent

```cpp
// game/components/PlayerTorchComponent.h — new file
#pragma once
#include <glm/glm.hpp>

struct PlayerTorchComponent {
    // Visual parameters (previously hardcoded k* constants)
    glm::vec3 torchColor{1.00f, 0.89f, 0.76f};
    float torchRadius = 3.1f;
    float torchIntensity = 0.07f;
    float spillRadius = 7.2f;
    float spillIntensity = 1.12f;
    float haloRadius = 5.4f;
    float haloIntensity = 0.54f;
    // Runtime flicker state
    float flickerPhase = 0.0f;
};
```

---

## Codebase Scout: Precise Violation Inventory

Confirmed violations by file (source of truth for task scope):

| File | Violation | Fix |
|------|-----------|-----|
| `src/engine/rendering/geometry/Renderer.h` | `#include "game/rendering/MaterialKind.h"` + `MaterialKind shadingModel` field | Remove include; replace field with `int shadingModelIndex` |
| `src/engine/input/InputSystem.h` | `#include "game/runtime/RuntimeInputState.h"` + `RuntimeInputState state_` member | Remove include + member; add `ActionMap actionMap_` member |
| `src/engine/CMakeLists.txt` line 48 | Compiles `game/runtime/RuntimeInputState.cpp` as part of `engine_input` | Remove that line; add `RuntimeInputState.cpp` to gameplay |
| `src/game/rendering/RuntimeSceneRenderer.cpp` lines 24–273 | Hardcoded torch constants + `playerTorchVisualFlicker` / `playerTorchLightFlicker` functions | Extract to `PlayerTorchComponent` + system |
| `src/game/components/MeshComponent.h` | `#include "game/rendering/MaterialKind.h"` + `MaterialKind material` field | Drop field; `materialId` string is the sole material identifier |
| `src/engine/ui/ImGuiLayer.h` | Forward declarations for game types (`PlayerMovementComponent`, etc.) + game-specific static methods | Acceptable per D-07 (editor is game-aware); optionally split to `game/ui/GameOverlay.h` |

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| MaterialKind enum in engine header | String materialId (D-01/D-02) | This phase | Engine no longer needs game headers to compile |
| RuntimeInputState owned by InputSystem | ActionMap in InputSystem, game owns RuntimeInputState (D-03/D-04) | This phase | engine_input compiles with zero game headers |
| Torch logic hardcoded in renderer | PlayerTorchComponent + system | This phase | RuntimeSceneRenderer becomes a generic ECS-reading renderer |
| RuntimeInputState.cpp compiled in engine_input | Compiled in gameplay only | This phase | Clean CMake dependency graph |

---

## Open Questions

1. **ActionMap: axis queries or boolean only?**
   - What we know: Current `RuntimeInputState` has `mouseDelta()` / `scrollDelta()` which are float axes, not boolean actions.
   - What's unclear: Should `ActionMap` also support float axes ("look_horizontal" → mouse X delta) or just boolean presses?
   - Recommendation: For Phase 4, implement boolean only (covers keyboard/button bindings). Mouse delta continues as a raw `InputSystem::mouseDelta()` call — it's already generic (no game coupling). Document "axis actions" as future work.

2. **MeshComponent field removal: all at once or with deprecation?**
   - What we know: `MeshComponent.material` (the enum field) is set in ~20+ game files.
   - What's unclear: How many editor files use `mesh.material` vs `mesh.materialId`?
   - Recommendation: Audit `grep -r "\.material"` in editor files before the task. The editor may already prefer `materialId`. Drop the field — no deprecation needed since this is a single codebase.

3. **PlayerTorchSystem vs inline in PlayerMovementSystem?**
   - What we know: The torch has 4 lights and flicker state, but no health/stamina dependency.
   - What's unclear: Does torch intensity need to change based on gameplay state (e.g., near fire, depleting)?
   - Recommendation: Start with `PlayerTorchComponent` + extraction to a dedicated `PlayerTorchSystem`. If gameplay coupling becomes necessary later, the component is already there.

---

## Environment Availability

Step 2.6: SKIPPED — Phase 4 is C++ source/header changes and CMakeLists.txt edits. No new external tools, runtimes, or services are required. The script pipeline (Node.js, esbuild, TypeScript) is already installed and confirmed working from the `codex/scripting-v1` branch.

---

## Sources

### Primary (HIGH confidence)
- Codebase direct read — `src/engine/rendering/geometry/Renderer.h` (lines 10-32): MaterialKind violation confirmed
- Codebase direct read — `src/engine/input/InputSystem.h` (lines 3, 39-50): RuntimeInputState violation confirmed
- Codebase direct read — `src/engine/CMakeLists.txt` (line 48): game code compiled in engine target confirmed
- Codebase direct read — `src/game/rendering/RuntimeSceneRenderer.cpp` (lines 24-42, 207-273): torch constants and flicker logic confirmed
- Codebase direct read — `src/game/components/MeshComponent.h`: secondary MaterialKind violation confirmed
- Codebase direct read — `src/engine/ui/ImGuiLayer.h`: game forward declarations confirmed
- 04-CONTEXT.md: All decisions D-01 through D-08 confirmed

### Secondary (MEDIUM confidence)
- CLAUDE.md architecture section: CMake target graph and engine/game/editor layering model
- CLAUDE.md conventions: naming (PascalCase components, camelCase methods, k-prefix constants)

---

## Metadata

**Confidence breakdown:**
- Violation inventory: HIGH — all violations confirmed by direct code read
- Fix patterns: HIGH — patterns follow existing codebase conventions (e.g., MeshComponent already has materialId, MaterialTextureLibrary already resolves strings)
- ActionMap API surface: MEDIUM — exact method signatures left for planner; decision D-03 provides intent but not API
- Torch extraction scope: HIGH — torch code is self-contained in RuntimeSceneRenderer; no hidden dependencies found

**Research date:** 2026-03-29
**Valid until:** 2026-04-29 (stable domain; no external API volatility)
