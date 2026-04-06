# Multiplatform C++ CI/CD Build Research

**Date:** 2026-04-05
**Context:** Custom C++ game engine — OpenGL 4.1, CMake 3.28+, FetchContent (Jolt/EnTT/ImGui), Homebrew on macOS, vcpkg on Windows CI

---

## 1. vcpkg vs FetchContent-only for Windows CI

**Short answer:** Keep the hybrid — but switch triplet from `x64-windows-static` to `x64-windows-static-md`.

### The CRT Mismatch Root Cause

`x64-windows-static` uses `/MT` (static CRT). MSVC and FetchContent default to `/MD` (dynamic CRT, `MultiThreadedDLL`). Mixing them causes linker error `LNK2038 mismatch detected for 'RuntimeLibrary'`.

**Fix:** Use `x64-windows-static-md` instead. This triplet builds static `.lib` files but links the MSVC runtime dynamically (`/MD`), matching what FetchContent-fetched code defaults to.

```
VCPKG_DEFAULT_TRIPLET: x64-windows-static-md
```

Confirmed: `x64-windows-static-md` is a built-in vcpkg triplet (not community). Its `VCPKG_CRT_LINKAGE` is `dynamic` while `VCPKG_LIBRARY_LINKAGE` is `static`. This is the "best of both" for redistributable Windows apps.

### FetchContent-only is theoretically cleaner but has CI costs

**Pros of FetchContent-only:**
- No CRT mismatch — everything compiles with the same flags
- No separate tool to install or version-pin
- Jolt, EnTT, and ImGui already work this way in this project

**Cons in CI:**
- vcpkg sets `FETCHCONTENT_FULLY_DISCONNECTED=ON` when used as a toolchain. This breaks FetchContent for game deps if you're also using vcpkg for _any_ dep. Workaround: pass `-DFETCHCONTENT_FULLY_DISCONNECTED=OFF` in `VCPKG_CMAKE_CONFIGURE_OPTIONS` or switch deps entirely.
- Compiling Jolt, EnTT, ImGui from source on every CI run without caching = 3–5 minutes of extra build time. FetchContent + `CPM_SOURCE_CACHE` or `sccache` mitigates this.

**Recommendation:** Keep vcpkg for system-level deps (glfw3, glm, openal-soft, spdlog, zlib) with `x64-windows-static-md`. Keep FetchContent for Jolt/EnTT/ImGui. Enforce CRT consistency via `CMAKE_MSVC_RUNTIME_LIBRARY` (see below).

---

## 2. Enforcing CRT Consistency Across All Deps

CMake 3.15+ introduced `CMAKE_MSVC_RUNTIME_LIBRARY` (policy `CMP0091`). Set this globally in the root `CMakeLists.txt` **before** any `FetchContent_MakeAvailable()` calls:

```cmake
# At the top of CMakeLists.txt, before project() or immediately after
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
```

The generator expression expands to:
- Release: `MultiThreadedDLL` → `/MD`
- Debug: `MultiThreadedDebugDLL` → `/MDd`

