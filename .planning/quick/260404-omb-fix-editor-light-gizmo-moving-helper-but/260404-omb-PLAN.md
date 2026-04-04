# Quick Task 260404-omb: Fix editor light gizmo moving helper but not preview light

**Created:** 2026-04-04
**Status:** In Progress

## Goal

Keep single-light gizmo edits in sync with the preview-world lighting so dragging a light updates the rendered scene, not just the helper gizmo.

## Tasks

1. Inspect the single-selection gizmo path and confirm whether light edits advance the document scene revision used by preview sync.
2. Patch the light gizmo mutation path so preview-world lights get refreshed after a drag.
3. Run targeted editor tests and record the outcome.
