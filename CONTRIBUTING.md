# Contributing

Thanks for contributing to Pixel Roguelike.

This repository is still early-stage, so the main goal is to keep changes easy to understand, easy to review, and easy to build locally.

If this is your first time building the project, start with the setup steps in `README.md` to create `.venv` and install `jinja2` before running CMake.

## Workflow Expectations

- Keep each pull request focused on one logical change.
- Prefer small, reviewable patches over broad mixed-purpose updates.
- Use imperative commit messages such as `Add root contributor docs`.
- Update docs when you change repo structure, build steps, asset conventions, or contributor workflow.

## Before Opening a Pull Request

- Make sure build outputs, virtual environments, editor config, and other local artifacts are not included in the diff.
- Run a relevant local verification pass for your change.
- If your change affects visuals, rendering, or editor behavior, include screenshots or a short visual note in the pull request.

The default local verification flow is:

```bash
cmake -S . -B build -DPython_EXECUTABLE=.venv/bin/python3
cmake --build build
ctest --test-dir build --output-on-failure
```

If your change only touches a narrow area, it is fine to mention a smaller relevant check in the PR instead of running every possible target.

## What Not to Commit

Do not commit:

- `build/`, `build-test/`, or other local build directories
- `.venv/` or other machine-local environment folders
- IDE/editor settings such as `.idea/` and `.vscode/`
- generated caches, local config, or session files such as `imgui.ini`

## Pull Request Notes

Use the pull request body to cover:

- what changed
- why it changed
- how it was tested
- whether docs were updated
- whether screenshots are attached for visual/editor-facing changes

If a change intentionally leaves follow-up work for later, call that out explicitly so reviewers do not have to infer it.
