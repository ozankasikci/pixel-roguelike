# Coding Conventions

**Analysis Date:** 2026-04-05

## Naming Patterns

**Files:**
- `PascalCase.h` / `PascalCase.cpp` matching the primary class name
- Examples: `Application.h`, `Shader.h`, `PlayerMovementSystem.cpp`, `EditorCommand.h`

**Classes/Structs:**
- `PascalCase` — `TransformComponent`, `PhysicsSystem`, `RuntimeGameSession`, `EditorSceneDocument`, `MaterialDefinition`, `LevelDef`
- Structs and POD components also use `PascalCase`: `BehaviorDeclaration`, `LevelMeshPlacement`, `ResolvedMaterialDefinition`

**Functions/Methods:**
- `camelCase` — `isKeyPressed()`, `setCharacterVelocity()`, `loadFromFile()`, `captureState()`, `markSceneDirty()`, `findObject()`

**Variables:**
- `snake_case` generally; private members use trailing underscore — `window_`, `registry_`, `physics_`, `vertexCount_`, `uniform_cache_`, `selectedIds_`
- Member variable pattern: consistently `name_` throughout codebase

**Constants:**
- `k` prefix + `PascalCase` — `kMaxRenderLights`, `kMaterialKindCount`, `kFloatEpsilon`, `kMaxCommands`
- Namespace-scoped constants also use `k` prefix: `Layers::NON_MOVING` (exception: these are const-assigned ObjectLayer enums)

**Enums:**
- `PascalCase` enum class with `PascalCase` values — `enum class UpdatePhase { Input, Render }`, `enum class MaterialProceduralSource { GeneratedBrick, GeneratedStone }`, `enum class GroundState { OnGround, OnSteepGround, InAir }`

## Code Style

**Formatting:**
- `.clang-format` based on LLVM style
- 4-space indentation
- 100-character column limit
- Attached braces: `if (x) {` not `if (x)\n{`
- Pointer alignment: left (`int* ptr`)
- No short functions on single line
- `AllowShortFunctionsOnASingleLine: None` enforced

**Linting:**
- `.clang-format` is the authority; no explicit linting tool detected
- Consistent application observed across all main source files

## Import Organization

**Order:**
1. Standard library headers (`#include <vector>`, `#include <memory>`, etc.)
2. Third-party framework headers (`#include <glad/gl.h>`, `#include <glm/glm.hpp>`, `#include <entt/entt.hpp>`, `#include <spdlog/spdlog.h>`, `#include <Jolt/Jolt.h>`)
3. Project headers (`#include "engine/..."`, `#include "game/..."`, `#include "editor/..."`)

**Path Aliases:**
- Project headers use paths relative to `src/` root: `#include "engine/core/Application.h"` not `#include "../core/Application.h"`
- No CMake-level path aliases like `@` or `~` used

**Header Guards:**
- `#pragma once` exclusively (no include guard macros)

## Error Handling

**Strategy:** Exceptions for unrecoverable errors; assertions for development-time contracts

**Patterns:**
- **Throw on initialization failure**: `Shader` constructor throws `std::runtime_error` if compilation/linking fails
  - Example in `src/engine/rendering/core/Shader.cpp`: `throw std::runtime_error("Shader program link failed")`
- **Logging + throw**: Errors logged via `spdlog::error()` before throwing
  - Pattern: `spdlog::error("Shader compile error ({}): \n{}", path, infoLog); glDeleteShader(shader); throw std::runtime_error(...);`
- **Assertions for null checks**: Heavy use of `assert()` for precondition validation
  - Example in `src/game/level/LevelLoader.cpp`: `assert(args.content != nullptr && "LevelLoadArgs::content must not be null");`
- **Silent null-return pattern in uniform setters**: Shader uniform setters silently skip if location is -1 (uniform not in shader)
  - Pattern in `src/engine/rendering/core/Shader.cpp`: `if (loc != -1) { glUniform1f(loc, v); }`
