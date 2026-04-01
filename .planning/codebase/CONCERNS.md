# Codebase Concerns

**Analysis Date:** 2026-04-01

---

## Performance Bottlenecks

**Per-frame std::string allocations in hot rendering paths:**
- Problem: Every draw call in `Renderer::drawScene()` constructs `std::string base = "uLights[" + std::to_string(i) + "].";` for each of up to 32 lights, concatenating a new string per uniform. Same pattern in `SsaoPass.cpp` (`"uSamples[" + std::to_string(i) + "]"` repeated for 32 kernel samples).
- Files: `src/engine/rendering/geometry/Renderer.cpp:53`, `src/engine/rendering/post/SsaoPass.cpp:210`
- Cause: Shader uniform setters take `const std::string&`, and array uniform names are built via string concatenation in the per-frame loop.
- Impact: ~(32 * N_lights + 32) heap allocations per frame just for uniform names. At 60fps with 32 lights this is roughly 64 allocations/frame that could be eliminated.
- Fix: Pre-cache uniform locations via `glGetUniformLocation` at shader init time into an array. Cache results in a fixed array indexed by slot number.

**Per-frame heap allocation of render object vectors:**
- Problem: `RuntimeSceneRenderer::render()` calls `collectSceneObjects()` and `collectViewmodelObjects()` which each construct and return a new `std::vector<RenderObject>` by value every frame. `SceneRenderPipeline::render()` then makes another copy of the lights vector (`std::vector<RenderLight> lights = *input.lights`).
- Files: `src/game/rendering/RuntimeSceneRenderer.cpp:318-320`, `src/engine/rendering/SceneRenderPipeline.cpp:55`
- Fix: Promote render vectors to member fields of `RuntimeSceneRenderer` and `SceneRenderPipeline` and call `clear()` + populate in-place. This converts per-frame heap allocations to amortized capacity reuse.

**`glGetUniformLocation` called every frame per uniform:**
- Problem: All five `Shader::set*()` methods call `glGetUniformLocation(program_, name.c_str())` on every invocation. With 191 `set*` calls per frame the driver is doing string hash lookups into the shader's uniform table every frame.
- Files: `src/engine/rendering/core/Shader.cpp:94-126`
- Fix: Cache uniform locations in a `std::unordered_map<std::string, GLint>` inside `Shader`, populated lazily on first call. Or use explicit uniform binding points (`layout(binding=N)` in GLSL + `glUniformBlockBinding`).

**Procedural texture generation on init:**
- Problem: `MaterialTextureLibrary::init()` generates CPU-side procedural pixel data (brick, stone, smooth wall, floor, ceiling) and uploads to GPU each time `reloadContent()` is called. This includes multi-octave FBM noise evaluated per-texel in `MaterialTextureLibrary.cpp`.
- Files: `src/game/rendering/MaterialTextureLibrary.cpp`, `src/game/rendering/MaterialTextureLibrary.h`
- Cause: No check for whether procedural textures are already loaded; `reloadContent()` calls `materialTextureLibrary_.init(content)` unconditionally.
- Fix: Guard procedural texture generation with a flag or check texture handle before regenerating.

---

## Tech Debt

**Jolt Physics max body count is a compile-time magic number:**
- Issue: `PhysicsSystem::init()` hardcodes `1024` for max bodies, body pairs, and contact constraints.
- File: `src/engine/physics/PhysicsSystem.cpp:187-194`
- Impact: The world already has multi-room levels with many colliders; the cap will silently stop adding bodies when exceeded — Jolt's `BodyInterface::CreateAndAddBody()` returns an invalid ID rather than throwing.
- Fix: Add a named constant (`kMaxBodies`) and add a debug assert or log on overflow. Scale the number to a real estimate or make it configurable via project settings.

**Dual `init()` API on `PhysicsSystem` — Application vs registry overloads:**
- Issue: `PhysicsSystem` has `init(Application&)`, `update(Application&, float)`, `shutdown()` (from `System` base) AND a separate `init(entt::registry&)`, `update(entt::registry&, float)`, `shutdownRuntime()` used by `RuntimeGameSession`. This doubles the API surface and creates confusion about which lifecycle path is active.
- File: `src/engine/physics/PhysicsSystem.h:19-25`
- Impact: The `System`-interface methods are only used in the standalone-app path. The direct-registry methods are used in the editor path. Any future caller must know which path they're on.
- Fix: Use only the direct-registry path everywhere. The `System` overloads delegate to the registry versions anyway.

