# Pitfalls Research

**Domain:** Custom C++ 3D first-person roguelike with 1-bit dithered rendering
**Researched:** 2026-03-23
**Confidence:** HIGH (verified across multiple sources; dithering specifics from Obra Dinn developer notes and technical papers)

---

## Critical Pitfalls

### Pitfall 1: The Engine Trap — Building Infrastructure Before Gameplay

**What goes wrong:**
The developer spends months building a renderer, level editor, resource pipeline, entity system, and tools without writing a single line of gameplay code. The thinking is "gameplay comes when the engine is mature enough." The engine never becomes mature enough. The project never ships.

This is the most common failure mode for custom engine projects, not a rare edge case.

**Why it happens:**
Engine architecture work feels productive and intellectually engaging. It's also safer — there's no risk of discovering the game isn't fun if you never test the game. Each system spawns three more systems that are "needed first." The lack of a playable build means there's no external pressure forcing a stop.

**How to avoid:**
Establish a rule on day one: every week must produce something playable. Start with the hardest game-feel problem (first-person movement + collision) before any abstraction layers. Build engine features only when the game explicitly demands them — not in anticipation of future needs. Keep a "game-first" checklist: if you haven't tested a mechanic this week, you haven't been working on the game.

**Warning signs:**
- Three or more engine systems exist but no player can move through a level
- Work session notes reference "abstraction layer" or "interface design" more than "combat" or "level"
- The word "generic" appears in any variable or class name (generic renderer, generic asset manager)
- No playable build exists after the first month

**Phase to address:**
Phase 1 (Engine Foundation) — The phase must be scoped to *only* what lets a player walk through a lit room. Defer everything else.

---

### Pitfall 2: The Swimming Dither — Temporal Instability in Screen-Space Patterns

**What goes wrong:**
The dithering shader uses screen-space UV coordinates for the Bayer/blue-noise pattern lookup. This works perfectly for still frames. The moment the camera moves, the dither pattern stays pinned to the screen while the geometry moves behind it. Every surface appears to have "swimming" or "crawling" pixel noise instead of stable, geometry-relative shading. This is actively nauseating in first-person over extended play.

This was Lucas Pope's primary technical challenge with Return of the Obra Dinn. He documented it extensively in developer logs.

**Why it happens:**
Screen-space UVs are the obvious, simple implementation. The post-process shader reads `gl_FragCoord / resolution` as the pattern index, which is constant per screen pixel regardless of what geometry is behind it. Works fine for UI, breaks for moving 3D geometry.

**How to avoid:**
The pattern must be anchored to world or object space, not screen space. The practical approach is to use the world-space position of the surface (passed from the vertex shader or reconstructed from the depth buffer) as the dither pattern index. For a first-person game this means: reconstruct world position per fragment in the post-process pass using `inverse(projection * view) * NDC_position`, then use `world_pos.xy` (or a projected variant) as the pattern coordinate. This keeps the pattern "pinned" to the geometry during camera rotation.

Note: complete stability during camera *translation* (walking) is harder — patterns will still swim somewhat. Obra Dinn accepted this tradeoff. Blue noise patterns are less perceptually distracting than Bayer during translation.

**Warning signs:**
- Dither pattern moves noticeably when rotating the mouse
- Any pixel on a static wall appears to flicker during camera pan
- Pattern implementation uses `gl_FragCoord` or screen UV directly as the sole pattern coordinate

**Phase to address:**
Phase 1 (Rendering Foundation) — The dithering pass architecture must be designed correctly from the start. Retrofitting world-space pattern anchoring after other systems are built is significantly harder.

---

### Pitfall 3: Gamma / Linear Color Space Mismatch Corrupting the Dithering Threshold

**What goes wrong:**
The game renders in linear color space (correct for physically-based lighting) but the dithering threshold comparison happens on gamma-corrected (non-linear) values, or vice versa. The result: dithering looks visually wrong — dark regions get too many black pixels, bright regions get too many white pixels, and the tonal range looks crushed rather than smooth.

