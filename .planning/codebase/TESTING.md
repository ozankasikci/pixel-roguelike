# Testing Patterns

**Analysis Date:** 2026-04-05

## Test Framework

**Runner:**
- Custom standalone executables (no external test framework)
- `tests/cmake/TestSupport.cmake` provides `pixel_roguelike_add_test()` macro
- Each test compiles to a binary; exit code = pass/fail (0 = success, non-zero = failure)

**Assertion Library:**
- `#include <cassert>` only — plain C++ `assert()` macro
- Custom helper functions in `tests/common/TestSupport.h` for floating-point and vector comparisons
- No external testing library (no Google Test, Catch2, or Doctest)

**Run Commands:**
```bash
ctest                                    # Run all tests
ctest -L engine                          # Run tests labeled "engine"
ctest -L game                            # Run tests labeled "game"
ctest -L editor                          # Run tests labeled "editor"
./build-test/tests/engine/test_level_def  # Run specific test executable
```

## Test File Organization

**Location:**
- Separate from source: `tests/engine/`, `tests/game/`, `tests/editor/` parallel to `src/` layers
- Data files in `tests/data/` (scene files, model files, test assets)

**Naming:**
- `test_*.cpp` for all test files — examples: `test_level_def.cpp`, `test_model_loader.cpp`, `test_editor_command_stack.cpp`
- Profile tests use `profile_*.cpp`: `profile_play_preview.cpp`

**Structure by Layer:**
```
tests/
├── common/
│   ├── TestSupport.h          # Helper functions (nearlyEqual, tempPath, resetTempDirectory)
│   └── cmake/
│       └── TestSupport.cmake   # CMake functions (pixel_roguelike_add_test, pixel_roguelike_add_profile)
├── engine/
│   ├── CMakeLists.txt
│   ├── test_mesh_geometry.cpp
│   ├── test_model_discovery.cpp
│   ├── test_model_loader.cpp
│   ├── test_asset_cache.cpp
│   └── test_questdoor_scale.cpp
├── game/
│   ├── CMakeLists.txt
│   ├── test_level_def.cpp
│   ├── test_level_lighting.cpp
│   ├── test_content_registry.cpp
│   ├── test_equipment_state.cpp
│   ├── test_cathedral_prefabs.cpp
│   ├── test_material_definitions.cpp
│   ├── test_silos_cloister_level.cpp
│   ├── test_level_roundtrip.cpp
│   ├── test_behavior_trigger_roundtrip.cpp
│   ├── test_environment_profiles.cpp
│   ├── test_environment_debug_sync.cpp
│   └── test_runtime_game_session.cpp
└── editor/
    ├── CMakeLists.txt
    ├── test_command_registry.cpp
    ├── test_editor_asset_browser.cpp
    ├── test_editor_command_stack.cpp
    ├── test_editor_debug_commands.cpp
    ├── test_editor_hierarchy.cpp
    ├── test_editor_layout_presets.cpp
    ├── test_editor_runtime_preview.cpp
    ├── test_editor_scene_document.cpp
    ├── test_editor_selection.cpp
    ├── test_editor_ui_preferences.cpp
    ├── test_runtime_preview_quality.cpp
    ├── test_session_record_replay.cpp
    └── profile_play_preview.cpp
```

## Test Structure

**Suite Organization:**
```cpp
int main() {
    // Setup phase
    const auto data = loadLevelDef(CATHEDRAL_SCENE_FILE);
    
    // Assertion sequence (no test framework grouping)
    assert(data.environmentProfile == EnvironmentProfile::Default);
    assert(data.meshes.size() == 126);
    assert(data.lights.size() == 13);
    
    // Conditional logic for complex checks
    assert(std::count_if(data.colliders.begin(), data.colliders.end(), 
        [](const LevelColliderPlacement& c) { return c.shape == ColliderShape::Box; }) == 25);
    
    // Return 0 on success (exit code signals pass)
    return 0;
}
```

