# Per-Material Weathering System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add opt-in per-material weathering to the fragment shader so materials can automatically show cavity grime, edge wear, dust, damp staining, and macro variation based on geometry context.

**Architecture:** Weathering is a second pass after existing `applyMaterialDetail()`. New uniforms flow through the existing `MaterialDefinition` → `ResolvedMaterialDefinition` → `RenderMaterialData` → shader pipeline. Each MaterialKind gets its own weathering function. Materials opt in via `weathering_enabled true` in their `.material` file.

**Tech Stack:** C++20, OpenGL 4.1 / GLSL 410 core, existing material definition parser

---

### Task 1: Add weathering fields to MaterialDefinition

**Files:**
- Modify: `src/game/rendering/MaterialDefinition.h:24-52` (MaterialDefinition struct)
- Modify: `src/game/rendering/MaterialDefinition.h:54-82` (ResolvedMaterialDefinition struct)

- [ ] **Step 1: Write the failing test**

Add a new test section to `tests/game/test_material_definitions.cpp` that loads a material with weathering fields:

```cpp
// Test: weathering field parsing
{
    const auto path = test_support::tempPath("weathering_test.material");
    {
        std::ofstream f(path);
        f << "id weathering_test\n";
        f << "detail_stone true\n";
        f << "weathering_enabled true\n";
        f << "weathering_dirt_strength 0.7\n";
        f << "weathering_dirt_color 0.18 0.14 0.10\n";
        f << "weathering_edge_wear_strength 0.5\n";
        f << "weathering_dust_strength 0.3\n";
        f << "weathering_damp_strength 0.6\n";
        f << "weathering_noise_scale 0.4\n";
    }
    const auto def = loadMaterialDefinitionAsset(path.string());
    std::filesystem::remove(path);
    assert(def.weatheringEnabled.has_value() && *def.weatheringEnabled == true);
    assert(def.weatheringDirtStrength.has_value() && test_support::nearlyEqual(*def.weatheringDirtStrength, 0.7f));
    assert(def.weatheringDirtColor.has_value());
    assert(test_support::nearlyEqual(def.weatheringDirtColor->x, 0.18f));
    assert(test_support::nearlyEqual(def.weatheringDirtColor->y, 0.14f));
    assert(test_support::nearlyEqual(def.weatheringDirtColor->z, 0.10f));
    assert(def.weatheringEdgeWearStrength.has_value() && test_support::nearlyEqual(*def.weatheringEdgeWearStrength, 0.5f));
    assert(def.weatheringDustStrength.has_value() && test_support::nearlyEqual(*def.weatheringDustStrength, 0.3f));
    assert(def.weatheringDampStrength.has_value() && test_support::nearlyEqual(*def.weatheringDampStrength, 0.6f));
    assert(def.weatheringNoiseScale.has_value() && test_support::nearlyEqual(*def.weatheringNoiseScale, 0.4f));
}

// Test: weathering defaults when not specified
{
    const auto path = test_support::tempPath("no_weathering_test.material");
    {
        std::ofstream f(path);
        f << "id no_weathering_test\n";
        f << "detail_stone true\n";
    }
    const auto def = loadMaterialDefinitionAsset(path.string());
    std::filesystem::remove(path);
    assert(!def.weatheringEnabled.has_value());
    assert(!def.weatheringDirtStrength.has_value());
}

// Test: weathering inheritance — parent weathering propagates to child
{
    std::unordered_map<std::string, MaterialDefinition> defs;
    MaterialDefinition parent;
    parent.id = "parent_weathered";
    parent.weatheringEnabled = true;
    parent.weatheringDirtStrength = 0.5f;
    parent.weatheringDirtColor = glm::vec3(0.2f, 0.16f, 0.12f);
    defs.emplace(parent.id, parent);

    MaterialDefinition child;
    child.id = "child_weathered";
    child.parent = "parent_weathered";
    child.weatheringDirtStrength = 0.8f; // override one field
    defs.emplace(child.id, child);

    const auto resolved = resolveMaterialDefinition("child_weathered", defs);
    assert(resolved.weatheringEnabled == true);
    assert(test_support::nearlyEqual(resolved.weatheringDirtStrength, 0.8f));
    assert(test_support::nearlyEqual(resolved.weatheringDirtColor.x, 0.2f)); // inherited
}

// Test: weathering roundtrip (serialize then reload)
{
    MaterialDefinition roundtrip;
    roundtrip.id = "weathering_roundtrip";
    roundtrip.weatheringEnabled = true;
    roundtrip.weatheringDirtStrength = 0.65f;
    roundtrip.weatheringDirtColor = glm::vec3(0.18f, 0.14f, 0.10f);
    roundtrip.weatheringEdgeWearStrength = 0.4f;
    roundtrip.weatheringDustStrength = 0.25f;
    roundtrip.weatheringDampStrength = 0.55f;
    roundtrip.weatheringNoiseScale = 0.35f;

    const auto path = test_support::tempPath("weathering_roundtrip.material");
    saveMaterialDefinitionAsset(path.string(), roundtrip);
    const auto loaded = loadMaterialDefinitionAsset(path.string());
    std::filesystem::remove(path);

    assert(loaded.weatheringEnabled.has_value() && *loaded.weatheringEnabled == true);
    assert(test_support::nearlyEqual(*loaded.weatheringDirtStrength, 0.65f));
    assert(test_support::nearlyEqual(loaded.weatheringDirtColor->x, 0.18f));
    assert(test_support::nearlyEqual(loaded.weatheringDirtColor->y, 0.14f));
    assert(test_support::nearlyEqual(loaded.weatheringDirtColor->z, 0.10f));
    assert(test_support::nearlyEqual(*loaded.weatheringEdgeWearStrength, 0.4f));
    assert(test_support::nearlyEqual(*loaded.weatheringDustStrength, 0.25f));
    assert(test_support::nearlyEqual(*loaded.weatheringDampStrength, 0.55f));
    assert(test_support::nearlyEqual(*loaded.weatheringNoiseScale, 0.35f));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd build && cmake --build . --target test_material_definitions 2>&1 | tail -20`
