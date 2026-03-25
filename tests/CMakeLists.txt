add_executable(test_mesh_geometry test_mesh_geometry.cpp)
target_include_directories(test_mesh_geometry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_mesh_geometry PRIVATE glm::glm)
add_test(NAME mesh_geometry COMMAND test_mesh_geometry)

add_executable(test_level_def
    test_level_def.cpp
    ${CMAKE_SOURCE_DIR}/src/game/level/LevelDef.cpp
)
target_include_directories(test_level_def PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_level_def PRIVATE glm::glm)
target_compile_definitions(test_level_def PRIVATE
    CATHEDRAL_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/cathedral.scene"
)
add_test(NAME level_def COMMAND test_level_def)

add_executable(test_content_registry
    test_content_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/core/PathUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/game/content/ContentRegistry.cpp
)
target_include_directories(test_content_registry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_content_registry PRIVATE glm::glm)
target_compile_definitions(test_content_registry PRIVATE
    WEAPON_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/weapons/old_dagger.weapon"
    ENEMY_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/enemies/sentinel.enemy"
    ITEM_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/items/kindling_shard.item"
    SKILL_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/skills/cathedral_attunement.skill"
    CHECKPOINT_ARCHETYPE_FILE="${CMAKE_SOURCE_DIR}/assets/prefabs/gameplay/checkpoint.prefab"
    DOOR_ARCHETYPE_FILE="${CMAKE_SOURCE_DIR}/assets/prefabs/gameplay/double_door.prefab"
)
add_test(NAME content_registry COMMAND test_content_registry)

add_executable(test_cathedral_prefabs
    test_cathedral_prefabs.cpp
    ${CMAKE_SOURCE_DIR}/src/game/level/LevelBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/game/prefabs/GameplayPrefabs.cpp
)
target_include_directories(test_cathedral_prefabs PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_cathedral_prefabs PRIVATE engine_rendering EnTT::EnTT glm::glm)
add_test(NAME cathedral_prefabs COMMAND test_cathedral_prefabs)
