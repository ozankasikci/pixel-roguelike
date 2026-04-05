# Codebase Concerns

**Analysis Date:** 2026-04-05

## Tech Debt

### Large Monolithic UI/Editor Files

**Problem:** Inspector, Outliner, Asset Browser, and Environment panels have grown to unsustainable sizes with heavy procedural rendering logic mixed with state management.

**Files:**
- `src/editor/ui/EditorInspectorPanel.cpp` (1516 lines)
- `src/editor/ui/EditorAssetBrowserPanel.cpp` (785 lines)
- `src/editor/ui/EditorOutlinerPanel.cpp` (561 lines)
- `src/editor/ui/EditorEnvironmentPanel.cpp` (555 lines)

**Impact:**
- Difficult to extract reusable UI components
- Testing individual UI features requires entire panel initialization
- Changes to one feature risk breaking others in the same file
- Behavior authoring UI (D-06, D-07, D-08) is deeply embedded in EditorInspectorPanel, preventing reuse by other systems

**Fix approach:**
- Decompose each panel into smaller helper modules (e.g., `BehaviorAuthoringPanel`, `AssetSelectionPanel`)
- Extract static helper functions and immutable data structures to separate headers
- Move procedural ImGui code into focused, single-responsibility functions with clear inputs/outputs

---

### Static Session State in EditorInspectorPanel

**Problem:** `AssetInspectorSession` is a static function-local singleton in `src/editor/ui/EditorInspectorPanel.cpp:38-41`, making its lifecycle and state implicit.

**Files:**
- `src/editor/ui/EditorInspectorPanel.cpp` lines 24-41

**Impact:**
- State persists across scene changes; can become stale if environment/material definitions are reloaded
- Cannot be reset or inspected by tests
- Hides dependency on `MaterialTextureLibrary` and `EditorAssetPreviewRenderer`, making ownership unclear
- Makes hot-reload of material assets fragile — dirty flags may not be properly synchronized

**Fix approach:**
- Convert `AssetInspectorSession` to an explicit member of the panel manager class
- Add lifecycle methods (`initialize()`, `shutdown()`, `reset()`)
- Make dirty-flag synchronization explicit: when `ContentRegistry` signals a material hot-reload, the session must reacquire the draft

---

### Complex Serialization Logic Scattered Across LevelDef

**Problem:** Parse/serialize logic for the `.scene` file format is distributed across hundreds of lines with limited error recovery. Custom tokenization and state machines make it fragile.

**Files:**
- `src/game/level/LevelDef.cpp` (1490 lines)
  - Lines 24-72: Parse helpers and format helpers
  - Lines 100-320: Behavior/action/interactable parsing
  - Lines 469-600: Main level file parsing
  - Lines 1350-1490: Serialization and save

**Impact:**
- A single malformed line can cause cascading parse failures
- Adding a new entity kind requires updates in 4+ locations (parse, serialize, validation, round-trip tests)
- Error messages don't indicate what was expected vs. received
- Round-trip fidelity depends on format string precision (`formatFloat`) and serialization order

**Fix approach:**
- Extract a simple `TokenStream` class with lookahead and error reporting
- Create a unified `EntityDescriptor` enum + visitor pattern for parsing/serializing all entity kinds
- Add round-trip validation tests for each entity type
- Document the `.scene` format grammar formally (BNF or EBNF)

---

### LevelDef <-> EditorSceneDocument Bidirectional Conversion

**Problem:** `EditorSceneDocument` wraps `LevelDef` concepts (meshes, lights, colliders) but the two types have different ownership semantics. The conversion `toLevelDef()` and loading from file happen in different places, risking data loss.

**Files:**
- `src/editor/scene/EditorSceneDocument.h` lines 110, 112-113
- `src/editor/scene/EditorSceneDocument.cpp` (924 lines)
- `src/game/level/LevelBuilder.cpp` (278 lines)

**Impact:**
- A property set in the editor may not round-trip if the serialization code hasn't been updated
- Duplicate parent ID tracking: `LevelDef` has string-based node IDs, `EditorSceneDocument` tracks hierarchy separately
- If a conversion path is missed, the editor can silently lose data

**Fix approach:**
- Define a canonical **intermediate representation** that both can serialize to/from
- Add explicit validation after each `toLevelDef()` call
- Use a diff-based save: only write changed fields to `.scene` files to preserve unknown future fields
- Add a round-trip test that loads a `.scene`, modifies it in the editor, saves, and re-loads

---

## Missing Test Coverage

### Behavior System and Action Execution

**Problem:** The behavior trigger/action system is core to gameplay but lacks unit test coverage for execution semantics.

