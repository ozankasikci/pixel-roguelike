add_library(editor STATIC
    assets/EditorAssetBrowser.cpp
    core/EditorCommand.cpp
    core/EditorLayoutPreset.cpp
    core/EditorRuntimePreviewSession.cpp
    core/LevelEditorCore.cpp
    render/EditorAssetPreviewRenderer.cpp
    render/EditorScenePreviewRenderer.cpp
    scene/EditorPreviewWorld.cpp
    scene/EditorSceneDocument.cpp
    scene/EditorSceneSerializer.cpp
    scene/EditorSelectionSystem.cpp
    ui/EditorAssetBrowserPanel.cpp
    ui/EditorEnvironmentPanel.cpp
    ui/EditorInspectorPanel.cpp
    ui/EditorOutlinerPanel.cpp
    ui/EditorPanelUtils.cpp
    viewport/EditorViewportController.cpp
    viewport/EditorViewportInteraction.cpp
    ${CMAKE_SOURCE_DIR}/external/ImGuizmo/ImGuizmo.cpp
)
target_include_directories(editor PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/ImGuizmo
)
target_link_libraries(editor PUBLIC game)
