# Coding Conventions

**Analysis Date:** 2026-04-01

## Naming Patterns

**Classes/Structs:**
- `PascalCase` matching the header filename: `Application`, `RuntimeGameSession`, `EditorSceneDocument`
- Component structs use `PascalCase` + `Component` suffix: `TransformComponent`, `DoorComponent`, `PlayerMovementComponent`
- Tag/marker components use `PascalCase` + `Tag` suffix: `PlayerTag`, `ControllableTag`, `PrimaryCameraTag`
- Systems use `PascalCase` + `System` suffix: `PlayerMovementSystem`, `PhysicsSystem`
- Data structs omit the Component suffix: `LevelDef`, `MaterialDefinition`, `WeaponDefinition`

**Functions/Methods:**
- `camelCase` for all functions and methods: `isKeyPressed()`, `setCharacterVelocity()`, `captureCamera()`
- Boolean predicates use `is`/`has`/`can` prefix: `isKeyJustPressed()`, `hasPlayerSpawn`, `canSetParent()`
- Factory functions use `create` prefix on static methods: `Mesh::createCube()`, `Mesh::createPlane()`
- Free-function initializers use `initialize` prefix: `initializeRuntimeDoors()`, `initializeRuntimeInventory()`
- Free-function per-frame update use `update` prefix: `updateRuntimePlayerMovement()`, `updateRuntimeDoors()`
- Load/save pairs use `load`/`save` prefix: `loadLevelDef()`, `saveLevelDef()`, `loadMaterialDefinitionAsset()`

**Variables:**
- `snake_case` for locals: `line_number`, `mesh_id`
- Private members use trailing underscore: `window_`, `registry_`, `running_`, `program_`
- In-class struct constants follow `PascalCase` with no underscore for small POD members: `leftLeaf`, `rightLeaf`

**Constants:**
- `k` prefix + `PascalCase` for named constants: `kMaxRenderLights`, `kMaterialPollIntervalMs`, `kFloatEpsilon`
- Exception in `RuntimeInputState`: `MaxKeys`, `MaxButtons` (no `k` prefix — minor inconsistency)
- Free-standing `constexpr` globals follow the same `k` prefix rule: `kMaxRenderLights = 32`

**Enums:**
- `enum class` exclusively (no plain `enum`): `enum class MaterialUvMode`, `enum class LightType`, `enum class UpdatePhase`
- Enum values in `PascalCase`: `MaterialUvMode::WorldProjected`, `GroundState::OnGround`

**Files:**
- `PascalCase.h` / `PascalCase.cpp` matching the primary class name
- No `.hpp` extension used anywhere — all C++ headers are `.h`

## Code Style

**Formatting:**
- `.clang-format` based on LLVM style: `BasedOnStyle: LLVM`
- 4-space indent, `ContinuationIndentWidth: 4`
- Column limit: 100 characters
- Braces: attached (`BreakBeforeBraces: Attach`) — K&R style
- Pointer alignment: left (`PointerAlignment: Left`), so `int* ptr` not `int *ptr`
- No short functions on a single line (`AllowShortFunctionsOnASingleLine: None`)
- No short if-statements on a single line (`AllowShortIfStatementsOnASingleLine: Never`)
- Function arguments: never bin-packed, must be aligned (`BinPackArguments: false`)

**Headers:**
- `#pragma once` universally — no include guards used anywhere
- No `.hpp` — only `.h` for all C++ headers

## Include Organization

**Order (enforced by convention, not tool — `SortIncludes: Never`):**
1. Own header (in `.cpp` files, the matching `.h` comes first)
2. Other project headers, relative to `src/` root (e.g., `"engine/core/Application.h"`)
3. Third-party headers (GLFW, GLM, spdlog, EnTT, GLAD)
4. Standard library headers

**Path style:**
- All project includes use paths relative to `src/`: `#include "game/rendering/MaterialDefinition.h"`
- No `../` relative paths anywhere in the source tree

**Path Aliases:**
- None — CMake `target_include_directories` sets `src/` as the include root for all targets

## Anonymous Namespace Usage

Every `.cpp` file uses `namespace { ... }` to contain file-local helpers and constants. This is consistent across all 45+ implementation files. File-local helpers are never declared `static`.

```cpp
// Correct pattern — used universally
namespace {

glm::mat4 makeModel(const glm::vec3& position, const glm::vec3& scale,
                    const glm::vec3& rotation = glm::vec3(0.0f)) {
    // ...
}

} // namespace
```

## Class Patterns

**Non-copyable by default:**
All resource-owning classes explicitly delete copy constructor and copy assignment. This is applied to every OpenGL resource class and any class owning unique resources:

```cpp
// Pattern used in Shader, Mesh, Framebuffer, Texture2D, PhysicsSystem, Application
Shader(const Shader&) = delete;
Shader& operator=(const Shader&) = delete;
```

**Move semantics:**
Applied selectively where ownership transfer is needed. `Mesh`, `Texture2D`, `TextureCube` are both non-copyable and movable:

```cpp
Mesh(Mesh&& other) noexcept;
Mesh& operator=(Mesh&& other) noexcept;
```

**RAII for OpenGL resources:**
Destructors unconditionally release OpenGL handles. Handles are zero-initialized in the header:

