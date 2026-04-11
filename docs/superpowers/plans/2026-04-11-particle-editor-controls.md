# Particle Editor Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add viewport gizmo, create menu, full parameter inspector with auto-save, and hot-reload for particle emitters in the level editor.

**Architecture:** All four features follow existing editor patterns exactly. Gizmo wires into the ImGuizmo switch statements. Create menu extends EditorPlacementKind. Inspector edits the `.particle` definition file via ContentRegistry. Hot-reload polls file timestamps like the material system.

**Tech Stack:** C++20, Dear ImGui, ImGuizmo, EnTT ECS

---

## File Map

### Modified Files

| File | Change |
|------|--------|
| `src/editor/viewport/EditorViewportInteraction.cpp` | Add ParticleEmitter to gizmo display, transform application, and multi-gizmo switch statements; add ParticleEmitter case to commitPlacement |
| `src/editor/ui/LevelEditorUi.h` | Add `ParticleEmitter` to `EditorPlacementKind` enum |
| `src/editor/ui/EditorAssetBrowserPanel.cpp` | Add "Place Particle Emitter" menu item |
| `src/editor/ui/EditorPanelUtils.cpp` | Handle ParticleEmitter in `beginPlacement()` |
| `src/editor/ui/inspectors/ParticleEmitterInspector.cpp` | Expand from 2 fields to full parameter editing with auto-save |
| `src/game/content/ContentRegistry.h` | Add particle hot-reload state (paths, timestamps, generation counter, poll method) |
| `src/game/content/ContentRegistry.cpp` | Implement hot-reload polling, track paths during directory load |
| `src/game/modules/particles/ParticleSystem.h` | Add generation tracking |
| `src/game/modules/particles/ParticleSystem.cpp` | Check generation counter, invalidate stale emitters |

---

### Task 1: Viewport Gizmo

**Files:**
- Modify: `src/editor/viewport/EditorViewportInteraction.cpp`

- [ ] **Step 1: Read the file**

Read `src/editor/viewport/EditorViewportInteraction.cpp` to find the exact locations of the three switch statements:
1. Single-object gizmo model matrix construction (search for `case EditorSceneObjectKind::PlayerSpawn:` in the gizmo display section)
2. Single-object gizmo transform application (search for `case EditorSceneObjectKind::PlayerSpawn:` in the transform apply section)
3. Multi-object gizmo (search for `case EditorSceneObjectKind::PlayerSpawn:` in the multi-gizmo section)

- [ ] **Step 2: Add ParticleEmitter to gizmo display switch**

Find the single-object gizmo model matrix switch (around line 307). Add `ParticleEmitter` before the `PlayerSpawn` case:

```cpp
case EditorSceneObjectKind::ParticleEmitter: {
    const auto& emitter = std::get<LevelParticleEmitterPlacement>(object->payload);
    model = makeModelMatrix(emitter.position, glm::vec3(0.5f));
    break;
}
```

- [ ] **Step 3: Add ParticleEmitter to gizmo transform apply switch**

Find the single-object transform application switch (around line 361). Add before `PlayerSpawn`:

```cpp
case EditorSceneObjectKind::ParticleEmitter: {
    if (!document.applyWorldTransform(object->id, model)) {
        return false;
    }
    break;
}
```

Note: `applyWorldTransform` already handles `LevelParticleEmitterPlacement` — it extracts `position = glm::vec3(localMatrix[3])`.

- [ ] **Step 4: Add ParticleEmitter to multi-gizmo switch**

Find the multi-object gizmo switch (around line 464). Add before `PlayerSpawn`:

```cpp
case EditorSceneObjectKind::ParticleEmitter: {
    auto it = multiGizmoState.cachedTransforms.find(id);
    if (it == multiGizmoState.cachedTransforms.end()) continue;
    const glm::mat4 newWorld = before * localDelta * invBefore * it->second;
    document.applyWorldTransform(id, newWorld);
    break;
}
```

Also add to the multi-gizmo cache initialization switch (around line 440, where `cachedTransforms` is populated):

```cpp
case EditorSceneObjectKind::ParticleEmitter:
    multiGizmoState.cachedTransforms[id] = document.worldTransformMatrix(id);
    break;
```

- [ ] **Step 5: Build and verify**

```bash
cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake --build . --target level-editor -j$(sysctl -n hw.ncpu)
```

- [ ] **Step 6: Commit**

