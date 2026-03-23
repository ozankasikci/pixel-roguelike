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

# Dear ImGui v1.92.6 via FetchContent
# Uses EXCLUDE_FROM_ALL to avoid adding ImGui demo targets to the default build
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.6
    GIT_PROGRESS   TRUE
    EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(imgui)
# imgui_SOURCE_DIR is set after MakeAvailable

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PRIVATE glfw)

# System packages via Homebrew
find_package(glfw3 3.4 REQUIRED)
find_package(glm REQUIRED)
find_package(spdlog REQUIRED)

add_executable(roguelike
    src/main.cpp
    src/engine/Window.cpp
    src/engine/Screenshot.cpp
    src/renderer/Shader.cpp
    src/renderer/Framebuffer.cpp
    src/renderer/DitherPass.cpp
    src/renderer/Mesh.cpp
    src/renderer/Renderer.cpp
    src/game/CathedralScene.cpp
    src/engine/ImGuiLayer.cpp
)

target_include_directories(roguelike PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(roguelike PRIVATE
    glad_gl
    glfw
    glm::glm
    spdlog::spdlog
    imgui
)

if(APPLE)
    target_link_libraries(roguelike PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
    )
    target_compile_definitions(roguelike PRIVATE GL_SILENCE_DEPRECATION)
endif()