This matters especially for the torch/point light falloff. The visual gradient from a torch that should dither gracefully into black will instead produce a harsh edge.

**Why it happens:**
OpenGL framebuffers default to outputting sRGB, but intermediate render targets may be linear. Developers often add gamma correction as an afterthought and don't consider that the dithering post-process sits at the end of the chain where color space is in a mixed state.

**How to avoid:**
Decide on one color space strategy before writing the first shader: linear throughout, apply gamma correction *last* (after dithering), or work in gamma-corrected space consistently. The recommended approach: render in linear space, apply dithering in linear space against a linear-space threshold, then apply gamma correction/sRGB conversion as the very final step. Use `GL_FRAMEBUFFER_SRGB` carefully — enable it only for the final blit to the window framebuffer, not for intermediate passes.

**Warning signs:**
- Torch falloff looks harsh or "steppy" rather than smooth gradient-to-black
- Dithered dark areas look noisier than expected
- `GL_FRAMEBUFFER_SRGB` is enabled globally rather than only on the output pass
- Dithering threshold is compared against a value that went through `pow(x, 2.2)` at some point before comparison

**Phase to address:**
Phase 1 (Rendering Foundation) — Establish the framebuffer pipeline, color space strategy, and render target format before the dithering pass is implemented.

---

### Pitfall 4: Picking Vulkan as the Graphics API

**What goes wrong:**
Vulkan is chosen because it's "modern" and "lower overhead." Drawing a triangle takes 1,000+ lines vs. ~100 in OpenGL. The first three months of the project are spent fighting resource barriers, descriptor sets, pipeline state objects, memory allocation, and validation layer errors. No dithering shader is written. No player moves. The engine trap (Pitfall 1) is made significantly worse.

**Why it happens:**
Vulkan has strong mindshare in 2025-2026. It genuinely is more performant and explicit. Developers underestimate how much the boilerplate costs in a solo/small team context.

**How to avoid:**
Use OpenGL 4.5 (core profile) or OpenGL 4.6 for this project. The performance headroom is irrelevant for a 1-bit roguelike — the 1-bit output is the bottleneck on visual fidelity, not GPU throughput. OpenGL 4.5 provides direct state access (DSA), compute shaders, shader storage buffers, and everything needed for a post-process dithering pipeline. It takes 100 lines to draw a triangle and 2 days to have a dithering prototype vs. 2 months.

If cross-platform support (Windows/macOS/Linux) is required: GLFW handles windowing and OpenGL context creation on all three platforms. On macOS, OpenGL 4.1 is the maximum supported version — this is a known constraint.

**Warning signs:**
- Any Vulkan tutorial is open during Phase 1 work
- "I'll abstract it behind an interface so we can swap APIs later" — this is premature abstraction (see Pitfall 6)
- More than 2 weeks pass without a triangle on screen

**Phase to address:**
Phase 1 (Engine Foundation) — The API decision is made once, early, and not revisited until the game ships (if ever).

---

### Pitfall 5: Collision Response Getting Players Permanently Stuck

**What goes wrong:**
Player movement works in open space but breaks at wall edges, corners, and steps. Players get caught on small geometry protrusions and cannot move. Corner collisions cause the player to teleport or vibrate. Moving along a wall while simultaneously pressing into it causes the "deadlock" problem where two collision normals cancel each other and the player cannot move at all.

This is a consistently reported pain point for custom 3D engine collision and requires specific architecture to solve correctly.

**Why it happens:**
Simple AABB-vs-AABB collision detection detects overlap but the response (depenetration) is implemented naively: push the player out along the deepest axis. This fails at corners (multiple simultaneous contacts). Discrete collision detection (check position, see if overlap) misses tunneling at high velocity. Developers build basic collision first, then discover it needs a full rewrite when level design starts.

