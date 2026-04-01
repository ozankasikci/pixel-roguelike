 # Game Mechanic Ideation — 2026-03-31

## Context

This document captures the ideation session for rethinking the game's core mechanic and story direction. The previous "Warden" design (prison simulation, MERI narrator, Daniel/Sophie story) was set aside because it lacked a real game mechanic — it read as a screenplay, not a game.

## Ideation Path — Options Explored & Rejected

### Step 1: What kind of game mechanic?

We identified six categories of mechanics from successful unique games:

1. **Defiance & narrator tension** (Stanley Parable, Beginner's Guide) — the game is a conversation with a voice ← **CHOSEN**
2. Space that lies (Layers of Fear, P.T., Antichamber) — environment changes when you're not looking
3. Knowledge as progression (Outer Wilds, Forgotten City) — no upgrades, only understanding
4. Investigation & deduction (Obra Dinn, Her Story, Golden Idol) — examine evidence, synthesize
5. Living schedules (Forgotten City, Phasmophobia) — NPCs on routines you learn and exploit
6. Nested/shifting game modes (Inscryption, Edith Finch) — the game becomes a different game

**Decision:** Option 1 — narrator defiance. The player's relationship with a voice is the core mechanic.

### Step 2: What kind of narrator tension?

Four options explored:

- **A)** Helpful voice that turns out to be lying/manipulating (Portal's GLaDOS)
- **B)** Openly antagonistic voice toying with you (Saw, game show host)
- **C)** The voice is *you* — internal monologue, past self (something new)
- **D)** Two voices competing for your obedience

**Decision:** Not locked in yet. But the narrator should start reasonable and the trust should be ambiguous — not obviously evil.

### Step 3: Story source

Rather than inventing a story from scratch, we looked for existing well-written works to build on. We researched 25+ books, films, and stories. The strongest candidates:

1. **Severance** — office setting, corporate narrator, absurd rewards/punishments
2. **The Remains of the Day** (Ishiguro) — narrator is your own internalized duty
3. **The Lobster** (Lanthimos) — dual systems, no free position
4. **"The Ones Who Walk Away from Omelas"** (Le Guin) — the system works, the cost is visible
5. **The Trial** (Kafka) — system processes you whether you participate or not
6. **Never Let Me Go** (Ishiguro) — total quiet acceptance, no resistance
7. **Dogtooth** (Lanthimos) — language as prison, defiance starts with vocabulary

**Decision:** Dystopian direction chosen, but no specific source locked in. The Platform (El Hoyo) cited as tonal inspiration — a system with simple visible rules that produce brutal outcomes.

### Step 4: What's the central object/mechanic?

We decided to focus the game around **doors** — the player's primary interaction.

### Step 5: What do doors mean mechanically?

Two options:
- **A)** Traversal and consequence — you open doors and deal with what's behind them ← **CHOSEN**
- **B)** Power and responsibility — you control doors for other people

**Decision:** Option A. The player is the one going through doors.

### Step 6: What are the stakes?

Three options discussed:
- **A)** Survival — wrong doors can kill you or set you back
- **B)** Knowledge — wrong doors give false information or hide truth
- **C)** Other people — what's behind doors affects others

**Decision:** Not locked in. Likely a combination.

### Step 7: Three concrete concepts from proven mechanics

**Concept A:** "Hades doors + Stanley Parable narrator + Silent Hill 2 profiling" — each room has doors with partial clues, narrator recommends one, game silently profiles your behavior ← **CHOSEN**

**Concept B:** "Slay the Spire branching map + Papers Please complicity + Outer Wilds knowledge" — visible map of doors ahead, simple tasks at each with moral weight, knowledge is the only progression

**Concept C:** "Darkest Dungeon escalating commitment + Amnesia placebo meter + Hades door rewards" — deeper = more invested, a meter that may not matter, narrator warns about going too deep

**Decision:** Concept A selected.

## Key Decisions

### What we're keeping
- First-person 3D perspective
- Custom C++ engine (OpenGL 4.1, EnTT ECS, Jolt Physics)
- Stanley Parable-inspired art direction (clean, minimalist, warm lighting, muted palette)
- A narrator/voice that talks to the player

### What we're changing
- The story needs to serve the mechanic, not the other way around
- The environment should not "dramatically reshape" — that's spectacle, not a mechanic
- Need an actual 30-second gameplay loop, not just narrative exploration

## Core Mechanic: Doors

The game is built around **doors as the primary interaction**.

A door is:
- A binary choice — open or closed
- A commitment — you walk through, you're somewhere new
- A rejection — you chose this door, not that one
- Information — what's behind it? What clues are available?
- Power — who decides which doors are open?

The mechanic is **traversal and consequence** — you open doors and deal with what's behind them.

## Chosen Concept: "Hades Doors + Stanley Parable Narrator + Silent Hill 2 Profiling"

Three proven mechanics combined:

### 1. Visible reward previews before choosing (from Hades)
Each room has 2-3 doors. Each door shows a **partial clue** about what's behind it — a symbol, a color, light leaking through, sound coming from the other side. The player reads the clues and chooses which door to enter. The information is always incomplete — you know *something*, but never everything.

### 2. Narrator defiance as the core verb (from Stanley Parable)
A narrator voice tells you which door to take. You can obey or defy. The game reacts to your choice. The narrator's advice might be helpful, manipulative, or a test. Over time, the player learns whether the narrator can be trusted — or whether that trust shifts depending on context.

