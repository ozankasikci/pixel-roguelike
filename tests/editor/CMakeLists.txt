set(SILOS_CLOISTER_SCENE_FILE_DEF
    SILOS_CLOISTER_SCENE_FILE="${CMAKE_SOURCE_DIR}/assets/scenes/silos_cloister.scene"
)

pixel_roguelike_add_test(test_editor_selection
    SOURCES test_editor_selection.cpp
    LIBRARIES editor
    LABELS editor
)

pixel_roguelike_add_test(test_editor_command_stack
    SOURCES test_editor_command_stack.cpp
    LIBRARIES editor
    LABELS editor
)

pixel_roguelike_add_test(test_editor_layout_presets
    SOURCES test_editor_layout_presets.cpp
    LIBRARIES editor
    LABELS editor
)

pixel_roguelike_add_test(test_editor_hierarchy
    SOURCES test_editor_hierarchy.cpp
    LIBRARIES editor
    LABELS editor
)

pixel_roguelike_add_test(test_editor_asset_browser
    SOURCES test_editor_asset_browser.cpp
    LIBRARIES editor
    LABELS editor
)

pixel_roguelike_add_test(test_editor_runtime_preview
    SOURCES test_editor_runtime_preview.cpp
    LIBRARIES editor
    DEFINITIONS ${SILOS_CLOISTER_SCENE_FILE_DEF}
    LABELS editor
)

pixel_roguelike_add_profile(profile_play_preview
    SOURCES profile_play_preview.cpp
    LIBRARIES editor
    DEFINITIONS ${SILOS_CLOISTER_SCENE_FILE_DEF}
)
