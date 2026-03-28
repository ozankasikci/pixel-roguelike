add_executable(procedural-model-viewer main.cpp)
target_link_libraries(procedural-model-viewer PRIVATE
    game_content
    engine_ui
)
configure_desktop_app(procedural-model-viewer)