Expected: Compilation failure — `weatheringEnabled` does not exist on `MaterialDefinition`

- [ ] **Step 3: Add weathering fields to MaterialDefinition struct**

In `src/game/rendering/MaterialDefinition.h`, add after line 51 (`std::optional<float> alphaCutoff;`):

```cpp
    std::optional<bool> weatheringEnabled;
    std::optional<float> weatheringDirtStrength;
    std::optional<glm::vec3> weatheringDirtColor;
    std::optional<float> weatheringEdgeWearStrength;
    std::optional<float> weatheringDustStrength;
    std::optional<float> weatheringDampStrength;
    std::optional<float> weatheringNoiseScale;
```

- [ ] **Step 4: Add weathering fields to ResolvedMaterialDefinition struct**

In `src/game/rendering/MaterialDefinition.h`, add after `bool detailFloor = false;` (line 81):

```cpp
    bool weatheringEnabled = false;
    float weatheringDirtStrength = 0.0f;
    glm::vec3 weatheringDirtColor{0.2f, 0.16f, 0.12f};
    float weatheringEdgeWearStrength = 0.0f;
    float weatheringDustStrength = 0.0f;
    float weatheringDampStrength = 0.0f;
    float weatheringNoiseScale = 0.3f;
```

- [ ] **Step 5: Add parsing in loadMaterialDefinitionAsset**

In `src/game/rendering/MaterialDefinition.cpp`, add before the final `throwParseError` line (before line 349):

```cpp
        if (key == "weathering_enabled" && tokens.size() == 2) {
            definition.weatheringEnabled = (tokens[1] == "true");
            continue;
        }
        if (key == "weathering_dirt_strength") {
            definition.weatheringDirtStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "weathering_dirt_color") {
            definition.weatheringDirtColor = parseVec3Record(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "weathering_edge_wear_strength") {
            definition.weatheringEdgeWearStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "weathering_dust_strength") {
            definition.weatheringDustStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "weathering_damp_strength") {
            definition.weatheringDampStrength = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
        if (key == "weathering_noise_scale") {
            definition.weatheringNoiseScale = parseFloatRecord(tokens, path, lineNumber, key);
            continue;
        }
```

- [ ] **Step 6: Add inheritance resolution for weathering fields**

