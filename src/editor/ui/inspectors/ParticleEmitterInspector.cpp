#include "editor/ui/inspectors/ParticleEmitterInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/content/ContentRegistry.h"
#include "game/level/LevelDef.h"
#include "game/particles/ParticleEmitterDefinition.h"

#include <algorithm>
#include <imgui.h>
#include <string>
#include <vector>

namespace {

void saveDefinition(ContentRegistry& content, const std::string& emitterId) {
    const auto* path = content.findParticleEmitterPath(emitterId);
    const auto* def = content.findParticleEmitter(emitterId);
    if (path && def) {
        saveParticleEmitterDefinition(*path, *def);
    }
}

const char* shapeLabelForParam0(const std::string& shapeType) {
    if (shapeType == "sphere") {
        return "Radius";
    }
    if (shapeType == "cone") {
        return "Angle";
    }
    return "Param 0";
}

int shapeTypeToIndex(const std::string& shapeType) {
    if (shapeType == "sphere") {
        return 1;
    }
    if (shapeType == "cone") {
        return 2;
    }
    return 0; // point
}

std::string indexToShapeType(int index) {
    switch (index) {
    case 1:
        return "sphere";
    case 2:
        return "cone";
    default:
        return "point";
    }
}

int blendModeToIndex(const std::string& mode) {
    if (mode == "alpha") {
        return 1;
    }
    return 0; // additive
}

std::string indexToBlendMode(int index) {
    if (index == 1) {
        return "alpha";
    }
    return "additive";
}

int simSpaceToIndex(const std::string& space) {
    if (space == "local") {
        return 1;
    }
    return 0; // world
}

std::string indexToSimSpace(int index) {
    if (index == 1) {
        return "local";
    }
    return "world";
}

int forceTypeToIndex(const std::string& type) {
    if (type == "drag") {
        return 1;
    }
    if (type == "turbulence") {
        return 2;
    }
    return 0; // gravity
}

std::string indexToForceType(int index) {
    switch (index) {
    case 1:
        return "drag";
    case 2:
        return "turbulence";
    default:
        return "gravity";
    }
}

} // namespace