**`DebugParams` mixes game logic with rendering config:**
- Issue: `DebugParams` (defined in `src/engine/ui/ImGuiLayer.h`) bundles post-process parameters, debug camera data, per-frame FPS stats, shadow settings, and directional light slots into one struct that is threaded through `RuntimeSceneRenderer`, `RenderSystem`, `EditorRuntimePreviewSession`, and `RuntimeGameSession`.
- Files: `src/engine/ui/ImGuiLayer.h:26-65`, 11 files reference it.
- Impact: Any change to `DebugParams` forces recompilation of all rendering and session code. Lighting config (hemisphere colors, shadow bias) lives in the engine UI layer when it belongs in game content.
- Fix: Split into `PostProcessParams` (already exists), `CameraDebugInfo`, and a separate `RuntimeLightingOverride` struct. `DebugParams` should shrink to just the debug overlay UI state.

**`ContentRegistry*` stored raw in `entt` context:**
- Issue: `RuntimeGameSession::rebuild()` inserts `ContentRegistry*` and `RunSession*` as raw pointers into the registry context via `registry_.ctx().insert_or_assign<ContentRegistry*>(&content)`. If the session is cleared or rebuilt while a system holds the pointer, the system sees a dangling reference.
- Files: `src/game/runtime/RuntimeGameSession.cpp:132-133`, `src/game/rendering/EnvironmentDebugSync.cpp:138-139`
- Fix: Store `ContentRegistry` as an owning member of `RuntimeGameSession` or pass it explicitly to functions that need it rather than threading through ECS context.

**`LevelLoader` has two `load()` overloads with different semantics:**
- Issue: `LevelLoader::load(Application&, ...)` and `LevelLoader::load(ContentRegistry&, RunSession&, ...)` exist side by side. The `Application` version is used by `GenericFileScene` (standalone game path); the direct version is used by `RuntimeGameSession` (editor/session path). These two code paths diverge silently.
- File: `src/game/level/LevelLoader.h:22-27`
- Impact: Feature additions to one path are easily missed in the other. `GenericFileScene` does not support environment overrides or baseline snapshots.
- Fix: Unify on a single `load()` that takes an explicit context struct.

**Static local `AssetInspectorSession` and `AssetBrowserSession` in panel files:**
- Issue: `EditorInspectorPanel.cpp:36` and `EditorAssetBrowserPanel.cpp:45` use `static` local variables to persist editor state across frames. This is effectively a hidden global.
- Files: `src/editor/ui/EditorInspectorPanel.cpp:36`, `src/editor/ui/EditorAssetBrowserPanel.cpp:45`
- Impact: State is not reset on scene load, cannot be owned/managed externally, and makes the editor hard to test (state leaks across test cases). It's also incompatible with any future multi-window editor design.
- Fix: Promote these to owned fields in the editor's top-level document state or `LevelEditorCore`.

**`extractRotationMatrix()` duplicated in two translation units:**
- Issue: Identical function body appears in `EditorSceneDocument.cpp:24` and `LevelDef.cpp:96`.
- Files: `src/editor/scene/EditorSceneDocument.cpp:24`, `src/game/level/LevelDef.cpp:96`
- Fix: Move to `engine/core/MathUtils.h` (which already exists and is the natural home).

**`makeModel()` duplicated in `LevelBuilder.cpp` and `GameAssets.cpp`:**
- Issue: Identical `makeModel(position, scale, rotation)` helper is copy-pasted in both files.
- Files: `src/game/level/LevelBuilder.cpp:16`, `src/game/levels/GameAssets.cpp:17`
- Fix: Extract to a shared utility or use GLM directly at call sites.

**`defaultTintForMesh()` / `defaultMaterialIdForMesh()` — magic mesh-name lookups:**
- Issue: `LevelBuilder.cpp` contains `if (meshName == "door_leaf_left" || ...)` chains that assign tint and material defaults by string comparison against mesh names. New meshes added to `GameAssets.cpp` must be manually registered here or they silently get wrong defaults.
- File: `src/game/level/LevelBuilder.cpp:27-95`
- Impact: Every new mesh addition has two steps that must stay in sync. Breakage is invisible at compile time.
- Fix: Attach default tint/material to a `MeshRegistration` struct when registering in `GameAssets.cpp`, eliminating the string switch.