In `src/game/rendering/MaterialDefinition.cpp`, in `resolveMaterialDefinitionRecursive`, add after the `alphaCutoff` block (after line 157):

```cpp
    if (definition.weatheringEnabled.has_value()) {
        resolved.weatheringEnabled = *definition.weatheringEnabled;
    }
    if (definition.weatheringDirtStrength.has_value()) {
        resolved.weatheringDirtStrength = *definition.weatheringDirtStrength;
    }
    if (definition.weatheringDirtColor.has_value()) {
        resolved.weatheringDirtColor = *definition.weatheringDirtColor;
    }
    if (definition.weatheringEdgeWearStrength.has_value()) {
        resolved.weatheringEdgeWearStrength = *definition.weatheringEdgeWearStrength;
    }
    if (definition.weatheringDustStrength.has_value()) {
        resolved.weatheringDustStrength = *definition.weatheringDustStrength;
    }
    if (definition.weatheringDampStrength.has_value()) {
        resolved.weatheringDampStrength = *definition.weatheringDampStrength;
    }
    if (definition.weatheringNoiseScale.has_value()) {
        resolved.weatheringNoiseScale = *definition.weatheringNoiseScale;
    }
```

- [ ] **Step 7: Add serialization for weathering fields**

In `src/game/rendering/MaterialDefinition.cpp`, in `serializeMaterialDefinitionAsset`, add after the `alphaCutoff` block (after line 481):

```cpp
    if (definition.weatheringEnabled.has_value()) {
        out << "weathering_enabled " << (*definition.weatheringEnabled ? "true" : "false") << '\n';
    }
    writeOptionalFloat(out, "weathering_dirt_strength", definition.weatheringDirtStrength);
    if (definition.weatheringDirtColor.has_value()) {
        out << "weathering_dirt_color "
            << definition.weatheringDirtColor->x << ' '
            << definition.weatheringDirtColor->y << ' '
            << definition.weatheringDirtColor->z << '\n';
    }
    writeOptionalFloat(out, "weathering_edge_wear_strength", definition.weatheringEdgeWearStrength);
    writeOptionalFloat(out, "weathering_dust_strength", definition.weatheringDustStrength);
    writeOptionalFloat(out, "weathering_damp_strength", definition.weatheringDampStrength);
    writeOptionalFloat(out, "weathering_noise_scale", definition.weatheringNoiseScale);
```

- [ ] **Step 8: Build and run tests**

Run: `cd build && cmake --build . --target test_material_definitions && ctest -R test_material_definitions -V`
Expected: All tests pass including the new weathering tests.

- [ ] **Step 9: Commit**

```bash
git add src/game/rendering/MaterialDefinition.h src/game/rendering/MaterialDefinition.cpp tests/game/test_material_definitions.cpp
git commit -m "Add weathering fields to MaterialDefinition with parsing, inheritance, and serialization"
```

---

### Task 2: Thread weathering data through the render pipeline

**Files:**
- Modify: `src/engine/rendering/geometry/Renderer.h:13-41` (RenderMaterialData struct)
- Modify: `src/engine/rendering/geometry/Renderer.cpp:94-126` (uniform binding)
- Modify: `src/game/rendering/MaterialTextureLibrary.cpp:208-237` (resolve function)

- [ ] **Step 1: Add weathering fields to RenderMaterialData**

In `src/engine/rendering/geometry/Renderer.h`, add after `float emissiveStrength = 0.0f;` (line 40):

```cpp
    bool weatheringEnabled = false;
    float weatheringDirtStrength = 0.0f;
    glm::vec3 weatheringDirtColor{0.2f, 0.16f, 0.12f};
    float weatheringEdgeWearStrength = 0.0f;
    float weatheringDustStrength = 0.0f;
    float weatheringDampStrength = 0.0f;
    float weatheringNoiseScale = 0.3f;
```

- [ ] **Step 2: Propagate weathering fields in MaterialTextureLibrary::resolve**

In `src/game/rendering/MaterialTextureLibrary.cpp`, add after line 237 (`renderMaterial.emissiveStrength = resolved.emissiveStrength;`):

