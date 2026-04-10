add_executable(level-editor main.cpp)
target_link_libraries(level-editor PRIVATE
    editor
    game_module_checkpoint
    game_module_door
    game_module_interaction
    game_module_inventory
    game_module_player_control
)
configure_desktop_app(level-editor)
