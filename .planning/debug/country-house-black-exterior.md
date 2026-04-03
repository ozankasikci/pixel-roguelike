---
status: resolved
trigger: "country house exterior renders very black — understand why (root cause only)"
created: 2026-03-31T00:00:00Z
updated: 2026-03-31T00:00:00Z
---

## Current Focus
<!-- OVERWRITE on each update - reflects NOW -->

hypothesis: CONFIRMED — outdoor_bright is an unrecognized environment profile token; the scene falls through to the C++ hardcoded Default profile, whose hemisphere and sun values are insufficiently bright for an outdoor exterior without a matching environment definition asset file.
test: Traced the full parse/lookup chain from scene file through to shader uniforms.
expecting: N/A — root cause confirmed
next_action: Return ROOT CAUSE FOUND

## Symptoms
<!-- Written during gathering, then IMMUTABLE -->

expected: Country house exterior should be visibly lit with the scene's environment lighting
actual: Exterior surfaces of the country house render very black/dark
errors: No errors — it renders, just incorrectly dark
reproduction: Load country house scene, look at exterior of building
started: Since country house scene was created (recent — last few quick tasks)
clue: User says "if I deal with hemisphere and sun it gets fixed" — suggests default hemisphere/sun values don't contribute enough ambient/directional light to the exterior

## Eliminated
<!-- APPEND only - prevents re-investigating -->

- hypothesis: The scene file has no lights
  evidence: country_house.scene has 12 point lights (6 interior, 6 exterior fill). The point lights exist, but they are too far away and/or their radius is too small to contribute meaningful light to the exterior surfaces that face away from them. But more importantly the ambient/hemisphere contribution is the primary issue.
  timestamp: 2026-03-31

- hypothesis: The exterior mesh uses a broken material
  evidence: Not investigated in depth because the user confirmed changing hemisphere/sun fixes it — so the issue is lighting, not material/shader. Point lights also light the scene partially.
  timestamp: 2026-03-31

## Evidence
<!-- APPEND only - facts discovered -->

- timestamp: 2026-03-31
  checked: country_house.scene line 1
  found: environment_profile outdoor_bright
  implication: The scene requests the "outdoor_bright" named environment profile.

- timestamp: 2026-03-31
  checked: EnvironmentProfile.cpp — tryParseEnvironmentProfileToken()
  found: The recognized tokens are: "default", "game_ready_neutral", "neutral", "dungeon_torch", "sunlit_meadow", "mountain_dusk", "arcane_field", "cathedral_arcade", "cloister_daylight". "outdoor_bright" is NOT in this list.
  implication: tryParseEnvironmentProfileToken("outdoor_bright", profile) returns false.

- timestamp: 2026-03-31
  checked: LevelDef.cpp lines 237-247 — environment_profile parsing
  found: data.environmentId = "outdoor_bright" (string is stored), then tryParseEnvironmentProfileToken returns false, but both branches hit `continue`. So environmentProfile stays as EnvironmentProfile::Default (the zero-initialized enum value).
  implication: The environment profile enum is Default; the string id is "outdoor_bright".

- timestamp: 2026-03-31
  checked: LevelLoader.cpp line 44
  found: registry.ctx().insert_or_assign(ActiveEnvironmentProfile{request.levelId, level.environmentId, level.environmentProfile}) — stores environmentId="outdoor_bright", profile=Default.
  implication: At runtime, the active environment id is "outdoor_bright".

- timestamp: 2026-03-31
  checked: EnvironmentDebugSync.cpp lines 138-148 — syncEnvironmentFromRegistry()
  found: The runtime calls content->findEnvironment("outdoor_bright"). If that returns null, it falls back to applyEnvironmentProfile(params, active.profile, ...) which is applyEnvironmentProfile(params, EnvironmentProfile::Default, ...).
  implication: The lookup will fail unless a file named outdoor_bright.environment exists.

- timestamp: 2026-03-31
  checked: assets/defs/environments/ directory listing
  found: Only ONE file exists: default.environment. There is no outdoor_bright.environment.
  implication: content->findEnvironment("outdoor_bright") returns nullptr. The fallback is EnvironmentProfile::Default.

