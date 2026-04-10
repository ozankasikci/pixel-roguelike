pixel_roguelike_add_test(test_mesh_geometry
    SOURCES test_mesh_geometry.cpp
    LIBRARIES glm::glm
    LABELS engine
)

pixel_roguelike_add_test(test_model_discovery
    SOURCES test_model_discovery.cpp
    LIBRARIES engine_rendering
    LABELS engine
)

pixel_roguelike_add_test(test_model_loader
    SOURCES test_model_loader.cpp
    LIBRARIES engine_rendering
    LABELS engine
)

pixel_roguelike_add_test(test_asset_cache
    SOURCES test_asset_cache.cpp
    LIBRARIES engine_rendering
    LABELS engine
)

pixel_roguelike_add_test(test_questdoor_scale
    SOURCES test_questdoor_scale.cpp
    LIBRARIES engine_rendering
    LABELS engine
)

pixel_roguelike_add_test(test_game_registry
    SOURCES test_game_registry.cpp
    LIBRARIES engine_core
    LABELS engine
)

pixel_roguelike_add_test(test_entity_active_state
    SOURCES test_entity_active_state.cpp
    LIBRARIES engine_core
    LABELS engine
)

pixel_roguelike_add_test(test_render_quality_settings
    SOURCES test_render_quality_settings.cpp
    LABELS engine
)

pixel_roguelike_add_test(test_camera_math
    SOURCES test_camera_math.cpp
    LIBRARIES engine_camera
    LABELS engine
)

pixel_roguelike_add_test(test_camera_manager
    SOURCES test_camera_manager.cpp
    LIBRARIES engine_camera
    LABELS engine
)

pixel_roguelike_add_test(test_transition_effect
    SOURCES test_transition_effect.cpp
    LIBRARIES engine_camera
    LABELS engine
)

pixel_roguelike_add_test(test_shake_effect
    SOURCES test_shake_effect.cpp
    LIBRARIES engine_camera
    LABELS engine
)

pixel_roguelike_add_test(test_fov_effect
    SOURCES test_fov_effect.cpp
    LIBRARIES engine_camera
    LABELS engine
)
