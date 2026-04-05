#pragma once

#include "editor/ui/LevelEditorUi.h"

#include <glm/vec3.hpp>
#include <string>
#include <vector>

struct BehaviorDeclaration;

// Behavior authoring sections — render on_activate/on_enter/on_exit/on_timer behavior lists.
// Modifies behaviors in-place; pushes undo commands via commandStack.
void drawBehaviorSections(std::vector<BehaviorDeclaration>& behaviors,
                          EditorSceneDocument& document,
                          EditorCommandStack& commandStack);

// Shared transform editing — position only.
// Used by LightInspector (conditionally), ArchetypeInspector, ReflectionProbeInspector,
// PlayerSpawnInspector.
void drawPositionSection(glm::vec3& position,
                         const char* label,
                         const char* undoLabel,
                         EditorSceneDocument& document,
                         EditorCommandStack& commandStack,
                         EditorPendingCommand& pendingCommand);

// Shared transform editing — position + rotation (no scale).
// Used by ColliderInspector.
void drawTransformSection(glm::vec3& position,
                          glm::vec3& rotation,
                          const char* posLabel,
                          const char* rotLabel,
                          const char* posUndoLabel,
                          const char* rotUndoLabel,
                          EditorSceneDocument& document,
                          EditorCommandStack& commandStack,
                          EditorPendingCommand& pendingCommand);

// Shared transform editing — position + scale + rotation (with scale clamp).
// Used by MeshInspector, GroupInspector.
void drawTransformSectionWithScale(glm::vec3& position,
                                   glm::vec3& rotation,
                                   glm::vec3& scale,
                                   const char* posLabel,
                                   const char* rotLabel,
                                   const char* scaleLabel,
                                   const char* posUndoLabel,
                                   const char* rotUndoLabel,
                                   const char* scaleUndoLabel,
                                   EditorSceneDocument& document,
                                   EditorCommandStack& commandStack,
                                   EditorPendingCommand& pendingCommand);
