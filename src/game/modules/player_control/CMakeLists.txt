add_library(game_module_player_control STATIC
    PlayerControlActionHandler.cpp
    PlayerControlCamera.cpp
    PlayerControlModule.cpp
    PlayerControlMovement.cpp
    PlayerControlSerializer.cpp
    PlayerControlSpawner.cpp
)
target_include_directories(game_module_player_control PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_player_control PUBLIC
    gameplay
)
