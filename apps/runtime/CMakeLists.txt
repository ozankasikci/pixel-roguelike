add_executable(pixel-roguelike main.cpp)
target_link_libraries(pixel-roguelike PRIVATE
    engine_scene
    gameplay
)
if(TARGET scripts_build)
    add_dependencies(pixel-roguelike scripts_build)
endif()
configure_desktop_app(pixel-roguelike)