```cpp
    renderMaterial.weatheringEnabled = resolved.weatheringEnabled;
    renderMaterial.weatheringDirtStrength = resolved.weatheringDirtStrength;
    renderMaterial.weatheringDirtColor = resolved.weatheringDirtColor;
    renderMaterial.weatheringEdgeWearStrength = resolved.weatheringEdgeWearStrength;
    renderMaterial.weatheringDustStrength = resolved.weatheringDustStrength;
    renderMaterial.weatheringDampStrength = resolved.weatheringDampStrength;
    renderMaterial.weatheringNoiseScale = resolved.weatheringNoiseScale;
```

- [ ] **Step 3: Set weathering uniforms in Renderer::drawScene**

In `src/engine/rendering/geometry/Renderer.cpp`, add after the `uAlphaCutoff` line (after line 125, inside the `if (!sameMaterial)` block):

```cpp
            shader_->setInt("uWeatheringEnabled", material.weatheringEnabled ? 1 : 0);
            shader_->setFloat("uWeatheringDirtStrength", material.weatheringDirtStrength);
            shader_->setVec3("uWeatheringDirtColor", material.weatheringDirtColor);
            shader_->setFloat("uWeatheringEdgeWearStrength", material.weatheringEdgeWearStrength);
            shader_->setFloat("uWeatheringDustStrength", material.weatheringDustStrength);
            shader_->setFloat("uWeatheringDampStrength", material.weatheringDampStrength);
            shader_->setFloat("uWeatheringNoiseScale", material.weatheringNoiseScale);
```

- [ ] **Step 4: Build to verify compilation**

Run: `cd build && cmake --build . 2>&1 | tail -20`
Expected: Clean build with no errors. The new uniforms will be set but the shader doesn't use them yet — OpenGL silently ignores uniforms that aren't referenced in the shader.

- [ ] **Step 5: Commit**

```bash
git add src/engine/rendering/geometry/Renderer.h src/engine/rendering/geometry/Renderer.cpp src/game/rendering/MaterialTextureLibrary.cpp
git commit -m "Thread weathering material data through render pipeline to shader uniforms"
```

---

### Task 3: Add weathering uniforms and utility functions to fragment shader

**Files:**
- Modify: `assets/shaders/game/scene.frag:69-94` (uniform declarations)
- Modify: `assets/shaders/game/scene.frag` (new utility functions section, before `applyMaterialDetail`)

- [ ] **Step 1: Add weathering uniform declarations**

In `assets/shaders/game/scene.frag`, add after line 94 (`uniform float uEmissiveStrength;`):

```glsl
uniform int uWeatheringEnabled;
uniform float uWeatheringDirtStrength;
uniform vec3 uWeatheringDirtColor;
uniform float uWeatheringEdgeWearStrength;
uniform float uWeatheringDustStrength;
uniform float uWeatheringDampStrength;
uniform float uWeatheringNoiseScale;
```

- [ ] **Step 2: Add weathering utility functions**

In `assets/shaders/game/scene.frag`, add a new section after the `applyMacroVariation` function (after line 359, before `detailStone`). This block provides the five geometry-context signals that per-MaterialKind weathering functions will use:

```glsl
// ============================================================
// Weathering utilities — geometry-context signals
// ============================================================

// Screen-space curvature approximation via fwidth() on world normals.
// Returns high values at hard mesh edges (convex features).
float weatherCurvature(vec3 worldNormal) {
    return clamp(length(fwidth(worldNormal)) * 20.0, 0.0, 1.0);
}

// Cavity mask: high in crevices/concavities, low on flat surfaces and convex edges.
// Uses curvature + downward-facing bias (undersides of ledges accumulate grime).
float weatherCavityMask(vec3 worldNormal) {
    float curvature = weatherCurvature(worldNormal);
    float underside = clamp(-worldNormal.y * 0.3, 0.0, 0.3);
    return clamp(curvature + underside, 0.0, 1.0);
}

// Edge wear mask: high on convex edges (sharp geometry transitions).
// Same curvature signal, but we want exposed/worn edges, not cavities.
float weatherEdgeWearMask(vec3 worldNormal) {
    return weatherCurvature(worldNormal);
}

// Up-facing mask: 1.0 for surfaces pointing straight up, 0.0 for vertical/downward.
// Drives dust, moss, rain accumulation.
float weatherUpFacingMask(vec3 worldNormal) {
    return clamp(worldNormal.y, 0.0, 1.0);
}

// Height gradient: 0.0 at ground level (y=0), 1.0 at y=4.0 and above.
// Lower surfaces get damp/mud, higher surfaces get dust/bleaching.
float weatherHeightGradient(float worldY) {
    return clamp(worldY * 0.25, 0.0, 1.0);
}

// Multi-scale world-space noise for macro variation.
// Large blotchy + medium detail, parameterized by scale uniform.
float weatherMacroNoise(vec3 worldPos, float scale) {
    float large = fbm(worldPos * scale * 0.3 + vec3(42.1, 7.3, 19.8));
    float medium = fbm(worldPos * scale * 1.2 + vec3(13.7, 28.4, 5.2));
    return large * 0.65 + medium * 0.35;
}
```

