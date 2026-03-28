# Building

This project is documented and verified as a macOS-first build today. The root [README.md](../README.md) gives the fast-start version; this page is the fuller reference.

## Platform Support

- macOS: supported and documented
- Linux: planned, but not yet documented here
- Windows: planned, but not yet documented here

## Prerequisites

- CMake 3.28 or newer
- Python 3
- Homebrew packages:
  - `glfw`
  - `glm`
  - `spdlog`
- A local Python virtual environment with `jinja2`

Install the system packages:

```bash
brew install cmake glfw glm spdlog
```

Create the Python environment used during configure:

```bash
python3 -m venv .venv
.venv/bin/pip install jinja2
```

`jinja2` is required because GLAD is generated during CMake configure.

## Configure

Run from the repository root:

```bash
cmake -S . -B build -DPython_EXECUTABLE=.venv/bin/python3
```

The root build file [CMakeLists.txt](../CMakeLists.txt) pulls pinned third-party dependencies with `FetchContent`, finds Homebrew/system packages, and then adds the engine, game, editor, app, test, and tool targets.

## Build

Build everything:

```bash
cmake --build build
```

Build a single target:

```bash
cmake --build build --target level-editor
cmake --build build --target mesh_generator
cmake --build build --target test_runtime_game_session
```

## Output Layout

Common outputs land under these directories:

- `build/apps/runtime/`: runtime executable
- `build/apps/level_editor/`: level editor executable
- `build/apps/model_viewer/`: procedural model viewer executable
- `build/tests/`: test executables grouped by subsystem
- `build/tools/`: offline helper executables

## Running the Main Apps

Run the playable runtime:

```bash
./build/apps/runtime/pixel-roguelike
```

Run the level editor:

```bash
./build/apps/level_editor/level-editor
```

Run the procedural model viewer:

```bash
./build/apps/model_viewer/procedural-model-viewer
```

## Testing After a Build

Run the full suite:

```bash
ctest --test-dir build --output-on-failure
```

Run one subsystem:

```bash
ctest --test-dir build --output-on-failure -L game
```

More testing details live in [testing.md](testing.md).

## Common Build Notes

- If configure fails around GLAD generation, make sure the virtual environment exists, `jinja2` is installed, and `-DPython_EXECUTABLE=.venv/bin/python3` was passed to CMake.
- The shared desktop-app wiring lives in [cmake/DesktopApp.cmake](../cmake/DesktopApp.cmake).
- On Apple platforms, the desktop apps receive Cocoa, OpenGL, and IOKit framework links through that shared helper.

## Related Docs

- [architecture.md](architecture.md)
- [testing.md](testing.md)
- [dependencies.md](dependencies.md)
- [tooling.md](tooling.md)