```bash
git add src/editor/viewport/EditorViewportInteraction.cpp
git commit -m "Add viewport gizmo support for particle emitters"
```

---

### Task 2: Create Menu

**Files:**
- Modify: `src/editor/ui/LevelEditorUi.h`
- Modify: `src/editor/ui/EditorAssetBrowserPanel.cpp`
- Modify: `src/editor/ui/EditorPanelUtils.cpp`
- Modify: `src/editor/viewport/EditorViewportInteraction.cpp` (commitPlacement)

- [ ] **Step 1: Add to EditorPlacementKind enum**

Read `src/editor/ui/LevelEditorUi.h`. Find the `EditorPlacementKind` enum. Add `ParticleEmitter` after `Archetype`:

```cpp
enum class EditorPlacementKind {
    None,
    Mesh,
    PointLight,
    SpotLight,
    DirectionalLight,
    Collider,
    PlayerSpawn,
    Checkpoint,
    Archetype,
    ParticleEmitter,
};
```

- [ ] **Step 2: Add menu item**

Read `src/editor/ui/EditorAssetBrowserPanel.cpp`. Find `renderAssetBrowserCreateContextMenu()`. Add after the Checkpoint menu item:

```cpp
if (ImGui::MenuItem("Place Particle Emitter")) {
    beginPlacement(placementState, EditorPlacementKind::ParticleEmitter);
}
```

- [ ] **Step 3: Handle in beginPlacement**

Read `src/editor/ui/EditorPanelUtils.cpp`. Find `beginPlacement()`. Add a case in the switch:

```cpp
case EditorPlacementKind::ParticleEmitter:
    break;
```

(No special state needed — the default emitterId is set in commitPlacement.)

- [ ] **Step 4: Handle in commitPlacement**

Read `src/editor/viewport/EditorViewportInteraction.cpp`. Find the `commitPlacement()` function. Add before the `None` case:

```cpp
case EditorPlacementKind::ParticleEmitter:
    document.addParticleEmitter(LevelParticleEmitterPlacement{
        .emitterId = "dust_motes",
        .position = position,
    });
    break;
```

- [ ] **Step 5: Build and verify**

```bash
cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake --build . --target level-editor -j$(sysctl -n hw.ncpu)
```

- [ ] **Step 6: Commit**

```bash
git add src/editor/ui/LevelEditorUi.h src/editor/ui/EditorAssetBrowserPanel.cpp src/editor/ui/EditorPanelUtils.cpp src/editor/viewport/EditorViewportInteraction.cpp
git commit -m "Add Place Particle Emitter to editor create menu"
```

---

### Task 3: Hot-Reload Infrastructure

**Files:**
- Modify: `src/game/content/ContentRegistry.h`
- Modify: `src/game/content/ContentRegistry.cpp`
- Modify: `src/game/modules/particles/ParticleSystem.h`
- Modify: `src/game/modules/particles/ParticleSystem.cpp`

- [ ] **Step 1: Add hot-reload state to ContentRegistry.h**

Read `src/game/content/ContentRegistry.h`. Add the following:

Public method:
```cpp
void pollParticleHotReload();
int particleDefinitionGeneration() const { return particleDefinitionGeneration_; }
```

Private members (add alongside the existing material hot-reload state):
```cpp
std::unordered_map<std::string, std::filesystem::file_time_type> particleEmitterFileTimes_;
std::unordered_map<std::string, std::string> particleEmitterFilePathById_;
std::chrono::steady_clock::time_point lastParticlePoll_ = std::chrono::steady_clock::now();
int particleDefinitionGeneration_ = 0;
```

Also add `#include <filesystem>` and `#include <chrono>` if not already present.

- [ ] **Step 2: Track paths during directory loading**

Read `src/game/content/ContentRegistry.cpp`. Find `loadParticleEmittersFromDirectory()`. Modify it to track file paths and timestamps, following the `loadMaterialsFromDirectory()` pattern:

