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

pixel_roguelike_add_test(test_voice_handle
    SOURCES test_voice_handle.cpp
    LIBRARIES glm::glm
    LABELS engine audio
)

pixel_roguelike_add_test(test_bus_graph
    SOURCES test_bus_graph.cpp ${CMAKE_SOURCE_DIR}/src/engine/audio/mix/BusGraph.cpp
    LIBRARIES glm::glm
    LABELS engine audio
)

pixel_roguelike_add_test(test_event_registry
    SOURCES test_event_registry.cpp ${CMAKE_SOURCE_DIR}/src/engine/audio/events/EventRegistry.cpp
    LIBRARIES glm::glm nlohmann_json::nlohmann_json
    DEFINITIONS TEST_SOUND_EVENTS_PATH="${CMAKE_SOURCE_DIR}/tests/engine/test_fixtures/test_sound_events.json"
    LABELS engine audio
)

pixel_roguelike_add_test(test_audio_command
    SOURCES test_audio_command.cpp
    LIBRARIES glm::glm
    LABELS engine audio
)

pixel_roguelike_add_test(test_voice_scoring
    SOURCES test_voice_scoring.cpp ${CMAKE_SOURCE_DIR}/src/engine/audio/mix/Voice.cpp ${CMAKE_SOURCE_DIR}/src/engine/audio/mix/BusGraph.cpp
    LIBRARIES glm::glm
    LABELS engine audio
)

pixel_roguelike_add_test(test_voice_manager
    SOURCES test_voice_manager.cpp ${CMAKE_SOURCE_DIR}/src/engine/audio/mix/VoiceManager.cpp ${CMAKE_SOURCE_DIR}/src/engine/audio/mix/Voice.cpp ${CMAKE_SOURCE_DIR}/src/engine/audio/mix/BusGraph.cpp
    LIBRARIES glm::glm
    LABELS engine audio
)

pixel_roguelike_add_test(test_reverb_manager
    SOURCES test_reverb_manager.cpp ${CMAKE_SOURCE_DIR}/src/engine/audio/mix/ReverbManager.cpp
    LIBRARIES glm::glm
    LABELS engine audio
)

pixel_roguelike_add_test(test_audio_engine_integration
    SOURCES test_audio_engine_integration.cpp
    LIBRARIES engine_audio
    DEFINITIONS TEST_AUDIO_BASE_PATH="${CMAKE_SOURCE_DIR}/assets/audio"
    LABELS engine audio integration
)

pixel_roguelike_add_test(test_value_animator
    SOURCES test_value_animator.cpp
    LIBRARIES glm::glm
    LABELS engine particles
)

pixel_roguelike_add_test(test_emitter_shape
    SOURCES test_emitter_shape.cpp
    LIBRARIES glm::glm
    LABELS engine particles
)

pixel_roguelike_add_test(test_force_function
    SOURCES test_force_function.cpp
    LIBRARIES glm::glm
    LABELS engine particles
)
