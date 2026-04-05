#pragma once

struct RuntimeSessionPerformanceStats;
struct SceneRenderPipelineStats;

void renderPerformancePanel(const RuntimeSessionPerformanceStats& perf,
                            const SceneRenderPipelineStats& pipeline,
                            bool isPreviewActive,
                            bool* open);
