add_executable(level-editor main.cpp)
target_link_libraries(level-editor PRIVATE
    editor
    game_module_door
)
configure_desktop_app(level-editor)
