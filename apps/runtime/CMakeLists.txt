add_executable(pixel-roguelike main.cpp)
target_link_libraries(pixel-roguelike PRIVATE
    engine_scene
    gameplay
)
configure_desktop_app(pixel-roguelike)
