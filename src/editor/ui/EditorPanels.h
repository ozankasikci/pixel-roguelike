#pragma once

#include "editor/ui/LevelEditorUi.h"

class ContentRegistry;

bool renderEnvironmentPanel(EditorSceneDocument& document,
                            ContentRegistry& content,
                            std::vector<std::string>& environmentIds,
                            bool* open,
                            EditorPendingCommand& pendingCommand,
                            EditorCommandStack& commandStack);

AssetBrowserActionResult renderAssetBrowser(EditorUiState& ui,
                                            EditorPlacementState& placementState,
                                            EditorSceneDocument& document,
                                            const std::vector<std::uint64_t>& selectedIds,
                                            const ContentRegistry& content,
                                            const std::vector<std::string>& meshIds,
                                            const std::vector<std::string>& materialIds,
                                            const std::vector<std::string>& archetypeIds,
                                            bool* open,
                                            EditorCommandStack& commandStack);

InspectorActionResult renderInspector(EditorUiState& ui,
                                      EditorSceneDocument& document,
                                      const std::vector<std::uint64_t>& selectedIds,
                                      ContentRegistry& content,
                                      const std::vector<std::string>& meshIds,
                                      const std::vector<std::string>& materialIds,
                                      const std::vector<std::string>& archetypeIds,
                                      bool* open,
                                      EditorPendingCommand& pendingCommand,
                                      EditorCommandStack& commandStack);
