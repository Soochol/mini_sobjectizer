# Examples CMakeLists.txt - Mini SObjectizer v3.0

# Minimum CMake version requirement
cmake_minimum_required(VERSION 3.10)

# =============================================================================
# New Comprehensive Examples with User-Friendly Macros
# =============================================================================

# Helper function to create example executable
function(add_example_executable TARGET_NAME SOURCE_FILE)
    add_executable(${TARGET_NAME} ${SOURCE_FILE} ${CMAKE_SOURCE_DIR}/src/freertos_mock.cpp)
    target_link_libraries(${TARGET_NAME} PRIVATE mini_sobjectizer)
    target_compile_definitions(${TARGET_NAME} PRIVATE UNIT_TEST=1)
    set_target_properties(${TARGET_NAME} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

# 01 - Beginner Tutorial (학습용)
add_example_executable(01_beginner_tutorial 01_beginner_tutorial.cpp)

# 02 - Smart Factory Example (스마트 팩토리)
add_example_executable(02_smart_factory_example 02_smart_factory_example.cpp)

# 03 - Autonomous Vehicle Example (자율주행)
add_example_executable(03_autonomous_vehicle_example 03_autonomous_vehicle_example.cpp)

# 04 - Medical Device Example (의료기기)
add_example_executable(04_medical_device_example 04_medical_device_example.cpp)

# 05 - Drone Control Example (드론 제어)
add_example_executable(05_drone_control_example 05_drone_control_example.cpp)

# 06 - Advanced Macros Example (고급 매크로)
add_example_executable(06_advanced_macros_example 06_advanced_macros_example.cpp)

# =============================================================================
# Legacy Examples (Phase 3 compatibility)
# =============================================================================

# Legacy examples using the same function for consistency
add_example_executable(phase3_final_example phase3_final_example.cpp)
add_example_executable(simple_iot_example simple_iot_example.cpp)

# Conditional example (only if file exists and header is available)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/safety_migration_example.cpp" AND 
   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../include/mini_sobjectizer/message_safety_patch.h")
    add_example_executable(safety_migration_example safety_migration_example.cpp)
endif()

# =============================================================================
# Installation
# =============================================================================

# Define list of all example targets
set(EXAMPLE_TARGETS
    01_beginner_tutorial
    02_smart_factory_example 
    03_autonomous_vehicle_example
    04_medical_device_example
    05_drone_control_example
    06_advanced_macros_example
    phase3_final_example 
    simple_iot_example
)

# Install all examples
install(TARGETS ${EXAMPLE_TARGETS}
    RUNTIME DESTINATION bin/examples
    COMPONENT examples
)

# Install conditional example if it exists
if(TARGET safety_migration_example)
    install(TARGETS safety_migration_example 
        RUNTIME DESTINATION bin/examples
        COMPONENT examples
    )
endif()

# =============================================================================
# Optional: Add custom target to build all examples
# =============================================================================

add_custom_target(examples ALL DEPENDS ${EXAMPLE_TARGETS})

# Add safety example to the custom target if it exists
if(TARGET safety_migration_example)
    add_dependencies(examples safety_migration_example)
endif()