- [ ] **Step 3: Build to verify shader compiles**

Run: `cd build && cmake --build . && ./pixel-roguelike --headless-frames 1 2>&1 | tail -5` (or whatever quick launch validates shader compilation)

If the engine doesn't have a headless mode, just build: `cd build && cmake --build . 2>&1 | tail -10`

Shader compilation errors will show at runtime — launch the level editor briefly to verify: `cd build && timeout 3 ./level-editor 2>&1 | tail -10`

- [ ] **Step 4: Commit**

```bash
git add assets/shaders/game/scene.frag
git commit -m "Add weathering uniform declarations and utility functions to scene fragment shader"
```

---

### Task 4: Implement per-MaterialKind weathering functions and dispatcher

**Files:**
- Modify: `assets/shaders/game/scene.frag` (new weathering functions after utilities, new dispatcher, call site in main)

- [ ] **Step 1: Add per-MaterialKind weathering functions**

In `assets/shaders/game/scene.frag`, add after the weathering utility functions (the block added in Task 3), before `detailStone`:

```glsl
// ============================================================
// Per-MaterialKind weathering functions
// ============================================================

void weatherStone(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float upFacing = weatherUpFacingMask(N);
    float height = weatherHeightGradient(worldPos.y);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);

    // Cavity grime — dark dirt accumulates in crevices
    float dirtMask = cavity * (0.6 + macro * 0.4);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.0, dirtMask * uWeatheringDirtStrength * 0.5);
    roughness = clamp(roughness + dirtMask * uWeatheringDirtStrength * 0.15, 0.08, 0.98);

    // Edge wear — lighter, smoother stone on convex edges
    float wearMask = edge * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * vec3(1.12, 1.10, 1.08), wearMask * uWeatheringEdgeWearStrength * 0.4);
    roughness = clamp(roughness - wearMask * uWeatheringEdgeWearStrength * 0.1, 0.08, 0.98);

    // Dust on up-facing surfaces
    float dustMask = upFacing * (0.4 + macro * 0.6);
    albedo = mix(albedo, albedo * vec3(1.04, 1.03, 1.00), dustMask * uWeatheringDustStrength * 0.3);
    roughness = clamp(roughness + dustMask * uWeatheringDustStrength * 0.08, 0.08, 0.98);

    // Damp staining on lower surfaces
    float dampMask = (1.0 - height) * smoothstep(0.4, 0.8, macro);
    albedo = mix(albedo, albedo * vec3(0.88, 0.90, 0.92), dampMask * uWeatheringDampStrength * 0.35);

    // Macro color variation to break uniformity
    albedo *= 0.95 + macro * 0.10;
}

void weatherBrick(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float upFacing = weatherUpFacingMask(N);
    float height = weatherHeightGradient(worldPos.y);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);

    // Mortar darkening in joints, soot accumulation
    float dirtMask = cavity * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.0, dirtMask * uWeatheringDirtStrength * 0.45);
    roughness = clamp(roughness + dirtMask * uWeatheringDirtStrength * 0.12, 0.08, 0.98);

    // Exposed lighter clay on brick corners
    float wearMask = edge * (0.4 + macro * 0.6);
    albedo = mix(albedo, albedo * vec3(1.14, 1.08, 1.00), wearMask * uWeatheringEdgeWearStrength * 0.35);
    roughness = clamp(roughness - wearMask * uWeatheringEdgeWearStrength * 0.06, 0.08, 0.98);

    // Dust on top of protruding bricks
    float dustMask = upFacing * (0.3 + macro * 0.7);
    albedo = mix(albedo, albedo * vec3(1.05, 1.04, 1.02), dustMask * uWeatheringDustStrength * 0.25);

    // Efflorescence near ground, soot higher up
    float efflMask = (1.0 - height) * smoothstep(0.45, 0.80, macro);
    albedo = mix(albedo, albedo * vec3(1.08, 1.06, 1.02), efflMask * uWeatheringDampStrength * 0.2);
    float sootMask = height * smoothstep(0.5, 0.85, macro);
    albedo = mix(albedo, albedo * vec3(0.85, 0.82, 0.80), sootMask * uWeatheringDampStrength * 0.2);

    // Macro variation
    albedo *= 0.96 + macro * 0.08;
}

void weatherWood(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float upFacing = weatherUpFacingMask(N);
    float height = weatherHeightGradient(worldPos.y);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);

    // Dark grain filling, dirt in plank cracks
    float dirtMask = cavity * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.0, dirtMask * uWeatheringDirtStrength * 0.4);
    roughness = clamp(roughness + dirtMask * uWeatheringDirtStrength * 0.1, 0.08, 0.98);

    // Bleached wood on exposed corners
    float wearMask = edge * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * vec3(1.16, 1.14, 1.10), wearMask * uWeatheringEdgeWearStrength * 0.3);
    roughness = clamp(roughness - wearMask * uWeatheringEdgeWearStrength * 0.08, 0.08, 0.98);

    // Water stains on horizontal surfaces
    float stainMask = upFacing * smoothstep(0.5, 0.8, macro);
    albedo = mix(albedo, albedo * vec3(0.92, 0.90, 0.86), stainMask * uWeatheringDustStrength * 0.3);

    // Damp/rot near floor
    float dampMask = (1.0 - height) * smoothstep(0.4, 0.75, macro);
    albedo = mix(albedo, albedo * vec3(0.86, 0.84, 0.80), dampMask * uWeatheringDampStrength * 0.3);
    roughness = clamp(roughness + dampMask * uWeatheringDampStrength * 0.12, 0.08, 0.98);

    // Plank-to-plank variation
    albedo *= 0.94 + macro * 0.12;
}

void weatherFloor(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);
    // Floor-specific: spatial zone noise at a different scale for traffic patterns
    float traffic = weatherMacroNoise(worldPos + vec3(77.3, 0.0, 33.1), uWeatheringNoiseScale * 0.6);

    // Grime in slab seams and chips
    float dirtMask = cavity * (0.6 + macro * 0.4);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.0, dirtMask * uWeatheringDirtStrength * 0.5);
    roughness = clamp(roughness + dirtMask * uWeatheringDirtStrength * 0.15, 0.08, 0.98);

    // Lighter wear on slab edges
    float wearMask = edge * (0.5 + macro * 0.5);
    albedo = mix(albedo, albedo * vec3(1.08, 1.06, 1.04), wearMask * uWeatheringEdgeWearStrength * 0.3);
    roughness = clamp(roughness - wearMask * uWeatheringEdgeWearStrength * 0.08, 0.08, 0.98);

    // Traffic patterns: worn areas lighter/smoother, neglected areas dirtier/rougher
    float wornZone = smoothstep(0.45, 0.75, traffic);
    albedo = mix(albedo, albedo * vec3(1.06, 1.05, 1.03), wornZone * uWeatheringDustStrength * 0.25);
    roughness = clamp(roughness - wornZone * uWeatheringDustStrength * 0.1, 0.08, 0.98);
    float grimyZone = smoothstep(0.55, 0.85, 1.0 - traffic);
    albedo = mix(albedo, albedo * uWeatheringDirtColor * 2.2, grimyZone * uWeatheringDampStrength * 0.2);
    roughness = clamp(roughness + grimyZone * uWeatheringDampStrength * 0.1, 0.08, 0.98);

    // Macro variation
    albedo *= 0.96 + macro * 0.08;
}

void weatherMetal(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    float cavity = weatherCavityMask(N);
    float edge = weatherEdgeWearMask(N);
    float upFacing = weatherUpFacingMask(N);
    float height = weatherHeightGradient(worldPos.y);
    float macro = weatherMacroNoise(worldPos, uWeatheringNoiseScale);

    // Rust in crevices and joints
    vec3 rustColor = vec3(0.45, 0.22, 0.08);
    float rustMask = cavity * (0.5 + macro * 0.5);
    albedo = mix(albedo, rustColor, rustMask * uWeatheringDirtStrength * 0.4);
    roughness = clamp(roughness + rustMask * uWeatheringDirtStrength * 0.25, 0.08, 0.98);

    // Polished bare metal on contact edges
    float polishMask = edge * (0.4 + macro * 0.6);
    albedo = mix(albedo, albedo * vec3(1.18, 1.16, 1.14), polishMask * uWeatheringEdgeWearStrength * 0.4);
    roughness = clamp(roughness - polishMask * uWeatheringEdgeWearStrength * 0.2, 0.08, 0.98);

    // Water spots on horizontal surfaces
    float spotMask = upFacing * smoothstep(0.55, 0.80, macro);
    albedo = mix(albedo, albedo * vec3(0.92, 0.90, 0.88), spotMask * uWeatheringDustStrength * 0.25);
    roughness = clamp(roughness + spotMask * uWeatheringDustStrength * 0.08, 0.08, 0.98);

    // Drip stains running downward
    float dripMask = (1.0 - height) * smoothstep(0.5, 0.8, macro);
    albedo = mix(albedo, mix(albedo, rustColor, 0.3), dripMask * uWeatheringDampStrength * 0.25);

    // Patchy oxidation
    albedo *= 0.94 + macro * 0.12;
}

// Dispatcher — routes to the correct weathering function based on material kind
void applyMaterialWeathering(inout vec3 albedo, inout float roughness, vec3 N, vec3 worldPos) {
    if (uWeatheringEnabled == 0) return;

    if (uMaterialStoneDetail != 0)      weatherStone(albedo, roughness, N, worldPos);
    else if (uMaterialBrickDetail != 0)  weatherBrick(albedo, roughness, N, worldPos);
    else if (uMaterialWoodDetail != 0)   weatherWood(albedo, roughness, N, worldPos);
    else if (uMaterialFloorDetail != 0)  weatherFloor(albedo, roughness, N, worldPos);
    else if (uMaterialMetalness > 0.5)   weatherMetal(albedo, roughness, N, worldPos);
    // Materials without a known kind: no weathering applied
}
```

