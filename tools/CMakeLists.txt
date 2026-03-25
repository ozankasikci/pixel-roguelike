add_executable(mesh_generator mesh_generator.cpp)
target_include_directories(mesh_generator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(mesh_generator PRIVATE glm::glm tinygltf)
