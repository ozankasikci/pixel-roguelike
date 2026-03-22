# Feature Research

**Domain:** First-person 3D roguelike dungeon crawler with 1-bit dithered aesthetic
**Researched:** 2026-03-23
**Confidence:** MEDIUM-HIGH (verified via multiple sources; custom-engine specifics from training data)

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete or broken.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| First-person camera with smooth mouse look | Genre standard; anything else feels wrong | LOW | WASD + mouse, adjustable sensitivity required |
| Player health system with visible HP indicator | Every combat game has this; absence is disorienting | LOW | HUD element; must work in 1-bit (no color coding — use shape/size) |
| Melee combat with hit feedback | Core loop of dungeon crawlers; players expect tactile impact | MEDIUM | Swing animation, hit detection, enemy reaction, sound effect — all must land together |
| Enemy AI (patrol, detect, engage) | Without enemies that react, it's a walking simulator | MEDIUM | Minimum: idle patrol, line-of-sight detection, chase, attack state machine |
| Multiple enemy types with distinct behaviors | Single enemy type feels like a tech demo, not a game | MEDIUM | 3-5 types minimum; behavior differentiation more important than visual differentiation given 1-bit aesthetic |
| Basic weapon variety (at least melee + ranged) | Players expect tactical options in dungeon crawlers | MEDIUM | One melee archetype + one ranged archetype covers genre expectations |
| Boss encounter(s) | Genre standard; telegraphs that the game has a "climax" | HIGH | At least one boss; clear visual/audio distinction from regular enemies |
| Lighting that works with 1-bit rendering | Gothic atmosphere demands light/dark contrast; point lights (torches) are expected in this setting | HIGH | Torch flicker, shadows, light falloff all rendered through dithering — the hardest technical piece |
| FOV slider | First-person games without this generate motion sickness complaints immediately; Obra Dinn had this exact problem | LOW | Must be in options menu; 60–120 degree range |
| Mouse sensitivity settings | Any first-person game without this is unshippable | LOW | Separate horizontal/vertical sliders |
| Save / checkpoint system | Players expect to be able to stop and resume; losing all progress on exit is not acceptable even in a roguelike | MEDIUM | Can be minimal (save at level start/end) — but must exist |
| Basic audio: footsteps, hit sounds, ambient | Audio absence breaks immersion immediately; silence reads as broken | MEDIUM | Three categories minimum: movement, combat, ambience |
| Clear death / game over state | Players need to understand when they've died and what their options are | LOW | Death screen with restart option |
| Options / settings menu | Expected on every PC game; without it players assume the game is unfinished | LOW | At minimum: FOV, sensitivity, audio volume, window/fullscreen toggle |

---

### Differentiators (Competitive Advantage)

Features that set this game apart. Aligned with core value: the 1-bit dithered aesthetic as primary identity.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| 1-bit ordered dithering post-process | The entire visual identity of the game; instantly recognizable; no competitor in the dungeon crawler space | HIGH | Stabilized dither (not animated noise) — Obra Dinn proved randomized > ordered for video compression. Spherical mapping prevents crawling artifacts during movement |
| Gothic cathedral architecture rendered in 1-bit | Unusual combination; arched ceilings, stone pillars, and torch light rendered in pure black/white creates a striking aesthetic not seen in the genre | HIGH | Environmental design effort; every architectural element must read clearly in 1-bit — test early |
| Dithering-responsive point lighting | Torches that visibly shift the dither density/pattern around them creates a unique atmospheric effect | HIGH | Requires dither threshold to sample from scene luminance; a purely technical differentiator that has visual payoff |
| Handcrafted levels (vs procedural) | Tighter atmosphere and pacing control; allows "authored" moments of discovery and surprise; contradicts the flood of procedural dungeon crawlers | MEDIUM | Design-intensive; but the gothic cathedral setting makes handcrafting worthwhile because architecture has to look plausible |
| Weapon-based progression (find/upgrade) | Player attachment to specific weapons feels more personal than loot treadmill; choosing between a known sword and an unidentified crossbow is a meaningful moment | MEDIUM | Sparse inventory (1 melee + 1 ranged slot) keeps it simple without feeling bare |
| Ranged magic/spell projectiles | Bows are expected; spells/magic projectiles in a gothic cathedral setting add thematic coherence and a higher skill ceiling | MEDIUM | Projectile travel, hit detection, visual effect that reads clearly in 1-bit (glowing orb → dithered bright circle) |
| Atmospheric enemy design readable in 1-bit | Enemies designed specifically for high-contrast silhouette readability — skeletal figures, armored knights, spectral shapes — are more visually distinctive than color-coded enemies | MEDIUM | Silhouette-first enemy design process |
| Screen-space ambient occlusion contributing to dither density | Corners and crevices darker via AO → denser dither patterns → visual depth without color | HIGH | Optional; adds significant visual quality but is technically demanding |

