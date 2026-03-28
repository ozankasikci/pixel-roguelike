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
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentProfile.cpp
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
    ENVIRONMENT_DIR="${CMAKE_SOURCE_DIR}/assets/defs/environments"
)
add_test(NAME content_registry COMMAND test_content_registry)

add_executable(test_equipment_state
    test_equipment_state.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/core/PathUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentProfile.cpp
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

add_executable(test_silos_cloister_level
    test_silos_cloister_level.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/MaterialDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/level/LevelDef.cpp
)
target_include_directories(test_silos_cloister_level PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_silos_cloister_level PRIVATE glm::glm)
target_compile_definitions(test_silos_cloister_level PRIVATE
    SILOS_CLOISTER_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/silos_cloister.scene"
)
add_test(NAME silos_cloister_level COMMAND test_silos_cloister_level)

add_executable(test_level_roundtrip
    test_level_roundtrip.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentProfile.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/MaterialDefinition.cpp
    ${CMAKE_SOURCE_DIR}/src/game/level/LevelDef.cpp
)
target_include_directories(test_level_roundtrip PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_level_roundtrip PRIVATE glm::glm)
add_test(NAME level_roundtrip COMMAND test_level_roundtrip)

add_executable(test_environment_profiles
    test_environment_profiles.cpp
    ${CMAKE_SOURCE_DIR}/src/game/rendering/EnvironmentProfile.cpp
)
target_include_directories(test_environment_profiles PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_environment_profiles PRIVATE glm::glm)
add_test(NAME environment_profiles COMMAND test_environment_profiles)

add_executable(test_editor_selection
    test_editor_selection.cpp
)
target_include_directories(test_editor_selection PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_editor_selection PRIVATE editor)
add_test(NAME editor_selection COMMAND test_editor_selection)

add_executable(test_editor_command_stack
    test_editor_command_stack.cpp
)
target_include_directories(test_editor_command_stack PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_editor_command_stack PRIVATE editor)
add_test(NAME editor_command_stack COMMAND test_editor_command_stack)

add_executable(test_editor_layout_presets
    test_editor_layout_presets.cpp
)
target_include_directories(test_editor_layout_presets PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_editor_layout_presets PRIVATE editor)
add_test(NAME editor_layout_presets COMMAND test_editor_layout_presets)

add_executable(test_editor_hierarchy
    test_editor_hierarchy.cpp
)
target_include_directories(test_editor_hierarchy PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_editor_hierarchy PRIVATE editor)
add_test(NAME editor_hierarchy COMMAND test_editor_hierarchy)

add_executable(test_editor_asset_browser
    test_editor_asset_browser.cpp
)
target_include_directories(test_editor_asset_browser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_editor_asset_browser PRIVATE editor)
add_test(NAME editor_asset_browser COMMAND test_editor_asset_browser)

add_executable(test_runtime_game_session test_runtime_game_session.cpp)
target_include_directories(test_runtime_game_session PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_runtime_game_session PRIVATE gameplay)
target_compile_definitions(test_runtime_game_session PRIVATE
    SILOS_CLOISTER_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/silos_cloister.scene"
)
add_test(NAME runtime_game_session COMMAND test_runtime_game_session)

add_executable(profile_play_preview profile_play_preview.cpp)
target_include_directories(profile_play_preview PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(profile_play_preview PRIVATE editor)
target_compile_definitions(profile_play_preview PRIVATE
    SILOS_CLOISTER_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/silos_cloister.scene"
)
