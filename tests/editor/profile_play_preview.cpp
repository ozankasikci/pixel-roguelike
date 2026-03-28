#include "editor/scene/EditorSceneDocument.h"
#include "editor/core/EditorRuntimePreviewSession.h"
#include "engine/core/Window.h"
#include "game/content/ContentRegistry.h"

#include <chrono>
#include <cstdio>

namespace {

double elapsedMs(const std::chrono::steady_clock::time_point& start,
                 const std::chrono::steady_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main() {
    using Clock = std::chrono::steady_clock;

    const auto windowStart = Clock::now();
    Window window(640, 360, "profile_play_preview");
    const auto windowEnd = Clock::now();

    const auto contentStart = Clock::now();
    ContentRegistry content;
    content.loadDefaults();
    const auto contentEnd = Clock::now();

    const auto documentStart = Clock::now();
    EditorSceneDocument document;
    document.loadFromSceneFile(SILOS_CLOISTER_SCENE_FILE, content);
    const auto documentEnd = Clock::now();

    const auto sessionConstructStart = Clock::now();
    EditorRuntimePreviewSession preview;
    const auto sessionConstructEnd = Clock::now();

    const auto rebuildStart = Clock::now();
    preview.rebuild(document, content);
    const auto rebuildEnd = Clock::now();

    const auto prewarmStart = Clock::now();
    preview.prewarmRenderer(content);
    const auto prewarmEnd = Clock::now();

    const auto firstRenderStart = Clock::now();
    preview.render(1.0f / 60.0f, 1280, 720, 1280, 720, 0);
    const auto firstRenderEnd = Clock::now();

    const auto secondRenderStart = Clock::now();
    preview.render(1.0f / 60.0f, 1280, 720, 1280, 720, 0);
    const auto secondRenderEnd = Clock::now();

    std::printf("window_init_ms=%.2f\n", elapsedMs(windowStart, windowEnd));
    std::printf("content_load_ms=%.2f\n", elapsedMs(contentStart, contentEnd));
    std::printf("document_load_ms=%.2f\n", elapsedMs(documentStart, documentEnd));
    std::printf("preview_construct_ms=%.2f\n", elapsedMs(sessionConstructStart, sessionConstructEnd));
    std::printf("preview_rebuild_ms=%.2f\n", elapsedMs(rebuildStart, rebuildEnd));
    std::printf("preview_prewarm_renderer_ms=%.2f\n", elapsedMs(prewarmStart, prewarmEnd));
    std::printf("preview_first_render_ms=%.2f\n", elapsedMs(firstRenderStart, firstRenderEnd));
    std::printf("preview_second_render_ms=%.2f\n", elapsedMs(secondRenderStart, secondRenderEnd));
    return 0;
}