---

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create disproportionate problems.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Procedural level generation | Replayability; "infinite content" | Destroys the handcrafted gothic atmosphere; rooms must fit together architecturally and procedural systems don't maintain that; undermines the authored pacing of boss reveals; explicitly out of scope per PROJECT.md | 3-5 handcrafted levels with strong internal variety; hidden rooms and branching paths deliver discovery without proceduralism |
| Inventory management UI | Genre tradition; player agency over loadout | Kills the lean, atmospheric experience; forces HUD complexity that fights the 1-bit visual aesthetic; v1 explicitly defers this per PROJECT.md | Simple weapon slots (one melee, one ranged found/swapped in world); no inventory screen needed |
| Stamina/dodge roll system | Soulslike feel; prevents button mashing | Adds a third combat resource on top of health and ammo; substantially increases combat design complexity; the gothic dungeon crawler identity doesn't require it | Deliberate enemy attack telegraphs and room layout that forces positioning; challenge through environment and timing, not stamina gates |
| Character class / RPG stats | Depth; replayability | Stat systems imply a character creation screen, leveling progression, and balance across multiple builds; all require significant design and testing work; undermines simplicity of the core loop | Weapon variety as the only "class" distinction; different weapon types serve different playstyles without class UI |
| Multiplayer co-op | Obvious social appeal | Multiplayer requires synchronization, session management, and netcode — none of which exist in the custom C++ engine; explicitly out of scope per PROJECT.md; Obra Dinn's success proves solo first-person can be commercially viable | Single-player only; design for intimate, atmospheric solo experience |
| Animated dithering (noise patterns that shift per frame) | "Living" dithering looks cool in motion | Destroys video stream/screenshot readability (YouTube and Twitch compression destroys dithered images; random patterns compress better but still degrade); creates visual noise fatigue for long sessions; harder to make readable | Stabilized ordered dithering with spherical mapping so patterns are anchored to world geometry, not screen pixels — patterns shift naturally with camera movement but don't flutter on static objects |
| Narrative/story system | Atmosphere and motivation | Story + cutscene systems are expensive to build and maintain; the 1-bit aesthetic actually supports absence of explicit narrative (the visual ambiguity invites player interpretation); explicitly out of scope | Environmental storytelling through level geometry: broken altars, scattered bones, torch patterns, door configurations — imply history without cutscenes |
| Procedural audio generation | Unique per playthrough | Technically complex to implement in custom C++ with no audio middleware; the aesthetic payoff is marginal | High-quality curated SFX + dynamic mixing via parameterized volume/pitch randomization; achieves variety without proceduralism |
| Full controller remapping | Player-friendliness | Low priority for v1; adds UI complexity; the default WASD + mouse mapping is genre-standard and needs to just work well | Sensible defaults with mouse sensitivity and FOV exposed; controller support can be v2 feature |

---

## Feature Dependencies

```
[1-bit Dither Post-Process]
    └──requires──> [3D scene rendering with depth buffer]
                       └──requires──> [Custom rendering pipeline (OpenGL/Vulkan)]

[Dithering-responsive lighting]
    └──requires──> [1-bit Dither Post-Process]
    └──requires──> [Point light sources in scene]
                       └──requires──> [Custom rendering pipeline]

[Boss encounter]
    └──requires──> [Enemy AI state machine]
    └──requires──> [Melee + ranged combat]
    └──requires──> [Player health system]

[Weapon progression]
    └──requires──> [Melee combat]
    └──requires──> [Ranged combat]
    └──requires──> [Weapon pickup / swap mechanic]

[Ranged combat (projectiles)]
    └──requires──> [Hit detection system]
    └──requires──> [Projectile physics/trajectory]

[Handcrafted level design]
    └──requires──> [Level geometry loading]
    └──requires──> [Gothic architectural asset set]

[Save / checkpoint system]
    └──requires──> [Level progression state tracking]

[Options menu (FOV, sensitivity)]
    └──requires──> [Windowing/input system]

[Audio: footsteps, combat, ambient]
    └──requires──> [Audio playback system]

[Enemy AI (patrol, detect, attack)]
    └──enhances──> [Multiple enemy types]

[Point lighting (torches)]
    └──enhances──> [Gothic cathedral environment]
    └──enhances──> [Dithering-responsive lighting]

[1-bit dither visual style]
    ──conflicts──> [Color-coded HUD elements]
    ──conflicts──> [Animated/flickering dither noise]
```

