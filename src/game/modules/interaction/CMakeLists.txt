add_library(game_module_interaction STATIC
    InteractionSystem.cpp
)
target_include_directories(game_module_interaction PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_interaction PUBLIC
    engine_input
)