void drawParticleEmitterInspector(LevelParticleEmitterPlacement& placement,
                                  EditorSceneDocument& document,
                                  ContentRegistry& content,
                                  EditorCommandStack& commandStack,
                                  EditorPendingCommand& pendingCommand,
                                  const EditorSceneDocumentState& beforeState) {
    (void)beforeState;

    const auto trackSceneItem = [&](const EditorSceneDocumentState& itemBefore,
                                    const std::string& label, bool changed) {
        if (changed) {
            document.markSceneDirty();
        }
        trackLastItemCommand(itemBefore, label, pendingCommand, commandStack, document);
    };

    // --- Emitter Id (combo dropdown) ---
    {
        std::vector<std::string> emitterIds;
        for (const auto& [id, _] : content.particleEmitters()) {
            emitterIds.push_back(id);
        }
        std::sort(emitterIds.begin(), emitterIds.end());

        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Emitter Id", [&]() {
            if (ImGui::BeginCombo("##value", placement.emitterId.c_str())) {
                for (const auto& id : emitterIds) {
                    const bool selected = (id == placement.emitterId);
                    if (ImGui::Selectable(id.c_str(), selected)) {
                        placement.emitterId = id;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
                return true;
            }
            return false;
        }, EditorInspectorFieldKind::Enum);
        trackSceneItem(itemBefore, "Change Emitter Id", changed);
    }

    // --- Position ---
    drawPositionSection(placement.position, "Position", "Move Particle Emitter",
                        document, commandStack, pendingCommand);

    endInspectorPropertyTable();

    // Look up the mutable definition for editing
    ParticleEmitterDefinition* def = content.findMutableParticleEmitter(placement.emitterId);
    if (!def) {
        ImGui::Separator();
        ImGui::TextDisabled("Emitter definition '%s' not found", placement.emitterId.c_str());
        return;
    }

    // ===================================================================
    // Emission
    // ===================================================================
    ImGui::Separator();
    ImGui::TextUnformatted("Emission");
    if (!beginInspectorPropertyTable("EmissionProperties")) {
        return;
    }

    // Emission Rate
    if (!def->emissionRate) def->emissionRate = 10.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Emission Rate", [&]() {
            return ImGui::DragFloat("##value", &*def->emissionRate, 0.5f, 0.1f, 1000.0f, "%.1f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Emission Rate", pendingCommand, commandStack,
                             document);
    }

    // Max Particles
    if (!def->maxParticles) def->maxParticles = 256;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Max Particles", [&]() {
            return ImGui::DragInt("##value", &*def->maxParticles, 1.0f, 1, 4096);
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Max Particles", pendingCommand, commandStack,
                             document);
    }

    // Looping
    if (!def->looping) def->looping = true;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Looping", [&]() {
            return ImGui::Checkbox("##value", &*def->looping);
        }, EditorInspectorFieldKind::Toggle);
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Toggle Looping", pendingCommand, commandStack, document);
    }

    // Duration
    if (!def->duration) def->duration = 0.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Duration", [&]() {
            return ImGui::DragFloat("##value", &*def->duration, 0.1f, 0.0f, 60.0f, "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Duration", pendingCommand, commandStack, document);
    }

    // Warm Up
    if (!def->warmUp) def->warmUp = false;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Warm Up", [&]() {
            return ImGui::Checkbox("##value", &*def->warmUp);
        }, EditorInspectorFieldKind::Toggle);
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Toggle Warm Up", pendingCommand, commandStack, document);
    }

    endInspectorPropertyTable();

    // ===================================================================
    // Particles
    // ===================================================================
    ImGui::Separator();
    ImGui::TextUnformatted("Particles");
    if (!beginInspectorPropertyTable("ParticleProperties")) {
        return;
    }

    // Lifetime Min
    if (!def->lifetimeMin) def->lifetimeMin = 0.5f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Lifetime Min", [&]() {
            return ImGui::DragFloat("##value", &*def->lifetimeMin, 0.01f, 0.01f, 60.0f, "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Lifetime Min", pendingCommand, commandStack,
                             document);
    }

    // Lifetime Max
    if (!def->lifetimeMax) def->lifetimeMax = 2.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Lifetime Max", [&]() {
            return ImGui::DragFloat("##value", &*def->lifetimeMax, 0.01f, 0.01f, 60.0f, "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Lifetime Max", pendingCommand, commandStack,
                             document);
    }

    // Initial Speed Min
    if (!def->initialSpeedMin) def->initialSpeedMin = 1.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Speed Min", [&]() {
            return ImGui::DragFloat("##value", &*def->initialSpeedMin, 0.05f, 0.0f, 100.0f,
                                    "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Initial Speed Min", pendingCommand, commandStack,
                             document);
    }

    // Initial Speed Max
    if (!def->initialSpeedMax) def->initialSpeedMax = 3.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Speed Max", [&]() {
            return ImGui::DragFloat("##value", &*def->initialSpeedMax, 0.05f, 0.0f, 100.0f,
                                    "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Initial Speed Max", pendingCommand, commandStack,
                             document);
    }

    // Rotation Speed Min
    if (!def->rotationSpeedMin) def->rotationSpeedMin = 0.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Rotation Min", [&]() {
            return ImGui::DragFloat("##value", &*def->rotationSpeedMin, 0.1f, -720.0f, 720.0f,
                                    "%.1f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Rotation Speed Min", pendingCommand, commandStack,
                             document);
    }

    // Rotation Speed Max
    if (!def->rotationSpeedMax) def->rotationSpeedMax = 0.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Rotation Max", [&]() {
            return ImGui::DragFloat("##value", &*def->rotationSpeedMax, 0.1f, -720.0f, 720.0f,
                                    "%.1f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Rotation Speed Max", pendingCommand, commandStack,
                             document);
    }

    endInspectorPropertyTable();

    // ===================================================================
    // Shape
    // ===================================================================
    ImGui::Separator();
    ImGui::TextUnformatted("Shape");
    if (!beginInspectorPropertyTable("ShapeProperties")) {
        return;
    }

    if (!def->shapeType) def->shapeType = "point";
    const char* shapeTypes[] = {"Point", "Sphere", "Cone"};
    int shapeIndex = shapeTypeToIndex(*def->shapeType);
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Shape", [&]() {
            return ImGui::Combo("##value", &shapeIndex, shapeTypes, 3);
        }, EditorInspectorFieldKind::Enum);
        if (changed) {
            *def->shapeType = indexToShapeType(shapeIndex);
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Shape Type", pendingCommand, commandStack,
                             document);
    }

    if (!def->shapeParam0) def->shapeParam0 = 0.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow(shapeLabelForParam0(*def->shapeType), [&]() {
            return ImGui::DragFloat("##value", &*def->shapeParam0, 0.01f, 0.0f, 180.0f, "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Shape Param 0", pendingCommand, commandStack,
                             document);
    }

    if (shapeIndex == 2) { // cone
        if (!def->shapeParam1) def->shapeParam1 = 0.0f;
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Base Radius", [&]() {
            return ImGui::DragFloat("##value", &*def->shapeParam1, 0.01f, 0.0f, 50.0f, "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Shape Base Radius", pendingCommand, commandStack,
                             document);
    }

    endInspectorPropertyTable();

    // ===================================================================
    // Rendering
    // ===================================================================
    ImGui::Separator();
    ImGui::TextUnformatted("Rendering");
    if (!beginInspectorPropertyTable("RenderingProperties")) {
        return;
    }

    // Blend Mode
    if (!def->blendMode) def->blendMode = "additive";
    const char* blendModes[] = {"Additive", "Alpha"};
    int blendIndex = blendModeToIndex(*def->blendMode);
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Blend Mode", [&]() {
            return ImGui::Combo("##value", &blendIndex, blendModes, 2);
        }, EditorInspectorFieldKind::Enum);
        if (changed) {
            *def->blendMode = indexToBlendMode(blendIndex);
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Blend Mode", pendingCommand, commandStack,
                             document);
    }

    // Simulation Space
    if (!def->simulationSpace) def->simulationSpace = "world";
    const char* simSpaces[] = {"World", "Local"};
    int simIndex = simSpaceToIndex(*def->simulationSpace);
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Sim Space", [&]() {
            return ImGui::Combo("##value", &simIndex, simSpaces, 2);
        }, EditorInspectorFieldKind::Enum);
        if (changed) {
            *def->simulationSpace = indexToSimSpace(simIndex);
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Simulation Space", pendingCommand, commandStack,
                             document);
    }

    // Emissive Strength
    if (!def->emissiveStrength) def->emissiveStrength = 1.0f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Emissive", [&]() {
            return ImGui::DragFloat("##value", &*def->emissiveStrength, 0.05f, 0.1f, 10.0f,
                                    "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Emissive Strength", pendingCommand, commandStack,
                             document);
    }

    // Soft Particle Fade
    if (!def->softParticleFade) def->softParticleFade = 0.5f;
    {
        auto itemBefore = document.captureState();
        bool changed = renderInspectorPropertyRow("Soft Fade", [&]() {
            return ImGui::DragFloat("##value", &*def->softParticleFade, 0.01f, 0.0f, 5.0f,
                                    "%.2f");
        });
        if (changed) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
        trackLastItemCommand(itemBefore, "Change Soft Particle Fade", pendingCommand, commandStack,
                             document);
    }

    endInspectorPropertyTable();

    // ===================================================================
    // Forces
    // ===================================================================
    ImGui::Separator();
    ImGui::TextUnformatted("Forces");

    {
        bool forcesChanged = false;
        int removeIndex = -1;

        for (int i = 0; i < static_cast<int>(def->forces.size()); ++i) {
            ImGui::PushID(i);
            auto& force = def->forces[static_cast<size_t>(i)];

            if (!beginInspectorPropertyTable(("Force" + std::to_string(i)).c_str())) {
                ImGui::PopID();
                continue;
            }

            const char* forceTypes[] = {"Gravity", "Drag", "Turbulence"};
            int forceIndex = forceTypeToIndex(force.type);
            if (renderInspectorPropertyRow("Type", [&]() {
                    return ImGui::Combo("##value", &forceIndex, forceTypes, 3);
                }, EditorInspectorFieldKind::Enum)) {
                force.type = indexToForceType(forceIndex);
                forcesChanged = true;
            }

            if (renderInspectorPropertyRow("Direction", [&]() {
                    return editVec3("##value", force.value, 0.01f);
                })) {
                forcesChanged = true;
            }

            if (renderInspectorPropertyRow("Coefficient", [&]() {
                    return ImGui::DragFloat("##value", &force.coefficient, 0.01f, 0.0f, 100.0f,
                                            "%.2f");
                })) {
                forcesChanged = true;
            }

            endInspectorPropertyTable();

            if (ImGui::SmallButton("Remove")) {
                removeIndex = i;
                forcesChanged = true;
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (removeIndex >= 0) {
            def->forces.erase(def->forces.begin() + removeIndex);
        }

        if (ImGui::Button("Add Force")) {
            ParticleForceDeclaration newForce;
            newForce.type = "gravity";
            newForce.value = glm::vec3(0.0f, -9.81f, 0.0f);
            newForce.coefficient = 1.0f;
            def->forces.push_back(newForce);
            forcesChanged = true;
        }

        if (forcesChanged) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
    }

    // ===================================================================
    // Color Over Lifetime
    // ===================================================================
    ImGui::Separator();
    ImGui::TextUnformatted("Color Over Lifetime");

    {
        bool colorChanged = false;
        int removeIndex = -1;

        for (int i = 0; i < static_cast<int>(def->colorStops.size()); ++i) {
            ImGui::PushID(1000 + i);
            auto& stop = def->colorStops[static_cast<size_t>(i)];

            if (!beginInspectorPropertyTable(("ColorStop" + std::to_string(i)).c_str())) {
                ImGui::PopID();
                continue;
            }

            if (renderInspectorPropertyRow("Time", [&]() {
                    return ImGui::DragFloat("##value", &stop.first, 0.01f, 0.0f, 1.0f, "%.3f");
                })) {
                colorChanged = true;
            }

            if (renderInspectorPropertyRow("Color", [&]() {
                    return ImGui::ColorEdit4("##value", &stop.second.x);
                }, EditorInspectorFieldKind::Color)) {
                colorChanged = true;
            }

            endInspectorPropertyTable();

            if (ImGui::SmallButton("Remove")) {
                removeIndex = i;
                colorChanged = true;
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (removeIndex >= 0) {
            def->colorStops.erase(def->colorStops.begin() + removeIndex);
        }

        if (ImGui::Button("Add Stop")) {
            float time = def->colorStops.empty() ? 0.0f : 1.0f;
            def->colorStops.emplace_back(time, glm::vec4(1.0f));
            colorChanged = true;
        }

        if (colorChanged) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
    }

    // ===================================================================
    // Size Over Lifetime
    // ===================================================================
    ImGui::Separator();
    ImGui::TextUnformatted("Size Over Lifetime");

    {
        bool sizeChanged = false;
        int removeIndex = -1;

        for (int i = 0; i < static_cast<int>(def->sizeKeyframes.size()); ++i) {
            ImGui::PushID(2000 + i);
            auto& kf = def->sizeKeyframes[static_cast<size_t>(i)];

            if (!beginInspectorPropertyTable(("SizeKf" + std::to_string(i)).c_str())) {
                ImGui::PopID();
                continue;
            }

            if (renderInspectorPropertyRow("Time", [&]() {
                    return ImGui::DragFloat("##value", &kf.first, 0.01f, 0.0f, 1.0f, "%.3f");
                })) {
                sizeChanged = true;
            }

            if (renderInspectorPropertyRow("Size", [&]() {
                    return ImGui::DragFloat("##value", &kf.second, 0.01f, 0.0f, 50.0f, "%.2f");
                })) {
                sizeChanged = true;
            }

            endInspectorPropertyTable();

            if (ImGui::SmallButton("Remove")) {
                removeIndex = i;
                sizeChanged = true;
            }
            ImGui::Separator();
            ImGui::PopID();
        }

        if (removeIndex >= 0) {
            def->sizeKeyframes.erase(def->sizeKeyframes.begin() + removeIndex);
        }

        if (ImGui::Button("Add Keyframe")) {
            float time = def->sizeKeyframes.empty() ? 0.0f : 1.0f;
            def->sizeKeyframes.emplace_back(time, 1.0f);
            sizeChanged = true;
        }

        if (sizeChanged) {
            document.markSceneDirty();
            saveDefinition(content, placement.emitterId);
        }
    }
}
