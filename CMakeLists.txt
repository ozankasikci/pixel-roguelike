cmake_minimum_required(VERSION 3.28)
project(pixel-roguelike LANGUAGES C CXX)

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

# EnTT v3.16.0 - Entity Component System
FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.16.0
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(entt)

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

# --- Per-module static libraries ---

add_library(engine_core STATIC
    src/engine/core/Window.cpp
    src/engine/core/Application.cpp
    src/engine/core/Time.cpp
)
target_include_directories(engine_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_core PUBLIC glad_gl glfw spdlog::spdlog EnTT::EnTT)

add_library(engine_rendering STATIC
    src/engine/rendering/Shader.cpp
    src/engine/rendering/Framebuffer.cpp
    src/engine/rendering/DitherPass.cpp
    src/engine/rendering/Mesh.cpp
    src/engine/rendering/Renderer.cpp
)
target_include_directories(engine_rendering PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_rendering PUBLIC engine_core glm::glm)

add_library(engine_ui STATIC
    src/engine/ui/ImGuiLayer.cpp
    src/engine/ui/Screenshot.cpp
)
target_include_directories(engine_ui PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
)
target_link_libraries(engine_ui PUBLIC engine_rendering imgui)

add_library(engine_scene STATIC
    src/engine/scene/SceneManager.cpp
)
target_include_directories(engine_scene PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_scene PUBLIC engine_core)

add_library(engine_input STATIC
    src/engine/input/InputSystem.cpp
)
target_include_directories(engine_input PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_input PUBLIC engine_core imgui)

add_library(game STATIC
    src/game/scenes/CathedralScene.cpp
    src/game/systems/CameraSystem.cpp
    src/game/systems/RenderSystem.cpp
)
target_include_directories(game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game PUBLIC engine_core engine_rendering engine_ui engine_input EnTT::EnTT)

add_executable(pixel-roguelike src/main.cpp)
target_include_directories(pixel-roguelike PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(pixel-roguelike PRIVATE
    engine_core
    engine_rendering
    engine_ui
    engine_scene
    engine_input
    game
    EnTT::EnTT
)

if(APPLE)
    target_link_libraries(pixel-roguelike PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework IOKit"
    )
    target_compile_definitions(pixel-roguelike PRIVATE GL_SILENCE_DEPRECATION)
endif()
