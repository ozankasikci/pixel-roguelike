#include "editor/ui/LevelEditorUi.h"
#include "common/TestSupport.h"

#include <cassert>
#include <string>

int main() {
    assert(std::string(editorPreviewQualityLabel(EditorPreviewQuality::Fast)) == "Fast");
    assert(std::string(editorPreviewQualityLabel(EditorPreviewQuality::Balanced)) == "Balanced");
    assert(std::string(editorPreviewQualityLabel(EditorPreviewQuality::High)) == "High");

    const auto fast = editorPreviewQualitySettings(EditorPreviewQuality::Fast);
    const auto balanced = editorPreviewQualitySettings(EditorPreviewQuality::Balanced);
    const auto high = editorPreviewQualitySettings(EditorPreviewQuality::High);

    assert(fast.shadowResolutionIndex == 0);
    assert(balanced.shadowResolutionIndex == 1);
    assert(high.shadowResolutionIndex == 2);
    assert(fast.renderScale < balanced.renderScale);
    assert(balanced.renderScale < high.renderScale);
    assert(test_support::nearlyEqual(high.renderScale, 1.0f));

    EditorUiState ui;
    assert(ui.previewQuality == EditorPreviewQuality::Balanced);
    assert(test_support::nearlyEqual(ui.previewRenderScale, balanced.renderScale));
    assert(ui.shadowResolutionIndex == balanced.shadowResolutionIndex);

    applyEditorPreviewQuality(ui, EditorPreviewQuality::Fast);
    assert(ui.previewQuality == EditorPreviewQuality::Fast);
    assert(test_support::nearlyEqual(ui.previewRenderScale, fast.renderScale));
    assert(ui.shadowResolutionIndex == fast.shadowResolutionIndex);

    applyEditorPreviewQuality(ui, EditorPreviewQuality::High);
    assert(ui.previewQuality == EditorPreviewQuality::High);
    assert(test_support::nearlyEqual(ui.previewRenderScale, high.renderScale));
    assert(ui.shadowResolutionIndex == high.shadowResolutionIndex);

    return 0;
}