**Patterns:**
- No test classes or fixtures (procedural style)
- Single `main()` function per test file
- Assertions executed sequentially; first failure terminates program with non-zero exit
- Local helper functions in anonymous namespace: `computeMin()`, `computeMax()` in `test_model_loader.cpp`
- Ternary construction for readable assertions: `const LevelDef level = resolveLevelHierarchy(...)`

## Mocking

**Framework:** None detected (no Gtest, Catch2, or manual mock infrastructure)

**Patterns:**
- **Constructor injection**: Systems take dependencies as references in constructor
  - Example: `PlayerMovementSystem(InputSystem& input, PhysicsSystem& physics);` in `src/game/systems/PlayerMovementSystem.h`
  - Tests can construct with real implementations or custom instances

**What to Mock:**
- Heavy I/O operations (file loading is tested directly, not mocked)
- Physical simulation (tests exercise real Jolt Physics or use simplified test data)

**What NOT to Mock:**
- ECS operations (`entt::registry`) — tested directly with real components
- Mesh/shader loading — tested with actual asset files
- Level loading — tested with real `.scene` files in `tests/data/`

**Integration test approach:**
- Tests construct real objects and exercise their interaction
- Example: `test_editor_command_stack.cpp` creates `EditorSceneDocument`, adds mesh, runs undo/redo on real state
- Example: `test_runtime_game_session.cpp` spawns real game session with level loading

## Fixtures and Factories

**Test Data:**
```cpp
// Named helper functions in anonymous namespace
namespace {

LevelMeshPlacement makeMeshPlacement() {
    LevelMeshPlacement placement;
    placement.meshId = "cube";
    placement.materialId = "stone_default";
    placement.position = glm::vec3(0.0f);
    placement.scale = glm::vec3(1.0f);
    placement.rotation = glm::vec3(0.0f);
    return placement;
}

glm::vec3 computeMin(const RawMeshData& mesh) {
    assert(!mesh.positions.empty());
    glm::vec3 result = mesh.positions.front();
    for (const auto& position : mesh.positions) {
        result = glm::min(result, position);
    }
    return result;
}

} // namespace
```

**Location:**
- Inline in test .cpp files (no separate fixture library)
- Helper functions in `tests/common/TestSupport.h`:
  - `nearlyEqual(float a, float b, float epsilon)` — floating-point comparison with tolerance
  - `nearlyEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon)` — vector comparison
  - `tempPath(std::string_view leaf)` — construct temp directory path
  - `resetTempDirectory(std::string_view leaf)` — create/clear temp directory

**Test Assets:**
- Real files referenced via compile-time defines:
  - `CATHEDRAL_SCENE_FILE` → `${CMAKE_SOURCE_DIR}/assets/scenes/cathedral.scene`
  - `SILOS_CLOISTER_SCENE_FILE` → `${CMAKE_SOURCE_DIR}/assets/scenes/silos_cloister.scene`
  - `MATERIAL_BRICK_FILE` → `${CMAKE_SOURCE_DIR}/assets/materials/brick_default.material`
  - Defined in `tests/game/CMakeLists.txt` as `DEFINITIONS` passed to `pixel_roguelike_add_test()`
  - Example: `test_level_def.cpp` uses `loadLevelDef(CATHEDRAL_SCENE_FILE)` where `CATHEDRAL_SCENE_FILE` is a macro

## Coverage

**Requirements:** Not enforced (no coverage target in CMakeLists.txt)

**View Coverage:** Not supported natively (no lcov, gcov integration)

## Test Types

**Unit Tests:**
- **Scope**: Single class or function in isolation
- **Examples**:
  - `test_mesh_geometry.cpp` — tests `Mesh` construction from raw vertex data
  - `test_asset_cache.cpp` — tests `AssetCache<Mesh>` caching behavior
  - `test_equipment_state.cpp` — tests `EquipmentState` struct serialization/deserialization
  - **Approach**: Construct object, call methods, assert on return values and side effects

