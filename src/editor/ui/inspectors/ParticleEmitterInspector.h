#pragma once

#include "editor/ui/LevelEditorUi.h"

struct LevelParticleEmitterPlacement;

void drawParticleEmitterInspector(LevelParticleEmitterPlacement& placement,
                                  EditorSceneDocument& document,
                                  EditorCommandStack& commandStack,
                                  EditorPendingCommand& pendingCommand,
                                  const EditorSceneDocumentState& beforeState);