**Files:**
- `src/game/behavior/BehaviorSystem.h` (54 lines)
- `src/game/behavior/BehaviorSystem.cpp` (361 lines)

**Risk:**
- `ActionEntry` firing logic (`fireOnce`, `fired`, delayed actions) is complex and easy to break
- Changes to `executeAction()` or `executeActionList()` timing can silently corrupt behavior trees
- Edge cases (e.g., action targeting a deleted entity, stale pending actions after level reload) are undiscovered

**Untested patterns:**
- Entity deletion while an action is pending
- Rapid re-activation of the same trigger before delayed actions fire
- Mixed fireOnce + delay behavior
- Invalid target node IDs in action parameters

**Improvement:** Add targeted tests in `tests/game/test_behavior_*.cpp`:
- Test pending action queue ordering
- Test fire-once semantics with delayed actions
- Test entity-not-found gracefully
- Test action parameter validation

---

### Editor Command Stack (Undo/Redo)

**Problem:** `EditorCommand` and the undo/redo system are lightly tested. Complex commands (transform gizmo, multi-object selection changes) can break undo state.

**Files:**
- `src/editor/core/EditorCommand.h` (150+ lines)
- `src/editor/core/EditorRuntimePreviewSession.h` (100+ lines)

**Risk:**
- Undo/redo state can become inconsistent if a command's `redo()` doesn't perfectly mirror the original action
- Selection state, viewport camera, and preview session state are not always captured in commands
- Test coverage in `tests/editor/test_editor_command_stack.cpp` is basic

**Improvement:**
- Add tests for complex multi-step sequences: select → move → scale → undo all
- Add serialization of command stack for debugging
- Capture and assert viewport state in undo/redo tests

---

### Physics Integration

**Problem:** Physics system integration with the editor preview is not well tested. Moving/scaling colliders in the editor preview can leave physics state inconsistent.

**Files:**
- `src/engine/physics/PhysicsSystem.cpp` (633 lines, mostly Jolt integration)
- `src/editor/scene/EditorPreviewWorld.cpp` (431 lines)

**Risk:**
- If an editor command changes an entity's transform while physics is active in preview, the Jolt body may be out of sync
- Collider shape changes (box ↔ sphere) may not rebuild the physics body
- Sensor vs. solid mode changes are not tested

**Improvement:**
- Add integration tests that spawn a physics body, modify its transform via command, and verify Jolt state matches
- Test shape kind transitions
- Test editor preview → runtime preview physics consistency

---

## Fragile Areas

### EditorSceneDocument Parenting Logic

**Files:** `src/editor/scene/EditorSceneDocument.cpp` lines 400-600 (parenting operations)

**Why fragile:**
- Parenting operations mutate `parentNodeId` strings in-place without validation
- Parent-child relationships are stored in two ways: as string pointers and as implicit tree traversal
- Circular parent assignments can crash: `setParent(A, B)` then `setParent(B, A)`
- `canSetParent()` validation is a linear scan of the hierarchy; adding 1000 objects slows down every parent set

**Safe modification:**
- Always call `canSetParent()` before `setParent()`
- Add an invariant check after bulk operations: iterate all objects and verify no cycles
- Add tests for pathological cases: deeply nested hierarchies, reparenting subtrees

---

### Material Definition Inheritance

**Files:**
- `src/game/rendering/MaterialDefinition.cpp` (491 lines)
- `src/game/rendering/MaterialTextureLibrary.cpp` (787 lines)
- `src/game/content/ContentRegistry.cpp` lines 200-280 (validation & hot-reload)

**Why fragile:**
- Material parent references are resolved at parse time; if a parent is deleted after loading, the child is orphaned
- Hot-reload (`.pollMaterialHotReload()`) only checks file modification time, not the content hash — renaming a parent material can silently break children
- The `MaterialTextureLibrary` caches lookups but doesn't invalidate when materials change
- Circular inheritance is possible: `mat_a` extends `mat_b`, `mat_b` extends `mat_a`

**Safe modification:**
- Add cycle detection in `validateMaterialInheritance()`
- Before hot-reload, collect all children of a changed material and mark them dirty too
- Store material definition hashes in `ContentRegistry`; only rebuild if the hash changes, not just mtime
- Add tests for inheritance chains 5+ levels deep

---

### Shader Compilation Error Handling

**Files:** `src/engine/rendering/core/Shader.cpp` lines 10-72

