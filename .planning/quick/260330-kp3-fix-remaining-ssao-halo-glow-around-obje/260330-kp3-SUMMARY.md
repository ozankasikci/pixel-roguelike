---
plan: 260330-kp3
status: complete
started: 2026-03-30
completed: 2026-03-30
---

# Quick Task 260330-kp3: Fix remaining SSAO halo glow around objects

## One-liner
Eliminated SSAO haloing by reverting to canonical fixed-denominator algorithm — no sample skipping, smoothstep range check handles depth discontinuities naturally.

## What Changed

**Root cause:** Previous fix attempts tried to skip hemisphere samples that hit closer surfaces (like a chair above the floor). This reduced the sample count near objects, making those areas brighter relative to flat floor — the visible halo.

**Fix:** Reverted ssao.frag to the canonical SSAO algorithm (LearnOpenGL / John Chapman reference):
- Fixed denominator of 32 (always divide by kernel size)
- No sample skipping — every sample contributes
- `smoothstep` range check naturally zeroes out contributions from depth-discontinuity samples without affecting the denominator

The bilateral depth-aware blur (ssao_blur.frag) remains from earlier fixes, preventing AO bleeding across depth edges.

## Key Files
- `assets/shaders/engine/ssao.frag` — canonical SSAO with fixed denominator, no skipping

## Verification
- SSAO ON: uniform floor darkening, no bright halo around chair/desk
- SSAO OFF: uniform floor without AO (brighter overall)
- Side-by-side comparison confirms no brightness difference near objects
