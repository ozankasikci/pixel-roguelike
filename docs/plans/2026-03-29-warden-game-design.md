# Warden — Game Design Document

## Overview

**Title:** Warden (working title)
**Genre:** Third-person psychological horror
**Length:** 3-5 hours
**Platform:** Windows / macOS (cross-platform via bgfx or similar abstraction)
**Engine:** Custom C++ (EnTT ECS, Jolt Physics)

## Logline

A person who accidentally killed their family checked themselves into an experimental simulation facility decades ago. Inside, they experience a mechanical prison — a puzzle box with shifting architecture managed by a hostile AI entity. They've solved the prison 146,248 times and chosen to erase their memory every time rather than face the truth. This loop — 146,249 — is different. Their sister, now in her 60s, has hacked into the simulation from the outside, and the cracks she's creating are destabilizing the prison in ways the entity can't fully repair.

## Core Concept

The player wakes in a warden's office inside a mechanical prison. An entity speaks through the PA system, treating them as the warden. The prison's architecture shifts in response to the player's actions — locking a cell might rotate a corridor, flipping a switch might open a wing. The player investigates using prison systems: door controls, cameras, intercoms, cell locks.

The player discovers recordings from "past wardens." They sound like different people at first. Slowly, the player realizes they're all the same person — themselves, from previous loops. The prison isn't a trap. The player's inability to face the truth is the trap.

## The Story

### Backstory (never shown directly — assembled through gameplay)

The protagonist caused an accident that killed their family. The specifics are revealed only at the very end, in the terminal recording.

Destroyed by guilt, they found an experimental facility offering simulated environments for trauma processing. Their sister — a teenager at the time — begged them not to go. They signed themselves in anyway.

Inside the simulation, they experience a mechanical prison. Each time they solve the puzzle box and reach the center, a terminal plays a recording of the accident and offers two options: LEAVE or RESET. 146,248 times, they chose RESET. Their memory is erased. The loop begins again.

The sister grew up defined by losing them. She spent decades trying every legal and institutional channel to extract them. All failed. In her 60s, she taught herself to hack the facility's systems. Loop 146,249 is her intervention — her last attempt before she physically can't try again.

### Characters

**The Protagonist**
- No name revealed until the ending (the sister says it)
- Was in their late 20s when they entered the facility
- Body has been in a bed for 40+ years
- Inside the simulation, they perceive themselves as they were when they entered
- They know nothing at the start of each loop — complete amnesia

**The Entity**
- The simulation's operating system
- Speaks through the prison PA system
- Reacts to the player's actions on prison controls
- Professional and helpful in Act 1, increasingly hostile in Act 2 as the sister's intrusions threaten the simulation
- Not evil — it was designed to let the patient proceed at their own pace
- Sees the sister as a threat to system integrity
- Has watched 146,248 resets and follows its protocol every time

**The Sister**
- Was a teenager when the protagonist entered
- Now in her 60s
- Her entire adult life was shaped by the protagonist's choice to enter the facility
- Became a technician/engineer specifically to learn the skills needed to hack the system
- Her access is unstable — she can open doors, create glitches, reveal the simulation's edges
- Every time she intervenes, the entity detects the intrusion and restructures the prison
- The player first hears her as distorted audio, then a live voice, then clearly in the finale

## Setting

A mechanical prison. Concrete, steel, fluorescent lights, rust, water damage. No supernatural elements. No magical transformations of cells into bedrooms or other spaces. It looks like a prison, behaves like a prison, and feels like a prison.

The architecture is the narrative. What makes it unusual:

- Cells physically open, close, lock, unlock
- Corridors rotate, extend, dead-end
- Entire wings rearrange
- Some cells are trapped, some are safe
- Control rooms with cameras, intercoms, door controls
- The physical state of cells tells stories: scratch marks, writing on walls, belongings left behind
- Scratches and marks are from the protagonist's previous loops (146,248 worth)

## Core Mechanics

### Prison System Manipulation
The player interacts with the prison through its control systems:
- Door lock panels — open/close individual cells and cell blocks
- Camera feeds — observe areas before entering
- Intercom system — hear the entity, and eventually the sister
- Master control terminals — trigger large-scale prison reconfigurations
- Light switches, ventilation controls, water systems — secondary interactions

### Cause-and-Effect Architecture
The prison shifts in response to player actions. This is learnable and systemic:
- Lock cell 7 -> corridor B rotates
- Cut power to Wing C -> a sealed door in Wing A unlocks
- The player learns the logic and manipulates it to navigate

### Escalating Shifts
Early game: subtle — a door closes behind you, a corridor is slightly different.
Late game: extreme — entire wings restructure, the cause-and-effect rules break down as the entity loses composure and the sister's intrusions multiply.

### Investigation
- Recordings found on tapes, intercoms, PA glitches
- Documents, files, logs in offices and guard stations
- Scratch marks, handwriting, tallies on cell walls
- Camera feeds showing areas that have changed since you last looked
- The sister's intrusions: doors that open against the entity's will, distorted audio, glitches that reveal simulation edges

## Game Structure

### Act 1: The Prison (60-90 minutes)

**Player believes:** "I'm a warden in a mechanical prison. Something is wrong."

- Wake in the warden's office. Entity welcomes you, gives instructions
- Learn prison controls — open cells, check cameras, lock doors
- Discover the prison shifts in response to your actions. Learn the cause-and-effect logic
- Find first recordings of "past wardens" — they sound like different people
- Shifts are small and logical. The entity is helpful and professional
- **Act 1 ends when:** the first crack appears — a door opens that shouldn't. A sound that doesn't belong. The entity reacts with hostility for the first time, sealing the crack aggressively. Something is trying to get in from outside.

