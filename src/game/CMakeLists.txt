add_library(game_content STATIC
    content/ContentRegistry.cpp
    level/LevelDef.cpp
    levels/ProceduralGameAssets.cpp
    rendering/EnvironmentDefinition.cpp
    rendering/EnvironmentProfile.cpp
    rendering/MaterialDefinition.cpp
)
target_include_directories(game_content PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_content PUBLIC
    engine_rendering
)

add_library(game_rendering STATIC
    rendering/EnvironmentDebugSync.cpp
    rendering/MaterialTextureLibrary.cpp
    rendering/RuntimeCameraMath.cpp
    rendering/RuntimeSceneRenderer.cpp
)
target_include_directories(game_rendering PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game_rendering PUBLIC
    game_content
    engine_ui
)
target_link_libraries(game_rendering PRIVATE tinygltf)

add_library(gameplay STATIC
    behavior/BehaviorSystem.cpp
    level/LevelBuilder.cpp
    level/LevelLoader.cpp
    levels/cathedral/CathedralPrefabs.cpp
    prefabs/GameplayPrefabs.cpp
    runtime/GameplayBehaviors.cpp
    runtime/RuntimeGameplay.cpp
    runtime/RuntimeGameSession.cpp
    scenes/GenericFileScene.cpp
    session/EquipmentState.cpp
    systems/AudioListenerSystem.cpp
    systems/KinematicColliderSystem.cpp
    systems/RenderSystem.cpp
    ui/GameOverlays.cpp
)
target_include_directories(gameplay PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(gameplay PUBLIC
    game_content
    game_rendering
    engine_audio
    engine_input
    engine_physics
    EnTT::EnTT
)

add_library(game INTERFACE)
target_include_directories(game INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game INTERFACE
    game_content
    game_rendering
    gameplay
)

# Feature modules (per D-03)
add_subdirectory(modules/door)
add_subdirectory(modules/checkpoint)