- **No null pointer exceptions**: Service locator patterns use `tryGetService<T>()` returning `nullptr` or `getService<T>()` throwing
  - Both provided: `T* tryGetService<T>()` and `T& getService<T>()` in `src/engine/core/Application.h`

**Anti-pattern observed:**
- Bare `new`/`delete` avoided; `std::unique_ptr` and `std::make_unique` preferred throughout
- Raw pointers used only for non-owning references (e.g., `Mesh* mesh` in `MeshComponent` with comment "non-owning pointer")

## Memory Management

**RAII Compliance:**
- All OpenGL resources (VAO, VBO, EBO, FBO, shader programs) wrapped in RAII classes
- `Shader::~Shader()` deletes program if non-zero: `if (program_) { glDeleteProgram(program_); }`
- `Framebuffer::~Framebuffer()` calls `destroy()`; resize() re-creates resources
- `Mesh::~Mesh()` cleans up VAO/VBO/EBO; move constructor/assignment supported

**Ownership Model:**
- `std::unique_ptr<T>` for exclusive ownership: `std::unique_ptr<Shader> load(...)`
- Non-copyable by default (explicit `= delete` on copy constructor/assignment)
- Move semantics where ownership transfer needed: `Mesh(Mesh&& other) noexcept`
- Comments mark non-owning pointers: `Mesh* mesh = nullptr; // non-owning pointer`

**Memory Allocation Patterns:**
- Factory functions return `std::unique_ptr`: `Shader::load()`, `ModelLoader::load()`
- No manual `new`/`delete` in production code (tests use `assert()` only, no `delete`)
- Constructor initializer lists used for member initialization

## C++20 Features Usage

**Actively used:**
- `std::ranges` algorithms: `std::remove_if`, `std::any_of`, `std::count_if` in tests
- `std::optional<T>` for nullable values: `std::optional<std::string> parent;`, `std::optional<glm::vec3> tint;`
- `std::span` indirectly via GLM and Jolt (not seen in direct use in this codebase yet)
- `std::function` + lambda captures: event subscriptions in `EventBus`
- `std::any` for type-erased service locator: `std::unordered_map<std::type_index, std::any> services_`
- Template specialization: `emplaceService<T>()`, `getService<T>()`, `hasService<T>()` in `Application`

**NOT used:**
- Concepts (C++20 feature but not detected in this codebase)
- Designated initializers (not observed; struct initialization uses member assignment)
- Modules (traditional includes used throughout)

## Object Construction Patterns

**POD Components:**
- Zero initialization with `{}`: `glm::vec3 position{0.0f};`, `glm::vec3 scale{1.0f};`
- Defaults in struct definitions: `ColliderShape shape = ColliderShape::Box;`, `bool castsShadows = false;`

**Systems:**
- Constructor takes dependencies as const references: `PlayerMovementSystem(InputSystem& input, PhysicsSystem& physics);`
- No `init()` constructor argument; init logic in `System::init(Application& app)` virtual override
- Unused parameters marked with `(void)param` to suppress warnings: `void PlayerMovementSystem::init(Application& app) { (void)app; }`

**Services:**
- Type-safe service locator via `std::type_index` and `std::any`
- Example: `auto& content = app.getService<ContentRegistry>();`

## Comments & Documentation

**When to Comment:**
- Constructor preconditions for non-obvious contracts: `// required`, `// optional`
- Non-owning pointer clarification: `// non-owning pointer, mesh lifetime managed by scene/resource manager`
- Complex math explanation: Euler angle order explicitly documented in `TransformComponent`
- File reading performance notes: comment about `readFile()` in Shader loading

**JSDoc/TSDoc:**
- Not used; minimal inline comments preferred
- Function intent implied by name; only document where name is insufficient

**Inline Comments:**
- Sparse; code is expected to be self-documenting via naming
- Used for non-obvious intent or workarounds

## Function Design