**How to avoid:**
Use capsule sweep testing for player collision from the start. A capsule (cylinder with hemispherical caps) naturally slides around corners without getting stuck. Implement swept collision (check for overlap along the movement vector, not just at the endpoint) to prevent tunneling. For the collision response, apply Quake-style "clip and slide": subtract the component of velocity perpendicular to the hit normal, leaving the parallel component to continue. This is the standard algorithm used by Quake, Doom, and most first-person games.

For this game: the engine likely needs only player-vs-level collision (not enemy-vs-enemy physics). Keep collision simple and specialized — a capsule vs. static triangle mesh or AABB-based level geometry.

**Warning signs:**
- Player can be pushed into a corner and gets stuck
- Moving diagonally into a wall causes a complete stop instead of sliding
- Player clips through thin walls at sprint speed
- The word "AABB" appears in the player movement code without a sweep test

**Phase to address:**
Phase 1 (Player Movement) — Must be solved before any level design begins. A bad collision system invalidates all level testing.

---

### Pitfall 6: Premature Abstraction of the Rendering Layer

**What goes wrong:**
An abstract `IRenderer` interface is created to "support multiple backends" (OpenGL, Vulkan, DirectX). The interface becomes the wrong abstraction — each backend exposes different primitives and the interface forces the lowest common denominator. Time is spent maintaining the abstraction rather than building the game. When a rendering feature is needed that doesn't fit the interface, the interface must be redesigned.

**Why it happens:**
Software engineering instincts. Developers know that coupling is bad, so they add indirection. The abstraction feels like architecture discipline. It is premature — the game doesn't need multiple rendering backends, and the "correct" abstraction only becomes apparent after having two concrete implementations.

**How to avoid:**
Write directly against OpenGL for the entire project. Only extract an abstraction when there are three concrete cases that share obvious structure. A single thin wrapper around common OpenGL calls (shader compilation, texture upload) is acceptable and useful. A full `IRenderer` interface is not. If the decision is later made to port to another API, the rewrite is well-scoped and the game's architecture is already proven.

**Warning signs:**
- An interface is defined before a single implementation of it exists
- Class names include "Base", "Abstract", "Interface", or "I" prefix with no corresponding concrete sibling
- Passing around `std::shared_ptr<ITexture>` when a `GLuint` would work

**Phase to address:**
Phase 1 (Architecture Setup) — Establish a "no abstraction without two concrete cases" rule at project start.

---

### Pitfall 7: Lighting That Doesn't Survive the Dithering Pass

**What goes wrong:**
The 3D lighting looks good in the intermediate (pre-dither) grayscale render but terrible after dithering. Common symptoms: torch falloff produces a hard concentric ring pattern instead of a smooth gradient dither; very dark areas collapse to solid black with no gradation; bright surfaces clip to solid white. The lighting that was tuned in grayscale stops communicating useful spatial information once reduced to 1-bit.

**Why it happens:**
Lighting is tuned visually in linear/grayscale. The dither quantizes the result and amplifies any sudden transitions. High-contrast lighting (common in dungeons) creates transitions too steep for the dither pattern to convey smoothly. Torch falloff (1/r² attenuation) drops off sharply enough to produce a visible edge in the dither pattern.

**How to avoid:**
Tune lighting *through* the dithering pass, not before it. Build the dithering post-process in Phase 1 and keep it active during all subsequent lighting work. Adjust light attenuation curves specifically for 1-bit output — the physically correct 1/r² often needs to be replaced with a softer falloff (e.g., linear or 1/r) to produce smooth dithered gradients. Limit the number of visible point lights per scene to avoid dither pattern conflicts between light sources. Obra Dinn used relatively few, carefully placed light sources for exactly this reason.

For torch flickering: temporal variation in light intensity must be subtle (5-10% amplitude) or it will cause the dither pattern to strobe visibly.

**Warning signs:**
- Hard concentric rings visible around torches in the final dithered view
- Large areas of the level read as pure black or pure white with no intermediate dithering
- Light is tuned in a non-dithered debug view

