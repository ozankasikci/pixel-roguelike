---
phase: quick
plan: 260408-x9w
subsystem: physics/colliders
tags: [physics, jolt, ecs, doors, colliders, kinematic]
dependency_graph:
  requires: []
  provides: [kinematic-door-colliders]
  affects: [PhysicsSystem, LevelLoader, RuntimeGameSession, ColliderInspector]
tech_stack:
  added: []
  patterns: [kinematic-body-sync, model-matrix-decompose, two-pass-node-resolution]
key_files:
  created:
    - src/game/components/KinematicLinkComponent.h
    - src/game/systems/KinematicColliderSystem.h
    - src/game/systems/KinematicColliderSystem.cpp
  modified:
    - src/game/components/ColliderComponent.h
    - src/game/level/LevelDef.h
    - src/game/level/LevelDef.cpp
    - src/engine/physics/PhysicsSystem.h
    - src/engine/physics/PhysicsSystem.cpp
    - src/game/level/LevelBuilder.cpp
    - src/game/level/LevelLoader.cpp
    - src/game/runtime/RuntimeGameSession.cpp
    - src/game/CMakeLists.txt
    - src/editor/ui/inspectors/ColliderInspector.cpp
decisions:
  - "Use Jolt MoveKinematic (not SetPositionAndRotation) for smooth physics interpolation and correct character controller push-back"
  - "Two-pass approach in LevelLoader: collect kinematic pending during placement loop, resolve parentNodeId after NodeIndex is built"
  - "Skip per-frame SetPositionAndRotation in PhysicsSystem.update for kinematic bodies — KinematicColliderSystem drives them via moveKinematicBody"
  - "Kinematic solid bodies placed in Layers::MOVING so they collide with the character controller"
metrics:
  duration: ~20 minutes
  completed: 2026-04-08T21:06:22Z
  tasks_completed: 3
  files_changed: 10
---

# Phase quick Plan 260408-x9w: Kinematic Door Colliders Summary

Kinematic collider bodies that follow door leaf mesh animations via per-frame model matrix decomposition and Jolt MoveKinematic calls.

## Tasks Completed

| Task | Name | Commit | Files |
|------|------|--------|-------|
| 1 | Data model + PhysicsSystem kinematic body support | 9b59c54 | ColliderComponent.h, KinematicLinkComponent.h, LevelDef.h, LevelDef.cpp, PhysicsSystem.h, PhysicsSystem.cpp |
| 2 | KinematicColliderSystem + LevelBuilder wiring + runtime registration | 001ab9a | KinematicColliderSystem.h/.cpp, CMakeLists.txt, LevelBuilder.cpp, LevelLoader.cpp, RuntimeGameSession.cpp |
| 3 | Editor support -- kinematic checkbox, serialization, preview world sync | e5d1eb5 | ColliderInspector.cpp |

## What Was Built

**KinematicLinkComponent** — POD component that stores the parent mesh entity reference (`entt::entity parentMesh`). Emplaced by LevelLoader's second pass when a collider has `kinematic=true` and a resolved parentNodeId.

**KinematicColliderSystem** (`tickKinematicColliders`) — Free function called each frame after `tickDoorAnimation`. Iterates entities with `ColliderComponent + KinematicLinkComponent`, decomposes the parent's `MeshComponent.modelOverride` matrix into position/rotation using `glm::decompose`, and calls `PhysicsSystem::moveKinematicBody`.

**PhysicsSystem::moveKinematicBody** — Uses Jolt's `MoveKinematic` (not `SetPositionAndRotation`) to smoothly interpolate the kinematic body's position and generate correct contact responses with the character controller. Searches both `staticBodies` (Solid mode) and `dualBodies` (SolidAndTrigger mode) maps.

**Scene file format** — `kinematic` keyword added to the collider optional modifier parser and serializer. Round-trips correctly through load/save.

**Editor** — ColliderInspector shows a "Kinematic" checkbox for Solid and SolidAndTrigger mode colliders. Checkbox is undo-able via commandStack.

## Decisions Made

1. **MoveKinematic vs SetPositionAndRotation**: Jolt's `MoveKinematic` interpolates body motion between physics steps and generates proper contact impulses, which is critical for the character controller to be pushed by the door. `SetPositionAndRotation` would teleport the body without pushing the character.

2. **Two-pass LevelLoader**: Kinematic collider parentNodeId resolution requires the NodeIndex, which is built after all placement loops. The first pass collects `{entity, parentNodeId}` pairs; the second pass (after NodeIndex construction) resolves and emplaces `KinematicLinkComponent`.

3. **MOVING layer for kinematic solids**: Kinematic bodies are placed in `Layers::MOVING` (not `NON_MOVING`) so the broadphase filter correctly pairs them with the character controller and other moving objects.

4. **Skip per-frame SetPositionAndRotation for kinematic bodies**: The `update()` loop's static body sync is skipped for kinematic bodies (`!collider.kinematic` guard) to avoid fighting with the KinematicColliderSystem's MoveKinematic calls.

## Deviations from Plan

None — plan executed exactly as written. The plan's Task 3 self-corrected (noting EditorSceneDocument.cpp and EditorPreviewWorld.cpp required no changes), which was confirmed during implementation.

## Self-Check: PASSED

| Item | Status |
|------|--------|
| KinematicLinkComponent.h | FOUND |
| KinematicColliderSystem.h | FOUND |
| KinematicColliderSystem.cpp | FOUND |
| Commit 9b59c54 (Task 1) | FOUND |
| Commit 001ab9a (Task 2) | FOUND |
| Commit e5d1eb5 (Task 3) | FOUND |
