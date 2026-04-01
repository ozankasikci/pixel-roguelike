# Testing Patterns

**Analysis Date:** 2026-04-01

## Test Framework

**Runner:**
- Custom CMake-based: no external test framework (no Catch2, gtest, doctest)
- Each test is a standalone executable; exit code 0 = pass, non-zero = fail
- CTest collects and runs all executables via `add_test()`
- Config: `tests/cmake/TestSupport.cmake` — defines `pixel_roguelike_add_test()` and `pixel_roguelike_add_profile()`

**Assertion Library:**
- `<cassert>` exclusively — `assert()` macros
- No matcher library; float comparisons use `test_support::nearlyEqual()` from `tests/common/TestSupport.h`

**Run Commands:**
```bash
cmake -B build-test -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test
cd build-test && ctest                     # Run all tests
ctest -L game                              # Run game-labelled tests only
ctest -L editor                            # Run editor-labelled tests only
ctest -L engine                            # Run engine-labelled tests only
ctest --output-on-failure                  # Show output for failing tests
```

## Test File Organization

**Location:**
- Separate from source, under `tests/` — not co-located with source files
- Mirror of the source layer structure: `tests/engine/`, `tests/game/`, `tests/editor/`

**Naming:**
- Files: `test_<subject>.cpp` — e.g., `test_level_def.cpp`, `test_material_definitions.cpp`
- CMake target: same as filename without `.cpp` — e.g., target `test_level_def`
- CTest name: strips `test_` prefix — e.g., CTest name `level_def`

**Structure:**
```
tests/
├── cmake/
│   └── TestSupport.cmake    # pixel_roguelike_add_test(), pixel_roguelike_add_profile()
├── common/
│   └── TestSupport.h        # nearlyEqual(), nearlyEqualVec3(), tempPath(), resetTempDirectory()
├── data/
│   ├── light_records.scene  # Test fixture scene
│   └── models/
│       └── pillar_import_test.fbx
├── engine/                  # 4 tests
├── game/                    # 11 tests
└── editor/                  # 6 tests + 1 profile
```

**Total test count:** 21 test executables + 1 profiling executable

## Test Structure

**Suite Organization:**
Each test file is a single `main()` function. Related assertions are grouped into anonymous scopes using `{}` blocks. No named sub-tests or test cases — scope blocks serve as logical groupings:

```cpp
int main() {
    // Test: base case
    const auto base = loadMaterialDefinitionAsset(MATERIAL_BASE_FILE);
    assert(base.id == "masonry_base");

    // Test: error handling — missing parent throws
    {
        std::unordered_map<std::string, MaterialDefinition> missingParent;
        // ...
        bool threw = false;
        try { (void)resolveMaterialDefinition("child", missingParent); }
        catch (const std::runtime_error&) { threw = true; }
        assert(threw);
    }

    // Test: roundtrip serialization
    {
        MaterialDefinition roundtrip;
        // ... set fields ...
        saveMaterialDefinitionAsset(path, roundtrip);
        const auto loaded = loadMaterialDefinitionAsset(path);
        assert(loaded.id == roundtrip.id);
    }

    return 0;
}
```

**Patterns:**
- No setup/teardown functions — each `{}` block is self-contained
- Temp files created via `test_support::tempPath()` and deleted manually with `std::filesystem::remove()`
- Temp directories via `test_support::resetTempDirectory()` which deletes and recreates
- No shared global state between test groups (within one test exe)

## CMake Test Registration

**`pixel_roguelike_add_test()` signature:**

```cmake
pixel_roguelike_add_test(<target>
    SOURCES <files>
    LIBRARIES <targets>
    [DEFINITIONS <defines>]   # For injecting asset file paths
    [LABELS <labels>]         # "game", "editor", "engine"
    [TEST_NAME <name>]        # Override default CTest name
)
```

**Asset path injection via `DEFINITIONS`:**
Test files that load real assets receive absolute paths as compile-time `#define` macros:

```cmake
# CMakeLists.txt
set(CATHEDRAL_SCENE_FILE_DEF
    CATHEDRAL_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/cathedral.scene"
)

pixel_roguelike_add_test(test_level_def
    SOURCES test_level_def.cpp
    LIBRARIES game_content
    DEFINITIONS ${CATHEDRAL_SCENE_FILE_DEF}
    LABELS game
)

# test_level_def.cpp
const auto data = loadLevelDef(CATHEDRAL_SCENE_FILE);  // expands to absolute path
```

**`pixel_roguelike_add_profile()` — profiling executables:**
Same as test registration but without `add_test()`, so profiling executables are not part of the CTest suite. Used for `profile_play_preview` (editor preview performance profiling).

## Shared Test Utilities

**`tests/common/TestSupport.h`:**

```cpp
namespace test_support {

inline constexpr float kFloatEpsilon = 0.0001f;

// Float comparison with tolerance
inline bool nearlyEqual(float a, float b, float epsilon = kFloatEpsilon);

// vec3 comparison component-wise with tolerance
inline bool nearlyEqualVec3(const glm::vec3& a, const glm::vec3& b,
                             float epsilon = kFloatEpsilon);

// Temp file path under OS temp dir
inline std::filesystem::path tempPath(std::string_view leaf);

// Delete-and-recreate a temp directory, return its path
inline std::filesystem::path resetTempDirectory(std::string_view leaf);

} // namespace test_support
```

**Local helpers per test file:**
Each test that needs factory helpers defines them in an anonymous `namespace {}` at the top of the file:

```cpp
namespace {

LevelMeshPlacement makeMeshPlacement() {
    LevelMeshPlacement placement;
    placement.meshId = "cube";
    // ...
    return placement;
}

} // namespace
```

## Mocking

**Framework:** None — no mocking library used.

**Strategy:** Real objects are used in tests. The free-function architecture (game logic in `RuntimeGameplay.cpp`) means systems can be tested by constructing minimal `entt::registry` instances and calling the update functions directly, without needing the full `Application` object.

```cpp
// test_runtime_game_session.cpp — direct ECS construction
entt::registry registry;
auto player = registry.create();
registry.emplace<TransformComponent>(player, ...);
registry.emplace<PlayerTag>(player);
// ...
PhysicsSystem physics;
physics.init(registry);
updateRuntimePlayerMovement(registry, input, physics, 1.0f / 60.0f);
```

**What to Mock:** Nothing — real implementations are always used.

**What NOT to Mock:** The test suite runs full physics (`PhysicsSystem` with Jolt), real serialization, and in some cases real OpenGL rendering (`test_editor_runtime_preview` creates a 320x200 `Window` and calls `preview.render()`).

## Fixtures and Factories

**Test Data Files (`tests/data/`):**
- `tests/data/light_records.scene` — synthetic scene for `test_level_lighting`
- `tests/data/models/pillar_import_test.fbx` — FBX model for loader comparison tests

**Real Asset Files (from `assets/`):**
Many tests load production assets directly to validate parsing correctness:
- `assets/scenes/cathedral.scene`
- `assets/scenes/silos_cloister.scene`
- `assets/materials/masonry_base.material`, `brick_default.material`, `brick_wall_old.material`
- `assets/defs/weapons/old_dagger.weapon`, `assets/defs/enemies/sentinel.enemy`
- `assets/prefabs/gameplay/checkpoint.prefab`, `double_door.prefab`

**In-test Constructed Data:**
Tests that need fresh data construct structs inline using designated initializers:

```cpp
level.meshes.push_back(LevelMeshPlacement{
    .meshId = "cube",
    .position = glm::vec3(1.0f, 2.0f, 3.0f),
    .nodeId = "root_mesh",
    .materialId = "brick_default",
    .tint = glm::vec3(0.4f, 0.5f, 0.6f),
});
```

## Coverage

**Requirements:** None enforced — no coverage tool configured in CMake.

**Observed Coverage:**

