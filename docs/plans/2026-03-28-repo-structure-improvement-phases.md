# Repository Structure Improvement Phases

**Goal:** Improve the repository's structure, onboarding, and maintainability by borrowing the best small-scale ideas from the [Godot Engine repository](https://github.com/godotengine/godot) without importing Godot's full complexity.

**Context:** The current repository already has a strong foundation with clear top-level areas for `apps/`, `assets/`, `src/engine`, `src/editor`, `src/game`, `tests`, and `tools`. The biggest opportunities are around contributor ergonomics, clearer module boundaries, less repetitive build/test wiring, and more durable documentation.

**Current pain points:**
- No root `README.md` or contributor-facing onboarding surface.
- No `.github/` metadata, issue templates, or pull request template.
- No repo-wide formatting/linting policy in checked-in config files.
- `apps/level_editor` compiles a long list of editor implementation files directly into the executable.
- `src/game` is growing as one wide library instead of a few smaller domains.
- `tests/CMakeLists.txt` is flat and repetitive, and several tests compile production `.cpp` files directly.
- Dependency policy is mixed between `FetchContent`, system packages, and vendored code under `external/`.
- `docs/` currently skews toward dated planning notes rather than durable project documentation.

**Inspiration from Godot:**
- Clear root navigation: `README`, `CONTRIBUTING`, `.github/`, formatting files.
- Strong subsystem boundaries: engine/editor/scene/modules/tests/thirdparty.
- Dedicated homes for tests and tooling support code.
- Consistent contributor workflow and documented conventions.

**Non-goals:**
- Do not flatten this repo into Godot-style top-level `core/`, `scene/`, `modules/`, `platform/`.
- Do not introduce a `modules/` system until optional or plugin-like features actually exist.
- Do not perform a large all-at-once reorganization that blocks gameplay work.

---

## Phase 1: Contributor Surface and Repo Hygiene

**Objective:** Make the repository understandable and welcoming from the root.

**Changes:**
- Add a root `README.md` with:
  - project overview
  - build instructions
  - app entrypoints (`runtime`, `level_editor`, `model_viewer`)
  - test instructions
  - brief repository map
- Add `CONTRIBUTING.md` with:
  - branch/commit expectations
  - formatting and testing expectations
  - how to propose structural changes
- Add `.github/` files:
  - `PULL_REQUEST_TEMPLATE.md`
  - lightweight issue templates if GitHub Issues are used
  - `CODEOWNERS` if ownership becomes important
- Add checked-in repo conventions:
  - `.editorconfig`
  - `.clang-format`
  - optional `.pre-commit-config.yaml`
- Expand `.gitignore` to cover local/dev artifacts more deliberately.

**Why this phase comes first:**
- It improves onboarding immediately.
- It creates a stable place to explain later structure changes.
- It mirrors one of Godot's strongest repo-level advantages with low risk.

**Exit criteria:**
- A new contributor can build, run, and test the project using only root docs.
- Formatting expectations are visible in the repo, not tribal knowledge.
- Pull requests have a shared checklist.

---

## Phase 2: Executable Boundaries and Build Cleanup

**Objective:** Clarify what is reusable engine/editor/game code versus what is an app entrypoint.

**Changes:**
- Move the runtime entrypoint out of `src/main.cpp` into `apps/runtime/main.cpp`.
- Keep `apps/` as the home for executable entrypoints only.
- Introduce `src/editor/CMakeLists.txt` and build editor code as one or more libraries instead of listing editor `.cpp` files directly inside `apps/level_editor/CMakeLists.txt`.
- Consider an initial split such as:
  - `editor_core`
  - `editor_render`
  - `editor_ui`
- Extract repeated Apple-specific target logic into a shared CMake helper under a new `cmake/` directory.

**Why this phase matters:**
- It makes the build graph easier to reason about.
- It aligns the repo more closely with the separation Godot maintains between reusable subsystems and final executables.
- It reduces repeated target boilerplate and makes future apps easier to add.

**Exit criteria:**
- Every executable target lives under `apps/`.
- Editor code is linked as libraries, not inlined into the app target definition.
- Platform-specific CMake glue is defined once and reused.

---

## Phase 3: Internal Module Boundaries

**Objective:** Reduce the size and responsibility of the current `game` target while preserving the existing top-level repo layout.

**Changes:**
- Review `src/game` and split the current `game` library into smaller domains.
- A likely first pass:
  - `game_content`
  - `gameplay`
  - `game_rendering`
- Keep `src/engine`, `src/editor`, and `src/game` as the top-level source buckets.
- Add lightweight ownership rules for where new code should live:
  - engine-agnostic runtime systems stay in `engine`
  - game-specific rules/content stay in `game`
  - authoring workflow/UI stays in `editor`

