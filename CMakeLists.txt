cmake_minimum_required(VERSION 3.28)
project(pixel-roguelike LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
include(${CMAKE_SOURCE_DIR}/cmake/DesktopApp.cmake)

find_program(NPM_EXECUTABLE npm)
if(NPM_EXECUTABLE)
    add_custom_target(scripts_build
        COMMAND ${NPM_EXECUTABLE} run build
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tools/script_pipeline
        COMMENT "Building TypeScript gameplay scripts"
        VERBATIM
    )
    add_custom_target(scripts_typecheck
        COMMAND ${NPM_EXECUTABLE} run typecheck
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tools/script_pipeline
        COMMENT "Typechecking TypeScript gameplay scripts"
        VERBATIM
    )
else()
    message(WARNING "npm was not found; scripts_build and scripts_typecheck targets will be unavailable.")
endif()

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

# Jolt Physics v5.4.0 - Rigid body physics, collision, character controller
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        v5.4.0
    GIT_PROGRESS   TRUE
    SOURCE_SUBDIR  Build
)
FetchContent_MakeAvailable(JoltPhysics)

# Dear ImGui v1.92.6 via FetchContent
# Uses EXCLUDE_FROM_ALL to avoid adding ImGui demo targets to the default build
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.6-docking
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

# tinygltf - header-only glTF 2.0 loader/writer
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_GL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF CACHE BOOL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG        v2.9.3
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(tinygltf)

# ufbx - lightweight FBX loader
FetchContent_Declare(ufbx
    GIT_REPOSITORY https://github.com/ufbx/ufbx.git
    GIT_TAG        v0.21.3
    GIT_PROGRESS   TRUE
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(ufbx)
if(NOT ufbx_POPULATED)
    FetchContent_Populate(ufbx)
endif()
add_library(ufbx STATIC
    ${ufbx_SOURCE_DIR}/ufbx.c
)
target_include_directories(ufbx PUBLIC ${ufbx_SOURCE_DIR})

FetchContent_Declare(quickjs
    URL https://bellard.org/quickjs/quickjs-2025-09-13-2.tar.xz
)
FetchContent_GetProperties(quickjs)
if(NOT quickjs_POPULATED)
    FetchContent_Populate(quickjs)
endif()
file(STRINGS "${quickjs_SOURCE_DIR}/VERSION" QUICKJS_VERSION LIMIT_COUNT 1)
add_library(quickjs STATIC
    ${quickjs_SOURCE_DIR}/quickjs.c
    ${quickjs_SOURCE_DIR}/libregexp.c
    ${quickjs_SOURCE_DIR}/libunicode.c
    ${quickjs_SOURCE_DIR}/cutils.c
    ${quickjs_SOURCE_DIR}/dtoa.c
)
target_include_directories(quickjs PRIVATE ${quickjs_SOURCE_DIR})
target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION="${QUICKJS_VERSION}"
)

enable_testing()

add_subdirectory(src/engine)
add_subdirectory(src/game)
add_subdirectory(src/editor)
add_subdirectory(apps/runtime)
add_subdirectory(apps/model_viewer)
add_subdirectory(apps/level_editor)
add_subdirectory(tests)
add_subdirectory(tools)
