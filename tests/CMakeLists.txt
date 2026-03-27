add_executable(test_mesh_geometry test_mesh_geometry.cpp)
target_include_directories(test_mesh_geometry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_mesh_geometry PRIVATE glm::glm)
add_test(NAME mesh_geometry COMMAND test_mesh_geometry)

add_executable(test_level_def
    test_level_def.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/MaterialDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/level/LevelDef.cpp
)
target_include_directories(test_level_def PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_level_def PRIVATE glm::glm)
target_compile_definitions(test_level_def PRIVATE
    CATHEDRAL_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/cathedral.scene"
)
add_test(NAME level_def COMMAND test_level_def)

add_executable(test_level_lighting
    test_level_lighting.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/MaterialDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/level/LevelDef.cpp
)
target_include_directories(test_level_lighting PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_level_lighting PRIVATE glm::glm)
target_compile_definitions(test_level_lighting PRIVATE
    LIGHT_RECORDS_SCENE_FILE="${CMAKE_SOURCE_DIR}/tests/data/light_records.scene"
)
add_test(NAME level_lighting COMMAND test_level_lighting)

add_executable(test_content_registry
    test_content_registry.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/core/PathUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/MaterialDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/content/ContentRegistry.cpp
)
target_include_directories(test_content_registry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_content_registry PRIVATE glm::glm)
target_compile_definitions(test_content_registry PRIVATE
    WEAPON_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/weapons/old_dagger.weapon"
    TWO_HANDED_WEAPON_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/weapons/brigand_axe.weapon"
    ENEMY_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/enemies/sentinel.enemy"
    ITEM_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/items/kindling_shard.item"
    SKILL_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/skills/cathedral_attunement.skill"
    CHECKPOINT_ARCHETYPE_FILE="${CMAKE_SOURCE_DIR}/assets/prefabs/gameplay/checkpoint.prefab"
    DOOR_ARCHETYPE_FILE="${CMAKE_SOURCE_DIR}/assets/prefabs/gameplay/double_door.prefab"
)
add_test(NAME content_registry COMMAND test_content_registry)

add_executable(test_equipment_state
    test_equipment_state.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/core/PathUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/MaterialDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/content/ContentRegistry.cpp
    ${CMAKE_SOURCE_DIR}/src/game/session/EquipmentState.cpp
)
target_include_directories(test_equipment_state PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_equipment_state PRIVATE glm::glm)
add_test(NAME equipment_state COMMAND test_equipment_state)

add_executable(test_cathedral_prefabs
    test_cathedral_prefabs.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/MaterialDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/level/LevelBuilder.cpp
    ${CMAKE_SOURCE_DIR}/src/game/prefabs/GameplayPrefabs.cpp
)
target_include_directories(test_cathedral_prefabs PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_cathedral_prefabs PRIVATE engine_rendering EnTT::EnTT glm::glm)
add_test(NAME cathedral_prefabs COMMAND test_cathedral_prefabs)

add_executable(test_material_definitions
    test_material_definitions.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/MaterialDefinition.cpp
)
target_include_directories(test_material_definitions PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_material_definitions PRIVATE glm::glm)
target_compile_definitions(test_material_definitions PRIVATE
    MATERIAL_BASE_FILE="${CMAKE_SOURCE_DIR}/assets/defs/materials/masonry_base.material"
    MATERIAL_BRICK_FILE="${CMAKE_SOURCE_DIR}/assets/defs/materials/brick_default.material"
    MATERIAL_BRICK_OLD_FILE="${CMAKE_SOURCE_DIR}/assets/defs/materials/brick_wall_old.material"
)
add_test(NAME material_definitions COMMAND test_material_definitions)
