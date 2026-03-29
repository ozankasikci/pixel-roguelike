cmake_minimum_required(VERSION 3.28)
project(pixel-roguelike LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
include(${CMAKE_SOURCE_DIR}/cmake/DesktopApp.cmake)

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

# Assimp v6.0.4 - multi-format model importer (FBX, glTF, and more)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
if(APPLE AND EXISTS "/opt/homebrew/opt/zlib")
    list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/zlib")
    set(ZLIB_ROOT "/opt/homebrew/opt/zlib" CACHE PATH "" FORCE)
endif()
if(DEFINED BUILD_SHARED_LIBS)
    set(_pixel_roguelike_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
    set(_pixel_roguelike_had_build_shared_libs TRUE)
else()
    set(_pixel_roguelike_had_build_shared_libs FALSE)
endif()
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG        v6.0.4
    GIT_PROGRESS   TRUE
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(assimp)
if(_pixel_roguelike_had_build_shared_libs)
    set(BUILD_SHARED_LIBS "${_pixel_roguelike_saved_build_shared_libs}" CACHE BOOL "" FORCE)
else()
    unset(BUILD_SHARED_LIBS CACHE)
endif()

enable_testing()

add_subdirectory(src/engine)
add_subdirectory(src/game)
add_subdirectory(src/editor)
add_subdirectory(apps/runtime)
add_subdirectory(apps/model_viewer)
add_subdirectory(apps/level_editor)
add_subdirectory(tests)
add_subdirectory(tools)
