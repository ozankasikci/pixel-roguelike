# Docs

This directory now holds two kinds of documentation:

- durable reference docs for how the project currently works
- dated plans in [`docs/plans/`](plans/) that capture design and implementation history

If a question is about the current repository, build flow, editor workflow, or content conventions, start with the durable docs below. If a question is about why a change was made or how a feature was planned, check the archived plans.

## Durable Docs

- [architecture.md](architecture.md): module boundaries, target graph, and repository layout
- [building.md](building.md): setup, configure, build, run, and target/output conventions
- [testing.md](testing.md): test layout, commands, and test-harness conventions
- [dependencies.md](dependencies.md): dependency policy, pins, and vendored-code rationale
- [tooling.md](tooling.md): offline tools, script ownership, and tooling conventions
- [content-pipeline.md](content-pipeline.md): asset layout, file formats, discovery rules, and contributor conventions
- [editor.md](editor.md): level editor structure, saved state, and runtime preview behavior

## Plans Archive

The [`plans/`](plans/) directory is the dated archive for design docs, implementation plans, and historical roadmap work. Those files are useful context, but they are not the primary source of truth for current behavior once durable docs exist.

## Maintenance Rule

When repository structure, build flow, content conventions, or editor behavior changes, update the matching durable doc in this directory alongside the code change.