```cpp
void ContentRegistry::loadParticleEmittersFromDirectory(const std::string& relativeDirectory) {
    namespace fs = std::filesystem;
    const fs::path directory = resolveProjectPath(relativeDirectory);
    if (!fs::exists(directory)) return;
    for (const auto& entry : fs::recursive_directory_iterator(directory,
            fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".particle") continue;
        try {
            auto def = loadParticleEmitterDefinition(entry.path().string());
            if (particleEmitters_.count(def.id)) {
                spdlog::error("Duplicate particle emitter id '{}' in '{}' — skipped",
                              def.id, entry.path().string());
                continue;
            }
            particleEmitterFilePathById_[def.id] = entry.path().string();
            particleEmitterFileTimes_[entry.path().string()] = fs::last_write_time(entry.path());
            particleEmitters_.emplace(def.id, std::move(def));
        } catch (const std::exception& e) {
            spdlog::error("Failed to load particle emitter '{}': {}", entry.path().string(), e.what());
        }
    }
}
```

Also clear the new maps in `loadDefaults()`:
```cpp
particleEmitterFileTimes_.clear();
particleEmitterFilePathById_.clear();
```

- [ ] **Step 3: Implement pollParticleHotReload()**

Add to `ContentRegistry.cpp`:

```cpp
void ContentRegistry::pollParticleHotReload() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastParticlePoll_);
    if (elapsed.count() < kMaterialPollIntervalMs) return;
    lastParticlePoll_ = now;

    namespace fs = std::filesystem;
    for (auto& [path, knownTime] : particleEmitterFileTimes_) {
        try {
            if (!fs::exists(path)) continue;
            auto currentTime = fs::last_write_time(path);
            if (currentTime == knownTime) continue;
            knownTime = currentTime;

            spdlog::info("Hot-reloading particle emitter: {}", path);
            auto updated = loadParticleEmitterDefinition(path);
            particleEmitters_[updated.id] = std::move(updated);
            ++particleDefinitionGeneration_;
        } catch (const std::exception& e) {
            spdlog::error("Hot-reload error for '{}': {}", path, e.what());
        }
    }
}
```

- [ ] **Step 4: Call pollParticleHotReload from the editor**

Search the codebase for where `pollMaterialHotReload` is called. Add `content.pollParticleHotReload()` right after it.

- [ ] **Step 5: Add generation tracking to ParticleUpdateSystem**

Read `src/game/modules/particles/ParticleSystem.h`. Add a private member:

```cpp
int lastDefinitionGeneration_ = 0;
```

Read `src/game/modules/particles/ParticleSystem.cpp`. In the `update()` method, add at the beginning (before the entity view loop):

```cpp
if (content_) {
    int currentGen = content_->particleDefinitionGeneration();
    if (currentGen != lastDefinitionGeneration_) {
        lastDefinitionGeneration_ = currentGen;
        emitters_.clear(); // Force all emitters to reconstruct from updated definitions
    }
}
```

- [ ] **Step 6: Build and run tests**

```bash
cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake .. && cmake --build . -j$(sysctl -n hw.ncpu) && ctest --output-on-failure
```

- [ ] **Step 7: Commit**

```bash
git add src/game/content/ContentRegistry.h src/game/content/ContentRegistry.cpp src/game/modules/particles/ParticleSystem.h src/game/modules/particles/ParticleSystem.cpp
git commit -m "Add particle definition hot-reload with generation-based emitter invalidation"
```

Find and add the editor call site too:
```bash
git add <editor file where pollParticleHotReload was added>
git commit --amend --no-edit
```

---

### Task 4: Full Parameter Inspector

**Files:**
- Modify: `src/editor/ui/inspectors/ParticleEmitterInspector.cpp`

This is the largest task. The inspector reads the `ParticleEmitterDefinition` from ContentRegistry, presents all fields as editable ImGui widgets, and saves changes back to the `.particle` file on edit.

- [ ] **Step 1: Read dependencies**

Read these files for API reference:
- `src/editor/ui/inspectors/ParticleEmitterInspector.cpp` (current implementation)
- `src/editor/ui/inspectors/LightInspector.cpp` (widget patterns)
- `src/game/particles/ParticleEmitterDefinition.h` (struct fields)
- `src/game/content/ContentRegistry.h` (find/access methods)

- [ ] **Step 2: Rewrite ParticleEmitterInspector.cpp**

Replace the entire file with the expanded implementation. The inspector needs access to the ContentRegistry, which is stored in the ECS registry context. The inspector function signature currently takes `LevelParticleEmitterPlacement&` — the definition is looked up via `placement.emitterId`.

