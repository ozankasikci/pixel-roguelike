#include "editor/ui/EditorPerformancePanel.h"

#include "editor/ui/LevelEditorUi.h"
#include "engine/rendering/SceneRenderPipeline.h"
#include "game/runtime/RuntimeGameSession.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace {

void renderTimingRow(const char* label, double valueMs, double maxMs) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", valueMs);
    const float textWidth = ImGui::CalcTextSize(buf).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - textWidth);
    ImGui::TextUnformatted(buf);

    ImGui::TableSetColumnIndex(2);
    const float barMaxWidth = ImGui::GetContentRegionAvail().x - 2.0f;
    const float fraction = (maxMs > 0.0) ? static_cast<float>(std::min(valueMs / maxMs, 1.0)) : 0.0f;
    const float barWidth = barMaxWidth * fraction;

    if (barWidth > 0.0f) {
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float height = ImGui::GetTextLineHeight() * 0.75f;
        const float yOffset = (ImGui::GetTextLineHeightWithSpacing() - height) * 0.5f;
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImVec2(pos.x, pos.y + yOffset),
            ImVec2(pos.x + barWidth, pos.y + yOffset + height),
            IM_COL32(220, 170, 60, 200));
    }
    ImGui::Dummy(ImVec2(barMaxWidth, 0.0f));
}

void beginTimingTable(const char* id) {
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2.0f, 1.0f));
    ImGui::BeginTable(id, 3,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoHostExtendX);
    ImGui::TableSetupColumn("System", ImGuiTableColumnFlags_WidthStretch, 0.38f);
    ImGui::TableSetupColumn("ms",     ImGuiTableColumnFlags_WidthStretch, 0.22f);
    ImGui::TableSetupColumn("Bar",    ImGuiTableColumnFlags_WidthStretch, 0.40f);
}

void endTimingTable() {
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

double computeAutoScale(std::initializer_list<double> values) {
    double maxVal = 0.0;
    for (double v : values) {
        if (v > maxVal) maxVal = v;
    }
    return maxVal > 2.0 ? maxVal * 1.1 : 2.0;
}

} // namespace

void renderPerformancePanel(const RuntimeSessionPerformanceStats& perf,
                            const SceneRenderPipelineStats& pipeline,
                            bool isPreviewActive,
                            bool* open) {
    if (open != nullptr && !*open) {
        return;
    }
    if (!beginCompactEditorPanelWindow("Performance", open)) {
        ImGui::End();
        return;
    }

    if (!isPreviewActive) {
        const ImVec2 size = ImGui::GetContentRegionAvail();
        const char* msg = "Enter Play Preview to see system timings";
        const float textWidth = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (size.x - textWidth) * 0.5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + size.y * 0.4f);
        ImGui::TextDisabled("%s", msg);
        ImGui::End();
        return;
    }

    // --- Gameplay Systems ---
    ImGui::SeparatorText("Gameplay Systems");
    const double tickScale = computeAutoScale({
        perf.interactionMs, perf.checkpointsMs, perf.physicsMs,
        perf.inventoryMs, perf.movementMs, perf.cameraMs});

    beginTimingTable("##gameplay_timing");
    renderTimingRow("Interaction",  perf.interactionMs,  tickScale);
    renderTimingRow("Checkpoints",  perf.checkpointsMs,  tickScale);
    renderTimingRow("Physics",      perf.physicsMs,      tickScale);
    renderTimingRow("Inventory",    perf.inventoryMs,    tickScale);
    renderTimingRow("Movement",     perf.movementMs,     tickScale);
    renderTimingRow("Camera",       perf.cameraMs,       tickScale);
    endTimingTable();

    ImGui::Spacing();
    ImGui::Text("Total Tick: %.2f ms", perf.totalTickMs);

    // --- Render Pipeline ---
    ImGui::Spacing();
    ImGui::SeparatorText("Render Pipeline");
    const double renderScale = computeAutoScale({
        pipeline.shadowPassMs, pipeline.scenePassMs,
        pipeline.ssaoMs, pipeline.bloomMs, pipeline.compositeMs});

    beginTimingTable("##render_timing");
    renderTimingRow("Shadow",    pipeline.shadowPassMs, renderScale);
    renderTimingRow("Scene",     pipeline.scenePassMs,  renderScale);
    renderTimingRow("SSAO",      pipeline.ssaoMs,       renderScale);
    renderTimingRow("Bloom",     pipeline.bloomMs,      renderScale);
    renderTimingRow("Composite", pipeline.compositeMs,  renderScale);
    endTimingTable();

    ImGui::Spacing();
    ImGui::Text("Total Render: %.2f ms", pipeline.totalRenderMs);
    ImGui::Spacing();
    ImGui::Text("Draws: %d  Objects: %d  Lights: %d  Culled: %d",
        pipeline.drawCalls, pipeline.objectCount, pipeline.lightCount, pipeline.culledCount);

    // --- Frame Summary ---
    ImGui::Spacing();
    ImGui::SeparatorText("Frame");
    const float fps = ImGui::GetIO().Framerate;
    const double frameMs = (fps > 0.0f) ? 1000.0 / fps : 0.0;
    const double otherMs = frameMs - perf.totalTickMs - pipeline.totalRenderMs;

    ImGui::Text("%.1f FPS  (%.2f ms/frame)", fps, frameMs);
    ImGui::Spacing();
    ImGui::Text("Tick: %.2f + Render: %.2f + Other: %.2f = Total: %.2f ms",
        perf.totalTickMs, pipeline.totalRenderMs,
        std::max(otherMs, 0.0), frameMs);

    ImGui::End();
}