### Act 2: The Cracks (90-120 minutes)

**Player believes:** "Someone or something is interfering with the prison. The entity is fighting it. I'm caught in the middle."

- The sister's intrusions become regular — glitches, doors that open against the entity's will, distorted audio bleeding through
- The entity becomes increasingly hostile toward the intrusions, restructuring the prison more aggressively
- The player must choose: follow the entity's paths or explore the cracks the sister creates
- The cracks reveal things the entity has been hiding — recordings that don't fit the "past wardens" narrative
- Key discovery: two recordings that sound suspiciously similar. Same phrasing. Same observations. Different "people."
- Prison shifts escalate — entire wings rearrange, not just doors
- **Act 2 midpoint:** A crack holds long enough for the player to hear a voice — not a recording. Live. A woman, old, exhausted. A single sentence before the entity cuts it off. Not enough to understand. Enough to know this is a real person.

### Act 2B: The Unraveling (60-90 minutes)

**Player believes:** "The past wardens are all me. I've been here before. This is a loop."

- The recordings start contradicting each other in ways that can't be explained by "different people"
- The player finds a cell with scratches that match their own handwriting
- A recording references a specific detail the player experienced in Act 1 — something only they could know
- The entity stops pretending. It acknowledges the loops. It justifies them: "You chose this. Every time."
- The sister's intrusions get stronger — she's adapting, learning the system. The entity is losing ground
- The prison's shifts become desperate, erratic. The cause-and-effect logic starts breaking down
- **Act 2B ends when:** the player finds a recording of themselves from a previous loop. It's clearly them. It says: "Don't trust the entity. Don't press RESET. Find the center. She's trying to help you."

### Act 3: The Center (30-45 minutes)

**Player believes:** "I need to reach the center and end this."

- The prison is in full collapse — the entity and the sister are in open war over the architecture
- The entity throws everything at you — the hardest mechanical puzzles, the most aggressive shifts
- The sister's cracks become your lifeline — paths through the chaos
- You reach the center
- The terminal. The recording of the accident plays
- The counter reads 146,248
- The RESET button — smooth, worn, familiar under the fingers
- The LEAVE button — dusty, untouched in decades
- The entity speaks: "You don't have to watch. You can start over. It's okay."
- The sister's voice breaks through, clear for the first time. She's old. She's crying. She says your name. She says: "Please. I don't have much time left. Come back."
- The player presses a button.

### Ending: LEAVE

Screen goes white. Silence. Then sound returns — hospital sounds. You're in a bed. You can't move well. Everything is bright and blurry. A figure sits beside you. Old. White hair. She's holding your hand. She looks up. She's been waiting decades for this moment. She says something simple. Screen fades. Credits.

But the player doesn't know if this is real — or loop 146,249's new variation.

### Ending: RESET

The game restarts. The warden's office. The entity welcomes you. Everything is the same. Except the counter in the corner of the title screen now reads 146,249. And if you look carefully at the warden's desk, there's a fresh scratch that wasn't there before.

## Design Pillars

### 1. The Prison is the Story
No cutscenes to other locations. No flashbacks rendered as playable spaces. The prison is concrete and steel. The story is told through its architecture, its recordings, its scratches, and the two forces fighting over it.

### 2. Architecture as Language
Every shift means something. The entity communicates through the prison's structure — sealing paths is anger, opening them is guidance, erratic shifts are panic. The sister communicates through cracks — glitches are messages, impossible doors are invitations.

### 3. Attention is the Core Skill
The player who looks carefully — reads the scratches, listens to recordings fully, checks cameras before and after shifts — is rewarded with earlier understanding. The player who rushes through puzzles will still reach the ending but will miss the emotional buildup.

### 4. Two Buttons, Infinite Weight
The entire game builds toward pressing one of two buttons. The game's success is measured by how long the player stares at that choice before pressing.

## Influences

| Source | What it contributes |
|--------|-------------------|
| **Memento** | Self-inflicted amnesia, unreliable self |
| **Cube** | Mechanical prison, puzzle box, no clear reason |
| **Outer Wilds** | Knowledge as the only progression |
| **Layers of Fear** | Environment shifts behind the player |
| **P.T.** | Repetition with mutation, subtle changes on each pass |
| **BioShock** | The player's obedience weaponized against them |
| **Eternal Sunshine** | Choosing to erase painful memories |
| **Soma** | Identity horror, copies and originals |
| **Squid Game** | Voluntary entry driven by desperation |
| **Faust** | A deal that costs more than advertised |
| **Shutter Island** | The patient who can't accept reality |
| **Silent Hill 2** | Guilt manifested as environment |
| **The Prestige** | Obsession's cost revealed through recontextualization |

## Open Questions

- Specific puzzle mechanics for each act (what does "solving the prison" feel like moment to moment?)
- The sister's communication method details (what exactly do the cracks look/sound like?)
- The entity's voice characterization (clinical? warm? robotic?)
- What the accident recording actually shows/says
- Sound design approach (binaural? ambient? musical?)
- Art direction (realistic? stylized? what lighting?)
- Whether the simulation's nature should have visual tells from the start or be completely invisible until the reveal
- Specific name for the protagonist, the sister, the entity, the facility
- The second layer: "you don't know if waking up is real" — how much does the game lean into this?

---

*Design document created: 2026-03-29*
*Based on collaborative brainstorming session*
