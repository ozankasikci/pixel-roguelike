# Requirements: 3D Roguelike

**Defined:** 2026-03-23
**Core Value:** The Stanley Parable-inspired art style — clean, minimalist environments with warm soft lighting, muted color palette, and stylized realism.

## v1 Requirements

Requirements for initial playable tech demo. Each maps to roadmap phases.

### Rendering

- [x] **RNDR-01**: Engine renders 3D scene to framebuffer with depth buffer
- [x] **RNDR-02**: Post-process stylize pass with edge detection, bloom, and tone mapping
- [x] **RNDR-03**: Smooth camera movement with no visual artifacts
- [x] **RNDR-04**: Scene renders at display resolution with GL_LINEAR filtering for smooth output
- [x] **RNDR-05**: Point light sources produce warm, soft illumination with correct attenuation

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

- [ ] **ENVR-01**: Prison/institutional level geometry loads from scene files
- [ ] **ENVR-02**: Player collides with walls, floors, and objects (no clipping through)
- [ ] **ENVR-03**: Light fixtures placed in levels emit warm illumination

### Game Systems

- [ ] **GSYS-01**: Game state saves at level boundaries (player can quit and resume)
- [ ] **GSYS-02**: Options menu with FOV slider (60-120°)
- [ ] **GSYS-03**: Options menu with mouse sensitivity adjustment
- [ ] **GSYS-04**: Options menu with fullscreen/windowed toggle

## v1.1 Requirements

Requirements for Editor UX milestone. Each maps to roadmap phases.

### Selection

- [ ] **SEL-01**: Selection overlay renders depth-correctly (no wireframe bleeding through geometry in front)
- [ ] **SEL-02**: Objects highlight on mouse hover before clicking
- [ ] **SEL-03**: Escape key clears all selection

### Object Manipulation

- [ ] **OBJ-01**: Delete key removes selected object globally (viewport + outliner)
- [ ] **OBJ-02**: Ctrl+D duplicates selected object with position offset
- [ ] **OBJ-03**: F key frames camera on selected object

### Discoverability

- [ ] **DISC-01**: Add Mesh picker button to add meshes to the scene

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Combat Expansion

- **CMBT-05**: Ranged combat with projectile weapons (bows, crossbows)
- **CMBT-06**: Magic/spell projectiles with stylized visual effects
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

- **RNDR-06**: Screen-space ambient occlusion for depth and contact shadows

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Procedural level generation | Destroys handcrafted gothic atmosphere; explicit design choice |
| Multiplayer | Requires netcode infrastructure that doesn't exist in custom engine |
| Inventory management UI | Kills lean atmospheric experience; keep minimal |
| Stamina/dodge roll system | Adds complexity without core value alignment |
| Character classes / RPG stats | Undermines simplicity of core loop |
| Narrative/story system | Environmental storytelling preferred over scripted narrative |
| Controller support | WASD + mouse is genre standard; defer to v2+ |

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
| CMBT-01 | Phase 3 | Pending |
| CMBT-02 | Phase 3 | Pending |
| CMBT-03 | Phase 3 | Pending |
| CMBT-04 | Phase 3 | Pending |
| ENMY-01 | Phase 3 | Pending |
| ENMY-02 | Phase 3 | Pending |
| ENMY-03 | Phase 3 | Pending |
| ENMY-04 | Phase 3 | Pending |
| GSYS-01 | Phase 4 | Pending |
| GSYS-02 | Phase 4 | Pending |
| GSYS-03 | Phase 4 | Pending |
| GSYS-04 | Phase 4 | Pending |
| SEL-01 | Phase 9 | Pending |
| SEL-02 | Phase 10 | Pending |
| SEL-03 | Phase 10 | Pending |
| OBJ-01 | Phase 10 | Pending |
| OBJ-02 | Phase 10 | Pending |
| OBJ-03 | Phase 10 | Pending |
| DISC-01 | Phase 11 | Pending |

**Coverage:**
- v1 requirements: 23 total — mapped to phases: 23 — unmapped: 0
- v1.1 requirements: 7 total — mapped to phases: 7 — unmapped: 0

---
*Requirements defined: 2026-03-23*
*Last updated: 2026-04-01 after v1.1 roadmap creation*
