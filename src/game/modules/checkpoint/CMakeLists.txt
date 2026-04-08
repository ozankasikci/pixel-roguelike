# Checkpoint module — self-contained feature module
add_library(game_module_checkpoint STATIC
    CheckpointModule.cpp
    CheckpointSpawner.cpp
    CheckpointSerializer.cpp
    CheckpointSystem.cpp
    CheckpointActionHandler.cpp
)
target_include_directories(game_module_checkpoint PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_checkpoint PUBLIC
    engine_core
    engine_rendering
    gameplay
)
