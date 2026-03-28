include(CMakeParseArguments)

function(pixel_roguelike_add_test target)
    set(options)
    set(oneValueArgs TEST_NAME)
    set(multiValueArgs SOURCES LIBRARIES DEFINITIONS LABELS)
    cmake_parse_arguments(TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT TEST_SOURCES)
        message(FATAL_ERROR "pixel_roguelike_add_test(${target}) requires SOURCES")
    endif()

    add_executable(${target} ${TEST_SOURCES})
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )

    if(TEST_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${TEST_LIBRARIES})
    endif()

    if(TEST_DEFINITIONS)
        target_compile_definitions(${target} PRIVATE ${TEST_DEFINITIONS})
    endif()

    if(TEST_TEST_NAME)
        set(test_name ${TEST_TEST_NAME})
    else()
        string(REGEX REPLACE "^test_" "" test_name "${target}")
    endif()

    add_test(NAME ${test_name} COMMAND ${target})

    if(TEST_LABELS)
        set_tests_properties(${test_name} PROPERTIES LABELS "${TEST_LABELS}")
    endif()
endfunction()

function(pixel_roguelike_add_profile target)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SOURCES LIBRARIES DEFINITIONS)
    cmake_parse_arguments(PROFILE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT PROFILE_SOURCES)
        message(FATAL_ERROR "pixel_roguelike_add_profile(${target}) requires SOURCES")
    endif()

    add_executable(${target} ${PROFILE_SOURCES})
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )

    if(PROFILE_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${PROFILE_LIBRARIES})
    endif()

    if(PROFILE_DEFINITIONS)
        target_compile_definitions(${target} PRIVATE ${PROFILE_DEFINITIONS})
    endif()
endfunction()
