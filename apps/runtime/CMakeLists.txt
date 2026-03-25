add_executable(pixel-roguelike ${CMAKE_SOURCE_DIR}/src/main.cpp)
target_include_directories(pixel-roguelike PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(pixel-roguelike PRIVATE
    engine_scene
    game
)

if(APPLE)
    target_link_libraries(pixel-roguelike PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
    )
    target_compile_definitions(pixel-roguelike PRIVATE GL_SILENCE_DEPRECATION)
endif()
