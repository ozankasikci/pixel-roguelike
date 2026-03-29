# Retrospective: 3D Roguelike

## Milestone: v1.0 — Engine Foundation

**Shipped:** 2026-03-29
**Phases:** 5 | **Plans:** 13

### What Was Built
- OpenGL 4.1 engine with 1-bit Bayer dither post-process and world-space anchoring
- Modular ECS architecture (EnTT) with Application class, EventBus, SceneManager
- Dark Souls-style equipment/inventory system with two-handed burden rules
- Editor build system (cmake process management, Build Output panel, Cmd+B/Cmd+R)
- Clean engine/game boundary — engine compiles with zero game-layer imports
- ActionMap input binding system, PlayerTorchComponent ECS extraction, GameOverlays namespace
- TypeScript scripting pipeline via QuickJS

### What Worked
- Phase-by-phase execution with parallel subagents — Wave 1 of Phase 4 ran two agents concurrently
- Inserted phases (01.1, 02.1, 3, 4) allowed the project to pivot from gameplay to engine quality without losing structure
- Engine/game boundary cleanup in Phase 4 was well-planned — 3 plans with clear dependencies across 2 waves
- Free-function system pattern (updatePlayerTorch, updateRuntimeCamera) proved simpler than System base class for game logic

### What Was Inefficient
- Phase 2 (Player, Environment, Lighting) was in the roadmap from the start but never executed — should have been removed or explicitly deferred earlier
- 18 of 29 v1 requirements were never addressed — the milestone scope drifted significantly toward engine/tooling
- 3 of 5 phases had no VERIFICATION.md — verification was only run on Phases 1 and 4
- Cherry-picking worktree commits required manual conflict resolution when parallel agents diverged

### Patterns Established
- Engine headers use `int` or `std::string` instead of game-layer enums at boundaries
- Game systems as free functions called from RuntimeGameSession::tick()
- ImGui overlays split: engine-only lifecycle in ImGuiLayer, game HUD in GameOverlays namespace
- Component-computed-lights pattern: system writes RenderLight vector, renderer just reads it

### Key Lessons
- Define milestone scope tightly — "v1 requirements" included gameplay that was never realistically in scope for the engine milestone
- Run verification on every phase, not just the first and last
- When parallel agents use worktrees, ensure they share the same base branch to avoid merge conflicts
- Inserted phases should trigger a scope reassessment of the original requirements

### Cost Observations
- Model mix: opus for planning/orchestration, sonnet for execution/verification
- Phase 4 execution: ~45 min for Wave 1 (parallel), ~10 min for Wave 2 (sequential)
- Integration checker provided high-value cross-phase wiring analysis

## Cross-Milestone Trends

| Metric | v1.0 |
|--------|------|
| Phases | 5 |
| Plans | 13 |
| Requirements satisfied | 11/29 |
| Integration flows | 6/6 |
| Duration | 7 days |
| LOC (C++) | ~23,000 |