**`GameAssets.cpp` is an 880-line monolith of procedural mesh construction:**
- Issue: Every procedural mesh (prison walls, furniture, door frames, padlocks, hvac vents etc.) is a named free function in a single file.
- File: `src/game/levels/GameAssets.cpp` (880 lines)
- Impact: File is already the 3rd-largest `.cpp` in the project and grows linearly with every new asset. It has no header besides the single `registerAllGameAssets()` declaration.
- Fix: Split by asset family into separate files (`CathedralAssets.cpp`, `PrisonAssets.cpp`, `InstitutionalAssets.cpp`) that all call into `registerAllGameAssets()` or a registrar table.

---

## Fragile Areas

**`SceneRenderPipeline` texture unit assignments are implicit magic numbers:**
- Issue: Texture unit indices (shadow maps 8-15, albedo 12, normal 13, roughness 14, AO 15, LTC mat 10, LTC amp 11, CSM 16) are scattered as bare integer constants across `Renderer.cpp` and `SceneRenderPipeline.cpp`. The assignments overlap at units 12-15 (material maps and shadow maps share the same range) which only works because shadow maps are bound before per-object material maps overwrite them.
- Files: `src/engine/rendering/geometry/Renderer.cpp:40-44`, `src/engine/rendering/SceneRenderPipeline.cpp:17-20`
- Impact: Adding a new texture to the scene shader requires knowing all existing slot assignments. Off-by-one errors produce silent visual corruption.
- Fix: Define an enum `TextureUnit { kShadowMap0 = 8, kAlbedo = 12, ... }` in a shared header and use those names everywhere.

**`inventoryTogglePressed()` encodes a locale workaround with raw Unicode:**
- Issue: `RuntimeGameplay.cpp:60-70` checks for the inventory key via 9 different conditions including raw Unicode code points `0x0130` and `0x0131` (Turkish dotted/dotless-I) to work around GLFW key name locale sensitivity.
- File: `src/game/runtime/RuntimeGameplay.cpp:60-70`
- Impact: This is the only place where a locale workaround exists; other keys (interact `E`, sprint, jump) do not have equivalent handling. The fix is inconsistently applied and will not generalize.
- Fix: Bind keyboard actions by scancode in `ActionMap`, not by key name. GLFW provides `glfwGetKeyScancode()` and `glfwGetKeyName()` — the action system should use scancodes as canonical identifiers.

**`resolveProjectPath()` silently returns the input path on failure:**
- Issue: When a file is not found, `resolveProjectPath()` returns the input `relativePath` string unchanged rather than returning an empty string or throwing.
- File: `src/engine/core/PathUtils.cpp:25`
- Impact: All callers — shader loading, content file loading, mesh loading — receive a non-empty string and attempt to open it, getting a misleading file-not-found error downstream rather than a clear "path resolution failed" message.
- Fix: Return `std::optional<std::string>` or log a warning and return `""` to make the failure explicit.

**`AssetCache` binary format uses `#pragma pack` with no endian guard:**
- Issue: `AssetCache.cpp` writes binary cache files using `#pragma pack(push, 1)` structs with `uint32_t`/`uint64_t` fields directly. The format is implicitly little-endian (x86/ARM macOS) with no byte-order marker.
- File: `src/engine/rendering/assets/AssetCache.cpp:15-43`
- Impact: Cache files built on one machine are silently corrupted if read on a big-endian platform. Not a practical concern today, but the format has no version negotiation if field order changes.
- Fix: Add a format version byte (already present as `version = 1`), and document the endianness requirement in the header.

**`GenericFileScene` has hard-coded level-specific scripted geometry:**
- Issue: `GenericFileScene::onEnter()` contains `if (request_.levelId == "institutional_room")` to inject scene-specific entities (doors, knobs) that are not represented in the `.scene` file.
- File: `src/game/scenes/GenericFileScene.cpp:29-75`
- Impact: Every new scene that needs code-driven geometry must add another `if` branch here. This scene class will grow into a large switch statement.
- Fix: Move scripted geometry setup into per-level `LevelLoadRequest::buildScriptedGeometry` callbacks registered somewhere outside `GenericFileScene`, or encode the objects in the `.scene` file.

**`RuntimeGameSession` has a split initialized/uninitialized state exposed to callers:**
- Issue: `tick()` and `render()` silently no-op when `physicsInitialized_ == false`. Callers have no way to distinguish "session not built yet" from "render failed" from "render succeeded".
- Files: `src/game/runtime/RuntimeGameSession.cpp:182`, `src/game/runtime/RuntimeGameSession.cpp:235`
- Impact: Silent no-ops are hard to debug. If `rebuild()` is never called before `tick()`, nothing happens and no error is surfaced.
- Fix: Add `SPDLOG_ASSERT` or a clear log message when entering `tick()`/`render()` without initialization. Alternatively use a state machine enum (`Uninitialized`, `Built`, `Running`) to make the state explicit.

