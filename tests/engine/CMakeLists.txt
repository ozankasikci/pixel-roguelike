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
