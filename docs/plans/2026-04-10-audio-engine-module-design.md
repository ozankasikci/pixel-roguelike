# Audio Engine Module Design

**Date:** 2026-04-10
**Status:** Approved
**Scope:** Clean-room redesign of `src/engine/audio/` as a self-contained, industry-standard audio module

## Context

The project has a working OpenAL Soft backend (~561 lines) with 16-source SFX pool, OGG Vorbis streaming, and 4 volume categories. However, the game layer doesn't use it — no AudioSourceSystem, no event subscribers, no sound triggers. The architecture needs to match industry patterns (FMOD, Wwise, Unreal, Godot) with a proper layered design.

## Research Summary

Every major engine/middleware uses a 3-layer architecture:

| Layer | FMOD | Wwise | Unreal | Godot |
|-------|------|-------|--------|-------|
| Backend | Core API (System, Channel, DSP) | AK::SoundEngine | FAudioDevice, FWaveInstance | AudioServer |
| Mix Engine | ChannelGroups, Bus hierarchy, Virtual voices | Bus hierarchy, Voice management, Spatial Audio | Submix graph, Concurrency, Virtualization | AudioBus + effects chains |
| Game Audio | Studio Events, Banks, Parameters | Events, Actor-Mixer, Game Syncs | Sound Cues, MetaSounds, Sound Classes | AudioStreamPlayer nodes |

Common subsystems across all: voice management with virtualization, hierarchical bus mixing, event-based abstraction, parameter controls (RTPCs), occlusion/obstruction, and adaptive music systems.

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Approach | Clean-room redesign | Existing code is small (~561 lines), not consumed by game code, replacing has near-zero integration cost |
| Mix engine scope | Voice manager + bus hierarchy + occlusion + reverb | All four essential for prison horror atmosphere — enclosed spaces need occlusion and reverb |
| Event model | Simple data-driven event mapping | Sufficient for doors, footsteps, ambience, interactions. Switch containers can layer on later |
| Threading | Command API on game thread, designed for audio thread later | Right abstraction boundary without premature threading complexity. Promotion is a mechanical swap |
| Asset organization | Convention-based directories + JSON event defs | Single contiguous environment doesn't need bank loading yet. Human-readable, easy to author |
| Audio formats | WAV for SFX (preloaded), OGG Vorbis for music/ambience (streamed) | Matches existing stb_vorbis support. Zero-latency SFX, small-on-disk music |

## Architecture

### Layer Diagram

```
+--------------------------------------------------+
|  Game Audio Layer                                 |
|  EventRegistry, SoundEventDef, AudioEmitter       |
|  (data-driven event mapping, variation, cooldown)  |
+--------------------------------------------------+
|  Mix Engine Layer                                 |
|  VoiceManager, BusGraph, OcclusionProcessor,      |
|  ReverbManager, CommandQueue                      |
|  (prioritization, mixing, spatial processing)     |
+--------------------------------------------------+
|  Backend Layer                                    |
|  ALDevice, ALSourcePool, ALBufferCache,           |
|  ALStreamPlayer, ALEffectSlots                    |
|  (OpenAL Soft wrapper, resource management)       |
+--------------------------------------------------+
```

**Dependency rule:** Each layer only calls down, never up. Game layer submits commands to the mix engine. Mix engine drives the backend. Backend knows nothing about events or buses.

**Public API:** Game and ECS systems interact exclusively through `AudioEngine` — a facade class that owns all three layers and exposes the command-based API.

**Integration:** `AudioEngine` is registered as a service on `Application`. A single `AudioEngineSystem` ECS system calls `AudioEngine::update()` each frame during the Gameplay phase.

### Backend Layer

Wraps OpenAL Soft into RAII resource types. No audio logic — just resource lifecycle and state management.

**ALDevice** — Owns the OpenAL device and context. Opens device on construction, destroys on teardown. Queries device capabilities (max sources, EFX support, HRTF availability). Single instance per AudioEngine.

**ALSourcePool** — Pre-allocates a fixed pool of ALsource handles (default 32). Sources are claimed/released by the voice manager. The pool has no opinion about priority or audibility. RAII cleanup deletes all sources on destruction.

**ALBufferCache** — Loads and caches ALbuffer handles keyed by file path. `getOrLoad(path)` returns a buffer handle, loading from WAV on first access. Reference-counted so unused buffers can be evicted. Preloaded sounds (SFX) stay resident.

