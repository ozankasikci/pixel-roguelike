# Phase 4: Improve Lighting Quality - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-03-30
**Phase:** 04-improve-lighting-quality-research-best-practices-and-implement-industry-standard-real-time-lighting
**Areas discussed:** Shadow quality, Ambient occlusion, Bloom & glow, Light types & shapes

---

## Shadow quality

### Shadow softness

| Option | Description | Selected |
|--------|-------------|----------|
| Soft & diffuse | PCF with large kernel or PCSS. Shadows fade smoothly at edges, matching Stanley Parable's gentle lighting. | ✓ |
| Sharp with soft edges | Standard PCF (3x3 or 5x5). Clear shadow shapes with slightly softened edges. | |
| You decide | Claude picks based on OpenGL 4.1 constraints and performance. | |

**User's choice:** Soft & diffuse (Recommended)
**Notes:** None

### Shadow scope

| Option | Description | Selected |
|--------|-------------|----------|
| Spot + directional | Add CSM for sun/directional. Keep spot shadows for key interior fixtures. | ✓ |
| All light types | Spot + directional + point (cubemap shadows). Most realistic but expensive. | |
| You decide | Claude determines based on performance analysis. | |

**User's choice:** Spot + directional (Recommended)
**Notes:** None

---

## Ambient occlusion

### AO style

| Option | Description | Selected |
|--------|-------------|----------|
| Subtle contact AO | Gentle darkening where walls meet floors, around door frames, under furniture. | ✓ |
| Strong atmospheric AO | More pronounced darkening. Moodier, slightly oppressive. | |
| You decide | Claude picks intensity and style. | |

**User's choice:** Subtle contact AO (Recommended)
**Notes:** None

### AO technique

| Option | Description | Selected |
|--------|-------------|----------|
| You decide | Let research determine best SSAO technique for OpenGL 4.1. | ✓ |
| Classic SSAO | John Chapman-style hemisphere sampling. | |
| GTAO | Ground Truth AO — modern, higher quality. | |

**User's choice:** You decide (Recommended)
**Notes:** None

---

## Bloom & glow

### Bloom character

| Option | Description | Selected |
|--------|-------------|----------|
| Soft warm glow | Multi-pass Gaussian downsample/upsample chain. Dreamy, warm halo. Stanley Parable signature. | ✓ |
| Subtle sharpened bloom | Tighter radius, less spread. More cinematic, less Stanley Parable. | |
| You decide | Claude picks bloom character during research. | |

**User's choice:** Soft warm glow (Recommended)
**Notes:** None

### Bloom scope

| Option | Description | Selected |
|--------|-------------|----------|
| Per-environment | Different rooms get different bloom. Already have per-environment params. | ✓ |
| Global only | One setting for whole game. Simpler but less variation. | |

**User's choice:** Per-environment (Recommended)
**Notes:** None

---

## Light types & shapes

### New light types

| Option | Description | Selected |
|--------|-------------|----------|
| Rectangular area lights | Recessed ceiling panels, fluorescent fixtures, windows. | ✓ |
| Tube/line lights | Fluorescent tube lights along corridors. | ✓ |
| Emissive mesh lighting | Let mesh surfaces emit light. More flexible but harder to control. | ✓ |
| You decide | Claude determines which to add based on research. | ✓ |

**User's choice:** All options selected — research all, implement what's achievable
**Notes:** None

### Light units

| Option | Description | Selected |
|--------|-------------|----------|
| Keep arbitrary | Current intensity float works for art-directed style. | ✓ |
| Physical units | Lumens/lux. More predictable but requires exposure system. | |
| You decide | Claude determines during research. | |

**User's choice:** Keep arbitrary (Recommended)
**Notes:** None

---

## Claude's Discretion

- SSAO technique selection (classic vs HBAO vs GTAO)
- Shadow map resolution and cascade count for CSM
- Bloom mip chain depth and threshold tuning
- Which new light types to actually implement based on feasibility in OpenGL 4.1
- Soft shadow technique details (PCF kernel size vs PCSS)
- Performance optimizations
- AO sample count, radius, blur approach

## Deferred Ideas

- Point light shadows (cubemap) — future optimization phase
- Physically-based light units — not needed for art-directed style
- Light probes / IBL — separate feature
- Screen-space reflections — distinct scope
- Volumetric lighting / god rays — separate scope