The key pattern: every field edit follows the LightInspector pattern:
1. `auto itemBefore = document.captureState();`
2. `trackSceneItem(itemBefore, "label", renderInspectorPropertyRow("Label", [&]() { return ImGui::Widget(...); }));`
3. After the widget returns true (changed), also: update the in-memory definition in ContentRegistry, call `saveParticleEmitterDefinition()`, and mark dirty.

However, the inspector edits the *definition*, not the placement. The definition is shared across all emitters with the same ID. The inspector needs to:
1. Get a mutable reference to the definition in ContentRegistry (or get a copy, edit it, and put it back)
2. On change, save to the `.particle` file path

Since ContentRegistry only exposes `const` pointers via `findParticleEmitter()`, add a mutable accessor. In `ContentRegistry.h`, add:
```cpp
ParticleEmitterDefinition* findMutableParticleEmitter(const std::string& id);
const std::string* findParticleEmitterPath(const std::string& id) const;
```

In `ContentRegistry.cpp`:
```cpp
ParticleEmitterDefinition* ContentRegistry::findMutableParticleEmitter(const std::string& id) {
    auto it = particleEmitters_.find(id);
    return it != particleEmitters_.end() ? &it->second : nullptr;
}

const std::string* ContentRegistry::findParticleEmitterPath(const std::string& id) const {
    auto it = particleEmitterFilePathById_.find(id);
    return it != particleEmitterFilePathById_.end() ? &it->second : nullptr;
}
```

Then the inspector implementation:

