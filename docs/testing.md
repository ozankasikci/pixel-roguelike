# Testing

The automated test suite is organized by subsystem so new coverage can live next to related tests instead of growing one large flat list.

## Layout

- `tests/engine/`: engine-focused unit tests
- `tests/game/`: content, gameplay, and runtime tests
- `tests/editor/`: editor-focused tests and profiling helpers
- `tests/common/`: shared test utilities
- `tests/data/`: test-only fixture data

## Command Matrix

Configure and build from the repository root:

```bash
cmake -S . -B build -DPython_EXECUTABLE=.venv/bin/python3
cmake --build build
```

Run the full suite:

```bash
ctest --test-dir build --output-on-failure
```

Run tests by subsystem label:

```bash
ctest --test-dir build --output-on-failure -L engine
ctest --test-dir build --output-on-failure -L game
ctest --test-dir build --output-on-failure -L editor
```

Run one named test:

```bash
ctest --test-dir build --output-on-failure -R '^level_roundtrip$'
```

Build one specific test executable:

```bash
cmake --build build --target test_runtime_game_session
```

Run the editor profiling helper directly:

```bash
./build/tests/editor/profile_play_preview
```

## Conventions

- Prefer linking production targets such as `game_content`, `gameplay`, and `editor` instead of compiling production `.cpp` files directly into many tests.
- Use `pixel_roguelike_add_test(...)` from `tests/cmake/TestSupport.cmake` for new tests so include paths, labels, and `add_test(...)` wiring stay consistent.
- Put shared filesystem or comparison helpers in `tests/common/` when more than one test needs them.
