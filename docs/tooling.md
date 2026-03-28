# Tooling

This repository keeps offline or developer-facing helpers in [`tools/`](../tools). Runtime, editor, and test code should not depend on those tools at execution time.

## Policy

- Buildable project tools belong in [`tools/`](../tools) and should be registered from [tools/CMakeLists.txt](../tools/CMakeLists.txt).
- Durable script-based helpers may also live in [`tools/`](../tools) when they are expected to be rerun as part of asset or content maintenance.
- One-off experiments, migrations, or throwaway debugging scripts should not be committed unless they are likely to be reused.
- Tool outputs should land in `build/` or documented asset locations under `assets/`, not in `src/`.
- Tool files should explain their purpose near the top of the file or in this document.

## Ownership

Ownership here means which part of the repository should be updated or reviewed when a tool changes. This repo does not currently assign named human owners in-tree, so ownership is expressed by domain.

## Current Tools

### `mesh_generator`

- Path: [tools/mesh_generator.cpp](../tools/mesh_generator.cpp)
- Type: buildable C++ tool
- Build target: `mesh_generator`
- Build path: `./build/tools/mesh_generator`
- Purpose: generate composite `.glb` meshes for game assets such as `pillar.glb` and `arch.glb`
- Inputs: procedural mesh definitions in the tool plus shared geometry helpers from `src/engine/rendering/geometry`
- Outputs: `assets/meshes/*.glb`
- Dependency touchpoints: `glm::glm`, `tinygltf`
- Domain ownership: rendering and mesh content pipeline

Build and run:

```bash
cmake --build build --target mesh_generator
./build/tools/mesh_generator
```

### `generate_hand_with_old_dagger.py`

- Path: [tools/generate_hand_with_old_dagger.py](../tools/generate_hand_with_old_dagger.py)
- Type: durable Python asset-generation script
- Purpose: build `hand_with_old_dagger.glb` from a reference hand mesh and a procedurally generated dagger
- Default input: `assets/meshes/hand_low_poly.glb`
- Default output: `assets/meshes/hand_with_old_dagger.glb`
- Optional output: preview image via `--preview`
- Python dependencies: `numpy`, plus `matplotlib` only when `--preview` is used
- Domain ownership: mesh content pipeline and generated hand asset maintenance

Run:

```bash
python3 tools/generate_hand_with_old_dagger.py
python3 tools/generate_hand_with_old_dagger.py --preview /tmp/hand_with_old_dagger.png
```

## Practical Guidelines

- If a new helper needs to be compiled, add it to [tools/CMakeLists.txt](../tools/CMakeLists.txt) instead of hiding it in an app or test target.
- If a new helper is script-based, document its inputs, outputs, and required Python packages here.
- If a tool regenerates committed assets, include that fact in the pull request and mention which files were regenerated.
- If a tool becomes part of the normal contributor setup, promote its documentation into [README.md](../README.md) as well.