**Why fragile:**
- If vertex and fragment shader compile successfully but linking fails, the GL program is destroyed but the error is logged to spdlog, not returned to the caller
- The constructor throws `std::runtime_error`, but the exception message doesn't include the shader paths, making it hard to diagnose which shader pair failed in a build with many shaders
- Hot-reloading shaders in the editor can succeed for the asset but fail when the pipeline binds it

**Safe modification:**
- Add shader name/path to the exception message
- Return a `std::optional<std::unique_ptr<Shader>>` from `load()` methods instead of throwing
- Add a post-link validation: try binding the program and checking for uniform conflicts
- Test: Load invalid shader → catch exception → reload valid shader → verify pipeline still works

---

## Performance Bottlenecks

### ECS Query Iteration in Tight Loops

**Problem:** Many systems iterate the same component views every frame. No caching of view handles.

**Files:**
- `src/game/behavior/BehaviorSystem.cpp` lines 59-73 (per-frame view iteration)
- `src/game/rendering/RuntimeSceneRenderer.cpp` (queries for every object type)

**Impact:** Minor in current game size, but scales poorly if entity counts grow (1000+ entities).

**Improvement:**
- Cache view handles in system state, re-use across frames
- Profile: measure the cost of `registry.view<>()` construction vs. iteration

---

### Material Texture Library Lookup

**Files:** `src/game/rendering/MaterialTextureLibrary.cpp` (787 lines)

**Problem:** Material texture lookup is a string-key unordered_map search. Textures are bound in the scene render loop per-object.

**Impact:** For 100+ objects with 5+ textures each, this is O(n*m) texture binds per frame.

**Improvement:**
- Pre-bind texture atlases per material kind
- Use material ID → texture handles mapping instead of string names
- Measure actual frame time cost

---

### Framebuffer Resizing in Render Loop

**Files:** `src/engine/rendering/SceneRenderPipeline.cpp` lines 1-100 (framebuffer setup)

**Problem:** `ensureFramebuffers()` can reallocate GPU memory if the window is resized. If called every frame with fractional resolutions, it will thrash.

**Impact:** Window resize → render stutter for several frames.

**Fix approach:**
- Cache the last framebuffer dimensions; only reallocate if width or height changes by more than a threshold
- Use `VSync` or a frame-rate cap to smooth resize transitions

---

## Security Considerations

### File I/O Without Path Validation

**Problem:** File paths are resolved relative to the asset directory without canonicalization checks.

**Files:**
- `src/game/level/LevelDef.cpp` line 470 (open level file)
- `src/game/rendering/MaterialTextureLibrary.cpp` (stbi_load texture files)
- `src/engine/rendering/assets/GltfLoader.cpp` (load glTF files)

**Risk:**
- Malicious `.scene` file could contain paths like `../../etc/passwd` or symlinks to outside the asset directory
- Not a critical issue in a single-player offline game, but could be exploited by mods or user-created content

**Mitigation:**
- Use `std::filesystem::weakly_canonical()` to resolve symlinks
- Ensure resolved path is within the game asset directory
- Log rejected paths for debugging

---

### Unchecked stbi_image Data

**Files:** `src/game/rendering/MaterialTextureLibrary.cpp` line 150+ (stbi_load)

**Problem:** Texture files are loaded with no validation of image dimensions or format.

**Risk:**
- A pathological PNG (1M x 1M pixels) could exhaust GPU VRAM
- stbi_load could fail silently if the file is corrupted

**Mitigation:**
- Clamp loaded image dimensions to reasonable limits (e.g., 8192x8192)
- Check `stbi_load()` return value and log failures
- Add fallback texture for missing/corrupt files

---

## Dependencies at Risk

### Assimp (Deprecated Path for New Code)

**Files:** `src/engine/rendering/assets/AssimpLoader.cpp` (345 lines)

**Risk:** Assimp is used for FBX/legacy format loading, but is 1M+ lines of code with a large attack surface. It's no longer actively maintained (last release 2023).

**Current status:** Acceptable for loading editor assets at dev time, but should not be in the runtime game binary.

**Mitigation:**
- Keep Assimp in editor-only code path
- For game delivery, pre-bake all meshes to glTF during build
- Monitor Assimp security announcements; consider forking if issues arise

---

### Jolt Physics with Double Precision

**Files:** `src/engine/physics/PhysicsSystem.cpp` lines 37-42

**Problem:** Code conditionally uses `JPH::RVec3` (double) vs. `JPH::Vec3` (float) depending on `JPH_DOUBLE_PRECISION` flag.

**Risk:**
- If the Jolt library is compiled with double precision but the engine uses float, conversions will lose precision
- Vice versa: if engine uses double but Jolt uses float, the conversion is silent data loss