**Phase to address:**
Phase 2 (Lighting System) — The dithering pass must be in place before any lighting is tuned.

---

### Pitfall 8: Shader Compilation Stutter on First Load

**What goes wrong:**
The game loads and immediately hitches for 200-500ms as the GPU compiles each shader the first time it's encountered. In a first-person game this produces visible frame drops during normal play, especially when entering a new area that uses a different shader combination.

**Why it happens:**
OpenGL (unlike Vulkan/DX12) compiles shaders lazily — the driver sees the full pipeline state only at draw time and may defer compilation. Even when shaders are compiled upfront, some GPU drivers re-compile at draw time based on render state.

**How to avoid:**
Compile all shaders at startup during a loading phase. Store compiled shader binaries where supported (`GL_ARB_get_program_binary` / OpenGL 4.1+) to avoid recompilation on subsequent launches. For this project, the total shader count will be small (dithering post-process, geometry, lighting — maybe 5-8 shaders), so up-front compilation is trivial. Do a "warm-up draw" of each shader combination during loading to force any driver-side compilation.

**Warning signs:**
- Frame time spikes visible in the GPU profiler when entering areas
- RenderDoc shows shader compilation time in the first draw using a given program
- Shaders are linked on-demand at scene load rather than at startup

**Phase to address:**
Phase 1 (Rendering Foundation) — Establish a startup shader compilation step from the beginning.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Global state for rendering context (OpenGL state machine used directly everywhere) | Fast to write, no boilerplate | Impossible to reason about render state; bugs appear only when draw call order changes | Never — use even a thin state tracker |
| Hardcoded level geometry in C++ structs | No asset pipeline needed for MVP | Adding/changing levels requires recompile; level designers (even solo) can't iterate | Only for the very first prototype level |
| Screen-space UV for dither pattern | Simple one-liner | Swimming dither destroys the visual (see Pitfall 2) | Never |
| Variable timestep (multiply everything by `deltaTime`) | Simple, works at low complexity | Physics becomes framerate-dependent; game plays differently at 30fps vs 144fps | Only if there is no physics or collision |
| AABB overlap (discrete) for player collision | 20 lines of code | Player gets stuck on every corner edge (see Pitfall 5) | Never for a first-person game |
| Single monolithic `Game` class | Everything accessible everywhere, no architecture planning | 2,000-line .cpp file that can't be tested or modified | First prototype only — refactor before adding combat |
| Heap allocation every frame for game objects | Flexible, easy to implement | Fragmentation, GC pressure, jitter in frame times | Never in the game loop hot path |

---

## Integration Gotchas

Common mistakes when integrating external libraries and systems.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| GLFW + OpenGL context | Forgetting `glfwMakeContextCurrent` on the right thread; calling OpenGL from a thread that doesn't own the context | Create context on main thread, make current immediately, never share across threads without explicit sync |
| stb_image texture loading | Loading textures with default gamma-uncorrected format; not specifying `STBI_rgb` vs `STBI_rgb_alpha` explicitly | Load textures, then upload with `GL_SRGB8_ALPHA8` for color textures, `GL_R8` for grayscale/metallic maps |
| Dither pattern texture | Enabling texture compression (driver default on some platforms) corrupts the precise per-texel values | Always upload dither textures with `GL_NEAREST` filtering and no mipmaps; explicitly set `GL_TEXTURE_MAX_LEVEL 0` and use uncompressed format |
| GLM math library | Mixing row-major vs column-major matrix conventions; forgetting to use `glm::value_ptr()` when passing to OpenGL | OpenGL expects column-major; GLM defaults to column-major; always pass via `glm::value_ptr(&matrix)` |
| Dear ImGui debug overlay | Leaving ImGui draw calls in release builds; ImGui state corruption if `NewFrame()` and `Render()` are not paired every frame | Guard ImGui behind a compile-time flag; assert that every `NewFrame` has a matching `Render` |

---

