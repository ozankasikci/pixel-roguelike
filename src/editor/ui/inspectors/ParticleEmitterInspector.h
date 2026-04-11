#pragma once

#include "editor/ui/LevelEditorUi.h"

class ContentRegistry;
struct LevelParticleEmitterPlacement;

void drawParticleEmitterInspector(LevelParticleEmitterPlacement& placement,
                                  EditorSceneDocument& document,
                                  ContentRegistry& content,
                                  EditorCommandStack& commandStack,
                                  EditorPendingCommand& pendingCommand,
                                  const EditorSceneDocumentState& beforeState);