**Mitigation:**
- Document the expected Jolt compilation flags in the build instructions
- Add a compile-time assertion: `static_assert(sizeof(JPH::Vec3) == 12, "Expected 32-bit float vectors");`
- Test: Load a physics-heavy scene and verify no jitter or precision loss

---

### OpenGL 4.1 Ceiling (macOS)

**Problem:** All shaders target GLSL 4.10 because macOS deprecated OpenGL at 4.1 in 2018. Newer GL features are unavailable.

**Files:** All shader files in `assets/shaders/`

**Risk:** If the game is ported to Windows/Linux, there's an incentive to use newer GL features (compute shaders, mesh shaders), which will require maintaining two shader codebases.

**Mitigation:**
- Document that any new feature must work on GLSL 4.10
- Consider WebGPU or Vulkan as a future rendering backend if extensibility becomes critical

---

## Scaling Limits

### Editor Preview World Complexity

**Problem:** The editor spawns a full `RuntimeGameSession` for preview. With 500+ objects in a scene, preview builds are slow.

**Files:** `src/editor/core/EditorRuntimePreviewSession.h`, `src/editor/scene/EditorPreviewWorld.cpp`

**Current behavior:**
- Preview spawns all entities from the scene
- All systems run (physics, behavior, rendering)
- For large scenes, preview stalls for 500ms+ on rebuild

**Improvement path:**
- Lazy-load entities: only spawn visible viewport bounds
- Add a "preview LOD" mode that skips distant entities
- Cache the previous preview build; only rebuild changed entities

---

### Level File Size

**Problem:** `.scene` files are text-based, one entity per line. A level with 1000+ entities creates a 500KB+ file.

**Files:** `src/game/level/LevelDef.cpp` serialization (lines 1350-1490)

**Risk:**
- Loading becomes slow (file I/O, parsing)
- Version control diffs become unreadable
- Round-trip fidelity degrades with floating-point precision

**Improvement path:**
- Evaluate binary format (UBJSON, MessagePack, or Protobuf)
- Compress `.scene` files with zstd on disk
- Add delta encoding for repeated transform values

---

## Architectural Concerns

### Editor <-> Game Layer Coupling

**Problem:** The editor layer directly imports game layer types (`RuntimeGameSession`, `LevelDef`, `BehaviorComponent`). This makes it hard to use the game layer without the editor.

**Files:**
- `src/editor/scene/EditorSceneDocument.h` lines 3-4 (imports `LevelDef`, `EnvironmentDefinition`)
- `src/editor/core/EditorRuntimePreviewSession.h` (imports `RuntimeGameSession`)

**Impact:**
- Game executable has editor dependencies
- Changing a game component requires recompiling editor
- Game cannot be packaged without editor infrastructure

**Ideal state:**
- **Game layer** = engine + game components + systems (no editor)
- **Editor layer** = engine + editor UI + game layer (depends on game)
- Use forward declarations and interfaces where editor needs game layer types

**Fix approach:**
- Create an abstract `IGameSession` interface in the game layer
- Have `RuntimeGameSession` implement it
- Editor imports the interface, not the concrete class
- Move game-agnostic logic from editor to engine layer (e.g., `EditorCommand` base class)

---

### RuntimeGameSession Knows About Content Registry

**Files:** `src/game/runtime/RuntimeGameSession.h` line 91 (ContentRegistry* content_)

**Problem:** `RuntimeGameSession` holds a pointer to the `ContentRegistry` but doesn't own it. If the registry is freed while the session is active, the pointer dangles.

**Impact:**
- No explicit lifetime contract; whoever created the session must keep the registry alive
- Makes testing difficult: tests must manage both lifetimes

**Fix approach:**
- Pass a `const ContentRegistry&` to `rebuild()` instead of storing the pointer
- If async asset loading is needed in the future, use a shared_ptr with reference counting

---

## Missing Abstractions

### No Unified Trigger/Event System

**Problem:** Behavior triggers use multiple mechanisms: `ColliderComponent::pendingEnter/Exit` (flags), `InteractionFocusState::activationRequested` (state in context), `PendingAction` (explicit queue).

**Files:**
- `src/game/behavior/BehaviorSystem.cpp` (70 lines of trigger processing)
- `src/game/components/ColliderComponent.h`
- `src/game/ui/InteractionFocusState.h`

**Impact:**
- Adding a new trigger type (e.g., `OnTimer`, `OnPropertyChange`) requires modifying `BehaviorSystem` in multiple places
- No unified deduplication: the same trigger can fire multiple times per frame if not careful

