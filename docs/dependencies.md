# Dependencies

This repository currently uses three dependency lanes:

1. `FetchContent` for pinned C++ libraries pulled during CMake configure
2. system packages, documented and verified on macOS via Homebrew
3. a very small vendored `external/` area for source drops that are easier to keep in-tree

The root [CMakeLists.txt](../CMakeLists.txt) is the source of truth for build dependencies.

## Policy

### `FetchContent`

Use `FetchContent` when a dependency needs an exact upstream pin, is awkward to rely on as a system package, or is tightly coupled to the local CMake build graph.

Rules:

- Pin every fetched dependency with an explicit `GIT_TAG`.
- Prefer one dependency update per pull request.
- Keep dependency-specific CMake cache toggles next to the `FetchContent_Declare(...)` block so the customization is easy to audit.
- Wrap upstream code in local targets when that makes downstream usage clearer, such as `glad_gl` and `imgui`.

### System packages

Use system packages for broadly available libraries that already have stable package-manager support and do not need an exact repo-owned pin.

Rules:

- The documented and verified path is macOS with Homebrew.
- Minimum versions should be expressed in `find_package(...)` when the build needs them.
- If the repo starts depending on a specific package version beyond the current minimum, document that tighter requirement here and in the root setup docs.

### Vendored `external/`

Use `external/` only for small source/header drops where vendoring is lower-friction than package-manager or `FetchContent` integration.

Rules:

- Keep vendored code small and explicit.
- Record the rationale for each vendored dependency in this document.
- If a vendored dependency does not carry a visible version in-tree, note the upstream version or commit in the pull request that updates it.
- If `external/` grows beyond a handful of small components or needs more formal license/accounting treatment, revisit the name and consider a `thirdparty/` or `third_party/` convention then.

## Current Inventory

| Dependency | Source lane | Pin / version policy | Used for | Current build touchpoint |
| --- | --- | --- | --- | --- |
| GLAD | `FetchContent` | pinned to `v2.0.8` | OpenGL 4.1 loader | root CMake creates `glad_gl` |
| EnTT | `FetchContent` | pinned to `v3.16.0` | ECS and registry APIs | root CMake, then `engine_core` / `gameplay` |
| Jolt Physics | `FetchContent` | pinned to `v5.4.0` | rigid-body physics and character controller | root CMake, then `engine_physics` |
| Dear ImGui | `FetchContent` | pinned to `v1.92.6-docking` | runtime/editor UI and docking | root CMake creates local `imgui` target |
| tinygltf | `FetchContent` | pinned to `v2.9.3` | glTF loading and offline `.glb` writing | root CMake, `engine_rendering`, `mesh_generator` |
| glfw3 | system package | minimum `3.4` required by CMake | windowing, context creation, input plumbing | root CMake and `engine_core` |
| glm | system package | package-managed, no repo pin today | math types and transforms | root CMake and multiple engine/game/tools targets |
| spdlog | system package | package-managed, no repo pin today | logging | root CMake and `engine_core` |
| ImGuizmo | vendored `external/` | version not recorded in-tree today | editor transform gizmos | `src/editor/CMakeLists.txt` and level editor app |
| `stb_image_write.h` | vendored `external/` | version not recorded in-tree today | screenshot writing helper | `src/engine/ui/Screenshot.cpp` |

## Decision: Keep `external/` for Now

`external/` currently contains only:

- [`external/ImGuizmo`](../external/ImGuizmo)
- [`external/stb_image_write.h`](../external/stb_image_write.h)

That is still small enough that renaming the directory would create more churn than clarity. The repo should keep `external/` as the small vendored area for now.

Revisit the naming only if one of these becomes true:

- the number of vendored dependencies increases materially
- vendored code starts needing its own license inventory or update manifest
- contributors begin confusing vendored code with first-party runtime/editor code

## Update Workflow

When changing a dependency:

1. Update the pin in [CMakeLists.txt](../CMakeLists.txt) or replace the vendored files under [`external/`](../external).
2. Reconfigure and rebuild:

```bash
cmake -S . -B build -DPython_EXECUTABLE=.venv/bin/python3
cmake --build build
```

3. Run the automated tests:

```bash
ctest --test-dir build --output-on-failure
```

4. If the change affects offline content generation, rerun the relevant tool and review its asset outputs.
5. Update this page and [README.md](../README.md) when the contributor-facing setup story changes.