## Performance Traps

Patterns that produce frame timing problems in a real-time context.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Full-screen post-process dithering reading depth + world position reconstruction every fragment | GPU time for dithering pass exceeds geometry pass on dense geometry | Profile the dithering pass early; world-position reconstruction from depth is expensive — consider storing world pos in a G-buffer texel instead | At 1440p+ with a complex scene |
| Multiple `glUniform` calls per object per frame instead of UBO | CPU-side GPU command overhead dominates frame time | Use Uniform Buffer Objects for per-frame and per-camera data; only per-object transforms need per-draw uniforms | At >100 draw calls per frame |
| Dynamic geometry generation for every animated enemy pose per frame | CPU bottleneck on skeleton transforms | Pre-compute bone matrices, upload only the final 4x4 matrices to GPU as a texture or SSBO | At >10 simultaneously animated enemies |
| Heap allocation inside the game loop for temporary strings (e.g., debug text) | Subtle frame time variance; fragmentation over time | Use stack allocators or pre-allocated string buffers for anything inside the game loop | After ~30 minutes of play |
| Rendering the full scene depth buffer every frame even when the level is static | Wasted GPU work on the Z-pass | For a handcrafted static level, pre-bake occlusion; re-render only when dynamic objects (enemies) are present | At larger level sizes with many draw calls |

---

## UX Pitfalls

Visual and gameplay experience mistakes specific to 1-bit first-person roguelikes.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Enemies and environment are the same shade — player can't parse the scene | Players can't identify threats, navigation is confusing | Use lighting deliberately to differentiate enemies from background: enemies are closer to the player (brighter), walls are darker |
| Melee swing animation obscures entire screen during attack | Player can't see enemies while attacking — feels blind and punishing | Keep weapon viewmodel in lower 1/3 of screen; swing animation moves weapon into center but returns quickly |
| No audio feedback for dithered hit detection | Players don't know if they hit or missed due to 1-bit visuals making impact hard to read | Sound design and screen shake carry hit feedback weight — these are essential, not polish |
| 1-bit rendering makes depth extremely hard to judge | Players repeatedly misjudge distances to enemies and ledges | Use motion parallax and head-bob aggressively — the only reliable depth cue in 1-bit is motion |
| Dither pattern variation with distance makes distant enemies invisible | Enemies at range dither down to near-solid black and blend with walls | Ensure a minimum brightness level for enemies (an ambient term that prevents full-black dithering) |
| Swimming dither causes motion sickness during camera rotation | Players stop playing after 20 minutes | See Pitfall 2 — non-negotiable fix |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **First-person movement:** Often missing wall sliding — verify the player can move diagonally along a wall without stopping
- [ ] **Dithering post-process:** Often missing temporal stability — verify the dither pattern does not swim when slowly panning the camera
- [ ] **Torch lighting:** Often missing graceful falloff through the dither — verify the transition from lit to dark around a torch shows smooth dithered gradient, not a hard ring
- [ ] **Enemy hit detection:** Often uses ray from camera center only — verify hits register when the weapon model is to the left/right, not just dead-center
- [ ] **Game loop timestep:** Often uncapped or tied to rendering — verify the game plays identically at 30fps and 144fps (enemy speed, projectile speed, collision)
- [ ] **Shader compilation:** Often lazy — verify no frame spike occurs when the dithering shader is used for the first time after load
- [ ] **Texture format for dither pattern:** Often accidentally compressed — verify the Bayer/blue-noise texture has GL_NEAREST filtering and no compression
- [ ] **Color space pipeline:** Often mixed — verify the dithering threshold comparison occurs before gamma correction, not after

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Engine trap (months of engine, no gameplay) | HIGH | Stop all engine work; spend one week implementing the single most important gameplay mechanic even if it's ugly; evaluate whether to continue or restart with different scope |
| Swimming dither already shipped | MEDIUM | Add world-position reconstruction to the dithering shader; world-pos is in the vertex shader already — pass it through and use in frag shader; test with slow pan |
| Gamma/color space mismatch | MEDIUM | Audit the render pass chain; add a debug visualization mode that shows pre-dither grayscale; fix the framebuffer format where the mismatch occurs |
| Player stuck in corners | MEDIUM | Replace AABB collision response with a capsule sweep + clip-and-slide; this is usually a full rewrite of the movement code but not the rest of the engine |
| Premature abstraction making new rendering features impossible to add | MEDIUM | Remove the abstraction and write to OpenGL directly; the code will get shorter, not longer |
| Lighting that kills the dithering aesthetic | LOW | Re-tune light attenuation curves with the dither pass enabled; soften falloff; reduce maximum light radius; add ambient floor to prevent full-black dithering |

