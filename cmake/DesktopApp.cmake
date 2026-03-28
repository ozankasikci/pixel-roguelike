function(configure_desktop_app target)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/external
    )

    if(APPLE)
        target_link_libraries(${target} PRIVATE
            "-framework Cocoa"
            "-framework OpenGL"
            "-framework IOKit"
        )
        target_compile_definitions(${target} PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endfunction()