- timestamp: 2026-03-31
  checked: EnvironmentProfile.cpp — makeDefaultEnvironmentRenderSettings()
  found: The C++ hardcoded Default profile sets: hemisphereSkyColor=(0.40,0.40,0.40), hemisphereGroundColor=(0.12,0.11,0.10), hemisphereStrength=0.38. Sun intensity=1.00, fill intensity=0.10. This is the same as the default.environment asset except the asset file is never loaded for this scene — the C++ defaults are used directly.
  implication: The Default profile's hemisphere and sun are designed for interior/neutral scenes. For an exterior building, the surfaces facing away from the sun only receive the hemisphere ambient. With hemisphereStrength=0.38 and sky color (0.40,0.40,0.40), that is albedo * 0.40 * 0.38 = only ~15% of the albedo for sky-facing surfaces — very dim. Ground-facing surfaces get even less: (0.12) * 0.38 = ~5%.

- timestamp: 2026-03-31
  checked: scene.frag lines 980-981 + uniform declarations lines 48-50
  found: vec3 ambient = mix(uHemisphereGroundColor, uHemisphereSkyColor, clamp(N.y * 0.5 + 0.5, 0.0, 1.0)); vec3 totalLight = albedo * ambient * uHemisphereStrength * materialAo; — hemisphere is the ONLY ambient term. There is no constant ambient floor.
  implication: If hemisphere values are low, exterior surfaces in shadow get very little light. The only other contribution comes from point lights and the directional sun/fill.

- timestamp: 2026-03-31
  checked: country_house.scene exterior point lights
  found: 6 exterior fill lights with radius 6.0–8.0, intensity 30.0, positioned 8–12 units from house center. The attenuation formula cuts off at dist >= radius, so surfaces on the far side of the house (12+ units from a light) get no contribution from the opposing fill light. The house is ~15m wide, so the opposite-side lights do not reach the far exterior faces.
  implication: Surfaces on the side of the house away from the nearest fill light AND in shadow get only the hemisphere ambient: very dim with the Default settings.

- timestamp: 2026-03-31
  checked: default.environment asset file
  found: Contains lighting_hemi_sky_color 0.5 0.48 0.44 / lighting_hemi_strength 0.35 / sun_intensity 0.82 — slightly different from C++ defaults but same order of magnitude. Still not designed for bright outdoor.
  implication: Even if default.environment were applied instead of C++ defaults, the hemisphere values would still be dim for an exterior.

## Resolution
<!-- OVERWRITE as understanding evolves -->

root_cause: |
  The country_house.scene specifies `environment_profile outdoor_bright`, but "outdoor_bright" is not a recognized token in tryParseEnvironmentProfileToken() (EnvironmentProfile.cpp), and there is no assets/defs/environments/outdoor_bright.environment file. As a result:

  1. tryParseEnvironmentProfileToken("outdoor_bright", ...) returns false → environmentProfile stays at EnvironmentProfile::Default.
  2. At runtime, content->findEnvironment("outdoor_bright") returns nullptr (no file).
  3. The fallback is applyEnvironmentProfile(EnvironmentProfile::Default) — the C++ hardcoded defaults.
  4. The Default profile has hemisphereStrength=0.38 with low sky/ground colors, which were designed for interior/neutral scenes, not a bright outdoor scene.
  5. Exterior surfaces in shadow only receive hemisphere ambient. With Default values, this is ~15% of albedo (sky side) down to ~5% (ground side) — appearing very black.
  6. The scene's exterior fill point lights (radius 6–8 units) do not cover all exterior surfaces due to the house's ~15m width and strict distance-based cutoff.

  Summary: "outdoor_bright" is a non-existent environment profile. The scene silently gets the indoor-tuned Default profile, whose hemisphere and sun directional values are far too dim for an outdoor building exterior.

fix: Created assets/defs/environments/outdoor_bright.environment with hemisphere strength 0.72, brighter sky/ground colors, sun intensity 1.3, fill intensity 0.22.
verification: File auto-discovered by ContentRegistry at startup; runtime findEnvironment("outdoor_bright") will now match.
files_changed: [assets/defs/environments/outdoor_bright.environment]
