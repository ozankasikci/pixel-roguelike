add_library(game STATIC
    content/ContentRegistry.cpp
    rendering/EnvironmentDebugSync.cpp
    rendering/EnvironmentDefinition.cpp
    rendering/EnvironmentProfile.cpp
    rendering/MaterialDefinition.cpp
    rendering/MaterialTextureLibrary.cpp
    rendering/RuntimeCameraMath.cpp
    rendering/RuntimeSceneRenderer.cpp
    runtime/RuntimeGameplay.cpp
    runtime/RuntimeGameSession.cpp
    session/EquipmentState.cpp
    scenes/CathedralScene.cpp
    scenes/SilosCloisterScene.cpp
    systems/InteractionSystem.cpp
    systems/InventorySystem.cpp
    systems/CameraSystem.cpp
    systems/CheckpointSystem.cpp
    systems/DoorSystem.cpp
    systems/PlayerMovementSystem.cpp
    systems/RenderSystem.cpp
    level/LevelBuilder.cpp
    level/LevelDef.cpp
    level/LevelLoader.cpp
    prefabs/GameplayPrefabs.cpp
    levels/cathedral/CathedralAssets.cpp
    levels/cathedral/CathedralPrefabs.cpp
)
target_include_directories(game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(game PUBLIC
    engine_core
    engine_rendering
    engine_ui
    engine_input
    engine_physics
    EnTT::EnTT
)
