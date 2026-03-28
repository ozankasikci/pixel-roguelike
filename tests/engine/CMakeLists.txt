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
