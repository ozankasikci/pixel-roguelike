# Pixel Roguelike

Pixel Roguelike is a first-person 3D roguelike built on a custom C++ engine with a stark 1-bit black-and-white rendering style. The project is currently in an early but active state, with a playable runtime, a level editor, a procedural model viewer, and a growing automated test suite.

## Documented Platform Support

The documented setup flow is currently **macOS-first**.

- macOS: supported and documented below.
- Linux: planned, but not yet documented in this repository.
- Windows: planned, but not yet documented in this repository.

## Prerequisites

- CMake 3.28 or newer
- Python 3
- Homebrew packages:
  - `glfw`
  - `glm`
  - `spdlog`
- A local `.venv` with `jinja2` installed

The GLAD dependency is generated during CMake configure, so `jinja2` is a required setup step.

Install the Homebrew packages if needed:

```bash
brew install cmake glfw glm spdlog
```

## Setup

From the repository root:

```bash
python3 -m venv .venv
.venv/bin/pip install jinja2
cmake -S . -B build -DPython_EXECUTABLE=.venv/bin/python3
cmake --build build
ctest --test-dir build --output-on-failure
```

## Running the Apps

Build outputs are placed under `build/apps/` by default.

Run the runtime:

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

## Repository Map

- `apps/`: executable entrypoints for the runtime, level editor, and model viewer
- `assets/`: game content such as scenes, prefabs, shaders, meshes, skies, and definition files
- `src/engine/`: shared engine systems such as application flow, rendering, input, physics, and scene management
- `src/editor/`: editor-specific document, viewport, rendering, and UI code
- `src/game/`: game-specific content loading, gameplay systems, scenes, and runtime rendering logic
- `tests/`: automated tests and test data
- `tools/`: offline or developer-facing helper tools such as mesh generation
- `docs/`: design notes, plans, and repository documentation

## Development Notes

- The project currently uses Homebrew-discovered packages for `glfw`, `glm`, and `spdlog`.
- Several content definitions live under `assets/defs/`, with supporting scenes and prefabs under `assets/scenes/` and `assets/prefabs/`.
- Root contributor workflow and pull request expectations are documented in [CONTRIBUTING.md](CONTRIBUTING.md).