**Notes:**
- This phase should be incremental. Only split along seams that already exist in code and link dependencies.
- Avoid a purely aesthetic reorg. The goal is cleaner build and ownership boundaries, not churn.

**Exit criteria:**
- The `game` area no longer behaves like a catch-all bucket.
- New features have an obvious placement path.
- Target dependencies become more intentional and narrower.

---

## Phase 4: Test Harness and Verification Structure

**Objective:** Make tests easier to extend and reduce repeated CMake/test wiring.

**Changes:**
- Introduce shared test helpers:
  - test registration helper function in CMake
  - optional `test_main.cpp` if useful
  - shared path/data helper utilities
- Group tests by subsystem instead of one long flat list in `tests/CMakeLists.txt`.
- Consider a directory layout such as:
  - `tests/engine/`
  - `tests/editor/`
  - `tests/game/`
  - `tests/data/`
- Prefer linking real production targets over compiling production `.cpp` files directly into many tests, except where a deliberately narrow unit test warrants it.
- Add a documented test command matrix in docs and optionally in CI.

**Why this echoes Godot well:**
- Godot treats tests as a first-class part of engine maintenance.
- The useful lesson is not test volume, but having a clear harness and stable conventions.

**Exit criteria:**
- Adding a new test does not require duplicating a large CMake block.
- Tests are discoverable by subsystem.
- Regression coverage can grow without making the build setup harder to maintain.

---

## Phase 5: Dependency and Tooling Policy

**Objective:** Make third-party code and support tools easier to understand and maintain.

**Changes:**
- Write down a dependency policy for:
  - `FetchContent`
  - system packages
  - vendored code in `external/`
- Decide whether `external/` remains a small vendored area or evolves toward a more explicit `third_party/` or `thirdparty/` convention.
- Document version pinning and update expectations for libraries in the root build.
- Separate durable helper tools from one-off scripts more clearly:
  - keep buildable tools in `tools/`
  - document script purpose and ownership
- Optionally add a short `docs/dependencies.md` or `docs/tooling.md`.

**Why this phase helps:**
- The current dependency story works, but it is not yet self-explanatory.
- Godot makes third-party boundaries obvious; we should do the same in a lighter-weight way.

**Exit criteria:**
- A contributor can tell where dependencies come from and how they should be updated.
- Vendored code has a clear rationale.
- Tooling is easier to distinguish from runtime/editor code.

---

## Phase 6: Durable Docs for Architecture and Content

**Objective:** Shift `docs/` from mostly dated plans toward reusable project knowledge.

**Changes:**
- Add durable documentation pages such as:
  - `docs/architecture.md`
  - `docs/building.md`
  - `docs/testing.md`
  - `docs/content-pipeline.md`
  - `docs/editor.md`
- Keep `docs/plans/` for dated design and implementation plans.
- Add a small `docs/README.md` or docs index page that links durable docs and archived plans.
- Document repository conventions for adding assets, scenes, prefabs, and content definitions.

**Why this matters:**
- Structure improvements only stick if they are documented where contributors will look.
- Godot benefits heavily from discoverable docs; this repo should capture the same advantage at project scale.

**Exit criteria:**
- `docs/` contains both durable reference docs and dated plans, with a clear distinction.
- Common contributor questions can be answered from docs without code archaeology.

---

## Recommended Execution Order

1. Phase 1: Contributor Surface and Repo Hygiene
2. Phase 2: Executable Boundaries and Build Cleanup
3. Phase 4: Test Harness and Verification Structure
4. Phase 3: Internal Module Boundaries
5. Phase 5: Dependency and Tooling Policy
6. Phase 6: Durable Docs for Architecture and Content

**Reasoning:** Start with visibility and low-risk cleanup, then improve target boundaries, then make tests easier before performing deeper module splits. Finish by codifying the resulting architecture and policies in durable docs.

---

## Suggested First Milestone

If this work is started immediately, the best first milestone is:

1. Add `README.md`, `CONTRIBUTING.md`, `.editorconfig`, `.clang-format`, and `.github/PULL_REQUEST_TEMPLATE.md`.
2. Move `src/main.cpp` to `apps/runtime/main.cpp`.
3. Create `src/editor/CMakeLists.txt` and an `editor` library consumed by `apps/level_editor`.
4. Add a small CMake helper for shared app target/platform setup.
5. Create a minimal test helper to reduce duplication in `tests/CMakeLists.txt`.

This milestone delivers a visible improvement in repo clarity without forcing a risky gameplay or rendering refactor.
