# Phase 12: Engine Quality — Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-01
**Phase:** 12-engine-quality-frustum-culling-texture-unit-enum-generic-asset-system-eventbus-raii-tokens
**Areas discussed:** Frustum culling scope, Generic asset system design, Phase scope and priorities

---

## Frustum Culling Scope

| Option | Description | Selected |
|--------|-------------|----------|
| AABB frustum culling only | Test each mesh's AABB against camera frustum. Uses existing aabbMin()/aabbMax(). | ✓ |
| AABB + material sort batching | Add frustum culling AND sort by material/shader to reduce state changes. | |
| You decide | Claude picks based on codebase complexity. | |

**User's choice:** AABB frustum culling only
**Notes:** None

| Option | Description | Selected |
|--------|-------------|----------|
| All render paths | SceneRenderPipeline is shared — culling at pipeline level benefits all three. | ✓ |
| Runtime only | Only cull in RuntimeSceneRenderer. | |
| You decide | Claude picks based on pipeline architecture. | |

**User's choice:** All render paths (after confirming it's industry standard)
**Notes:** User asked "is this an industry standard?" — confirmed yes, Unity/Unreal/Godot/Bevy all cull before any render path.

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, cull lights too | Skip lights whose attenuation radius doesn't overlap frustum. | |
| Meshes only for now | Keep scope tight. Light culling is smaller win with 32-light cap. | |
| You decide | Claude evaluates complexity vs benefit. | ✓ |

**User's choice:** You decide
**Notes:** Claude's discretion on light culling.

---

## Generic Asset System Design

| Option | Description | Selected |
|--------|-------------|----------|
| Split GameAssets.cpp by family | Break 880-line monolith into per-scene files. | |
| Unified asset registry | Single AssetRegistry for meshes, materials, textures via common interface. | |
| File-based asset discovery | Scan asset directories at startup, auto-register everything found. | ✓ |
| You decide | Claude picks based on codebase. | |

**User's choice:** File-based asset discovery
**Notes:** "We should never have hardcoded assets, it should have been generic like what Unity and Unreal Engine does."

| Option | Description | Selected |
|--------|-------------|----------|
| Keep procedural, cache to disk | Generate once, save as .glb, discover from filesystem. | |
| Replace with modeled assets over time | Long-term replace all procedural meshes. | |
| You decide | Claude picks migration strategy. | |

**User's choice:** Other — rename GameAssets to ProceduralGameAssets
**Notes:** "For the meshes generated procedurally, we can still use GameAssets, but we can call it ProceduralGameAssets."

| Option | Description | Selected |
|--------|-------------|----------|
| Not in this phase | Keep synchronous loading. Lazy loading is a future concern. | ✓ |
| Yes, add lazy loading | Defer GPU upload until first use. | |
| You decide | Claude evaluates. | |

**User's choice:** Not in this phase
**Notes:** None

---

## Phase Scope and Priorities

| Option | Description | Selected |
|--------|-------------|----------|
| DebugParams split | Split 40-field struct into PostProcessParams + CameraDebugInfo + RuntimeLightingOverride. | ✓ |
| LevelLoader unification | Merge two load() overloads into single context-struct path. | ✓ |
| GenericFileScene hard-coded branches | Move scripted geometry out of GenericFileScene. | ✓ |
| None of these | Keep Phase 12 tight — only the 4 title items. | |

**User's choice:** All three additional concerns folded in
**Notes:** Phase expanded from 4 to 7 items.

| Option | Description | Selected |
|--------|-------------|----------|
| All equal | Implement all four with no particular ordering preference. | ✓ |
| Frustum culling first | Biggest performance win first. | |
| Asset system first | Foundational change, others build on top. | |
| You decide | Claude orders by dependency. | |

**User's choice:** All equal
**Notes:** Claude will order by dependency analysis.

---

## Claude's Discretion

- Light frustum culling — evaluate complexity vs benefit given 32-light cap
- Texture/environment unification into discovery pattern — evaluate beyond meshes
- Implementation ordering across 7 items — dependency-based

## Deferred Ideas

- Material/shader sort batching — future phase when draw call count is a bottleneck
- Lazy/deferred asset loading — future phase when load times become a problem
- Scene file versioning — separate concern
- Component validation / schema enforcement — separate concern
