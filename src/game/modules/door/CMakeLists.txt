# Door module — self-contained feature module (per D-03)
add_library(game_module_door STATIC
    DoorAnimationSystem.cpp
    DoorMath.cpp
    DoorSpawner.cpp
    DoorSerializer.cpp
    DoorActionHandler.cpp
    DoorModule.cpp
)
target_include_directories(game_module_door PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_door PUBLIC
    engine_core
    engine_rendering
    engine_physics
    gameplay
)
