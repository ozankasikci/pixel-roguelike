add_executable(level-editor main.cpp)
target_link_libraries(level-editor PRIVATE
    editor
)
if(TARGET scripts_build)
    add_dependencies(level-editor scripts_build)
endif()
configure_desktop_app(level-editor)
