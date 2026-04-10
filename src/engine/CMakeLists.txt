add_library(engine_core STATIC
    core/Window.cpp
    core/Application.cpp
    core/PathUtils.cpp
    core/ProjectConfig.cpp
    core/Time.cpp
    ecs/EntityActiveState.cpp
)
target_include_directories(engine_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_core PUBLIC glad_gl glfw spdlog::spdlog EnTT::EnTT)

add_library(engine_rendering STATIC
    rendering/core/Shader.cpp
    rendering/core/Framebuffer.cpp
    rendering/lighting/ShadowMap.cpp
    rendering/lighting/CascadedShadowMap.cpp
    rendering/lighting/LtcData.cpp
    rendering/lighting/ReflectionProbeRenderer.cpp
    rendering/post/BloomPass.cpp
    rendering/post/CompositePass.cpp
    rendering/post/SkyTextureLibrary.cpp
    rendering/post/SsaoPass.cpp
    rendering/geometry/Mesh.cpp
    rendering/geometry/MeshLibrary.cpp
    rendering/geometry/Renderer.cpp
    rendering/post/StylizePass.cpp
    rendering/assets/AssimpLoader.cpp
    rendering/assets/GltfLoader.cpp
    rendering/assets/ModelLoader.cpp
    rendering/assets/Texture2D.cpp
    rendering/assets/TextureCube.cpp
    rendering/assets/AssetCache.cpp
    rendering/SceneRenderPipeline.cpp
    rendering/FrustumCulling.cpp
)
target_include_directories(engine_rendering PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_rendering PUBLIC engine_core glm::glm)
target_link_libraries(engine_rendering PRIVATE tinygltf assimp)

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
    input/ActionMap.cpp
)
target_include_directories(engine_input PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_input PUBLIC engine_core imgui glm::glm)

add_library(engine_physics STATIC
    physics/PhysicsSystem.cpp
)
target_include_directories(engine_physics PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_physics PUBLIC engine_core glm::glm)
target_link_libraries(engine_physics PRIVATE Jolt)

add_library(engine_audio STATIC
    audio/AudioEngine.cpp
    audio/backend/ALDevice.cpp
    audio/backend/ALSourcePool.cpp
    audio/backend/ALBufferCache.cpp
    audio/backend/ALStreamPlayer.cpp
    audio/backend/ALEffectSlots.cpp
    audio/mix/Voice.cpp
    audio/mix/VoiceManager.cpp
    audio/mix/BusGraph.cpp
    audio/mix/OcclusionProcessor.cpp
    audio/mix/ReverbManager.cpp
    audio/events/EventRegistry.cpp
    ${CMAKE_SOURCE_DIR}/external/stb_vorbis.c
)
target_include_directories(engine_audio PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(engine_audio PRIVATE ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(engine_audio PUBLIC engine_core glm::glm)
target_link_libraries(engine_audio PRIVATE OpenAL::OpenAL nlohmann_json::nlohmann_json spdlog::spdlog)
set_source_files_properties(${CMAKE_SOURCE_DIR}/external/stb_vorbis.c PROPERTIES LANGUAGE C)

add_library(engine_camera STATIC
    camera/CameraMath.cpp
    camera/CameraManager.cpp
    camera/effects/TransitionEffect.cpp
    camera/effects/ShakeEffect.cpp
    camera/effects/FOVEffect.cpp
)
target_include_directories(engine_camera PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(engine_camera PUBLIC engine_core glm::glm)