| Area | Coverage |
|------|----------|
| `MaterialDefinition` serialization and inheritance | High — roundtrip, error paths, inheritance chains |
| `LevelDef` load/save/roundtrip | High — cathedral scene, silos scene, hierarchy |
| `EditorSceneDocument` — hierarchy, parenting, reordering | High |
| `EditorCommandStack` — undo/redo, dirty state | High |
| `ContentRegistry` — weapon/enemy/item/skill/archetype parsing | High |
| `EnvironmentDefinition` / `EnvironmentProfile` | High |
| `MeshGeometry` — cube/plane/cylinder generation, merging | High |
| `AssetCache` — mesh and texture cache read/write | High |
| `ModelLoader` — glTF vs FBX parity | Good |
| `RuntimeGameSession` / `EditorRuntimePreviewSession` — play/reset flow | Good |
| `EquipmentState` — equip/unequip logic | High |
| Rendering systems (`SceneRenderPipeline`, shaders) | None |
| `InputSystem` | None |
| `AudioSystem` | None |
| `Window` / `Application` | None |
| Individual ECS systems (`CameraSystem`, `CheckpointSystem`) | Indirect only (via session tests) |

## Test Types

**Unit Tests (data/logic only):**
Tests that do not require OpenGL or a window. These are the majority:
- `test_mesh_geometry` — pure math, no GL
- `test_level_def`, `test_level_roundtrip`, `test_level_lighting`
- `test_material_definitions`, `test_content_registry`, `test_environment_profiles`
- `test_equipment_state`, `test_cathedral_prefabs`, `test_editor_command_stack`
- `test_editor_hierarchy`, `test_editor_layout_presets`, `test_editor_selection`
- `test_asset_cache`, `test_model_discovery`, `test_model_loader`

**Integration Tests (require OpenGL context / window):**
Tests that create a `Window` (which initializes OpenGL via GLFW):
- `test_editor_runtime_preview` — creates `Window(320, 200, "test")`, builds a full `EditorRuntimePreviewSession`, renders frames, resets state
- `test_runtime_game_session` — requires GLFW for input (`GLFW_KEY_W`, `GLFW_KEY_I`) but does not create a visible window (uses `PhysicsSystem` directly)

**E2E Tests:** Not applicable — game is not web-based; no browser automation.

**Profiling Executables (not in CTest suite):**
- `profile_play_preview` — measures editor preview rebuild/render time

## Common Patterns

**Roundtrip Serialization Testing:**
The dominant pattern for data-layer tests: construct → serialize → deserialize → assert equality.

```cpp
const std::string serialized = serializeLevelDef(level);
assert(serialized.find("material brick_default tint 0.4 0.5 0.6") != std::string::npos);

const LevelDef loaded = loadLevelDef(tempPath);
assert(loaded.meshes.front().materialId == "brick_default");
assert(*loaded.meshes.front().tint == glm::vec3(0.4f, 0.5f, 0.6f));
```

**Error Path Testing:**
Exception-based errors tested via manual `try/catch` + boolean flag:

```cpp
bool threw = false;
try {
    (void)resolveMaterialDefinition("child", missingParent);
} catch (const std::runtime_error&) {
    threw = true;
}
assert(threw);
```

**Simulation Testing (ECS integration):**
For gameplay logic: build a minimal ECS world, run update functions for N frames, assert world state changed:

```cpp
const glm::vec3 startPosition = registry.get<TransformComponent>(player).position;
for (int frame = 0; frame < 12; ++frame) {
    input.setKeyPressed(GLFW_KEY_W, true);
    physics.update(registry, 1.0f / 60.0f);
    updateRuntimePlayerMovement(registry, input, physics, 1.0f / 60.0f);
}
const glm::vec3 movedPosition = registry.get<TransformComponent>(player).position;
assert(glm::length(movedPosition - startPosition) > 0.01f);
```

**Asset Snapshot Testing:**
Some tests encode expected counts/values from real scene files as assertions. These will break if the scene is edited:

```cpp
// test_level_def.cpp — brittle if cathedral.scene is edited
assert(data.meshes.size() == 126);
assert(data.lights.size() == 13);
```

---

*Testing analysis: 2026-04-01*