---

## Missing Error Handling

**Jolt `CreateAndAddBody()` return value not checked:**
- Issue: `PhysicsSystem::update()` calls `bodyInterface.CreateAndAddBody()` and stores the result, but does not check whether the returned `JPH::BodyID` is valid (`bodyId.IsInvalid()`).
- File: `src/engine/physics/PhysicsSystem.cpp:239`
- Impact: When the 1024 body limit is exceeded, new bodies are silently dropped. Static colliders simply don't exist in the physics world, causing the player to fall through geometry with no diagnostic.
- Fix: Check `bodyId.IsInvalid()` after creation and log an error with the entity ID and collider bounds.

**WAV loader does not check `file.read()` return values:**
- Issue: `AudioSystem.cpp` reads WAV header fields with bare `file.read(...)` calls and then validates the header after all reads. A truncated or corrupt file can produce reads of zero bytes while `audioFormat`, `channels` etc. contain uninitialized stack data.
- File: `src/engine/audio/AudioSystem.cpp:350-368`
- Fix: Check `file.good()` after each read group or after the full header block before proceeding to validation.

**`loadEnvironmentDefinitionAsset()` not checked at `ContentRegistry::loadDefaults()`:**
- Issue: `ContentRegistry.cpp` calls free-standing `load*Asset()` functions that throw `std::runtime_error` on file open failure. If any environment or archetype file is missing, the entire `loadDefaults()` call unwinds without loading the rest.
- File: `src/game/content/ContentRegistry.cpp`
- Fix: Wrap per-file loads in try/catch inside `loadDefaults()` and log per-file errors so partial loading is possible.

**Shader compilation failure throws in `init()` without pipeline cleanup:**
- Issue: `SceneRenderPipeline::init()` creates six shaders sequentially. If `csmShader_` construction throws, `sceneShader_` and `shadowShader_` are already initialized and hold GPU resources, but the exception propagates before `shutdown()` is called. `sceneShader_` destructor runs, but `renderer_` may not, depending on throw point.
- File: `src/engine/rendering/SceneRenderPipeline.cpp:23-36`
- Impact: On shader compilation failure (e.g., syntax error after edit), GPU resource leak is possible in debug builds.
- Fix: Use a builder pattern or an RAII wrapper around the multi-shader init sequence.

---

## Security Considerations

**Asset cache binary deserialization with no bounds check on vertex/index count:**
- Risk: `AssetCache::findMeshCache()` reads `vertexCount` and `indexCount` from the cache header then does `resize(vertexCount * 11)` and `resize(indexCount)` without checking upper bounds. A malformed or manually crafted cache file can cause multi-gigabyte allocations.
- Files: `src/engine/rendering/assets/AssetCache.cpp:160-168`
- Current mitigation: None. Cache files are local to the dev machine.
- Recommendation: Add a sanity check (e.g., `vertexCount > 10'000'000` → reject) before allocating.

---

## Scaling Limits

**Physics world limited to 1024 bodies:**
- Current capacity: 1024 static bodies, 1024 body pairs, 1024 contact constraints (see PhysicsSystem hardcodes above).
- Limit: Silently fails to register colliders beyond this count.
- Scaling path: Increase constants; Jolt itself can handle 65536+ bodies without architectural changes.

**Light limit capped at 32 per frame:**
- Current capacity: `kMaxRenderLights = 32` in `src/engine/rendering/lighting/RenderLight.h:13`.
- Limit: The scene shader GLSL array is sized at 32. Adding more lights requires a GLSL recompile and shader reload.
- Scaling path: Move lights to a SSBO (Shader Storage Buffer Object) for dynamic sizing, or implement a light-culling / tiling pass.

**Shadowed spot lights hard-capped at 8:**
- Current capacity: `kMaxShadowedSpotLights = 8` — defined in both `RenderLight.h:14` and `SceneRenderPipeline.h:60` (creating a redundant definition).
- Limit: More than 8 spot lights that cast shadows silently drop shadows for the extras.
- Fix: Remove the duplicate constant in `SceneRenderPipeline.h` and use the one from `RenderLight.h`.