- [ ] **Step 2: Add the weathering call site in the main function**

In `assets/shaders/game/scene.frag`, find the line where `albedo` is computed (around line 1062-1064):

```glsl
    vec3 albedo = (uUseProceduralDetail != 0 || uUseMaterialMaps == 0)
        ? applyMaterialDetail(materialBaseColor, geometricNormal)
        : applyMacroVariation(materialBaseColor, geometricNormal);
```

Add immediately after this block (before the `float roughness` line):

```glsl
    // Apply per-material weathering (opt-in via weathering_enabled in .material)
    // Roughness is pre-declared here so weathering can modify it alongside albedo.
    float roughness = clamp(uMaterialRoughnessScale * uMaterialRoughnessBias, 0.08, 0.98);
```

Then replace the existing `float roughness` line (the one that was at ~line 1066) with just a comment:

```glsl
    // roughness already declared above (weathering needs to modify it)
```

Wait — that's awkward. A cleaner approach: keep `roughness` declared where it is, and call weathering after roughness is fully computed (after the `uUseMaterialMaps` roughness sampling block around line 1090). Find the line after `materialAo` is finalized:

After line 1090 (`}`), before line 1092 (`vec3 ambient = ...`), add:

```glsl

    // Per-material weathering — modifies albedo and roughness based on geometry context
    applyMaterialWeathering(albedo, roughness, geometricNormal, vWorldPos);
```