**Integration Tests:**
- **Scope**: Multiple layers interacting (ECS, systems, content registry, etc.)
- **Examples**:
  - `test_level_def.cpp` — loads cathedral.scene file, validates mesh/light/collider counts and specific placements
  - `test_level_roundtrip.cpp` — loads level from file → converts to ECS entities → serializes back → compares
  - `test_runtime_game_session.cpp` — spawns full `RuntimeGameSession`, runs gameplay systems for a frame
  - `test_editor_command_stack.cpp` — creates document, performs edits, executes undo/redo, validates state
  - **Approach**: Use real implementations of dependencies; exercise cross-layer communication

**E2E Tests:**
- **Framework or "Not used"**: Implicit via level_editor and runtime binaries; no formal E2E test suite
- Profile tests (`profile_play_preview.cpp`) load editor, play preview session, profile frame times — closest to E2E

## Common Patterns

**Assertion patterns in test_level_def.cpp:**
```cpp
// Direct equality
assert(data.environmentProfile == EnvironmentProfile::Default);
assert(data.meshes.size() == 126);

// Container counting with predicates
assert(std::count_if(data.colliders.begin(), data.colliders.end(),
    [](const LevelColliderPlacement& c) { return c.shape == ColliderShape::Box; }) == 25);

// Existence checking
assert(std::any_of(data.meshes.begin(), data.meshes.end(),
    [](const LevelMeshPlacement& mesh) { return mesh.position == glm::vec3(-9.0f, 4.5f, -6.5f); }));

// Floating-point comparison with tolerance
assert(test_support::nearlyEqualVec3(gltfMin, fbxMin, 0.02f));

// Optional value checking
assert(mesh.tint.has_value() && *mesh.tint == glm::vec3(0.60f, 0.45f, 0.29f));
```

**State roundtrip pattern in test_editor_command_stack.cpp:**
```cpp
// Capture initial state
const EditorSceneDocumentState before = document.captureState();

// Mutate object
auto* object = document.findObject(meshId);
auto& mesh = std::get<LevelMeshPlacement>(object->payload);
mesh.position.x = 2.5f;
document.markSceneDirty();

// Push command with before/after states
const bool pushed = stack.pushDocumentStateCommand("Move Mesh", before, document.captureState(), document);
assert(pushed);

// Test undo
const bool undone = stack.undo(document);
assert(undone);
assert(test_support::nearlyEqual(mesh.position.x, 0.0f));
```

**Async Testing:** Not applicable (no async/concurrent code in main engine)

**Error Testing:**
- Limited error path testing observed
- `test_asset_cache.cpp` tests "asset not found" path
- Most tests focus on happy path (success cases)

## Test Organization & Execution

**CMake test function:**
```cmake
pixel_roguelike_add_test(target
    SOURCES test_file.cpp
    LIBRARIES library1 library2
    DEFINITIONS ASSET_PATH="/path"
    LABELS engine
)
```

**Test registration:**
- All tests registered with `add_test()` via `pixel_roguelike_add_test()` macro
- CTest discovers and runs all via `ctest` command
- Labels enable filtering: `ctest -L engine` runs only engine tests

**Test isolation:**
- Each test is standalone executable with its own `main()`
- No shared state between tests
- Tests can run in parallel (independent binaries)

## Coverage Gaps & Quality Issues

**Gap 1: Error path testing is minimal**
- Most tests exercise happy path (load succeeds, parse succeeds, round-trip succeeds)
- **Files affected**: `src/engine/rendering/assets/GltfLoader.cpp`, `src/game/content/ContentRegistry.cpp`
- **Risk**: Loading errors (corrupted files, missing assets) may not be caught until runtime
- **Recommendation**: Add tests for invalid input (malformed JSON, missing mesh references)

