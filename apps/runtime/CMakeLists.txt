add_executable(pixel-roguelike main.cpp)
target_link_libraries(pixel-roguelike PRIVATE
    engine_scene
    gameplay
    game_module_door
)
configure_desktop_app(pixel-roguelike)
