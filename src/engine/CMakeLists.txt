add_library(engine_core STATIC
    core/Window.cpp
    core/Application.cpp
    core/PathUtils.cpp
    core/Time.cpp
)
target_include_directories(engine_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_core PUBLIC glad_gl glfw spdlog::spdlog EnTT::EnTT)

add_library(engine_rendering STATIC
    rendering/core/Shader.cpp
    rendering/core/Framebuffer.cpp
    rendering/lighting/ShadowMap.cpp
    rendering/post/CompositePass.cpp
    rendering/post/SkyTextureLibrary.cpp
    rendering/geometry/Mesh.cpp
    rendering/geometry/MeshLibrary.cpp
    rendering/geometry/Renderer.cpp
    rendering/post/StylizePass.cpp
    rendering/assets/GltfLoader.cpp
    rendering/assets/Texture2D.cpp
    rendering/assets/TextureCube.cpp
)
target_include_directories(engine_rendering PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_rendering PUBLIC engine_core glm::glm)
target_link_libraries(engine_rendering PRIVATE tinygltf)

add_library(engine_ui STATIC
    ui/ImGuiLayer.cpp
    ui/Screenshot.cpp
)
target_include_directories(engine_ui PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external
)
target_link_libraries(engine_ui PUBLIC engine_rendering imgui)

add_library(engine_scene STATIC
    scene/SceneManager.cpp
)
target_include_directories(engine_scene PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_scene PUBLIC engine_core)

add_library(engine_input STATIC
    input/InputSystem.cpp
)
target_include_directories(engine_input PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_input PUBLIC engine_core imgui glm::glm)

add_library(engine_physics STATIC
    physics/PhysicsSystem.cpp
)
target_include_directories(engine_physics PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_physics PUBLIC engine_core glm::glm)
target_link_libraries(engine_physics PRIVATE Jolt)