### Dependency Notes

- **1-bit dither requires depth buffer:** The dither threshold must sample scene luminance and depth; a plain framebuffer capture is insufficient.
- **Dithering-responsive lighting requires point lights:** Without dynamic point lights (torches), the dither density is uniform and flat — the differentiating visual effect doesn't manifest.
- **Boss requires full AI + combat systems:** A boss is a specialized enemy; both melee and ranged combat systems must be solid before a boss encounter is designed.
- **1-bit conflicts with color-coded HUD:** Standard game UI conventions (red = danger, green = health) are unavailable. All HUD information must be conveyed through shape, size, position, or animation rather than color.
- **Handcrafted levels conflict with late design changes to the rendering pipeline:** Level geometry authored against early renderer assumptions will need re-work if the rendering pipeline changes significantly. Build rendering first, levels second.

---

## MVP Definition

### Launch With (v1)

Minimum viable to validate the core concept: does the 1-bit first-person dungeon combat feel good?

- [ ] Custom C++ renderer with 1-bit dithering post-process — the entire visual identity; without this, nothing else is on-brand
- [ ] First-person camera with WASD + mouse look, FOV slider, sensitivity settings — locomotion must feel smooth before any other mechanic is tested
- [ ] Player health system with 1-bit-readable HUD indicator — shape-based, not color-coded
- [ ] Melee combat with hit detection and feedback (audio + enemy reaction) — core loop validation
- [ ] Ranged combat (at least one projectile weapon) — differentiates from pure melee dungeon crawlers
- [ ] 3 enemy types minimum (grunt, ranged, heavy) with patrol/detect/attack AI — enough variety to make combat interesting
- [ ] One boss encounter — climax; confirms the game has a shape
- [ ] Point light sources (torches) that affect dither density — the signature visual effect
- [ ] 2-3 handcrafted levels in gothic cathedral setting — enough content to feel like a game, not a prototype
- [ ] Save at level boundaries — players must be able to stop and resume
- [ ] Basic audio: footsteps, weapon impacts, ambient dungeon sounds — silence breaks immersion
- [ ] Options menu: FOV, mouse sensitivity, audio volume, fullscreen toggle

### Add After Validation (v1.x)

Add once core loop is confirmed fun:

- [ ] Weapon upgrade mechanic — trigger: players ask "can I improve my weapon?" or express attachment to specific weapons
- [ ] Additional enemy types (4-6 total) — trigger: combat feels repetitive after 30 minutes
- [ ] Additional handcrafted levels (4-6 total) — trigger: players express desire for more content
- [ ] Screen-space effects that enhance dithering (AO, depth of field approximation) — trigger: visuals feel flat in playtesting
- [ ] Additional boss encounters — trigger: core boss design is validated as fun
- [ ] Death/run statistics screen — trigger: players ask "how far did I get?"

### Future Consideration (v2+)

Defer until product-market fit is established:

- [ ] Full controller support — defer: WASD + mouse is genre standard; controller adds input mapping complexity
- [ ] Mod/level editor support — defer: significant infrastructure; only relevant if the game has an active community
- [ ] Advanced accessibility options (colorblind mode is irrelevant for 1-bit; subtitles for ambient audio cues) — defer: validate concept first
- [ ] Steam achievements / leaderboards — defer: platform integration is non-trivial in custom C++; has zero impact on game quality

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| 1-bit dithering post-process | HIGH | HIGH | P1 — core identity, build first |
| First-person camera + locomotion | HIGH | MEDIUM | P1 — nothing works without this |
| Player health + HUD | HIGH | LOW | P1 — trivial, do immediately |
| Melee combat with feedback | HIGH | MEDIUM | P1 — core loop |
| Enemy AI (3 states) | HIGH | MEDIUM | P1 — core loop enabler |
| Point light sources (torches) | HIGH | HIGH | P1 — differentiating visual effect |
| Ranged combat (projectiles) | MEDIUM | MEDIUM | P1 — expected for genre variety |
| Boss encounter | HIGH | HIGH | P1 — confirms game has a shape |
| Gothic level geometry (2-3 levels) | HIGH | HIGH | P1 — content needed for validation |
| Audio (footsteps, combat, ambient) | HIGH | MEDIUM | P1 — absence breaks immersion |
| Save / checkpoint system | HIGH | LOW | P1 — unshippable without it |
| FOV + sensitivity options | HIGH | LOW | P1 — motion sickness mitigation |
| Multiple enemy types (3+) | MEDIUM | MEDIUM | P2 — adds combat depth |
| Weapon variety / upgrade | MEDIUM | MEDIUM | P2 — adds progression arc |
| Weapon pickup / swap | MEDIUM | LOW | P2 — enables weapon progression |
| Screen-space AO / depth effects | MEDIUM | HIGH | P2 — visual polish |
| Run statistics / death screen | LOW | LOW | P2 — player feedback |
| Full controller support | LOW | MEDIUM | P3 — niche for keyboard-first genre |
| Steam integration | LOW | MEDIUM | P3 — post-validation |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