```cpp
#include "editor/ui/inspectors/ParticleEmitterInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelDef.h"
#include "game/particles/ParticleEmitterDefinition.h"

#include <imgui.h>

#include <algorithm>

namespace {

void saveDefinition(ContentRegistry& content, const std::string& emitterId) {
    const auto* path = content.findParticleEmitterPath(emitterId);
    const auto* def = content.findParticleEmitter(emitterId);
    if (path && def) {
        saveParticleEmitterDefinition(*path, *def);
    }
}

} // namespace

void drawParticleEmitterInspector(LevelParticleEmitterPlacement& placement,
                                  EditorSceneDocument& document,
                                  EditorCommandStack& commandStack,
                                  EditorPendingCommand& pendingCommand,
                                  const EditorSceneDocumentState& beforeState) {
    (void)beforeState;

    const auto trackSceneItem = [&](const EditorSceneDocumentState& itemBefore,
                                    const std::string& label, bool changed) {
        if (changed) document.markSceneDirty();
        trackLastItemCommand(itemBefore, label, pendingCommand, commandStack, document);
    };

    // --- Emitter ID (combo dropdown) ---
    auto* contentPtr = document.registry()
        ? document.registry()->ctx().find<ContentRegistry*>()
        : nullptr;
    ContentRegistry* content = contentPtr ? *contentPtr : nullptr;

    if (content) {
        const auto& emitters = content->particleEmitters();
        std::vector<std::string> ids;
        ids.reserve(emitters.size());
        for (const auto& [id, _] : emitters) ids.push_back(id);
        std::sort(ids.begin(), ids.end());

        int currentIndex = 0;
        for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
            if (ids[i] == placement.emitterId) { currentIndex = i; break; }
        }

        auto itemBefore = document.captureState();
        if (renderInspectorPropertyRow("Emitter Id", [&]() {
            if (ImGui::BeginCombo("##value", placement.emitterId.c_str())) {
                for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
                    bool selected = (i == currentIndex);
                    if (ImGui::Selectable(ids[i].c_str(), selected)) {
                        placement.emitterId = ids[i];
                        ImGui::EndCombo();
                        return true;
                    }
                }
                ImGui::EndCombo();
            }
            return false;
        }, EditorInspectorFieldKind::Enum)) {
            document.markSceneDirty();
            trackLastItemCommand(itemBefore, "Change Emitter Id", pendingCommand, commandStack, document);
        }
    } else {
        auto itemBefore = document.captureState();
        trackSceneItem(itemBefore, "Change Emitter Id",
            renderInspectorPropertyRow("Emitter Id", [&]() {
                return editString("##value", placement.emitterId, "emitter id");
            }, EditorInspectorFieldKind::Text));
    }

    // --- Position ---
    drawPositionSection(placement.position, "Position", "Move Particle Emitter",
                        document, commandStack, pendingCommand);

    // --- Definition parameters (only if ContentRegistry available) ---
    ParticleEmitterDefinition* def = content
        ? content->findMutableParticleEmitter(placement.emitterId)
        : nullptr;

    if (!def) {
        endInspectorPropertyTable();
        return;
    }

    // Helper: edit a definition field, auto-save on change
    const auto editDef = [&](const char* label, const char* undoLabel, auto widgetFn) {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow(label, widgetFn);
        if (changed) {
            document.markSceneDirty();
            saveDefinition(*content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, undoLabel, pendingCommand, commandStack, document);
    };

    // Ensure optionals have values for editing (use defaults from ResolvedParticleEmitterDefinition)
    if (!def->emissionRate) def->emissionRate = 10.0f;
    if (!def->looping) def->looping = true;
    if (!def->duration) def->duration = 0.0f;
    if (!def->warmUp) def->warmUp = false;
    if (!def->lifetimeMin) def->lifetimeMin = 0.5f;
    if (!def->lifetimeMax) def->lifetimeMax = 2.0f;
    if (!def->initialSpeedMin) def->initialSpeedMin = 1.0f;
    if (!def->initialSpeedMax) def->initialSpeedMax = 3.0f;
    if (!def->rotationSpeedMin) def->rotationSpeedMin = 0.0f;
    if (!def->rotationSpeedMax) def->rotationSpeedMax = 0.0f;
    if (!def->shapeType) def->shapeType = "point";
    if (!def->shapeParam0) def->shapeParam0 = 0.0f;
    if (!def->shapeParam1) def->shapeParam1 = 0.0f;
    if (!def->blendMode) def->blendMode = "additive";
    if (!def->simulationSpace) def->simulationSpace = "world";
    if (!def->emissiveStrength) def->emissiveStrength = 1.0f;
    if (!def->softParticleFade) def->softParticleFade = 0.5f;
    if (!def->maxParticles) def->maxParticles = 256;

    ImGui::Separator();
    ImGui::TextUnformatted("Emission");

    editDef("Emission Rate", "Change Emission Rate", [&]() {
        return ImGui::DragFloat("##value", &*def->emissionRate, 0.5f, 0.1f, 1000.0f, "%.1f");
    });
    editDef("Max Particles", "Change Max Particles", [&]() {
        return ImGui::DragInt("##value", &*def->maxParticles, 1.0f, 1, 4096);
    });
    editDef("Looping", "Toggle Looping", [&]() {
        return ImGui::Checkbox("##value", &*def->looping);
    });
    editDef("Duration", "Change Duration", [&]() {
        return ImGui::DragFloat("##value", &*def->duration, 0.1f, 0.0f, 60.0f, "%.1f");
    });
    editDef("Warm Up", "Toggle Warm Up", [&]() {
        return ImGui::Checkbox("##value", &*def->warmUp);
    });

    ImGui::Separator();
    ImGui::TextUnformatted("Particles");

    editDef("Lifetime Min", "Change Lifetime Min", [&]() {
        return ImGui::DragFloat("##value", &*def->lifetimeMin, 0.05f, 0.01f, 30.0f, "%.2f");
    });
    editDef("Lifetime Max", "Change Lifetime Max", [&]() {
        return ImGui::DragFloat("##value", &*def->lifetimeMax, 0.05f, 0.01f, 30.0f, "%.2f");
    });
    editDef("Speed Min", "Change Speed Min", [&]() {
        return ImGui::DragFloat("##value", &*def->initialSpeedMin, 0.05f, 0.0f, 50.0f, "%.2f");
    });
    editDef("Speed Max", "Change Speed Max", [&]() {
        return ImGui::DragFloat("##value", &*def->initialSpeedMax, 0.05f, 0.0f, 50.0f, "%.2f");
    });
    editDef("Rot Speed Min", "Change Rotation Speed Min", [&]() {
        return ImGui::DragFloat("##value", &*def->rotationSpeedMin, 0.05f, -10.0f, 10.0f, "%.2f");
    });
    editDef("Rot Speed Max", "Change Rotation Speed Max", [&]() {
        return ImGui::DragFloat("##value", &*def->rotationSpeedMax, 0.05f, -10.0f, 10.0f, "%.2f");
    });

    ImGui::Separator();
    ImGui::TextUnformatted("Shape");

    {
        const char* shapeTypes[] = {"point", "sphere", "cone"};
        int shapeIndex = 0;
        if (*def->shapeType == "sphere") shapeIndex = 1;
        else if (*def->shapeType == "cone") shapeIndex = 2;

        editDef("Shape Type", "Change Shape Type", [&]() {
            bool changed = ImGui::Combo("##value", &shapeIndex, shapeTypes, 3);
            if (changed) def->shapeType = shapeTypes[shapeIndex];
            return changed;
        });

        if (*def->shapeType == "sphere") {
            editDef("Radius", "Change Shape Radius", [&]() {
                return ImGui::DragFloat("##value", &*def->shapeParam0, 0.05f, 0.01f, 20.0f, "%.2f");
            });
        } else if (*def->shapeType == "cone") {
            editDef("Angle", "Change Cone Angle", [&]() {
                return ImGui::DragFloat("##value", &*def->shapeParam0, 0.5f, 1.0f, 89.0f, "%.1f");
            });
            editDef("Base Radius", "Change Cone Base Radius", [&]() {
                return ImGui::DragFloat("##value", &*def->shapeParam1, 0.01f, 0.0f, 5.0f, "%.3f");
            });
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Rendering");

    {
        const char* blendModes[] = {"additive", "alpha"};
        int blendIndex = (*def->blendMode == "alpha") ? 1 : 0;
        editDef("Blend Mode", "Change Blend Mode", [&]() {
            bool changed = ImGui::Combo("##value", &blendIndex, blendModes, 2);
            if (changed) def->blendMode = blendModes[blendIndex];
            return changed;
        });
    }
    {
        const char* simSpaces[] = {"world", "local"};
        int spaceIndex = (*def->simulationSpace == "local") ? 1 : 0;
        editDef("Sim Space", "Change Simulation Space", [&]() {
            bool changed = ImGui::Combo("##value", &spaceIndex, simSpaces, 2);
            if (changed) def->simulationSpace = simSpaces[spaceIndex];
            return changed;
        });
    }
    editDef("Emissive", "Change Emissive Strength", [&]() {
        return ImGui::DragFloat("##value", &*def->emissiveStrength, 0.05f, 0.1f, 10.0f, "%.2f");
    });
    editDef("Soft Fade", "Change Soft Particle Fade", [&]() {
        return ImGui::DragFloat("##value", &*def->softParticleFade, 0.05f, 0.0f, 5.0f, "%.2f");
    });

    // --- Forces ---
    ImGui::Separator();
    ImGui::TextUnformatted("Forces");

    bool forcesChanged = false;
    for (int i = 0; i < static_cast<int>(def->forces.size()); ++i) {
        ImGui::PushID(i);
        auto& force = def->forces[i];

        const char* forceTypes[] = {"gravity", "drag"};
        int forceIndex = (force.type == "drag") ? 1 : 0;
        ImGui::SetNextItemWidth(70);
        if (ImGui::Combo("##type", &forceIndex, forceTypes, 2)) {
            force.type = forceTypes[forceIndex];
            forcesChanged = true;
        }
        ImGui::SameLine();

        if (force.type == "gravity") {
            ImGui::SetNextItemWidth(180);
            if (ImGui::DragFloat3("##val", &force.value.x, 0.1f, -50.0f, 50.0f, "%.1f")) {
                forcesChanged = true;
            }
        } else {
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("##coeff", &force.coefficient, 0.05f, 0.0f, 20.0f, "%.2f")) {
                forcesChanged = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            def->forces.erase(def->forces.begin() + i);
            forcesChanged = true;
            --i;
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("+ Add Force")) {
        def->forces.push_back({"gravity", glm::vec3(0.0f, -9.81f, 0.0f), 0.0f});
        forcesChanged = true;
    }
    if (forcesChanged) {
        document.markSceneDirty();
        saveDefinition(*content, placement.emitterId);
    }

    // --- Color Over Lifetime ---
    ImGui::Separator();
    ImGui::TextUnformatted("Color Over Lifetime");

    bool colorChanged = false;
    for (int i = 0; i < static_cast<int>(def->colorStops.size()); ++i) {
        ImGui::PushID(1000 + i);
        auto& [t, color] = def->colorStops[i];

        ImGui::SetNextItemWidth(50);
        if (ImGui::DragFloat("##t", &t, 0.01f, 0.0f, 1.0f, "%.2f")) colorChanged = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        if (ImGui::ColorEdit4("##c", &color.x,
                ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaBar)) {
            colorChanged = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            def->colorStops.erase(def->colorStops.begin() + i);
            colorChanged = true;
            --i;
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("+ Add Color Stop")) {
        float nextT = def->colorStops.empty() ? 0.0f : def->colorStops.back().first + 0.25f;
        def->colorStops.push_back({std::min(nextT, 1.0f), glm::vec4(1.0f)});
        colorChanged = true;
    }
    if (colorChanged) {
        document.markSceneDirty();
        saveDefinition(*content, placement.emitterId);
    }

    // --- Size Over Lifetime ---
    ImGui::Separator();
    ImGui::TextUnformatted("Size Over Lifetime");

    bool sizeChanged = false;
    for (int i = 0; i < static_cast<int>(def->sizeKeyframes.size()); ++i) {
        ImGui::PushID(2000 + i);
        auto& [t, val] = def->sizeKeyframes[i];

        ImGui::SetNextItemWidth(50);
        if (ImGui::DragFloat("##t", &t, 0.01f, 0.0f, 1.0f, "%.2f")) sizeChanged = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::DragFloat("##v", &val, 0.005f, 0.001f, 5.0f, "%.3f")) sizeChanged = true;
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            def->sizeKeyframes.erase(def->sizeKeyframes.begin() + i);
            sizeChanged = true;
            --i;
        }
        ImGui::PopID();
    }
    if (ImGui::SmallButton("+ Add Size Keyframe")) {
        float nextT = def->sizeKeyframes.empty() ? 0.0f : def->sizeKeyframes.back().first + 0.25f;
        def->sizeKeyframes.push_back({std::min(nextT, 1.0f), 0.1f});
        sizeChanged = true;
    }
    if (sizeChanged) {
        document.markSceneDirty();
        saveDefinition(*content, placement.emitterId);
    }

    endInspectorPropertyTable();
}
```

