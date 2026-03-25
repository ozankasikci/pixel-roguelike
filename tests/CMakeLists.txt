add_executable(test_mesh_geometry test_mesh_geometry.cpp)
target_include_directories(test_mesh_geometry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_mesh_geometry PRIVATE glm::glm)
add_test(NAME mesh_geometry COMMAND test_mesh_geometry)

add_executable(test_cathedral_scene_data
    test_cathedral_scene_data.cpp
    ${CMAKE_SOURCE_DIR}/src/game/levels/cathedral/CathedralSceneData.cpp
)
target_include_directories(test_cathedral_scene_data PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_cathedral_scene_data PRIVATE glm::glm)
target_compile_definitions(test_cathedral_scene_data PRIVATE
    CATHEDRAL_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/cathedral.scene"
)
add_test(NAME cathedral_scene_data COMMAND test_cathedral_scene_data)