---

## Competitor Feature Analysis

| Feature | Return of the Obra Dinn | Gunfire Reborn / Roboquest (FPS Roguelites) | Our Approach |
|---------|------------------------|----------------------------------------------|--------------|
| Visual style | 1-bit dithered (investigation game, not action) | Full color, stylized | 1-bit dithered action — unique combination |
| Perspective | First-person | First-person | First-person |
| Level structure | Handcrafted, linear | Procedurally generated | Handcrafted, gothic cathedral |
| Combat | None (exploration/puzzle) | Fast-paced gunplay | Melee + ranged, deliberate pacing |
| Progression | Item identification (mystery) | Roguelite meta-progression + run upgrades | Weapon find/upgrade per run |
| Enemy variety | N/A | Many enemy types + boss waves | 3-5 types + boss(es) |
| Audio | Atmospheric, minimal music | Energetic soundtrack | Gothic ambient, deliberate sound design |
| FOV control | Present (community mods exist for it) | Standard | Required; Obra Dinn's oversight is a cautionary note |
| Story / narrative | Central to the game | Absent / minimal | Absent; environmental only |
| Accessibility | Limited (motion sickness was a complaint) | Standard FPS accessibility | FOV slider + sensitivity as minimum mitigation |

---

## Sources

- [The Best First-Person Roguelike Video Games — The Gamer](https://www.thegamer.com/best-first-person-roguelikes/)
- [The Best First-Person Roguelites — Rogueliker](https://rogueliker.com/fps-roguelikes/)
- [First-Person Dungeon Crawler RPGs: Their Resurrection — Turn Based Lovers](https://turnbasedlovers.com/originals/first-person-dungeon-crawler-rpgs-their-resurrection-what-to-play-now-and-what-lies-ahead/)
- [Lucas Pope and the rise of the 1-bit dither-punk aesthetic — Game Developer](https://www.gamedeveloper.com/design/lucas-pope-and-the-rise-of-the-1-bit-dither-punk-aesthetic)
- [Lucas Pope on the challenge of creating Obra Dinn's 1-bit aesthetic — PC Gamer](https://www.pcgamer.com/lucas-pope-on-the-challenge-of-creating-obra-dinns-1-bit-aesthetic/)
- [Fix Obra Dinn Motion Sickness — GameHelper](https://www.gamehelper.io/games/return-of-the-obra-dinn/articles/common-issues-and-fixes-in-return-of-the-obra-dinn-complete-resolution-guide)
- [Analysis: The Eight Rules of Roguelike Design — Game Developer](https://www.gamedeveloper.com/game-platforms/analysis-the-eight-rules-of-roguelike-design)
- [What makes a dungeon crawl good? — Skeleton Code Machine](https://www.skeletoncodemachine.com/p/what-makes-a-dungeon-crawl-good)
- [Scope Creep: The Silent Killer of Solo Indie Game Development — Wayline](https://www.wayline.io/blog/scope-creep-solo-indie-game-development)
- [Juicy damage UI feedback in video games — Lennart Nacke / Medium](https://acagamic.medium.com/juicy-damage-feedback-in-games-7c1758d69a42)
- [HUD (video games) — Wikipedia](https://en.wikipedia.org/wiki/HUD_(video_gaming))

---

*Feature research for: first-person 3D roguelike dungeon crawler with 1-bit dithered aesthetic*
*Researched: 2026-03-23*
