# Requirements: 3D Roguelike

**Defined:** 2026-03-23
**Core Value:** The 1-bit dithered 3D rendering — a visually striking, modern take on retro aesthetics that makes the game instantly recognizable.

## v1 Requirements

Requirements for initial playable tech demo. Each maps to roadmap phases.

### Rendering

- [x] **RNDR-01**: Engine renders 3D scene to framebuffer with depth buffer
- [x] **RNDR-02**: Post-process 1-bit dithering shader converts scene to pure black and white using Bayer matrix
- [x] **RNDR-03**: Dither pattern uses world-space anchoring to prevent swimming during camera movement
- [x] **RNDR-04**: Scene renders at low internal resolution and nearest-neighbor upscales to display
- [x] **RNDR-05**: Point light sources (torches) affect dither density in their radius

### Player

- [ ] **PLYR-01**: First-person camera with smooth WASD movement and mouse look
- [ ] **PLYR-02**: Player has health that decreases when hit by enemies
- [ ] **PLYR-03**: Health indicator visible on HUD using shape/size (not color)

### Combat

- [ ] **CMBT-01**: Player can swing melee weapon with visible animation
- [ ] **CMBT-02**: Melee hits detect collision with enemies and deal damage
- [ ] **CMBT-03**: Enemies react visibly when hit (knockback/flinch)
- [ ] **CMBT-04**: Player dies and sees game over screen when health reaches zero

### Enemies

- [ ] **ENMY-01**: Enemies patrol predefined paths when player is not detected
- [ ] **ENMY-02**: Enemies detect player via line-of-sight within range
- [ ] **ENMY-03**: Enemies chase and attack player when detected
- [ ] **ENMY-04**: Enemies have health and can be killed

### Environment

- [ ] **ENVR-01**: Gothic cathedral-style level geometry loads from model files
- [ ] **ENVR-02**: Player collides with walls, floors, and objects (no clipping through)
- [ ] **ENVR-03**: Torch objects placed in levels emit point light

### Game Systems

- [ ] **GSYS-01**: Game state saves at level boundaries (player can quit and resume)
- [ ] **GSYS-02**: Options menu with FOV slider (60-120)
- [ ] **GSYS-03**: Options menu with mouse sensitivity adjustment
- [ ] **GSYS-04**: Options menu with fullscreen/windowed toggle

### Engine Boundary (added for Phase 4)

- [ ] **ENG-BOUNDARY-01**: Engine rendering layer compiles with zero game-layer includes
- [ ] **ENG-BOUNDARY-02**: RenderMaterialData uses opaque int instead of game's MaterialKind enum
- [ ] **ENG-BOUNDARY-03**: Engine provides generic action mapping system for input bindings
- [ ] **ENG-BOUNDARY-04**: Engine input layer compiles with zero game-layer includes
- [ ] **ENG-BOUNDARY-05**: RuntimeSceneRenderer has no hardcoded game-specific constants
- [ ] **ENG-BOUNDARY-06**: ImGuiLayer has no game-specific forward declarations or methods

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Combat Expansion

- **CMBT-05**: Ranged combat with projectile weapons (bows, crossbows)
- **CMBT-06**: Magic/spell projectiles with 1-bit-readable visual effects
- **CMBT-07**: Weapon progression — find and upgrade weapons through levels

### Enemies Expansion

- **ENMY-05**: Multiple enemy types (3+) with distinct behaviors
- **ENMY-06**: Boss encounter(s) with unique attack patterns

### Audio

- **AUDIO-01**: Footstep sounds that vary by surface type
- **AUDIO-02**: Melee weapon impact sounds
- **AUDIO-03**: Ambient dungeon atmosphere audio
- **AUDIO-04**: Enemy alert/attack sounds

### Visual Polish

- **RNDR-06**: Screen-space ambient occlusion contributing to dither density

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Procedural level generation | Destroys handcrafted gothic atmosphere; explicit design choice |
| Multiplayer | Requires netcode infrastructure that doesn't exist in custom engine |
| Inventory management UI | Kills lean atmospheric experience; fights 1-bit aesthetic |
| Stamina/dodge roll system | Adds complexity without core value alignment |
| Character classes / RPG stats | Undermines simplicity of core loop |
| Narrative/story system | 1-bit aesthetic supports environmental storytelling instead |
| Animated/flickering dither | Destroys video stream readability; creates visual fatigue |
| Controller support | WASD + mouse is genre standard; defer to v2+ |
| Color in any form | Strictly 1-bit black and white — no grayscale, no color |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| RNDR-01 | Phase 1 | Complete |
| RNDR-02 | Phase 1 | Complete |
| RNDR-03 | Phase 1 | Complete |
| RNDR-04 | Phase 1 | Complete |
| RNDR-05 | Phase 1 | Complete |
| PLYR-01 | Phase 2 | Pending |
| PLYR-02 | Phase 2 | Pending |
| PLYR-03 | Phase 2 | Pending |
| ENVR-01 | Phase 2 | Pending |
| ENVR-02 | Phase 2 | Pending |
| ENVR-03 | Phase 2 | Pending |
| CMBT-01 | TBD | Pending |
| CMBT-02 | TBD | Pending |
| CMBT-03 | TBD | Pending |
| CMBT-04 | TBD | Pending |
| ENMY-01 | TBD | Pending |
| ENMY-02 | TBD | Pending |
| ENMY-03 | TBD | Pending |
| ENMY-04 | TBD | Pending |
| GSYS-01 | TBD | Pending |
| GSYS-02 | TBD | Pending |
| GSYS-03 | TBD | Pending |
| GSYS-04 | TBD | Pending |
| ENG-BOUNDARY-01 | Phase 4 | Pending |
| ENG-BOUNDARY-02 | Phase 4 | Pending |
| ENG-BOUNDARY-03 | Phase 4 | Pending |
| ENG-BOUNDARY-04 | Phase 4 | Pending |
| ENG-BOUNDARY-05 | Phase 4 | Pending |
| ENG-BOUNDARY-06 | Phase 4 | Pending |

**Coverage:**
- v1 requirements: 29 total (23 original + 6 ENG-BOUNDARY)
- Mapped to phases: 17 (RNDR 5 + PLYR 3 + ENVR 3 + ENG-BOUNDARY 6)
- TBD (future phases): 12 (CMBT 4 + ENMY 4 + GSYS 4)

---
*Requirements defined: 2026-03-23*
*Last updated: 2026-03-29 — Added ENG-BOUNDARY requirements for Phase 4; moved CMBT/ENMY/GSYS to TBD (phase numbering shifted after inserted phases)*