**ALStreamPlayer** — OGG Vorbis streaming for music/ambience. Double-buffered: two AL buffers queued on a source, refilled in alternation from stb_vorbis. Exposes play/stop/pause/setGain/isFinished. One instance per active stream (typically 2-3 concurrent).

**ALEffectSlots** — Manages OpenAL EFX auxiliary effect slots for reverb. Creates ALeffectslot + ALeffect pairs. Exposes `setReverbPreset(slot, params)` mapping to AL_EFFECT_EAXREVERB properties (decay time, room size, damping).

All backend types are move-only, non-copyable, following project RAII conventions.

### Mix Engine Layer

#### Voice Manager

**Voice** — Internal struct tracking a playing or virtual sound: sound handle, position, velocity, volume, pitch, priority, bus assignment, audibility score, state (Playing, Virtual, Stopping). Virtual voices track elapsed time for correct resume.

**Audibility scoring** — Each frame: `baseVolume * distanceAttenuation * busVolume * priorityWeight`. Distance attenuation computed on CPU using inverse distance clamped model to make scoring decisions before committing to AL sources.

**Voice allocation:**
1. Free source in pool → assign immediately
2. Pool full → compare new sound's audibility against lowest-scoring active voice
3. New sound wins → steal source (short fade-out on victim to avoid clicks)
4. New sound loses → start virtual

**Concurrency limits** — Each SoundEventDef specifies maxInstances. Before spawning, check active voice count for that event. Resolution: stop farthest instance or reject new one.

**Virtual-to-real promotion** — Each frame, voices sorted by audibility. Top N (pool size) become real, rest go virtual. Transitioning voices resume from tracked elapsed time.

Default pool size: 32 real voices. Configurable via AudioEngine construction.

#### Bus Graph

**Bus** — Named mixing node with: volume (0.0-1.0), mute flag, solo flag, parent pointer, child list, optional effects slot index. Buses are logical grouping — the voice manager reads a voice's bus chain to compute final gain as the product of all ancestors.

**Default hierarchy:**
```
Master (1.0)
+-- Music (1.0)
+-- SFX (1.0)
|   +-- Footsteps (1.0)
|   +-- Doors (1.0)
|   +-- Environment (1.0)
+-- Ambience (1.0)
+-- UI (1.0)
```

Bus resolution happens at voice-scoring time, not per-sample. If any bus in the chain is muted, all voices in that subtree score zero and go virtual. Solo mutes sibling buses.

API: `setVolume(name, value)`, `mute(name)`, `solo(name)`. Game code accesses these through AudioEngine commands.

Extensibility: per-bus DSP effect chains can be added later. For now, the only per-bus effect is reverb auxiliary send.

#### Occlusion Processor

Runs at reduced rate (10-15 Hz). Casts rays from listener to each active 3D voice using Jolt Physics raycasts. Produces occlusion factor (0.0 = clear, 1.0 = fully blocked).

Occlusion drives:
- **Direct low-pass filter** — AL_LOWPASS_GAIN and AL_LOWPASS_GAINHF via OpenAL AL_FILTER_LOWPASS
- **Volume reduction** — Subtle (e.g., 0.7x at full occlusion)

Results cached per voice, refreshed at query rate. Virtual voices skip occlusion queries.

#### Reverb Manager

Manages 2-4 EFX auxiliary effect slots (hardware-dependent). Each slot holds a reverb preset.

Named presets: `Cell` (short decay, small room), `Corridor` (medium decay, narrow), `OpenArea` (long decay, large room).

Game layer tells the reverb manager which preset is active based on player location. Manager crossfades between presets (~500ms ramp on auxiliary send gains).

Each 3D voice gets an auxiliary send to the active reverb slot. Send level modulated by occlusion — occluded sounds still contribute to reverb but with reduced direct signal.

### Game Audio Layer

#### Event System

**SoundEventDef** — POD struct loaded from JSON:
```json
{
  "door_open": {
    "sounds": ["sfx/doors/door_open_01.wav", "sfx/doors/door_open_02.wav"],
    "pick": "random_no_repeat",
    "pitch_range": [0.95, 1.05],
    "volume_range": [0.9, 1.0],
    "max_instances": 2,
    "cooldown": 0.1,
    "bus": "Doors",
    "spatial": "3D",
    "ref_distance": 1.0,
    "max_distance": 30.0
  }
}
```

