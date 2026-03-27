add_executable(procedural-model-viewer main.cpp)
target_include_directories(procedural-model-viewer PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(procedural-model-viewer PRIVATE
    engine_core
    engine_rendering
    engine_ui
    game
)

if(APPLE)
    target_link_libraries(procedural-model-viewer PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
    )
    target_compile_definitions(procedural-model-viewer PRIVATE GL_SILENCE_DEPRECATION)
endif()
