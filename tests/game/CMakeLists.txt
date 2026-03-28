set(CATHEDRAL_SCENE_FILE_DEF
    CATHEDRAL_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/cathedral.scene"
)
set(LIGHT_RECORDS_SCENE_FILE_DEF
    LIGHT_RECORDS_SCENE_FILE="${CMAKE_SOURCE_DIR}/tests/data/light_records.scene"
)
set(SILOS_CLOISTER_SCENE_FILE_DEF
    SILOS_CLOISTER_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/silos_cloister.scene"
)
set(MATERIAL_DEFINITION_FILE_DEFS
    MATERIAL_BASE_FILE="${CMAKE_SOURCE_DIR}/assets/defs/materials/masonry_base.material"
    MATERIAL_BRICK_FILE="${CMAKE_SOURCE_DIR}/assets/defs/materials/brick_default.material"
    MATERIAL_BRICK_OLD_FILE="${CMAKE_SOURCE_DIR}/assets/defs/materials/brick_wall_old.material"
)
set(CONTENT_REGISTRY_FILE_DEFS
    WEAPON_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/weapons/old_dagger.weapon"
    TWO_HANDED_WEAPON_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/weapons/brigand_axe.weapon"
    ENEMY_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/enemies/sentinel.enemy"
    ITEM_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/items/kindling_shard.item"
    SKILL_DEF_FILE="${CMAKE_SOURCE_DIR}/assets/defs/skills/cathedral_attunement.skill"
    CHECKPOINT_ARCHETYPE_FILE="${CMAKE_SOURCE_DIR}/assets/prefabs/gameplay/checkpoint.prefab"
    DOOR_ARCHETYPE_FILE="${CMAKE_SOURCE_DIR}/assets/prefabs/gameplay/double_door.prefab"
    ENVIRONMENT_DIR="${CMAKE_SOURCE_DIR}/assets/defs/environments"
)

pixel_roguelike_add_test(test_level_def
    SOURCES test_level_def.cpp
    LIBRARIES game_content
    DEFINITIONS ${CATHEDRAL_SCENE_FILE_DEF}
    LABELS game
)

pixel_roguelike_add_test(test_level_lighting
    SOURCES test_level_lighting.cpp
    LIBRARIES game_content
    DEFINITIONS ${LIGHT_RECORDS_SCENE_FILE_DEF}
    LABELS game
)

pixel_roguelike_add_test(test_content_registry
    SOURCES test_content_registry.cpp
    LIBRARIES game_content
    DEFINITIONS ${CONTENT_REGISTRY_FILE_DEFS}
    LABELS game
)

pixel_roguelike_add_test(test_equipment_state
    SOURCES test_equipment_state.cpp
    LIBRARIES gameplay
    LABELS game
)

pixel_roguelike_add_test(test_cathedral_prefabs
    SOURCES test_cathedral_prefabs.cpp
    LIBRARIES gameplay
    LABELS game
)

pixel_roguelike_add_test(test_material_definitions
    SOURCES test_material_definitions.cpp
    LIBRARIES game_content
    DEFINITIONS ${MATERIAL_DEFINITION_FILE_DEFS}
    LABELS game
)

pixel_roguelike_add_test(test_silos_cloister_level
    SOURCES test_silos_cloister_level.cpp
    LIBRARIES game_content
    DEFINITIONS ${SILOS_CLOISTER_SCENE_FILE_DEF}
    LABELS game
)

pixel_roguelike_add_test(test_level_roundtrip
    SOURCES test_level_roundtrip.cpp
    LIBRARIES game_content
    LABELS game
)

pixel_roguelike_add_test(test_environment_profiles
    SOURCES test_environment_profiles.cpp
    LIBRARIES game_content
    LABELS game
)

pixel_roguelike_add_test(test_runtime_game_session
    SOURCES test_runtime_game_session.cpp
    LIBRARIES gameplay
    DEFINITIONS ${SILOS_CLOISTER_SCENE_FILE_DEF}
    LABELS game
)
