add_executable(level-editor
    main.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorAssetBrowser.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorAssetPreviewRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorCommand.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorLayoutPreset.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorPreviewWorld.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorSceneDocument.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorSceneSerializer.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorSelectionSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/EditorViewportController.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/core/LevelEditorCore.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/core/EditorRuntimePreviewSession.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/render/EditorScenePreviewRenderer.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ui/EditorAssetBrowserPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ui/EditorOutlinerPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ui/EditorEnvironmentPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ui/EditorInspectorPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/ui/EditorPanelUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/viewport/EditorViewportInteraction.cpp
    ${CMAKE_SOURCE_DIR}/external/ImGuizmo/ImGuizmo.cpp
)
target_include_directories(level-editor PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/ImGuizmo
)
target_link_libraries(level-editor PRIVATE
    engine_core
    engine_rendering
    engine_ui
    game
)

if(APPLE)
    target_link_libraries(level-editor PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
    )
    target_compile_definitions(level-editor PRIVATE GL_SILENCE_DEPRECATION)
endif()