**Gap 2: No mocking/test doubles for complex systems**
- `PhysicsSystem` tests use real Jolt Physics (slow, brittle)
- `AudioSystem` not tested
- **Files affected**: `src/engine/physics/`, `src/engine/audio/`
- **Recommendation**: Create test doubles for physics/audio to enable faster unit tests

**Gap 3: Shader compilation not tested**
- `Shader` class has error handling for GL_COMPILE_ERROR but no tests verify it
- **Files affected**: `src/engine/rendering/core/Shader.cpp`
- **Recommendation**: Add test that verifies exception is thrown on shader compile failure

**Gap 4: Memory/lifetime issues hard to detect**
- Non-copyable/move-only patterns enforced via `= delete` but tests don't explicitly verify
- Raw pointer non-ownership not verified by tests
- **Files affected**: All GPU resource RAII classes (`Shader`, `Framebuffer`, `Mesh`)
- **Recommendation**: Consider AddressSanitizer in CI; add lifetime validation tests

**Gap 5: No performance regression tests**
- Profile test `profile_play_preview.cpp` exists but results not tracked
- **Recommendation**: Add performance baseline tracking (frame time, memory usage)

**Gap 6: Editor debug harness tested minimally**
- Unix socket protocol in `src/editor/debug/DebugServer.cpp` has 33 commands but only ~5 tested
- **Files affected**: `src/editor/debug/`
- **Risk**: Debug protocol drift from implementation; untested commands may bitrot
- **Recommendation**: Add command-by-command roundtrip tests for harness

**Gap 7: Integration between game systems untested**
- `PlayerMovementSystem` + `PhysicsSystem` interaction tested indirectly via `test_runtime_game_session.cpp`
- Specific failure modes (e.g., character falls through collider) not isolated
- **Recommendation**: Add targeted tests for system interaction edge cases

**Gap 8: Test coverage for string parsing**
- Material definitions, level files, and behavior declarations use custom string parsers
- Limited error path testing for malformed input
- **Files affected**: `src/game/rendering/MaterialDefinition.cpp`, `src/game/level/LevelDef.cpp`, `src/game/content/ContentRegistry.cpp`
- **Recommendation**: Add negative tests (invalid enum values, missing fields, etc.)

## Test Count & Distribution

- **Engine layer**: 5 tests (asset loading, mesh geometry, model discovery, asset caching, scale validation)
- **Game layer**: 13 tests (level loading, content, prefabs, serialization, environment, equipment, gameplay)
- **Editor layer**: 11 tests (commands, UI, selection, hierarchy, debug harness, preview quality)
- **Total**: 29 standalone test executables

## Framework Assessment

**Strengths:**
- No external dependencies (no test framework to install/maintain)
- Fast compilation (each test is small, standalone)
- Clear pass/fail semantics (exit code)
- Parallelizable (independent binaries, no shared fixtures)
- Integration-focused (tests real interactions, not mocks)

**Weaknesses:**
- No parametrized tests (hard to test multiple input cases)
- No test descriptions/reporting beyond binary pass/fail
- No setup/teardown hooks (everything inline)
- Hard to debug failure (no assertion messages beyond `assert(0)`)
- No test grouping (all tests in CMake `add_test()` have flat namespace)
- Error messages from failed assertions are cryptic

**Migration path:**
- If test complexity grows, migrate to Google Test or Catch2
- Current framework sufficient for ~30 tests; becomes unwieldy at 100+

## Quality Metrics

| Metric | Value | Assessment |
|--------|-------|------------|
| Test count | 29 | Small but focused suite |
| Framework overhead | Minimal | No external dependencies |
| Average test size | ~50 lines | Concise, readable |
| Test execution time | <5s total | Fast feedback loop |
| Parametrization support | None | Single code path per test |
| Mock support | None | Integration-focused |
| Coverage reporting | None | Not tracked |
| Assertion messages | None | Exit code only |

---

*Testing analysis: 2026-04-05*
