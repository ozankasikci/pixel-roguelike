# Phase 14 Implementation Notes

## Sun seam debugging and fix

While validating the Phase 14 lighting work, the initial room scene showed a thin bright seam along the wall/floor contact and wall corner edges when the sun was enabled.

### What we verified

- The artifact disappeared when the sun was disabled, so the issue was in the directional sun path.
- The issue persisted even after removing scene-geometry overlap experiments, so the scene file itself was not the real fix.
- The issue remained visible in the `Sun Shadow` debug mode, which confirmed it was in shadow visibility rather than general lighting/post.
- CSM UV bounds were not the main problem.
- Receiver-bias-only tuning did not materially fix the issue.

### Root cause

The QuestDoors wall/floor assets used in `initial_scene.scene` are effectively paper-thin planes. In the cascaded sun shadow map, those zero-thickness casters left tiny sunlit leaks at room seams.

### Final renderer fix

- Added viewport/debug harness support for:
  - screenshot capture
  - play-preview toggle
  - preview-mode switching
  - runtime camera inspection and control
- Added dedicated debug preview modes:
  - `Sun Direct`
  - `Sun Shadow`
  - `CSM UV`
  - `Cascade`
- Updated the directional CSM depth vertex shader to accept normals and a configurable normal offset.
- In the CSM render pass, each caster is rendered twice with a small `+/-` normal extrusion so thin planes behave like a tiny shadow slab.
- Kept a modest light-direction caster offset so the slabbed caster still closes contact leaks cleanly.

### Final tuning that produced the best result

- `uShadowCasterOffset = 0.18`
- `uShadowNormalOffset = +/-0.03`
- no negative receiver-bias hack

### Notes

- Earlier scene overlap/scale hacks in `initial_scene.scene` were removed.
- The floor-edge seam was reduced from a continuous bright band to a few stray pixels in raw `Sun Shadow` debug.
- The right wall/corner trace was also significantly reduced.

