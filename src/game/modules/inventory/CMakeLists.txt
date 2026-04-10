add_library(game_module_inventory STATIC
    InventorySystem.cpp
)
target_include_directories(game_module_inventory PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_module_inventory PUBLIC
    engine_input
    game_content
)