```cpp
// Mesh.h
GLuint vao_ = 0;
GLuint vbo_ = 0;
GLuint ebo_ = 0;
```

**Pimpl idiom:**
Used for `PhysicsSystem` to hide Jolt Physics internals:

```cpp
// PhysicsSystem.h
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
```

**Virtual destructors:**
All base classes use `virtual ~ClassName() = default;`. The `System` base class follows this: `virtual ~System() = default;`.

**Inline getters:**
Short accessors are defined inline in the header. Single-line getter bodies stay on the same line as the method signature:

```cpp
Window& window() { return window_; }
const Time& time() const { return time_; }
float deltaTime() const { return time_.deltaTime(); }
```

## ECS Component Design

Components are POD structs with no methods (except `TransformComponent`, which has a `modelMatrix()` helper — acceptable). No inheritance in components. Components use in-class member initializers:

```cpp
struct DoorComponent {
    entt::entity leftLeaf = entt::null;
    float interactDistance = 3.0f;
    bool opened = false;
};
```

Tag components are empty structs in single-header files — one tag per file.

## Free Functions Over Member Functions (Game Logic)

Game logic is implemented as free functions in `src/game/runtime/RuntimeGameplay.cpp` rather than methods on systems. Systems are thin wrappers that delegate to free functions:

```cpp
// PlayerMovementSystem.cpp — thin shell
void PlayerMovementSystem::update(Application& app, float deltaTime) {
    updateRuntimePlayerMovement(app.registry(), input_, physics_, deltaTime);
}
```

This pattern makes game logic testable without instantiating a full `Application`.

## Designated Initializers (C++20)

Used consistently in test code for struct initialization:

```cpp
level.meshes.push_back(LevelMeshPlacement{
    .meshId = "cube",
    .position = glm::vec3(1.0f, 2.0f, 3.0f),
    .materialId = "brick_default",
    .tint = glm::vec3(0.4f, 0.5f, 0.6f),
});
```

Not yet applied in production code — production code uses member-by-member assignment or in-class defaults.

## Error Handling

**Strategy:** Exception-based for fatal/unrecoverable errors; return values / `std::optional` / logging for recoverable errors.

**Patterns:**
- Parse errors throw `std::runtime_error` with file path + line number context via `throwParseError()` in `src/game/content/ParseUtils.h`
- Shader compile/link failure throws `std::runtime_error` after logging via spdlog
- Missing content definitions throw `std::runtime_error`: `throw std::runtime_error("Unknown material id: " + id)`
- Material inheritance cycles throw `std::runtime_error`: `throw std::runtime_error("Material inheritance cycle detected at: " + id)`
- OpenGL resource failures that cannot recover throw `std::runtime_error`
- Audio/texture load failures log a warning and continue (graceful degradation)
- Lookup methods return raw pointer (nullable): `const WeaponDefinition* findWeapon(const std::string& id) const`
- Disk cache misses return `std::optional<CachedMeshData>`

**No exceptions in tests** — tests catch exceptions manually to verify error paths:

```cpp
bool threw = false;
try {
    (void)resolveMaterialDefinition("child", missingParent);
} catch (const std::runtime_error&) {
    threw = true;
}
assert(threw);
```

## Logging

**Framework:** spdlog (`spdlog::info`, `spdlog::warn`, `spdlog::error`)

**Patterns:**
- Subsystem name in brackets for non-trivial systems: `spdlog::warn("[AudioSystem] Failed to open OGG file '{}' (error {})", path, error)`
- Shorter messages for game/editor: `spdlog::error("Material save failed: {}", ex.what())`
- Info logs on system init/shutdown: `spdlog::info("PhysicsSystem initialized")`
- No debug-level logs in production code observed

## Comments

**When to Comment:**
- Complex math operations always get an explanation: rotation order in `TransformComponent::modelMatrix()`, tangent generation in `MeshGeometry.h`
- Public API parameters or non-obvious semantics get inline comments
- File-local helper functions in `.cpp` files get a separator comment block when there are many

**Comment style:**
- `//` only — no `/* */` block comments in production code
- Section dividers use `// ---` style in larger files like `test_asset_cache.cpp`

## Modern C++ Feature Usage

**Used:**
- `std::optional<T>` extensively for nullable fields in `MaterialDefinition`, `LevelMeshPlacement`
- `std::variant` in `EditorSceneObjectPayload`
- `std::filesystem` for all file paths in tests and serialization code
- `std::unique_ptr` for ownership (systems in `Application`, `PhysicsSystem::Impl`)
- Structured bindings: `for (const auto& [mesh, transform] : parts)`
- `[[noreturn]]` on `throwParseError()`
- `constexpr` for all named integer/float constants at class and namespace scope
- `noexcept` on move constructors/assignment operators
- `enum class` exclusively

**Not used (C++20 features available but absent):**
- `std::span` — not used anywhere; raw `std::vector` refs passed instead
- `std::ranges` — not used; manual `std::sort`, `std::count_if`, `std::any_of` used
- Concepts (`requires`, `concept`) — not used; templates without constraints
- `std::format` — spdlog format strings used instead

---

*Convention analysis: 2026-04-01*