This is the correct insertion point because both `albedo` and `roughness` are finalized by this point (including material map sampling), and the weathering layer modifies them before lighting.

- [ ] **Step 3: Build and launch to verify shader compiles**

Run: `cd build && cmake --build . 2>&1 | tail -10`

Then launch the level editor to verify the shader compiles at runtime (shader compilation happens on first frame):

Run: `cd build && timeout 5 ./level-editor 2>&1 | grep -i "error\|shader\|fail" | head -10`

Expected: No shader compilation errors. Visually, nothing changes because no material has `weathering_enabled true` yet.

- [ ] **Step 4: Commit**

```bash
git add assets/shaders/game/scene.frag
git commit -m "Add per-MaterialKind weathering functions and dispatcher to scene shader"
```

---

### Task 5: Create weathered material variants and verify visually

**Files:**
- Create: `assets/materials/stone_weathered.material`
- Create: `assets/materials/brick_weathered.material`
- Create: `assets/materials/wood_weathered.material`
- Create: `assets/materials/floor_weathered.material`
- Create: `assets/materials/metal_weathered.material`

- [ ] **Step 1: Create stone_weathered.material**

```
id stone_weathered
parent stone_default
weathering_enabled true
weathering_dirt_strength 0.6
weathering_dirt_color 0.18 0.14 0.10
weathering_edge_wear_strength 0.4
weathering_dust_strength 0.25
weathering_damp_strength 0.5
weathering_noise_scale 0.35
```