### 3. Invisible behavioral profiling (from Silent Hill 2)
The game secretly tracks how the player behaves:
- Which doors they linger at before choosing
- How fast they make decisions
- Whether they tend to obey or defy the narrator
- Whether they go back to re-examine things
- What clues they pay attention to

This behavioral profile silently shapes what appears behind future doors. The game reads you without telling you it's reading you.

### How it plays (30-second loop)

1. **Enter a room** — 2-3 doors on the walls
2. **Read the clues** — symbols above doors, sounds through them, light under them, the narrator's recommendation
3. **Choose a door** — obey the narrator or defy, follow the clues or gamble
4. **Deal with the consequence** — what's behind the door changes your situation
5. **Repeat** — the next room has new doors, and what's behind them has been shaped by your history of choices

### What makes it deep

- The narrator might be lying. The clues might be misleading.
- Your pattern of choices (cautious? reckless? obedient? defiant?) shapes what the system puts behind future doors.
- Knowledge from previous runs carries over — you start recognizing patterns, symbols, and the narrator's tells.
- The game is about learning who to trust: yourself, the narrator, or the evidence.

## Setting: Dystopian (TBD)

The setting should be dystopian but the specific world is not yet defined. Requirements:
- A contained, explorable space (facility, building, institution)
- A system with rules the player can learn and defy
- A narrator voice that has a reason to exist in the world
- A reason for doors to matter (architecturally, narratively)

## Influences & Proven Mechanics Referenced

### Primary mechanical influences
| Game | What we're taking |
|------|------------------|
| **Stanley Parable** | Narrator defiance as the core verb — the voice says go left, you decide |
| **Hades** | Visible partial reward icons on doors before choosing — strategic decision every 30 seconds |
| **Silent Hill 2** | Invisible behavioral profiling that shapes outcomes without the player knowing |
| **Slay the Spire** | Branching map where choosing one path locks out another — route planning is strategy |
| **Outer Wilds** | Knowledge as the only progression — nothing carries over except what you've learned |
| **Papers, Please** | Simple mundane actions with moral weight — the horror is what the action means |
| **Amnesia** | Placebo meter — a visible system the player *believes* matters more than it does |
| **Darkest Dungeon** | Escalating commitment — the deeper you go, the more you've invested, the harder to turn back |

### Story/tone influences
| Source | What it contributes |
|--------|-------------------|
| **The Platform** | A system with simple visible rules that produce brutal outcomes; compliance vs. defiance |
| **Stanley Parable** | Clean institutional aesthetic, dark humor, meta-awareness |
| **Severance** | Corporate/institutional setting where the mundane becomes horrifying |
| **Kafka (The Trial)** | A system that processes you whether you participate or not |

### Proven psychological principles at work
| Principle | How it applies |
|-----------|---------------|
| **Variable ratio reinforcement** | Random/partial door rewards on unpredictable schedule |
| **Loss aversion** | Consequences behind wrong doors threaten what you've accumulated |
| **Reactance/defiance instinct** | Telling players "take this door" triggers the desire to take the other one |
| **Placebo control** | Visible meters/status that drive behavior through belief, not actual consequence |
| **Sunk cost escalation** | Deeper = more invested = harder to turn back |

## Door Types Available as Design Tools

From research across fiction, mythology, games, and philosophy:

| Type | Description | Example |
|------|-------------|---------|
| **Choice Door** | 2+ doors, pick one, others are foreclosed | Stanley Parable, Hades |
| **Lying Door** | Doesn't lead where it should | Antichamber, Layers of Fear |
| **Refusing Door** | Should open but won't | Kafka's "Before the Law" |
| **Forbidden Door** | Told not to open it — you will | Bluebeard, Room 237 |
| **Protective Door** | Barrier between you and danger | Amnesia |
| **Sorting Door** | Filters what passes by hidden criteria | Monty Hall, Maxwell's Demon |
| **Identity Door** | Crossing it changes who you are | Severance, Being John Malkovich |
| **Trap Door** | Easy in, impossible out | P.T.'s loop, Backrooms |
| **Memory Door** | Leads to the past or erases memory | Edith Finch, Doorway Effect |

## Concept Art

Initial room concept sketches generated (2026-03-31), stored in `docs/plans/2026-03-31-concept-art/`:
- `first-room-concept-a.png` — Wide view, three doors with colored symbols, ceiling speaker
- `first-room-concept-b.png` — Annotated "Room 427" with differentiated door clues (light, sound, lock)
- `first-room-floorplan.png` — Blueprint floor plan, 5m x 4m, entry opposite three doors

## Open Questions

- What is the specific dystopian setting/world?
- What is the narrator's identity and motivation within the world?
- What are the consequences behind doors? (Survival stakes? Knowledge? Other people affected?)
- What carries over between runs? (Knowledge only? Or some persistent state?)
- How does the game end? What's the "center" the player is moving toward?
- How many rooms/doors total? What's the game's length?
- Is there a branching map the player can see (Slay the Spire style) or is it blind?
- What do the door symbols/clues actually mean? What's the visual language?

---

*Ideation session: 2026-03-31*
*Previous design: docs/plans/2026-03-29-warden-game-design.md (set aside)*
