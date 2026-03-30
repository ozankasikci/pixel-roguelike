---
plan: 260330-wc2
status: complete
tasks_completed: 1
tasks_total: 1
---

## Summary

Reduced asset browser scroll speed from ImGui's default 5 lines per mouse wheel notch to 2 lines. Applied `ImGuiWindowFlags_NoScrollWithMouse` to disable the default and re-implemented with a custom step using `ImGui::GetIO().MouseWheel`.

## Key Changes

- `src/editor/ui/EditorAssetBrowserPanel.cpp`: Added `ImGuiWindowFlags_NoScrollWithMouse` flag and custom scroll handler with 2x font_size step
