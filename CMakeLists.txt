cmake_minimum_required(VERSION 3.28)
project(roguelike LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# GLAD 2 - OpenGL 4.1 Core Profile function loader
# Requires jinja2: python3 -m venv .venv && .venv/bin/pip install jinja2
# Configure with: cmake .. -DPython_EXECUTABLE=.venv/bin/python3
FetchContent_Declare(glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v2.0.8
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  cmake
)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl STATIC REPRODUCIBLE LOADER API gl:core=4.1)

# System packages via Homebrew
find_package(glfw3 3.4 REQUIRED)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)

add_executable(roguelike
    src/main.cpp
    src/engine/Window.cpp
    src/renderer/Shader.cpp
    src/renderer/Framebuffer.cpp
    src/renderer/DitherPass.cpp
    src/renderer/Mesh.cpp
    src/renderer/Renderer.cpp
    src/game/CathedralScene.cpp
)

target_include_directories(roguelike PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(roguelike PRIVATE
    glad_gl
    glfw
    glm::glm
    spdlog::spdlog
)

if(APPLE)
    target_link_libraries(roguelike PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
    )
    target_compile_definitions(roguelike PRIVATE GL_SILENCE_DEPRECATION)
endif()
