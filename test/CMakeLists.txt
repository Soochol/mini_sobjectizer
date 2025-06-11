# Test CMakeLists.txt - Mini SObjectizer v3.0

# Test configuration
add_compile_definitions(
    UNIT_TEST=1
    MINI_SO_ENABLE_TESTING=1
    MINI_SO_DEBUG_LOGGING=1
    MINI_SO_MAX_AGENTS=16
    MINI_SO_MAX_QUEUE_SIZE=32
    MINI_SO_MAX_MESSAGE_SIZE=128
)

# Mock FreeRTOS definitions for testing
add_compile_definitions(
    portMAX_DELAY=0xFFFFFFFF
    pdTRUE=1
    pdFALSE=0
    configTICK_RATE_HZ=1000
    configMINIMAL_STACK_SIZE=128
)

# Common test setup function
function(add_mini_so_test test_name source_file)
    add_executable(${test_name} ${source_file})
    target_link_libraries(${test_name} mini_sobjectizer)
    target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    set_property(TARGET ${test_name} PROPERTY CXX_STANDARD 17)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Core component tests
foreach(test_file 
    test_message_queue.cpp
    test_agent.cpp 
    test_environment.cpp
)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_file}")
        get_filename_component(test_name ${test_file} NAME_WE)
        add_mini_so_test(${test_name} ${test_file})
    endif()
endforeach()

# Production-ready tests
file(GLOB PRODUCTION_TESTS "production_ready_tests/test_*.cpp")
foreach(test_file ${PRODUCTION_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_mini_so_test(${test_name} ${test_file})
endforeach()

# Development tests (optional - only for debugging)
if(MINI_SO_BUILD_DEV_TESTS)
    file(GLOB DEV_TESTS "development_tests/test_*.cpp")
    foreach(test_file ${DEV_TESTS})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_mini_so_test(${test_name} ${test_file})
    endforeach()
endif()

# Test summary
message(STATUS "Configured tests for Mini SObjectizer v3.0:")
get_property(all_tests DIRECTORY PROPERTY TESTS)
foreach(test ${all_tests})
    message(STATUS "  - ${test}")
endforeach()