**`MeshLibrary` is a flat name→mesh hash map with string key lookups:**
- Current capacity: Fine for current scale (<100 meshes).
- Limit: At hundreds of procedural meshes (each new asset type adds entries), name-collision risk increases and string lookup overhead grows.
- Scaling path: Assign integer mesh IDs at registration time; use string names only for editor display and `.scene` file serialization.

---

## Comparison with Industry Standards

**No frustum or occlusion culling:**
- All scene objects are submitted to the GPU every frame regardless of whether they are in the camera frustum. Godot, Hazel, and Bevy all implement at least AABB frustum culling before draw submission.
- Files: `src/game/rendering/RuntimeSceneRenderer.cpp:124-149` — `collectSceneObjects()` iterates all `MeshComponent` entities unconditionally.
- Fix: Implement per-object AABB frustum test using the already-present `mesh->aabbMin()`/`aabbMax()` and the camera projection matrix.

**No material/shader batching (N draw calls per object):**
- Each scene object results in exactly one `mesh->draw()` call with full uniform re-upload. The renderer has no ability to batch objects sharing the same material, even for simple primitives. Well-structured engines use instanced rendering or at minimum sort by material to reduce state changes.
- Files: `src/engine/rendering/geometry/Renderer.cpp:71-119`
- Impact: With 50+ objects, each using a different material, the driver receives 50+ full uniform state changes per frame. Not a bottleneck at current scale but will become one.

**No asset streaming — everything loaded synchronously on the main thread:**
- `GameAssets.cpp::registerAllGameAssets()` is called from `RuntimeGameSession` constructor and `GenericFileScene::onEnter()`. All mesh construction, file loading, and GPU uploads happen before the first frame.
- Impact: Level load times grow linearly with asset count. No background loading.

**Event bus has no unsubscription mechanism:**
- `EventBus::subscribe()` has no return token or handle. Once a handler is subscribed it lives for the lifetime of the `EventBus`. If a system is destroyed but the bus outlives it, the stored `std::function` captures a dangling reference.
- File: `src/engine/core/EventBus.h:11-16`
- Fix: Return a subscription token (RAII handle) from `subscribe()` that unregisters the handler on destruction, matching patterns used in Godot's signal system and Hazel's event system.

**No serialization versioning on `.scene` files:**
- The custom text-based `.scene` format in `LevelDef.cpp` has no format version header. Any change to a field name or order silently drops the old data on reload.
- Files: `src/game/level/LevelDef.cpp`, `src/game/level/LevelDef.h`
- Fix: Add a `version N` header line and a migration path, or switch to a versioned binary/JSON format.

**No component validation or schema enforcement:**
- Components like `LightComponent`, `MeshComponent`, `DoorComponent` are plain structs populated from `.scene` files. There is no validation that a `DoorComponent` has a valid `archetypeId`, or that a `MeshComponent`'s `meshId` exists in the library, until runtime when the entity fails to render silently.
- Fix: Add a `ContentRegistry::validate(const LevelDef&)` function that checks referential integrity before building entities, similar to Godot's `_get_configuration_warnings()` system.

---

## Test Coverage Gaps

**Rendering pipeline is entirely untested:**
- What's not tested: `SceneRenderPipeline`, `RuntimeSceneRenderer`, `Renderer`, all post-process passes (Bloom, SSAO, Composite, Stylize). These require an OpenGL context.
- Files: All of `src/engine/rendering/`
- Risk: Regressions in shader uniform binding, texture unit conflicts, or framebuffer resize logic go undetected until visual inspection.
- Priority: Medium — headless OpenGL testing via osmesa is possible but expensive to set up.

**Physics system untested at unit level:**
- What's not tested: `PhysicsSystem::init()`, body registration from `StaticColliderComponent`, character controller behavior.
- File: `src/engine/physics/PhysicsSystem.cpp`
- Risk: Body overflow behavior (silent drop at 1024) is never exercised; physics-level regressions require running the full game.
- Priority: Medium.

**`GenericFileScene` / scene hot-path not covered:**
- What's not tested: `GenericFileScene::onEnter()`, the scripted geometry injection branch, or the full `LevelLoader::load(Application&, ...)` path.
- Files: `src/game/scenes/GenericFileScene.cpp`, `src/game/level/LevelLoader.cpp`
- Risk: The "institutional_room" hard-coded branch and any future branches can silently break.
- Priority: Low-Medium.

---

*Concerns audit: 2026-04-01*