**Important notes for the implementer:**
- The `ContentRegistry*` access pattern depends on how the editor stores it. Read the existing inspector code to find how other inspectors access game services. The `document.registry()` might not exist — if the editor doesn't expose it, pass the `ContentRegistry*` through the inspector function signature or get it from the editor's application context. Search for how other inspectors access ContentRegistry (e.g., the ArchetypeInspector might reference it).
- If `ContentRegistry*` isn't accessible from the inspector, an alternative: add it as a parameter to `drawParticleEmitterInspector()` and pass it from the dispatch site in `SceneSelectionInspector.cpp`.

- [ ] **Step 3: Add mutable accessors to ContentRegistry**

In `src/game/content/ContentRegistry.h`, add:
```cpp
ParticleEmitterDefinition* findMutableParticleEmitter(const std::string& id);
const std::string* findParticleEmitterPath(const std::string& id) const;
```

In `src/game/content/ContentRegistry.cpp`:
```cpp
ParticleEmitterDefinition* ContentRegistry::findMutableParticleEmitter(const std::string& id) {
    auto it = particleEmitters_.find(id);
    return it != particleEmitters_.end() ? &it->second : nullptr;
}

const std::string* ContentRegistry::findParticleEmitterPath(const std::string& id) const {
    auto it = particleEmitterFilePathById_.find(id);
    return it != particleEmitterFilePathById_.end() ? &it->second : nullptr;
}
```

- [ ] **Step 4: Build and verify**

```bash
cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake .. && cmake --build . -j$(sysctl -n hw.ncpu) && ctest --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add src/editor/ui/inspectors/ParticleEmitterInspector.cpp src/game/content/ContentRegistry.h src/game/content/ContentRegistry.cpp
git commit -m "Add full parameter inspector for particle emitters with auto-save"
```

---

### Task 5: Build, Test, and Verify

- [ ] **Step 1: Full rebuild**

```bash
cd /Users/ozan/Projects/gsd-3d-roguelike/build && cmake .. && cmake --build . -j$(sysctl -n hw.ncpu)
```

- [ ] **Step 2: Run all tests**

```bash
ctest --output-on-failure
```

All 56+ tests must pass.

- [ ] **Step 3: Launch editor and verify**

```bash
open ./build/apps/level_editor/level-editor
```

Verify:
- Can select particle emitter in outliner, gizmo appears, can drag to move
- Right-click context menu has "Place Particle Emitter", clicking places one
- Inspector shows all parameters (emission, shape, forces, colors, sizes)
- Editing a parameter in inspector updates the live preview
- Multiple emitters with same ID all update when definition changes