Pick modes: Random, RandomNoRepeat, RoundRobin, Sequential.

**EventRegistry** — Loads `assets/audio/sound_events.json` at startup. Builds hashmap of name to def. On load, triggers preloading of all referenced WAV files into ALBufferCache. OGG files (music/ambience) are not preloaded.

#### Command API

Game code submits commands through AudioEngine. Never touches OpenAL or internal state directly.

```cpp
audioEngine.play("door_open", position);        // fire-and-forget 3D
audioEngine.play("ui_click");                    // 2D, no position
auto handle = audioEngine.playLooping("ambient_hum", position);
audioEngine.stop(handle);
audioEngine.setListenerTransform(pos, fwd, up);
audioEngine.setBusVolume("Music", 0.5f);
audioEngine.setReverbPreset("Cell");
```

`play()` returns a VoiceHandle (opaque ID) for sounds needing later control. Fire-and-forget sounds can ignore the return.

**Command processing** — Currently synchronous: `AudioEngine::update()` processes all queued commands on the game thread. Queue is a `std::vector<AudioCommand>` swapped each frame. Promoting to a real audio thread later swaps this to a lock-free ring buffer and moves update() to the audio thread — no call-site changes.

#### ECS Integration

- `AudioEngineSystem` calls `audioEngine.update()` each frame
- `AudioEmitterSystem` iterates entities with AudioSourceComponent + TransformComponent, submits play/stop/position-update commands
- `AudioListenerSystem` (refactored from existing) submits listener transform
- `PlaySoundEvent` on EventBus gets a subscriber in AudioEmitterSystem forwarding to `audioEngine.play()`

## File Layout

```
src/engine/audio/
+-- AudioEngine.h / .cpp            // Facade: owns all layers, public API
+-- AudioCommand.h                   // Command types (Play, Stop, SetVolume, etc.)
+-- VoiceHandle.h                    // Opaque handle type
+-- backend/
|   +-- ALDevice.h / .cpp           // Device + context RAII
|   +-- ALSourcePool.h / .cpp       // Fixed source pool
|   +-- ALBufferCache.h / .cpp      // Load-once buffer cache
|   +-- ALStreamPlayer.h / .cpp     // OGG Vorbis double-buffer streaming
|   +-- ALEffectSlots.h / .cpp      // EFX reverb slots + filters
+-- mix/
|   +-- Voice.h                     // Voice state struct
|   +-- VoiceManager.h / .cpp       // Allocation, scoring, virtualization
|   +-- BusGraph.h / .cpp           // Hierarchical bus mixing
|   +-- OcclusionProcessor.h / .cpp // Raycast-based occlusion queries
|   +-- ReverbManager.h / .cpp      // Preset management, crossfade
+-- events/
    +-- SoundEventDef.h             // POD struct for event definitions
    +-- EventRegistry.h / .cpp      // JSON loading, name -> def lookup
```

**CMake target:** `engine_audio` links OpenAL::OpenAL privately (no leakage), exposes engine_core, engine_physics (for occlusion raycasts), and GLM publicly.

## Asset Layout

```
assets/audio/
+-- sound_events.json
+-- sfx/
|   +-- doors/
|   +-- footsteps/
|   +-- interactions/
|   +-- environment/
+-- music/
+-- ambience/
+-- ui/
```

Formats: WAV for SFX (preloaded), OGG Vorbis for music/ambience (streamed).

## What Gets Replaced

- `AudioSystem.h/.cpp` — replaced entirely by the new module
- `AudioListenerSystem` — refactored into new AudioEmitterSystem (handles both listener and emitter updates)
- stb_vorbis — stays, moves into backend layer

## Future Extensions (Not in This Build)

- **RTPC parameter controls** — continuous game values driving audio properties via curves
- **Switch containers** — surface-type footstep selection, weather-driven ambience
- **Music state machine** — horizontal re-sequencing with beat-aware transitions, vertical layering
- **Per-bus DSP chains** — low-pass, compressor, limiter per bus
- **Mixer snapshots** — save/restore full mixer state for game mode transitions
- **Bank-based loading** — group sounds by level/area for memory management
- **Dedicated audio thread** — promote command queue to lock-free ring buffer