- [ ] **Step 2: Create brick_weathered.material**

```
id brick_weathered
parent brick_default
weathering_enabled true
weathering_dirt_strength 0.5
weathering_dirt_color 0.20 0.15 0.10
weathering_edge_wear_strength 0.35
weathering_dust_strength 0.2
weathering_damp_strength 0.45
weathering_noise_scale 0.4
```

- [ ] **Step 3: Create wood_weathered.material**

```
id wood_weathered
parent wood_default
weathering_enabled true
weathering_dirt_strength 0.45
weathering_dirt_color 0.22 0.18 0.12
weathering_edge_wear_strength 0.3
weathering_dust_strength 0.2
weathering_damp_strength 0.4
weathering_noise_scale 0.35
```

- [ ] **Step 4: Create floor_weathered.material**

```
id floor_weathered
parent floor_default
weathering_enabled true
weathering_dirt_strength 0.5
weathering_dirt_color 0.20 0.16 0.12
weathering_edge_wear_strength 0.3
weathering_dust_strength 0.3
weathering_damp_strength 0.4
weathering_noise_scale 0.3
```

- [ ] **Step 5: Create metal_weathered.material**

```
id metal_weathered
parent metal_default
weathering_enabled true
weathering_dirt_strength 0.55
weathering_dirt_color 0.45 0.22 0.08
weathering_edge_wear_strength 0.5
weathering_dust_strength 0.15
weathering_damp_strength 0.4
weathering_noise_scale 0.35
```

- [ ] **Step 6: Build and launch the level editor**

Run: `cd build && cmake --build . 2>&1 | tail -5`

Launch the level editor. To test visually, assign one of the weathered materials to a mesh in a test scene (e.g., swap `stone_default` to `stone_weathered` on a wall). Verify:
- Crevices/edges show visible grime/wear differentiation
- Flat horizontal surfaces show dust
- Lower surfaces are slightly darker/damper
- Adjacent surfaces of the same material look varied (macro noise)
- The effect is subtle and stylistically consistent with the Stanley Parable aesthetic

- [ ] **Step 7: Commit**

```bash
git add assets/materials/stone_weathered.material assets/materials/brick_weathered.material assets/materials/wood_weathered.material assets/materials/floor_weathered.material assets/materials/metal_weathered.material
git commit -m "Add weathered material variants for stone, brick, wood, floor, and metal"
```

---

### Task 6: Run full test suite and verify no regressions

**Files:** None (verification only)

- [ ] **Step 1: Run the full test suite**

Run: `cd build && cmake --build . && ctest --output-on-failure 2>&1 | tail -30`

Expected: All existing tests pass. The weathering changes are additive — no existing behavior is modified.

- [ ] **Step 2: Verify existing materials are unchanged**

Launch the level editor with an existing scene (e.g., cathedral or silos_cloister). Navigate through the level and verify that all materials that do NOT have `weathering_enabled` look identical to before. There should be zero visual regression for non-weathered materials.

- [ ] **Step 3: Commit (if any test fixes were needed)**

Only needed if step 1 revealed issues that required fixes. Otherwise skip.