This propagates to FetchContent sub-projects because `CMAKE_MSVC_RUNTIME_LIBRARY` is a CMake cache variable inherited by subprojects. Jolt Physics respects it (Jolt's CMake explicitly checks for `CMAKE_MSVC_RUNTIME_LIBRARY`).

**For vcpkg side:** Use `x64-windows-static-md` (dynamic CRT) — this aligns with `/MD` above.

---

## 3. Platform-Specific Code (fork/pipe/SIGUSR1)

### Recommended approach: compile-time platform abstraction via separate files

Avoid cluttering platform-agnostic code with `#ifdef _WIN32` chains. Instead:

```
src/editor/debug/
├── DebugHarness.h          # interface — platform-agnostic
├── DebugHarnessPosix.cpp   # Unix: fork, pipe, SIGUSR1, sys/wait.h
└── DebugHarnessWin32.cpp   # Windows: CreateProcess, HANDLE, WaitForSingleObject
```

In CMakeLists.txt:

```cmake
if(WIN32)
    target_sources(editor PRIVATE src/editor/debug/DebugHarnessWin32.cpp)
else()
    target_sources(editor PRIVATE src/editor/debug/DebugHarnessPosix.cpp)
endif()
```

This is the pattern used by Unreal Engine (platform file system), O3DE (AzCore platform layer), and GLFW itself (separate `win32_*.c` / `posix_*.c` files). The interface stays clean.

### For features that simply don't exist on Windows

Unix sockets at `/tmp/*.sock` have no Windows equivalent without WSL. Options:
1. **Named pipes on Windows** (`\\.\pipe\pixel-roguelike-editor`) — same concept, different API
2. **Compile out on Windows** — gate the debug harness entirely with `#ifndef _WIN32` or `if(NOT WIN32)` in CMake
3. **TCP loopback** — portable but heavier than either

For a debug harness (not shipped), option 2 is cleanest. Option 3 is correct if you ever want the harness on Windows.

### Minimal guard pattern for headers that leak POSIX

```cpp
// DebugHarness.h
#pragma once
#ifdef __unix__
#include "DebugHarnessImpl.h"   // pulls in Unix-specific details
#endif
```

```cmake
# Don't add the socket server to the Windows build
if(UNIX)
    target_sources(editor PRIVATE src/editor/debug/EditorSocketServer.cpp)
endif()
```

Headers including `<sys/wait.h>`, `<unistd.h>`, `<signal.h>` must not be transitively included on Windows. Use forward declarations or pimpl to keep them out of public headers.

---

## 4. CMake Presets (`CMakePresets.json`)

Worth adding now — it's the single source of truth for CI, local dev, and IDEs (VS Code, CLion, Visual Studio all support it natively).

### Recommended structure

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "windows-release",
      "inherits": "base",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows" },
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "VCPKG_TARGET_TRIPLET": "x64-windows-static-md",
        "CMAKE_MSVC_RUNTIME_LIBRARY": "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL",
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "macos-release",
      "inherits": "base",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Darwin" },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "linux-release",
      "inherits": "base",
      "condition": { "type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux" },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    }
  ],
  "buildPresets": [
    { "name": "windows-release", "configurePreset": "windows-release" },
    { "name": "macos-release", "configurePreset": "macos-release" }
  ]
}
```

**Key points:**
- `VCPKG_ROOT` is an env var set by the CI runner or locally — not hardcoded
- `CMakeUserPresets.json` is gitignored — local overrides go there
- CI invocation becomes: `cmake --preset windows-release && cmake --build --preset windows-release`

---

## 5. GitHub Actions Matrix Strategy

### Recommended matrix (2 runners, not 3)

Windows and macOS cover the primary target platforms. Linux is cheap to add later.

```yaml
jobs:
  build:
    name: Build (${{ matrix.os }})
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        include:
          - os: windows-latest
            preset: windows-release
            targets: "pixel-roguelike level-editor"
          - os: macos-latest
            preset: macos-release
            targets: "pixel-roguelike level-editor"

    steps:
      - uses: actions/checkout@v4

      # Windows only: restore vcpkg packages
      - name: Restore vcpkg cache
        if: runner.os == 'Windows'
        uses: actions/cache@v4
        with:
          path: C:/vcpkg/installed
          key: vcpkg-${{ matrix.os }}-${{ hashFiles('vcpkg.json') }}

      # macOS only: install Homebrew deps
      - name: Install macOS deps
        if: runner.os == 'macOS'
        run: brew install glfw glm spdlog openal-soft

      # Both platforms: cache FetchContent/build artifacts
      - name: Cache build
        uses: actions/cache@v4
        with:
          path: build/${{ matrix.preset }}/_deps
          key: fetchcontent-${{ matrix.os }}-${{ hashFiles('CMakeLists.txt') }}

      - name: Configure
        run: cmake --preset ${{ matrix.preset }}

      - name: Build
        run: cmake --build --preset ${{ matrix.preset }} --target ${{ matrix.targets }} --parallel
```

**Key decisions:**
- `fail-fast: false` — see all failures, not just the first
- Cache `_deps` (FetchContent downloads) separately from build artifacts — cache keys differ
- Explicit `--target` list avoids building `procedural-model-viewer` until its missing header is fixed
- Use `--parallel` (maps to `-j` under the hood) for faster builds

### On the broken `procedural-model-viewer` target

Exclude it from CI until fixed. In CMakeLists.txt:

```cmake
option(BUILD_MODEL_VIEWER "Build procedural model viewer" OFF)
if(BUILD_MODEL_VIEWER)
    add_subdirectory(src/viewer)
