---
phase: quick
plan: 260330-mzu
type: execute
wave: 1
depends_on: []
files_modified:
  - assets/defs/environments/default.environment
autonomous: true
must_haves:
  truths:
    - "Editor scene objects are visible with proper lighting after loading default environment"
    - "Tone mapping, bloom, fog, and grain post-processing are active in editor"
  artifacts:
    - path: "assets/defs/environments/default.environment"
      provides: "Default environment with post-processing enabled"
      contains: "enable_tone_map true"
  key_links:
    - from: "assets/defs/environments/default.environment"
      to: "CompositePass"
      via: "EnvironmentDefinition loading"
      pattern: "enable_tone_map true"
---

<objective>
Restore accidentally disabled post-processing flags in default.environment that were turned off in commit 772df83 (SSAO pipeline wiring).

Purpose: The editor has no player torch lights and relies on hemisphere ambient + directional sun + fill light, producing very dim linear colors (~0.03-0.07). Without tone mapping, bloom, and fog, these pass through the composite pipeline nearly unchanged, making everything appear as uniform gray.

Output: default.environment with four post-processing flags restored to true.
</objective>

<execution_context>
@$HOME/.claude/get-shit-done/workflows/execute-plan.md
@$HOME/.claude/get-shit-done/templates/summary.md
</execution_context>

<context>
@assets/defs/environments/default.environment
</context>

<tasks>

<task type="auto">
  <name>Task 1: Restore post-processing flags in default.environment</name>
  <files>assets/defs/environments/default.environment</files>
  <action>
In assets/defs/environments/default.environment, change exactly these four lines:

- `enable_fog false` to `enable_fog true`
- `enable_tone_map false` to `enable_tone_map true`
- `enable_bloom false` to `enable_bloom true`
- `enable_grain false` to `enable_grain true`

Do NOT change any other values. The other tuning parameters (exposure, contrast, saturation, fog density/start, vignette strength, bloom_threshold, bloom_intensity, etc.) were intentional adjustments and must remain as-is.
  </action>
  <verify>
    <automated>grep -E "enable_(fog|tone_map|bloom|grain)" /Users/ozan/Projects/gsd-3d-roguelike/assets/defs/environments/default.environment | grep -c "true" | grep -q "4" && echo "PASS: all 4 flags are true" || echo "FAIL: not all flags restored"</automated>
  </verify>
  <done>enable_fog, enable_tone_map, enable_bloom, and enable_grain are all set to true in default.environment. No other values changed.</done>
</task>

<task type="auto">
  <name>Task 2: Build and verify editor compiles</name>
  <files>assets/defs/environments/default.environment</files>
  <action>
Build the project to confirm the environment file change does not cause any issues. The environment file is a data file loaded at runtime, so this is primarily a sanity check that the build still succeeds.

Run: cmake --build build --target level-editor -j$(sysctl -n hw.ncpu)
  </action>
  <verify>
    <automated>cd /Users/ozan/Projects/gsd-3d-roguelike && cmake --build build --target level-editor -j$(sysctl -n hw.ncpu) 2>&1 | tail -5</automated>
  </verify>
  <done>level-editor builds successfully with no errors.</done>
</task>

</tasks>

<verification>
- All four post-processing boolean flags read `true` in default.environment
- No other lines in default.environment were modified
- Project builds cleanly
</verification>

<success_criteria>
- enable_fog, enable_tone_map, enable_bloom, enable_grain all set to true
- Editor builds without errors
- Environment file otherwise unchanged from current state
</success_criteria>

<output>
After completion, create `.planning/quick/260330-mzu-fix-editor-scene-objects-not-visible-aft/260330-mzu-SUMMARY.md`
</output>