**Size:** 
- No functions longer than ~50 lines observed in core engine layers
- Helper functions extracted into anonymous namespaces: `test_model_loader.cpp` has local `computeMin()`, `computeMax()`
- Systems and complex operations delegate to free functions: `updateRuntimePlayerMovement()`, `makeSignature()`

**Parameters:**
- Pass-by-const-reference for large objects: `const glm::mat4& mat`, `const std::string& name`
- Pass-by-value for simple types: `float v`, `int v`
- No pass-by-non-const-reference except when output parameter is semantically needed
- Builder pattern used for complex object construction: `LevelBuilder::addMesh()`, `LevelBuilder::attachInteractable()`

**Return Values:**
- `std::unique_ptr<T>` for factory methods
- Bare values for simple returns: `float deltaTime()`, `const Time& time() const`
- Optional return for operations that might fail: `std::optional<std::string> parent;` in struct member
- No exceptions leaked; methods document failure via precondition asserts

## Module Design

**Exports:**
- Classes and structs in headers; implementations in .cpp
- Free functions declared in headers (e.g., `loadLevelDef()`, `serializeLevelDef()`)
- Helper functions hidden in anonymous namespaces: `namespace { ... } // namespace`

**Barrel Files:**
- Not used; direct includes of specific headers preferred
- Each header pulls in only its direct dependencies

**File Organization by Layer:**
- **Engine (`src/engine/`)**: Core systems (Application, Window, EventBus), rendering (Shader, Mesh, Framebuffer), physics, input
- **Game (`src/game/`)**: Components, systems (PlayerMovementSystem, CameraSystem), content registry, levels, prefabs, scenes
- **Editor (`src/editor/`)**: Scene document, commands, UI panels, debug harness, viewport controller

## Type Conversions

**Explicit conversions:**
- Jolt ↔ GLM conversions use inline helper functions: `toJolt()`, `toGlm()`, `toJoltQuat()` in `PhysicsSystem.cpp`
- GLM functions used for matrix math: `glm::translate()`, `glm::scale()`, `glm::mat4_cast()`
- Quaternion construction: `glm::angleAxis()` for Euler angle → quaternion

**Casting:**
- `static_cast<>` used for type-safe conversions: `static_cast<std::size_t>(UpdatePhase::Count)`
- `std::any_cast<>` used in service locator: `std::any_cast<T&>(services_.at(key))`
- `std::get<>` used with std::variant: `std::get<LevelMeshPlacement>(object.payload)`

## Inconsistencies & Anti-Patterns

1. **Variable parameter naming inconsistency:**
   - Most use trailing underscore: `input_`, `physics_`, `window_`
   - Some don't follow this for const references in small contexts (e.g., parameters in function signatures)

2. **Assert usage for production code:**
   - Heavy reliance on `assert()` for null checks in game/editor code
   - Assertions may be disabled in Release builds; consider exceptions for recoverable errors in critical paths

3. **Silent failure in uniform setters:**
   - Shader uniform setters in `Shader.cpp` silently skip if location is -1
   - Could log warnings for missing uniforms in development

4. **Pimpl pattern incomplete:**
   - `PhysicsSystem` uses pimpl (`struct Impl`) but pattern not consistently applied
   - Other complex systems expose implementation details

5. **Void parameter suppression:**
   - `(void)param;` used to suppress unused parameter warnings
   - Modern approach: use `[[maybe_unused]]` attribute instead

6. **Error recovery in shaders:**
   - Shader compilation errors set `program_ = 0` but don't expose this state to caller
   - Caller assumes success; could add `isValid()` query

## Consistency Summary

**Strengths:**
- Naming is consistent across 224 source files
- RAII is properly applied to all GPU resources
- Memory ownership is clear via `std::unique_ptr` and comments
- Include order is consistent
- No use of bare `new`/`delete`

**Areas for improvement:**
- Replace `assert()` with exceptions in production code paths
- Add `[[maybe_unused]]` instead of `(void)param;`
- Log warnings for silent failures (missing shader uniforms)
- Standardize pimpl usage or abandon it in favor of direct composition

---

*Convention analysis: 2026-04-05*