---

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Engine trap | Phase 1 (Player Movement first, not systems) | Playable first-person movement exists before any abstraction work |
| Swimming dither | Phase 1 (Dithering Pass Architecture) | Camera rotation test shows stable, non-swimming dither pattern |
| Gamma/color space mismatch | Phase 1 (Framebuffer + Render Pass Setup) | Pre-dither grayscale and post-dither 1-bit both look correct; torch falloff is smooth |
| Vulkan API choice | Phase 1 (Project Setup, Day 1) | Decision logged in PROJECT.md before any code is written |
| Player stuck on corners | Phase 1 (Player Movement) | Player can strafe into a corner and slide along both walls freely |
| Premature abstraction | Phase 1 (Architecture Setup) | No interface has only one implementation |
| Lighting fails through dithering | Phase 2 (Lighting) | All lighting is tuned with dithering pass enabled |
| Shader compilation stutter | Phase 1 (Rendering Foundation) | No frame spike on first draw of any shader in a profiler |

---

## Sources

- Lucas Pope developer logs on Obra Dinn dithering (TIGSource): https://dukope.com/devlogs/obra-dinn/tig-32/
- "Stabilizing the Obra Dinn 1-bit dithering process" Hacker News discussion: https://news.ycombinator.com/item?id=42084080
- "Surface-Stable Fractal Dither" by Aras Pranckevičius (2025): https://www.aras-p.info/blog/2025/02/09/Surface-Stable-Fractal-Dither-on-Playdate/
- "Solodevs and the trap of the game engine" by Karl Zylinski: https://zylinski.se/posts/solodevs-and-the-trap-of-the-game-engine/
- "Real reasons (not) to build custom game engines in 2024" — Game Developer: https://www.gamedeveloper.com/programming/real-reasons-not-to-build-custom-game-engines-in-2024
- "How I learned Vulkan and wrote a small game engine with it" (recommends OpenGL first): https://edw.is/learning-vulkan/
- LearnOpenGL — Deferred Shading, Gamma Correction: https://learnopengl.com
- "What I Learned Building My Own Game Engine from Scratch in C++ & DirectX 12": https://dev.to/shahfarhadreza/what-i-learned-building-my-own-game-engine-from-scratch-in-c-directx-12-2538
- "Fix Your Timestep!" — Gaffer on Games (canonical game loop reference): https://gafferongames.com/post/fix_your_timestep/
- "Game Engines and Shader Stuttering" — Unreal Engine tech blog: https://www.unrealengine.com/en-US/tech-blog/game-engines-and-shader-stuttering-unreal-engines-solution-to-the-problem
- Capsule collision for player characters — Wicked Engine: https://wickedengine.net/2020/04/capsule-collision-detection/
- "Dithering on the GPU" — Alex Charlton: https://alex-charlton.com/posts/Dithering_on_the_GPU/
- "Obra Dinn 1-bit shader effect" Unity tutorial (dithering technique breakdown): https://discussions.unity.com/t/tutorial-obra-dinn-1-bit-shader-effect/831511

---
*Pitfalls research for: Custom C++ 3D first-person roguelike, 1-bit dithered rendering*
*Researched: 2026-03-23*
