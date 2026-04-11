#include "editor/ui/inspectors/ParticleEmitterInspector.h"

#include "editor/ui/inspectors/InspectorUtils.h"
#include "editor/ui/LevelEditorUi.h"
#include "game/level/LevelDef.h"

#include <imgui.h>

void drawParticleEmitterInspector(LevelParticleEmitterPlacement& placement,
                                  EditorSceneDocument& document,
                                  EditorCommandStack& commandStack,
                                  EditorPendingCommand& pendingCommand,
                                  const EditorSceneDocumentState& beforeState) {
    (void)beforeState;

    const auto trackSceneItem = [&](const EditorSceneDocumentState& itemBefore, const std::string& label, bool changed) {
        if (changed) {
            document.markSceneDirty();
        }
        trackLastItemCommand(itemBefore, label, pendingCommand, commandStack, document);
    };

    auto itemBefore = document.captureState();
    trackSceneItem(itemBefore, "Change Emitter Id", renderInspectorPropertyRow("Emitter Id", [&]() { return editString("##value", placement.emitterId, "emitter id"); }, EditorInspectorFieldKind::Text));
    drawPositionSection(placement.position, "Position", "Move Particle Emitter",
                        document, commandStack, pendingCommand);
    endInspectorPropertyTable();
}
