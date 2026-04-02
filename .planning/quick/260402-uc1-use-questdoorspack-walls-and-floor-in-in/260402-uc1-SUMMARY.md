# Quick Task 260402-uc1: Summary

## What Changed

### assets/scenes/initial_scene.scene

1. **Replaced `prison_wall` with `SM_Wall`** (14 panels) — QuestDoorsPack flat wall quad at scale (0.008, 0.016, 0.008) giving 2m×4m panels. Positions adjusted for corner-origin mesh.

2. **Replaced `prison_floor` with `SM_Floor`** (20 tiles) — QuestDoorsPack floor tile at scale 0.008 giving 2m×2m tiles. Positions adjusted for corner-origin mesh (+X, -Z extent).

3. **Kept `prison_wall_door`** for 3 door-opening walls — SM_Wall_Door's opening distorts badly at non-uniform scale (would be 3.5m tall), so procedural mesh with correct 0.9m×2.1m opening is preserved.

4. **Scaled doors/frames from 0.21 to 0.23** — DoorA at 0.23 = 2.12m tall (was 1.93m), filling the 2.1m opening with no visible gap. All 3 door+frame pairs updated.

5. **Removed all 36 `prison_baseboard` lines** — floor baseboards (18) and crown molding (18) eliminated.

## Dimensions Verified

| Model | Raw Height | At 0.21 | At 0.23 | Opening |
|-------|-----------|---------|---------|---------|
| SM_DoorA | 9.22 | 1.94m (gap!) | 2.12m ✓ | 2.1m |
| SM_DoorC | 9.16 | 1.92m (gap!) | 2.11m ✓ | 2.1m |
| SM_DoorD | 9.16 | 1.92m (gap!) | 2.11m ✓ | 2.1m |
| SM_FrameA | 9.57 | 2.01m | 2.20m ✓ | covers opening |

## Verification

- Editor loaded scene without errors
- Debug harness confirmed: 20 SM_Floor, 14 SM_Wall, 3 prison_wall_door, 0 baseboard entities
- QDP floor textures auto-loaded from assets/packs/QuestDoorsPack/Texture/Floor/
