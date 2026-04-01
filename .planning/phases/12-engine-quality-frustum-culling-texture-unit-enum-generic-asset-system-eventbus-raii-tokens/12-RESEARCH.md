# Phase 12: Engine Quality — Research

**Researched:** 2026-04-01
**Domain:** Custom C++ engine internals — rendering pipeline, ECS, asset system, event bus
**Confidence:** HIGH (all findings from direct codebase inspection; no external library research needed)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** AABB frustum culling only — no material/shader sort batching in this phase
- **D-02:** Culling happens at the `SceneRenderPipeline` level so it applies to all render paths (runtime, editor viewport, model viewer)
- **D-03:** Light frustum culling (skipping lights whose attenuation radius doesn't overlap the frustum) — Claude's discretion
- **D-04:** Replace all magic-number texture unit indices with a named enum in a shared header. Use those names everywhere across Renderer.cpp, SceneRenderPipeline.cpp, SsaoPass.cpp, etc.
- **D-05:** Meshes in `assets/meshes/` should be auto-discovered from the filesystem at startup, like materials already are
- **D-06:** Procedural meshes stay code-generated but move to `ProceduralGameAssets` (renamed from `GameAssets`). These register into `MeshLibrary` alongside file-discovered meshes.
- **D-07:** New assets should be files whenever possible. Procedural generation is reserved for geometry that's cheaper to generate than model.
- **D-08:** No lazy/deferred loading — keep synchronous loading.
- **D-09:** `EventBus::subscribe()` returns an RAII subscription token that unregisters the handler on destruction.
- **D-10:** Decompose the 40-field `DebugParams` struct into `PostProcessParams`, `CameraDebugInfo`, and `RuntimeLightingOverride`.
- **D-11:** Merge the two `LevelLoader::load()` overloads into a single overload that takes an explicit context struct.
- **D-12:** Move the `if (request_.levelId == "institutional_room")` scripted geometry injection out of `GenericFileScene::onEnter()` into per-level callbacks or encode the objects in the `.scene` file.

### Claude's Discretion

- Light frustum culling (D-03) — evaluate whether the win justifies the complexity given the 32-light cap
- Texture/environment unification into the discovery pattern — evaluate whether it makes sense beyond meshes
- Priority ordering across the 7 items — all equal priority, Claude orders by dependency analysis

### Deferred Ideas (OUT OF SCOPE)

- Material/shader sort batching
- Lazy/deferred asset loading
- Scene file versioning
- Component validation / schema enforcement
</user_constraints>

---

## Summary

Phase 12 is a pure internal engine-quality improvement pass — seven tightly scoped improvements with no user-facing behavior changes. All changes are to C++ source files and headers; no `.scene` data migrations, no external dependencies, no shader recompilation required.

The work divides into three dependency tiers: (1) the texture unit enum and the EventBus RAII token are isolated header/class changes with no inter-item dependencies; (2) the GenericFileScene refactor, DebugParams split, and LevelLoader unification each involve one main file plus a handful of call sites; (3) the generic asset system (file-based mesh discovery) has a dependency on the completed `ProceduralGameAssets` rename and provides an updated `registerAllGameAssets()` call that feeds into the scene loading path. Frustum culling is independent of all of the above and can be delivered in any wave.

The `ModelLoader::discoverProjectAssets()` API and `DiscoveredModelAsset` struct already exist and are exercised by `test_model_discovery.cpp`. The material auto-discovery pattern in `ContentRegistry::loadMaterialsFromDirectory()` is the canonical template for the mesh equivalent. The `Mesh::aabbMin()` / `Mesh::aabbMax()` accessors are already present on every mesh. These three facts mean no greenfield infrastructure is needed — this phase is entirely wiring.

**Primary recommendation:** Execute in wave order: (Wave 1) texture unit enum + EventBus RAII token — isolated, zero risk; (Wave 2) DebugParams split + LevelLoader unification + GenericFileScene refactor — structural changes that establish clean interfaces; (Wave 3) ProceduralGameAssets rename + file-based mesh discovery + frustum culling — these touch the broadest call sites and benefit from the clean interfaces established in Wave 2.

---

## Standard Stack

This phase uses no new libraries. All work is within the existing stack.

| Technology | Version | Relevant to Phase |
|------------|---------|-------------------|
| C++20 | C++20 | RAII token can use move-only types and designated initializers |
| EnTT 3.16 | v3.16.0 | ECS views in `collectSceneObjects()` where culling is inserted |
| GLM 1.0.3 | 1.0.3 | Frustum plane extraction from projection matrix (`glm::frustum`-style math) |
| OpenGL 4.1 Core | 4.1 | Texture unit enum maps to `GL_TEXTURE0 + N` |
| `std::filesystem` | C++17 | Already used by `ContentRegistry::loadMaterialsFromDirectory()` — same pattern for mesh discovery |

---

## Architecture Patterns

### Pattern 1: Frustum Culling — AABB vs Six Planes

**What:** Extract six frustum planes from `viewProjection = projection * view`. For each scene object, transform the object's AABB (world-space min/max) and test all eight corners against all six planes. Skip the object if all eight corners are on the negative side of any single plane.

**Where:** Inside `RuntimeSceneRenderer::collectSceneObjects()` at `src/game/rendering/RuntimeSceneRenderer.cpp:124`. The viewProjection matrix must be passed in; `collectSceneObjects()` currently takes only `registry`. Either pass it as a parameter or store it from the camera capture step.

The culling loop structure:
```cpp
// Source: direct codebase analysis — Mesh already exposes aabbMin()/aabbMax()
glm::mat4 vp = camera.projectionMatrix * camera.viewMatrix;
// Extract planes from vp rows (Gribb-Hartmann method — standard OpenGL convention)
// For each RenderObject candidate:
//   Compute 8 AABB corners transformed into clip space by model*vp
//   Test: if all 8 corners outside any single plane -> cull
```

**Gribb-Hartmann plane extraction (HIGH confidence):** The six clip planes are extracted by summing/subtracting rows of the view-projection matrix. Plane normals are not normalized for the inside/outside test — only the sign matters. This is the industry-standard approach used in Hazel, Godot, and documented in GPU Gems 1.

```cpp
// Standard Gribb-Hartmann extraction from column-major row vectors:
// Left:   m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0]
// Right:  m[0][3] - m[0][0], ...
// Bottom: m[0][3] + m[0][1], ...
// Top:    m[0][3] - m[0][1], ...
// Near:   m[0][3] + m[0][2], ...
// Far:    m[0][3] - m[0][2], ...
// (GLM stores column-major, so m[col][row])
```

**D-02 implication:** The decision places culling in `SceneRenderPipeline`, but the `collectSceneObjects()` function is in `RuntimeSceneRenderer` (game layer). The `SceneRenderPipeline` receives a pre-collected `std::vector<RenderObject>`. To satisfy D-02 without violating the engine/game layer boundary, frustum culling should occur in `SceneRenderPipeline::render()` by filtering the received objects vector using the already-available `input.viewMatrix` and `input.projectionMatrix`. This is where `SceneRenderPipeline` can cull before submitting to `renderer_->drawScene()`. The `RenderObject::mesh` already exposes `aabbMin()`/`aabbMax()` but the mesh AABB is in local space — the model matrix must be applied to get world AABB, or conservative world-AABB from the model matrix.

**Pragmatic approach:** Cull in `SceneRenderPipeline::renderScenePass()` immediately before the `renderer_->drawScene()` call. Filter the objects vector by testing the world-space AABB against the frustum planes extracted from `input.projectionMatrix * input.viewMatrix`. Update `lastStats_.drawCalls` to reflect the post-cull count.

**When to use:** Always active; no toggle needed for this phase.

### Pattern 2: Texture Unit Enum

**What:** Define a scoped enum in a shared engine header. Replace every bare integer at `glActiveTexture(GL_TEXTURE0 + N)` and `shader_->setInt("uX", N)` call sites.

**Discovered texture unit assignments (from codebase scan):**

| Unit | Name | Used By |
|------|------|---------|
| 0-7 | Post-process passes (CompositePass, SsaoPass, StylizePass, BloomPass) | Various per-pass textures — these are local to each pass and do NOT conflict with scene shader units |
| 8-15 | Shadow maps 0-7 | `Renderer.cpp:40-44` — `const int textureUnit = 8 + i` |
| 10 | LTC mat | `SceneRenderPipeline.cpp:kLtcMatUnit`, `EditorAssetPreviewRenderer.cpp:kLtcMatUnit` |
| 11 | LTC amp | `SceneRenderPipeline.cpp:kLtcAmpUnit`, `EditorAssetPreviewRenderer.cpp:kLtcAmpUnit` |
| 12 | Albedo map | `Renderer.cpp:46` |
| 13 | Normal map | `Renderer.cpp:47` |
| 14 | Roughness map | `Renderer.cpp:48` |
| 15 | AO map | `Renderer.cpp:49` |
| 16 | CSM shadow map | `SceneRenderPipeline.cpp:kCsmTextureUnit`, `EditorAssetPreviewRenderer.cpp:206` |

**Critical overlap:** Shadow maps 8-15 overlap with LTC mat (10), LTC amp (11), and material maps (12-15). This is intentional and timing-dependent — shadows are bound once per frame in `renderShadowPass()`, then per-object material maps overwrite units 12-15 during `drawScene()`. LTC is bound once in `renderScenePass()` before the draw loop. The enum must encode these overlapping ranges; the comments in the enum are load-bearing documentation.

**Enum placement:** New file `src/engine/rendering/TextureUnits.h`. Include it from `Renderer.h`, `SceneRenderPipeline.h`, and `EditorAssetPreviewRenderer.h`.

**Local post-process pass units:** CompositePass, SsaoPass, StylizePass, and BloomPass each use units 0-9 locally within their render functions. These do not conflict with scene shader units because they run in separate render passes. They can optionally use enum values too if named constants add clarity, but they are not the fragile part — the scene shader overlap at 8-15 is.

### Pattern 3: EventBus RAII Token

**What:** `subscribe()` returns a move-only `SubscriptionToken` that calls an unsubscribe function on destruction.

**Current state of EventBus.h (31 lines):**
```cpp
// Stores handlers in a flat vector per type:
std::unordered_map<std::type_index, std::vector<std::function<void(const std::any&)>>> subscribers_;
```

**Problem:** No handle is returned; no way to unsubscribe. A destroyed subscriber with a captured `this` pointer leaves a dangling closure in the vector.

**Implementation approach:** Change the storage to an indexed map (or vector of `{id, handler}` pairs) so individual entries can be removed by ID. Return a token wrapping the type index and slot ID; the token's destructor calls `eventBus_.unsubscribe(typeIndex, id)`.

```cpp
// Target API — source: design from CONTEXT.md D-09
class EventBus {
public:
    class SubscriptionToken {
    public:
        SubscriptionToken() = default;
        ~SubscriptionToken() { if (bus_) bus_->unsubscribe(key_, id_); }
        // Non-copyable, movable
        SubscriptionToken(const SubscriptionToken&) = delete;
        SubscriptionToken& operator=(const SubscriptionToken&) = delete;
        SubscriptionToken(SubscriptionToken&& other) noexcept;
        SubscriptionToken& operator=(SubscriptionToken&& other) noexcept;
    private:
        friend class EventBus;
        SubscriptionToken(EventBus* bus, std::type_index key, uint64_t id);
        EventBus* bus_ = nullptr;
        std::type_index key_{typeid(void)};
        uint64_t id_ = 0;
    };

    template<typename EventType>
    SubscriptionToken subscribe(std::function<void(const EventType&)> handler);

    template<typename EventType>
    void publish(const EventType& event);

private:
    void unsubscribe(std::type_index key, uint64_t id);
    uint64_t nextId_ = 0;
    std::unordered_map<std::type_index, std::vector<std::pair<uint64_t, std::function<void(const std::any&)>>>> subscribers_;
};
```

**Callers:** Grep for `eventBus().subscribe` or `eventBus_.subscribe` to find all current call sites that need to capture the returned token. Since no existing caller captures a return value (the current API returns void), this is a purely additive change — existing callers will compile without modification (the token is constructed and immediately destroyed, which is a no-op unsubscribe since the handler was just registered then immediately removed — see **Pitfall 3** below).

**Pitfall:** Callers that do NOT store the token will immediately destroy it and unsubscribe. This is the correct RAII behavior, but will silently break existing call sites that expect perpetual subscription. Every current call site must be updated to store the token as a member field.

### Pattern 4: Generic Asset System (File-Based Mesh Discovery)

**What:** On startup, scan `assets/meshes/` using `ModelLoader::discoverProjectAssets()` (already implemented) and call `meshLibrary.loadFromFile(asset.meshId, asset.absolutePath.string())` for each discovered asset. This replaces the hand-written `registerFileAlias()` / `loadFromFile()` / `loadFromFileMulti()` calls currently in `registerAllGameAssets()`.

**Discovery API already exists:**
```cpp
// src/engine/rendering/assets/ModelLoader.h — already implemented, tested
static std::vector<DiscoveredModelAsset> discoverProjectAssets(
    const std::filesystem::path& rootPath,
    const std::filesystem::path& relativeBase);
```

`test_model_discovery.cpp` exercises this API end-to-end.

**Current file-registered meshes in GameAssets.cpp (lines 823-834):**
- `pillar` → `assets/meshes/pillar.glb`
- `arch` → `assets/meshes/arch.glb`
- `hand` → `assets/meshes/hand_with_old_dagger.glb`
- `country_house` → multi-submesh FBX (`loadFromFileMulti`)
- `country_house_door` → FBX
- `country_house_doors` → FBX
- `wood_door` → FBX

**Complication — `loadFromFileMulti`:** `country_house` is loaded via `loadFromFileMulti()` which registers as `country_house#MaterialName` subentries. Auto-discovery cannot infer multi-submesh intent from the filesystem alone. Options:
1. Auto-discovery calls `loadFromFile()` for all meshes and separately calls `loadFromFileMulti()` for known multi-material files via a naming convention (e.g., `.multi.fbx` suffix)
2. Always use `loadFromFile()` for auto-discovered assets; keep `loadFromFileMulti()` as an explicit override where needed
3. Use a discovery config file (`assets/meshes/discovery.cfg`) listing which files need multi-load treatment

**Recommended approach (D-05/D-07 guidance):** Auto-discover all mesh files with `loadFromFile()` by default. The `country_house` multi-submesh use case is exceptional; handle it by keeping a small explicit override list in the new `ProceduralGameAssets.cpp` — or better, migrate it to multi-file meshes. The goal is "no manual code registration for new meshes," not "zero exceptions for legacy meshes."

**Where auto-discovery runs:** Same site where `registerAllGameAssets()` is currently called:
- `GenericFileScene::onEnter()` via `request_.registerAssets = [](MeshLibrary& library) { registerAllGameAssets(library); }`
- `RuntimeGameSession::rebuild()` (if applicable)

After this phase, `request_.registerAssets` calls `MeshLibrary::loadDiscoveredAssets("assets/meshes")` (new method) then `registerProceduralAssets(library)` (renamed from `registerAllGameAssets`).

### Pattern 5: ProceduralGameAssets Rename

**What:** Rename `GameAssets.cpp`/`GameAssets.h` to `ProceduralGameAssets.cpp`/`ProceduralGameAssets.h`. Rename the function `registerAllGameAssets()` to `registerProceduralAssets()`. Update all `#include "game/levels/GameAssets.h"` references.

**Current callers of `registerAllGameAssets()`:**
- `src/game/scenes/GenericFileScene.cpp` (confirmed from codebase read)

No structural change to the function itself — only the filename, class name, and function name change.

**D-06 scope:** The rename makes explicit that this file is for code-generated geometry only. File-based assets are now handled by the auto-discovery path. Procedural meshes are NOT deleted or deprecated.

**Splitting GameAssets.cpp by asset family** (mentioned in CONCERNS.md as a fix for the 880-line monolith): This is a CONCERNS.md item that D-06 does NOT require. D-06 only requires the rename. Do NOT split into CathedralAssets/PrisonAssets/InstitutionalAssets unless user explicitly requests it.

### Pattern 6: DebugParams Split

**What:** `DebugParams` currently lives in `src/engine/ui/ImGuiLayer.h` and bundles post-process params, camera debug info, FPS stats, shadow settings, and lighting override into one struct. `PostProcessParams` already exists as a separate header at `src/engine/rendering/post/PostProcessParams.h` and is embedded in `DebugParams` as `DebugParams::post`.

**Current DebugParams fields (from ImGuiLayer.h:26-65):**
- `PostProcessParams post` — already extracted ✓
- `int internalResIndex`, `bool resolutionChanged` — resolution UI state
- `glm::vec3 cameraPos/Dir`, `float cameraFov/Speed` — read-only camera debug info → `CameraDebugInfo`
- `float fps/frameTimeMs`, `int drawCalls` — perf counters → stays in `DebugParams` (UI overlay state)
- `bool shadowsEnabled`, `int shadowMapResolutionIndex`, `float shadowBias/NormalBias` — lighting config → `RuntimeLightingOverride`
- `glm::vec3 hemisphereSky/GroundColor`, `float hemisphereStrength` — lighting → `RuntimeLightingOverride`
- `bool enableDirectionalLights`, `DirectionalLightSlot sunDirectional/fillDirectional` — lighting → `RuntimeLightingOverride`

**Proposed new structs:**
```cpp
// New: src/engine/rendering/CameraDebugInfo.h
struct CameraDebugInfo {
    glm::vec3 position{0.0f};
    glm::vec3 direction{0.0f};
    float fov = 70.0f;
    float moveSpeed = 3.0f;
};

// New: src/game/rendering/RuntimeLightingOverride.h
struct RuntimeLightingOverride {
    bool shadowsEnabled = true;
    int shadowMapResolutionIndex = 1;
    float shadowBias = 0.0018f;
    float shadowNormalBias = 0.03f;
    glm::vec3 hemisphereSkyColor{0.17f, 0.18f, 0.20f};
    glm::vec3 hemisphereGroundColor{0.06f, 0.05f, 0.045f};
    float hemisphereStrength = 0.32f;
    bool enableDirectionalLights = true;
    DirectionalLightSlot sunDirectional{...};
    DirectionalLightSlot fillDirectional{...};
};

// Slimmed DebugParams in ImGuiLayer.h:
struct DebugParams {
    PostProcessParams post;             // existing header
    CameraDebugInfo camera;             // new
    RuntimeLightingOverride lighting;   // new
    int internalResIndex = 2;
    bool resolutionChanged = false;
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    int drawCalls = 0;
};
```

**Files that reference DebugParams (9 files from grep):**
1. `src/game/rendering/RuntimeSceneRenderer.cpp` — writes camera pos/dir, reads shadow settings
2. `src/game/rendering/RuntimeSceneRenderer.h`
3. `src/game/rendering/EnvironmentDebugSync.cpp` — reads hemisphere/lighting fields
4. `src/engine/ui/ImGuiLayer.cpp` — renders the debug overlay
5. `src/game/systems/RenderSystem.h`
6. `src/game/runtime/RuntimeGameSession.h`
7. `src/engine/ui/ImGuiLayer.h` — definition
8. `src/editor/core/EditorRuntimePreviewSession.h`
9. `src/game/rendering/EnvironmentDebugSync.h`

All 9 files must be updated to use the nested field paths (e.g., `params.lighting.shadowsEnabled` instead of `params.shadowsEnabled`).

**`RuntimeLightingOverride` placement:** Note the class name `RuntimeLightingOverride` — lighting override belongs in the game layer (`src/game/rendering/`) not the engine layer since `DirectionalLightSlot` is defined in `engine/rendering/lighting/RenderLight.h`. However the struct uses `DirectionalLightSlot` which is engine-layer. Either place it in the engine rendering layer (`src/engine/rendering/`) or move to game layer. The game layer (`src/game/rendering/`) is correct since this is a runtime overlay (game concept), and `DirectionalLightSlot` in engine rendering is fine to include from the game layer (game depends on engine, not vice versa).

### Pattern 7: LevelLoader Unification

**What:** The two `load()` overloads:
1. `load(Application&, const LevelLoadRequest&)` — used by `GenericFileScene`
2. `load(ContentRegistry&, RunSession&, const LevelLoadRequest&, const LevelDef&)` — used by `RuntimeGameSession`

The first overload delegates to the second (confirmed from LevelLoader.cpp:25-29). The problem is the first overload resolves `Application` services internally, which means callers via path 1 cannot provide pre-loaded `LevelDef` or override the registry.

**Unified approach — explicit context struct:**
```cpp
struct LevelLoadArgs {
    ContentRegistry* content = nullptr;   // required
    RunSession* session = nullptr;        // required
    const LevelDef* levelDef = nullptr;   // optional: if null, load from request.levelPath
};

class LevelLoader {
public:
    explicit LevelLoader(LevelBuildContext& context);
    void load(const LevelLoadRequest& request, const LevelLoadArgs& args);
    // No Application overload — callers extract services before calling
};
```

**Impact:** `GenericFileScene::onEnter()` changes from:
```cpp
loader.load(app, request_);
```
to:
```cpp
LevelLoadArgs args{
    .content = &app.getService<ContentRegistry>(),
    .session = &app.getService<RunSession>()
};
loader.load(request_, args);
```

This is a small change but eliminates the divergent path.

### Pattern 8: GenericFileScene Scripted Geometry Refactor

**What:** Move the `if (request_.levelId == "institutional_room")` block out of `GenericFileScene::onEnter()`.

**Options from D-12:**
1. Per-level callbacks registered externally (e.g., a static registry map `levelId → buildScriptedGeometry callback`)
2. Encode the objects in the `.scene` file

**Codebase reality:** The institutional_room scripted geometry attaches `InteractableComponent` to doors. The `.scene` file format does not support component-level configuration beyond `meshId`, `position`, `scale`, `rotation`, `tint`, `materialId` (from LevelLoader.cpp inspection). Adding `InteractableComponent` data to the `.scene` format requires a format change (deferred per the Deferred Ideas section).

**Recommended approach:** External callback registration. The `LevelLoadRequest::buildScriptedGeometry` callback field already exists for this purpose. The problem is only that `GenericFileScene` constructs this callback itself based on `levelId`. Instead, provide a `ScriptedGeometryRegistry` (a simple static map or a function registered at startup) that maps `levelId` to a `buildScriptedGeometry` callback.

```cpp
// Minimal approach: a static map registered at startup
// In GenericFileScene.cpp or a separate registrar:
static std::unordered_map<std::string, std::function<void(LevelBuilder&)>> s_scriptedGeometry;

// In main.cpp or game setup:
ScriptedGeometryRegistry::registerLevel("institutional_room", buildInstitutionalRoomGeometry);

// In GenericFileScene::onEnter():
request_.buildScriptedGeometry = ScriptedGeometryRegistry::find(request_.levelId);
// No more if-chain in GenericFileScene itself
```

This satisfies D-12 without requiring a `.scene` format change.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Mesh file discovery | Custom filesystem walker | `ModelLoader::discoverProjectAssets()` | Already implemented and tested in `test_model_discovery.cpp` |
| Frustum plane extraction | Custom matrix decomposition | Gribb-Hartmann method on the VP matrix | Standard, zero dependencies, already have GLM |
| RAII unsubscription tracking | Custom destructor chains | `SubscriptionToken` move-only wrapper with id lookup | Pattern established by Godot/Hazel; maps cleanly to the existing `std::function` vector storage |

---

## Common Pitfalls

### Pitfall 1: AABB Frustum Test — Local vs World Space

**What goes wrong:** The AABB stored on `Mesh` (`aabbMin()`/`aabbMax()`) is in mesh-local space. Testing it against frustum planes without applying the model matrix gives wrong results — objects near the frustum edge get incorrectly culled or kept.

**Why it happens:** The model matrix scales and translates the AABB. A mesh with aabbMin = (-1,-1,-1) and a scale of 10 in world space has world AABB (-10,-10,-10) to (10,10,10).

**How to avoid:** Apply the model matrix to all 8 AABB corners to get world-space corners, then test each corner. Alternative (less accurate but faster): compute a world-space sphere from the AABB center and half-diagonal, then do a sphere-plane test. Either works; the corner-test approach has no false positives.

**Warning signs:** Objects disappearing when they should be visible near screen edges — indicates culling against local AABB without model transform.

### Pitfall 2: EventBus Token — Callers That Don't Store the Token

**What goes wrong:** Existing call sites like `eventBus().subscribe<MyEvent>([this](const MyEvent& e) { ... })` that do not capture the return value will compile with the new API (return value is discarded). But since the token is immediately destroyed, the handler is registered and immediately unsubscribed, producing a silent no-op subscription.

**Why it happens:** C++ allows discarding return values. The compiler does not warn.

**How to avoid:** After implementing the RAII token, grep all call sites for `subscribe<` and verify each one captures the token into a member field. Use `[[nodiscard]]` on `subscribe()` to force a compiler warning when the return value is discarded.

**Warning signs:** Event handlers that stop firing after the RAII refactor — a subscriber that was previously permanent is now a no-op.

### Pitfall 3: Texture Unit Enum — Overlapping Ranges Are Intentional

**What goes wrong:** Seeing that shadow maps (8-15) overlap with material maps (12-15) and "fixing" the overlap by renumbering, breaking the bind ordering assumption.

**Why it happens:** The overlap is intentional. Shadow maps are bound once before the draw loop. Per-object material texture binds at 12-15 overwrite the shadow map slots for that object, which is fine since shadow maps are sampled by the shader using the shadow map sampler (not the material map sampler). They occupy the same unit numbers but serve different shader uniforms.

**How to avoid:** Keep the enum values as-is. Document the overlap explicitly in the enum comments. The enum is for naming, not for eliminating the overlap.

**Warning signs:** Visual corruption (missing shadows, wrong textures) after renumbering — indicates the binding order invariant was broken.

### Pitfall 4: LevelLoader Unification — RunSession Initialization Order

**What goes wrong:** The `Application`-based `load()` overload calls `app.getService<RunSession>()`, which means `RunSession` must be registered as a service before `LevelLoader::load()` is called. The unified `LevelLoadArgs` struct makes this dependency explicit, but the caller must ensure `session` is not null.

**Why it happens:** The service locator pattern hides dependencies at the type level.

**How to avoid:** Add an assert or null-check in `LevelLoader::load()` at entry: `assert(args.content != nullptr && args.session != nullptr)`.

### Pitfall 5: `kMaxShadowedSpotLights` Duplicate Definition

**What goes wrong:** `RenderLight.h:14` and `SceneRenderPipeline.h:60` both define `kMaxShadowedSpotLights = 8`. When adding the texture unit enum, the temptation is to reference `kMaxShadowedSpotLights` in the enum for the shadow map range end. If both constants exist, the enum in `TextureUnits.h` must pick one to reference.

**How to avoid:** Per CONCERNS.md, remove the duplicate in `SceneRenderPipeline.h` and use the one from `RenderLight.h`. Do this as part of the texture unit enum task.

### Pitfall 6: DebugParams Split — `DirectionalLightSlot` Default Initializers

**What goes wrong:** `DebugParams` currently holds `DirectionalLightSlot sunDirectional{true, glm::vec3(0.24f,...), ...}` with non-default initializer values. Moving these to `RuntimeLightingOverride` must preserve the exact initializer values, or the sun direction and color will silently change to defaults.

**How to avoid:** Copy the initializer values verbatim, including the aggregate initialization braces.

---

## Dependency Ordering

The seven items have these dependencies:

```
[independent] TextureUnitEnum
[independent] EventBusRAII
[independent] FrustumCulling
[independent] DebugParamsSplit
[GenericFileSceneRefactor depends on LevelLoaderUnification (both modify LevelLoadRequest path)]
[ProceduralGameAssetsRename must precede FileBasedMeshDiscovery]
```

**Recommended wave order:**

- **Wave 1:** TextureUnitEnum + EventBusRAII (isolated header/class changes, zero risk to rendering)
- **Wave 2:** DebugParamsSplit + LevelLoaderUnification + GenericFileSceneRefactor (structural interface changes)
- **Wave 3:** ProceduralGameAssetsRename + FileBasedMeshDiscovery + FrustumCulling

**Rationale for GenericFileScene ordering:** LevelLoaderUnification changes the `load()` signature; GenericFileScene is the primary caller of `load(Application&, ...)`. Unifying the overload first means the GenericFileScene refactor can use the clean interface immediately.

---

## Light Frustum Culling Recommendation (D-03 Discretion)

**Recommendation: Skip light culling in this phase.**

**Rationale:**
- 32-light cap is already small enough that iterating all lights to set uniforms is negligible (the Renderer already iterates all `numLights` unconditionally)
- The `kMaxShadowedSpotLights = 8` slot assignment loop is similarly cheap
- Light frustum culling requires computing a light's influence sphere / cone footprint, which is non-trivial for spot lights (cone frustum vs camera frustum intersection)
- Object AABB frustum culling delivers the meaningful win (skipping draw calls and shadow pass objects) — light culling saves only uniform upload time which is already cached

CONCERNS.md rates the 32-light uniform loop as a performance concern only at 60fps sustained. Light culling complexity > benefit for this phase.

---

## Code Examples

### AABB World-Space Frustum Test
```cpp
// Source: Gribb-Hartmann "Fast Extraction of Viewing Frustum Planes" — standard algorithm
// Extract 6 clip planes from VP matrix (column-major GLM layout)
std::array<glm::vec4, 6> extractFrustumPlanes(const glm::mat4& vp) {
    std::array<glm::vec4, 6> planes;
    // Left: col3 + col0
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0],
                          vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right: col3 - col0
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0],
                          vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom: col3 + col1
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1],
                          vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top: col3 - col1
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1],
                          vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near: col3 + col2
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2],
                          vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far: col3 - col2
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2],
                          vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);
    return planes;
}

bool isAabbInsideFrustum(const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                          const glm::mat4& modelMatrix,
                          const std::array<glm::vec4, 6>& planes) {
    // Transform 8 AABB corners to world space
    static const glm::vec4 kCornerOffsets[8] = {
        {0,0,0,1},{1,0,0,1},{0,1,0,1},{1,1,0,1},
        {0,0,1,1},{1,0,1,1},{0,1,1,1},{1,1,1,1}
    };
    for (const auto& plane : planes) {
        bool allOutside = true;
        for (int c = 0; c < 8; ++c) {
            glm::vec3 corner = aabbMin + glm::vec3(kCornerOffsets[c]) * (aabbMax - aabbMin);
            glm::vec4 worldCorner = modelMatrix * glm::vec4(corner, 1.0f);
            if (glm::dot(plane, worldCorner) >= 0.0f) {
                allOutside = false;
                break;
            }
        }
        if (allOutside) return false; // all corners outside this plane = cull
    }
    return true;
}
```

### TextureUnits.h Target Structure
```cpp
// Source: Codebase analysis — all assignments documented above
// File: src/engine/rendering/TextureUnits.h
#pragma once

// Scene shader texture unit assignments.
//
// Units 8-15 are used for shadow maps (bound once per frame before the draw loop).
// Units 12-15 are ALSO used for per-object material maps (albedo/normal/roughness/ao)
// bound within the draw loop — overwriting the shadow map bindings for each object.
// This overlap is intentional: shadow maps are sampled via separate uniforms
// (uShadowMaps[]) while material maps are sampled via uAlbedoMap etc.
// Units 10-11 are LTC lookup tables, bound once per scene pass.
// Unit 16 is the CSM depth array.
namespace TextureUnits {
    // Shadow map slots (8 shadow lights max)
    constexpr int kShadowMap0     = 8;   // ... kShadowMap7 = 15
    // LTC area-light lookup tables (bound once per scene pass, sit within shadow range)
    constexpr int kLtcMat         = 10;
    constexpr int kLtcAmp         = 11;
    // Per-object material maps (overwrite shadow slots 12-15 per draw call)
    constexpr int kAlbedo         = 12;
    constexpr int kNormalMap      = 13;
    constexpr int kRoughnessMap   = 14;
    constexpr int kAoMap          = 15;
    // Cascaded shadow map (directional sun)
    constexpr int kCsmShadowMap   = 16;
} // namespace TextureUnits
```

### File-Based Mesh Discovery
```cpp
// Pattern from ContentRegistry::loadMaterialsFromDirectory() — adapt for meshes
// New method on MeshLibrary or free function in ProceduralGameAssets.cpp:
void loadDiscoveredMeshAssets(MeshLibrary& library, const std::string& meshDirectory) {
    const auto assets = ModelLoader::discoverProjectAssets(
        resolveProjectPath(meshDirectory),
        resolveProjectPath(meshDirectory));
    for (const auto& asset : assets) {
        if (library.has(asset.meshId)) {
            spdlog::debug("Mesh '{}' already registered — skipping auto-discovery", asset.meshId);
            continue;
        }
        try {
            library.loadFromFile(asset.meshId, asset.absolutePath.string());
        } catch (const std::exception& e) {
            spdlog::error("Failed to auto-discover mesh '{}': {}", asset.relativePath, e.what());
        }
    }
}
```

---

## Environment Availability

Step 2.6: SKIPPED — this phase is entirely code and header changes with no external tool dependencies. No new binaries, services, or runtimes are required.

---

## Validation Architecture

`workflow.nyquist_validation` is explicitly `false` in `.planning/config.json`. This section is omitted per configuration.

---

## Project Constraints (from CLAUDE.md)

| Directive | Impact on Phase 12 |
|-----------|-------------------|
| Engine: Custom C++ | All work in C++; no external engine frameworks |
| OpenGL 4.1 Core Profile | Texture unit enum maps to `GL_TEXTURE0 + N`; all shader targets `#version 410 core` |
| C++20 standard | `SubscriptionToken` can use designated initializers, `[[nodiscard]]` |
| Naming: `PascalCase` classes, `camelCase` methods, `k` prefix constants | `SubscriptionToken`, `CameraDebugInfo`, `RuntimeLightingOverride`, `kShadowMap0`, `kAlbedo` etc. |
| `#pragma once` | All new headers use `#pragma once` |
| Include order: stdlib → third-party → project | New headers must follow this order |
| Non-copyable by default | `SubscriptionToken` is move-only (no copy ctor/assignment) |
| RAII for resources | EventBus token is RAII; consistent with engine pattern |
| Three executables | `pixel-roguelike`, `level-editor`, `procedural-model-viewer` — all may need updating if they call `registerAllGameAssets()` |
| `[[nodiscard]]` not mandated in CLAUDE.md | Use it on `subscribe()` regardless — prevents the Pitfall 2 silent no-op at zero cost |
| No Boost | No concern for this phase |
| Shaders target GLSL 4.10 | No shader changes required for any of the 7 items |

---

## Open Questions

1. **Does `RuntimeGameSession` also call `registerAllGameAssets()`?**
   - What we know: `GenericFileScene` calls it. `RuntimeGameSession` may have its own asset registration path.
   - What's unclear: Whether `RuntimeGameSession::rebuild()` calls `registerAllGameAssets()` directly or via a different code path.
   - Recommendation: Grep for `registerAllGameAssets` before planning Wave 3.

2. **Does `procedural-model-viewer` executable call `registerAllGameAssets()`?**
   - What we know: It is listed as a separate executable that uses `game_rendering`. It may have its own asset loading.
   - Recommendation: Grep before planning.

3. **`gothic_door_static.glb` (42.9 MB) in `assets/meshes/`**
   - What we know: It is present on disk. It is NOT currently registered in `GameAssets.cpp`. Under auto-discovery, it will be loaded automatically, adding 42.9 MB of mesh data to every load.
   - What's unclear: Whether this file is intentionally excluded (too large, placeholder, unused).
   - Recommendation: Either move non-production meshes out of `assets/meshes/` before enabling auto-discovery, or add a discovery exclusion mechanism (e.g., `.discovery-ignore` file or mesh directory organization by production/draft).

4. **SubscriptionToken thread safety**
   - What we know: EventBus is owned by `Application` and accessed from the main thread only (no evidence of multi-thread publish/subscribe).
   - Recommendation: No mutex needed; document as main-thread-only in the header.

---

## Sources

### Primary (HIGH confidence — direct codebase inspection)
- `src/engine/core/EventBus.h` — current 31-line subscribe-only implementation
- `src/engine/ui/ImGuiLayer.h` — DebugParams definition (40 fields)
- `src/game/level/LevelLoader.h` / `.cpp` — dual overload confirmed
- `src/game/scenes/GenericFileScene.cpp` — hard-coded institutional_room branch
- `src/engine/rendering/SceneRenderPipeline.cpp` — kCsmTextureUnit=16, kLtcMatUnit=10, kLtcAmpUnit=11
- `src/engine/rendering/geometry/Renderer.cpp` — shadow maps 8-15, albedo 12, normal 13, roughness 14, AO 15
- `src/engine/rendering/geometry/MeshLibrary.h` — registration API
- `src/engine/rendering/assets/ModelLoader.h` — `discoverProjectAssets()` exists
- `src/game/content/ContentRegistry.cpp` — `loadMaterialsFromDirectory()` pattern
- `src/engine/rendering/geometry/Mesh.h` — `aabbMin()`/`aabbMax()` accessors confirmed
- `tests/engine/test_model_discovery.cpp` — discovery API test coverage
- `.planning/codebase/CONCERNS.md` — source for D-10 through D-12
- `assets/meshes/` directory listing — identifies `gothic_door_static.glb` (42.9 MB) as potential auto-discovery issue
- `.planning/config.json` — `nyquist_validation: false` confirmed

### Secondary (MEDIUM confidence)
- Gribb-Hartmann "Fast Extraction of Viewing Frustum Planes from the WorldView-Projection Matrix" — frustum plane extraction algorithm. This is the canonical reference used in Godot, Hazel, and Game Engine Architecture (3rd ed). The GLM column-major indexing must be verified at implementation time.

---

## Metadata

**Confidence breakdown:**
- All 7 improvement items: HIGH — all implementation details derived from direct source file reads
- Frustum culling plane extraction math: MEDIUM — standard algorithm, GLM indexing should be verified
- Discovery `gothic_door_static.glb` risk: HIGH — file confirmed on disk, not registered in GameAssets

**Research date:** 2026-04-01
**Valid until:** 2026-05-01 (stable codebase; no external dependencies)
