# Milestones

## v1.0 Engine Foundation (Shipped: 2026-03-29)

**Phases completed:** 5 phases, 13 plans, 25 tasks

**Key accomplishments:**

- OpenGL 4.1 Core Profile C++ engine with 8x8 Bayer dither post-process using sphere-map world-space anchoring at 480p internal resolution upscaled with nearest-neighbor
- Blinn-Phong point lighting with quartic attenuation on a cathedral test scene, rendered 1-bit dithered at 480p, with a Dear ImGui overlay for runtime tuning of dither threshold, pattern scale, resolution, camera, and per-light parameters
- Reorganized source tree into engine/game modules, restructured CMake into per-module static libraries, and added EnTT v3.16.0 via FetchContent — binary compiles and runs identically
- Application class with game loop and system lifecycle, EventBus, Time, and four ECS component types (Transform, Mesh, Light, Camera) -- architectural backbone for all plan 03 and 04 systems
- Five concrete ECS systems replacing main.cpp inline logic: Scene/SceneManager stack, InputSystem (GLFW+ImGui wrapper), CameraSystem (exact yaw/pitch camera math), RenderSystem (FBO/dither/ImGui pipeline) -- all implementing the System base class
- ECS capstone: CathedralScene migrated to spawn all geometry/lights/camera as entt entities, main.cpp reduced from 210 lines to 46 lines using Application class pattern -- full architecture wired end-to-end
- Weapon inventory metadata, explicit owned-vs-equipped hand state, and test-backed two-handed burden rules for the upcoming paused inventory UI
- A Dark Souls-style paused weapon inventory now opens from gameplay, renders through the existing ImGui path, and cleanly restores first-person controls when closed
- POSIX fork/exec cmake process manager with pipe-based output streaming, SIGTERM cancellation, and GenericFileScene for --scene runtime argument
- Build menu with progress display, dockable Build Output panel with colored compiler output, Cmd+B/Cmd+R shortcuts, unsaved-changes modal, and build-and-run with --scene wired into the level editor
- RenderMaterialData now uses `int shadingModelIndex` instead of `MaterialKind`, removing all game-layer includes from engine_rendering and eliminating the dual-field anti-pattern from MeshComponent
- 1. [Rule 1 - Bug] Fixed test_runtime_game_session.cpp using RuntimeInputState
- Player torch extracted to PlayerTorchComponent+PlayerTorchSystem and game ImGui overlays moved from engine ImGuiLayer to GameOverlays namespace, making RuntimeSceneRenderer a generic ECS light collector and ImGuiLayer a clean engine-only lifecycle manager

---
