add_executable(pixel-roguelike main.cpp)
target_link_libraries(pixel-roguelike PRIVATE
    engine_scene
    gameplay
    game_module_checkpoint
    game_module_door
    game_module_interaction
    game_module_inventory
    game_module_particles
    game_module_player_control
)
configure_desktop_app(pixel-roguelike)
