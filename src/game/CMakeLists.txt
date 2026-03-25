add_library(game STATIC
    scenes/CathedralScene.cpp
    systems/CameraSystem.cpp
    systems/CheckpointSystem.cpp
    systems/DoorSystem.cpp
    systems/PlayerMovementSystem.cpp
    systems/RenderSystem.cpp
    levels/cathedral/CathedralAssets.cpp
    levels/cathedral/CathedralSceneData.cpp
    levels/cathedral/CathedralLayout.cpp
)
target_include_directories(game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game PUBLIC
    engine_core
    engine_rendering
    engine_ui
    engine_input
    engine_physics
    EnTT::EnTT
)