**Improvement:**
- Create a `TriggerEvent` type that unifies all trigger kinds
- Publish triggers on the `EventBus` instead of checking flags
- `BehaviorSystem` subscribes to trigger events and executes actions
- This also enables non-behavior systems to listen to triggers

---

### No Component Validation Schema

**Problem:** Components are POD structs with no validation. An interactable can have `distance = -1` or `dotThreshold = 2.0` (invalid).

**Files:** All in `src/game/components/`

**Impact:**
- Invalid data silently behaves wrong at runtime
- Editor UI doesn't enforce constraints
- Tests don't catch invalid setups

**Improvement:**
- Add a validation function per component: `bool validateInteractableComponent(const InteractableComponent& c, std::string& error);`
- Call validation in the level builder and editor UI
- Use constrained types: `struct ConstrainedFloat { float value; float min; float max; };`

---

### No Post-Processing Pipeline Modularity

**Problem:** `SceneRenderPipeline` orchestrates bloom, SSAO, composite, stylize. Adding a new post-pass requires modifying the pipeline class.

**Files:** `src/engine/rendering/SceneRenderPipeline.h` lines 110-129

**Impact:**
- Hard to disable/reorder passes at runtime
- Hard to add experimental passes without core changes
- Pass parameters are copied through, not referenced

**Improvement:**
- Create a `PostProcessPass` interface with `render(input, output)` method
- Store passes in a vector; execute in order
- Allow passes to declare their input/output dependencies

---

## Code Quality Smells

### Excessive Use of std::variant for Type Unions

**Problem:** Action parameters use `std::variant<DoorActionParams, LightActionParams, ...>` (ActionTypes.h has 10+ variants). Every action execution requires a switch on the variant.

**Files:**
- `src/game/behavior/ActionTypes.h`
- `src/game/behavior/BehaviorSystem.cpp` lines 150-280
- `src/editor/ui/EditorInspectorPanel.cpp` lines 230-280

**Impact:**
- Easy to forget a case when adding a new action type
- The variant type is exposed in both game and editor, coupling them
- Editor authoring UI has long series of `std::holds_alternative<>` checks

**Improvement:**
- Use a virtual interface `ActionParams { virtual void execute(...) = 0; };`
- Each action type is a concrete class implementing the interface
- Reduces coupling between action types

---

### String-Based IDs Without Validation

**Problem:** Node IDs, material IDs, archetype IDs are all unvalidated strings. A typo in a reference silently results in "not found."

**Files:**
- `src/game/level/LevelDef.h` (node IDs as strings)
- `src/game/behavior/ActionTypes.h` (targetNodeId as string)
- `src/editor/scene/EditorSceneDocument.cpp` (validation at lines 218-229)

**Impact:**
- Broken references are hard to debug
- Renaming an ID requires manual find/replace across multiple systems
- No compile-time checking

**Improvement:**
- Use a `NodeID` strong type: `struct NodeID { std::string value; };`
- Add a centralized validation step: `bool resolveNodeID(const std::string& id, entt::entity& out);`
- For critical IDs (archetypes, materials), consider enums or constants

---

## Test Coverage Gaps

### No Round-Trip Tests for Complex Objects

**Problem:** Tests check that a `.scene` file loads and that an entity renders, but don't verify that saving and re-loading preserves all state.

**Files:**
- `tests/game/test_level_roundtrip.cpp` (basic)
- `tests/game/test_level_def.cpp` (basic)

**Missing:**
- Round-trip tests for behaviors with complex action parameters
- Round-trip tests for material inheritance chains
- Round-trip tests for deeply nested group hierarchies
- Diff-based testing: load A, serialize to string S1, load from S1, serialize to S2, assert S1 == S2

---

### No Stress Tests

**Problem:** No tests for pathological scenarios.

**Missing tests:**
- 10,000 entities in one scene
- Circular behavior dependencies (entity A triggers entity B which triggers entity A)
- Very deep group hierarchies (100+ levels)
- Massive material inheritance chains (50+ levels)
- Very large `.scene` files (10MB+)

---

## Recommended Priority Fixes

**High Priority (Maintainability & Stability):**
1. Break down large UI panel files (EditorInspectorPanel, EditorAssetBrowserPanel)
2. Add serialization round-trip tests (all entity types, behaviors, materials)
3. Convert static `AssetInspectorSession` to explicit member with lifecycle

**Medium Priority (Extensibility):**
4. Extract unified trigger event system
5. Refactor action parameters to use virtual interface instead of variant
6. Add component validation schema

**Low Priority (Performance & Scale):**
7. Cache ECS view handles in tight loops
8. Optimize material texture library lookups
9. Implement level preview LOD system

---

*Concerns audit: 2026-04-05*