endif()
```

CI sets `BUILD_MODEL_VIEWER=OFF` by default. Re-enable when the missing header is resolved.

---

## 6. Handling vcpkg's `FETCHCONTENT_FULLY_DISCONNECTED` Interference

When vcpkg toolchain is active, it sets `FETCHCONTENT_FULLY_DISCONNECTED=ON`, which blocks FetchContent for Jolt/EnTT/ImGui at configure time. Two clean solutions:

**Option A (recommended):** Override in CMakePresets.json for the Windows preset:

```json
"cacheVariables": {
  "FETCHCONTENT_FULLY_DISCONNECTED": "OFF"
}
```

This re-enables FetchContent while keeping vcpkg for its packages. The two don't conflict — vcpkg provides packages that `find_package()` resolves; FetchContent works on its own targets.

**Option B:** Pre-populate the FetchContent cache by running `cmake -S . --preset windows-release` once on a fresh runner, caching `build/windows-release/_deps`, and using `FETCHCONTENT_FULLY_DISCONNECTED=ON` in subsequent runs. This is the "offline" pattern — faster CI but more complex cache invalidation.

---

## Decision Summary

| Question | Answer | Confidence |
|----------|--------|------------|
| vcpkg vs FetchContent-only? | Keep hybrid: vcpkg for system libs, FetchContent for engine deps | HIGH |
| CRT triplet for vcpkg? | `x64-windows-static-md` (static libs + dynamic CRT = `/MD`) | HIGH |
| Enforce CRT in CMake? | `cmake_policy(SET CMP0091 NEW)` + `CMAKE_MSVC_RUNTIME_LIBRARY` globally | HIGH |
| Platform code strategy? | Separate `.cpp` files selected by CMake `if(WIN32)`, not `#ifdef` chains | HIGH |
| Unix sockets on Windows? | Compile out with `if(UNIX)` in CMake — debug harness is dev-only | MEDIUM |
| CMake Presets? | Yes — add `CMakePresets.json` now; it's the CI/IDE source of truth | HIGH |
| CI matrix? | `windows-latest` + `macos-latest`, `fail-fast: false` | HIGH |
| Broken target? | Gate behind `BUILD_MODEL_VIEWER=OFF` option until fixed | HIGH |
| FetchContent + vcpkg conflict? | Set `FETCHCONTENT_FULLY_DISCONNECTED=OFF` in Windows preset | HIGH |

---

## Sources

- [vcpkg Triplet Variables — Microsoft Learn](https://learn.microsoft.com/en-us/vcpkg/users/triplets)
- [Windows with MSVC — vcpkg](https://learn.microsoft.com/en-us/vcpkg/users/platforms/windows)
- [MSVC_RUNTIME_LIBRARY — CMake docs](https://cmake.org/cmake/help/latest/prop_tgt/MSVC_RUNTIME_LIBRARY.html)
- [CMP0091 — CMake policy](https://cmake.org/cmake/help/latest/policy/CMP0091.html)
- [FETCHCONTENT_FULLY_DISCONNECTED issue — microsoft/vcpkg #28386](https://github.com/microsoft/vcpkg/issues/28386)
- [vcpkg CRT mismatch discussion — microsoft/vcpkg #9994](https://github.com/microsoft/vcpkg/issues/9994)
- [x64-windows-static-md triplet discussion — microsoft/vcpkg #15122](https://github.com/microsoft/vcpkg/issues/15122)
- [GitHub Actions CMake starter workflow](https://github.com/actions/starter-workflows/blob/main/ci/cmake-multi-platform.yml)
- [CMake Presets docs](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [FetchContent vs vcpkg — CMake Discourse](https://discourse.cmake.org/t/fetchcontent-vs-vcpkg-conan/6578)
- [Matt Gibson: 2024 CMake + vcpkg workflow](https://mgibson.ca/posts/my-2024-c-workflow-using-modern-cmake-and-vcpkg/)
