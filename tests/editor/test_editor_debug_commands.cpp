#include "editor/core/EditorCommand.h"
#include "editor/debug/EditorCommander.h"
#include "editor/debug/EditorInspector.h"
#include "editor/scene/EditorSceneDocument.h"
#include "editor/ui/LevelEditorUi.h"
#include "common/TestSupport.h"

#include <cassert>
#include <string>
#include <vector>

namespace {

LevelMeshPlacement makeMeshPlacement() {
    LevelMeshPlacement placement;
    placement.meshId = "cube";
    placement.materialId = "stone_default";
    placement.position = glm::vec3(0.0f);
    placement.scale = glm::vec3(1.0f);
    placement.rotation = glm::vec3(0.0f);
    return placement;
}

} // namespace

int main() {
    // Test setup
    EditorSceneDocument doc;
    doc.clear();
    const std::uint64_t meshObjId = doc.addMesh(makeMeshPlacement());

    std::vector<std::uint64_t> selectedIds;
    EditorUiState ui;
    EditorCommandStack cmdStack;
    cmdStack.reset(doc);
    EditorCamera camera;
    EditorCameraAnimation cameraAnim;
    EditorViewportState viewport;
    std::string screenshotRequestPath;

    EditorCommander commander(doc, selectedIds, ui, cmdStack, nullptr, camera, cameraAnim, viewport, nullptr, nullptr, &screenshotRequestPath);

    // ---- EditorCommander tests ----

    // Test 1: selectEntity by label finds the object.
    {
        const EditorSceneObject* obj = doc.findObject(meshObjId);
        assert(obj != nullptr);
        const std::string label = editorSceneObjectLabel(*obj);

        nlohmann::json result = commander.selectEntity({{"name", label}});
        assert(result.value("ok", false) == true);
        assert(selectedIds.size() == 1);
        assert(selectedIds[0] == meshObjId);
    }

    // Test 2: selectEntity with unknown name returns error.
    {
        selectedIds.clear();
        nlohmann::json result = commander.selectEntity({{"name", "nonexistent_entity_xyz"}});
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
        const std::string err = result["error"].get<std::string>();
        assert(err.find("not found") != std::string::npos);
    }

    // Test 3: selectEntity with missing name arg returns error.
    {
        nlohmann::json result = commander.selectEntity(nlohmann::json::object());
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
    }

    // Test 4: deselectAll clears selectedIds.
    {
        selectedIds = {meshObjId, 999u};
        nlohmann::json result = commander.deselectAll(nlohmann::json::object());
        assert(result.value("ok", false) == true);
        assert(selectedIds.empty());
    }

    // Test 5: setGizmo Translate sets tool to Translate.
    {
        nlohmann::json result = commander.setGizmo({{"mode", "Translate"}});
        assert(result.value("ok", false) == true);
        assert(ui.tool == EditorTransformTool::Translate);
    }

    // Test 6: setGizmo Rotate sets tool to Rotate.
    {
        nlohmann::json result = commander.setGizmo({{"mode", "Rotate"}});
        assert(result.value("ok", false) == true);
        assert(ui.tool == EditorTransformTool::Rotate);
    }

    // Test 7: setGizmo Scale sets tool to Scale.
    {
        nlohmann::json result = commander.setGizmo({{"mode", "Scale"}});
        assert(result.value("ok", false) == true);
        assert(ui.tool == EditorTransformTool::Scale);
    }

    // Test 8: setGizmo with invalid mode returns error.
    {
        nlohmann::json result = commander.setGizmo({{"mode", "Invalid"}});
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
    }

    // Test 9: togglePanel outliner toggles showOutliner.
    {
        ui.showOutliner = true;
        nlohmann::json r1 = commander.togglePanel({{"panel", "outliner"}});
        assert(r1.value("ok", false) == true);
        assert(!ui.showOutliner);
        nlohmann::json r2 = commander.togglePanel({{"panel", "outliner"}});
        assert(r2.value("ok", false) == true);
        assert(ui.showOutliner);
    }

    // Test 10: togglePanel inspector toggles showInspector.
    {
        ui.showInspector = true;
        commander.togglePanel({{"panel", "inspector"}});
        assert(!ui.showInspector);
        commander.togglePanel({{"panel", "inspector"}});
        assert(ui.showInspector);
    }

    // Test 11: togglePanel asset_browser toggles showAssetBrowser.
    {
        ui.showAssetBrowser = true;
        commander.togglePanel({{"panel", "asset_browser"}});
        assert(!ui.showAssetBrowser);
        commander.togglePanel({{"panel", "asset_browser"}});
        assert(ui.showAssetBrowser);
    }

    // Test 12: togglePanel environment toggles showEnvironment.
    {
        ui.showEnvironment = true;
        commander.togglePanel({{"panel", "environment"}});
        assert(!ui.showEnvironment);
        commander.togglePanel({{"panel", "environment"}});
        assert(ui.showEnvironment);
    }

    // Test 13: togglePanel viewport toggles showViewport.
    {
        ui.showViewport = true;
        commander.togglePanel({{"panel", "viewport"}});
        assert(!ui.showViewport);
        commander.togglePanel({{"panel", "viewport"}});
        assert(ui.showViewport);
    }

    // Test 14: togglePanel build_output toggles showBuildOutput.
    {
        ui.showBuildOutput = false;
        commander.togglePanel({{"panel", "build_output"}});
        assert(ui.showBuildOutput);
        commander.togglePanel({{"panel", "build_output"}});
        assert(!ui.showBuildOutput);
    }

    // Test 15: togglePanel with unknown panel returns error.
    {
        nlohmann::json result = commander.togglePanel({{"panel", "unknown_panel"}});
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
    }

    // Test 15a: togglePlayPreview queues a play preview toggle.
    {
        ui.playPreview = false;
        ui.playPreviewToggleRequested = false;
        nlohmann::json result = commander.togglePlayPreview(nlohmann::json::object());
        assert(result.value("ok", false) == true);
        assert(ui.playPreviewToggleRequested);
        assert(result.value("currently_active", true) == false);
    }

    // Test 15b: setPreviewMode updates the editor preview mode.
    {
        nlohmann::json result = commander.setPreviewMode({{"mode", "sun_shadow"}});
        assert(result.value("ok", false) == true);
        assert(ui.previewMode == EditorPreviewMode::SunShadowVisibility);
        assert(result.value("mode", "") == "sun_shadow");
    }

    // Test 15c: setPreviewMode rejects unknown values.
    {
        nlohmann::json result = commander.setPreviewMode({{"mode", "unknown_mode"}});
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
    }

    // Test 15d: setRuntimeCamera reports unavailable without a runtime preview session.
    {
        nlohmann::json result = commander.setRuntimeCamera({{"x", 1.0f}});
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
    }

    // Test 16: undo with nothing to undo returns ok=false.
    {
        // Fresh stack with no commands
        EditorSceneDocument freshDoc;
        freshDoc.clear();
        freshDoc.addMesh(makeMeshPlacement());
        std::vector<std::uint64_t> freshSel;
        EditorUiState freshUi;
        EditorCommandStack freshStack;
        freshStack.reset(freshDoc);
        EditorCamera freshCamera;
        EditorCameraAnimation freshCameraAnim;
        EditorViewportState freshViewport;
        std::string freshScreenshotRequestPath;
        EditorCommander freshCmd(freshDoc, freshSel, freshUi, freshStack, nullptr, freshCamera, freshCameraAnim, freshViewport, nullptr, nullptr, &freshScreenshotRequestPath);

        nlohmann::json result = freshCmd.undo(nlohmann::json::object());
        assert(result.value("ok", true) == false);
        assert(result.contains("error"));
    }

    // Test 17: undo/redo roundtrip through EditorCommander.
    {
        EditorSceneDocument roundDoc;
        roundDoc.clear();
        const std::uint64_t rId = roundDoc.addMesh(makeMeshPlacement());
        std::vector<std::uint64_t> roundSel;
        EditorUiState roundUi;
        EditorCommandStack roundStack;
        roundStack.reset(roundDoc);
        EditorCamera roundCamera;
        EditorCameraAnimation roundCameraAnim;
        EditorViewportState roundViewport;
        std::string roundScreenshotRequestPath;
        EditorCommander roundCmd(roundDoc, roundSel, roundUi, roundStack, nullptr, roundCamera, roundCameraAnim, roundViewport, nullptr, nullptr, &roundScreenshotRequestPath);

        // Push a document state change
        const EditorSceneDocumentState before = roundDoc.captureState();
        EditorSceneObject* obj = roundDoc.findObject(rId);
        assert(obj != nullptr);
        std::get<LevelMeshPlacement>(obj->payload).position.x = 5.0f;
        roundDoc.markSceneDirty();
        roundStack.pushDocumentStateCommand("Move Mesh", before, roundDoc.captureState(), roundDoc);

        // Verify position was changed
        assert(test_support::nearlyEqual(
            std::get<LevelMeshPlacement>(roundDoc.findObject(rId)->payload).position.x, 5.0f));

        // Undo via commander
        nlohmann::json undoResult = roundCmd.undo(nlohmann::json::object());
        assert(undoResult.value("ok", false) == true);
        assert(test_support::nearlyEqual(
            std::get<LevelMeshPlacement>(roundDoc.findObject(rId)->payload).position.x, 0.0f));

        // Redo via commander
        nlohmann::json redoResult = roundCmd.redo(nlohmann::json::object());
        assert(redoResult.value("ok", false) == true);
        assert(test_support::nearlyEqual(
            std::get<LevelMeshPlacement>(roundDoc.findObject(rId)->payload).position.x, 5.0f));
    }

    // Test 18: captureScreenshot queues a requested path.
    {
        screenshotRequestPath.clear();
        nlohmann::json result = commander.captureScreenshot({{"path", "/tmp/debug-harness-shot.png"}});
        assert(result.value("ok", false) == true);
        assert(result.value("queued", false) == true);
        assert(screenshotRequestPath == "/tmp/debug-harness-shot.png");
    }

    // ---- EditorInspector tests ----

    EditorInspector inspector(doc, selectedIds, ui, cmdStack, camera, viewport, nullptr, nullptr);

    // Test 19: undoStack with empty cmdStack returns ok=true, can_undo=false, can_redo=false.
    {
        // Reset to fresh state with no commands
        EditorSceneDocument inspDoc;
        inspDoc.clear();
        inspDoc.addMesh(makeMeshPlacement());
        std::vector<std::uint64_t> inspSel;
        EditorUiState inspUi;
        EditorCommandStack inspStack;
        inspStack.reset(inspDoc);
        EditorCamera inspCamera;
        EditorViewportState inspViewport;
        EditorInspector insp(inspDoc, inspSel, inspUi, inspStack, inspCamera, inspViewport, nullptr);

        nlohmann::json result = insp.undoStack();
        assert(result.value("ok", false) == true);
        assert(result.contains("data"));
        assert(result["data"].value("can_undo", true) == false);
        assert(result["data"].value("can_redo", true) == false);

        // Push a command — now can_undo should be true
        const EditorSceneDocumentState before = inspDoc.captureState();
        EditorSceneObject* obj = inspDoc.findObject(inspDoc.objects()[0].id);
        assert(obj != nullptr);
        std::get<LevelMeshPlacement>(obj->payload).position.y = 3.0f;
        inspDoc.markSceneDirty();
        inspStack.pushDocumentStateCommand("Move Y", before, inspDoc.captureState(), inspDoc);

        nlohmann::json result2 = insp.undoStack();
        assert(result2.value("ok", false) == true);
        assert(result2["data"].value("can_undo", false) == true);
    }

    // Test 20: panels returns ok=true and data matches ui field values.
    {
        EditorSceneDocument panelDoc;
        panelDoc.clear();
        std::vector<std::uint64_t> panelSel;
        EditorUiState panelUi;
        panelUi.showOutliner = true;
        panelUi.showInspector = false;
        panelUi.showAssetBrowser = true;
        panelUi.showEnvironment = false;
        panelUi.showViewport = true;
        panelUi.showBuildOutput = false;
        EditorCommandStack panelStack;
        panelStack.reset(panelDoc);
        EditorCamera panelCamera;
        EditorViewportState panelViewport;
        EditorInspector panelInsp(panelDoc, panelSel, panelUi, panelStack, panelCamera, panelViewport, nullptr);

        nlohmann::json result = panelInsp.panels();
        assert(result.value("ok", false) == true);
        assert(result.contains("data"));
        assert(result["data"].value("outliner", false) == true);
        assert(result["data"].value("inspector", true) == false);
        assert(result["data"].value("asset_browser", false) == true);
        assert(result["data"].value("environment", true) == false);
        assert(result["data"].value("viewport", false) == true);
        assert(result["data"].value("build_output", true) == false);
    }

    return 0;
}
