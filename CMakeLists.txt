cmake_minimum_required(VERSION 3.15)

# Project definition
project(Mini_SObjectizer 
    VERSION 3.0.0
    DESCRIPTION "Production-ready Zero-overhead Actor Model framework for embedded systems"
    LANGUAGES CXX C
)

# Set C++ standard - Phase 3 requires C++17 for modern features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler settings
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

# Build configuration
option(MINI_SO_BUILD_TESTS "Build unit tests" ON)
option(MINI_SO_BUILD_EXAMPLES "Build examples" ON)
option(MINI_SO_ENABLE_METRICS "Enable performance metrics" ON)
option(MINI_SO_ENABLE_VALIDATION "Enable message validation" ON)

# Configuration defines
if(MINI_SO_ENABLE_METRICS)
    add_compile_definitions(MINI_SO_ENABLE_METRICS=1)
else()
    add_compile_definitions(MINI_SO_ENABLE_METRICS=0)
endif()

if(MINI_SO_ENABLE_VALIDATION)
    add_compile_definitions(MINI_SO_ENABLE_VALIDATION=1)
else()
    add_compile_definitions(MINI_SO_ENABLE_VALIDATION=0)
endif()

# Default configuration values
add_compile_definitions(
    MINI_SO_MAX_AGENTS=16
    MINI_SO_MAX_QUEUE_SIZE=64
    MINI_SO_MAX_MESSAGE_SIZE=128
)

# Include directories
include_directories(include)
include_directories(lib/freertos_minimal/include)

# Source files for Mini SObjectizer library
set(MINI_SO_SOURCES
    src/mini_sobjectizer.cpp
)

set(MINI_SO_HEADERS
    include/mini_sobjectizer/mini_sobjectizer.h
)

# Create static library
add_library(mini_sobjectizer STATIC ${MINI_SO_SOURCES} ${MINI_SO_HEADERS})

target_include_directories(mini_sobjectizer PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Set library properties
set_target_properties(mini_sobjectizer PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${MINI_SO_HEADERS}"
)

# For testing on host platform (mock FreeRTOS)
if(MINI_SO_BUILD_TESTS OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_definitions(UNIT_TEST=1)
endif()

# Examples
if(MINI_SO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(MINI_SO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Installation rules - Source Distribution
# Install header files
install(FILES ${MINI_SO_HEADERS}
    DESTINATION include/mini_sobjectizer
)

# Install source files for user compilation
install(FILES ${MINI_SO_SOURCES}
    DESTINATION src/mini_sobjectizer
)

# Install FreeRTOS mock for testing
install(FILES src/freertos_mock.cpp
    DESTINATION src/mini_sobjectizer
)

# Install library target (optional - users can compile directly from source)
install(TARGETS mini_sobjectizer
    EXPORT mini_sobjectizer_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(EXPORT mini_sobjectizer_targets
    FILE mini_sobjectizer_targets.cmake
    NAMESPACE mini_sobjectizer::
    DESTINATION lib/cmake/mini_sobjectizer
)

# Generate config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "mini_sobjectizer_config_version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "cmake/mini_sobjectizer_config.cmake.in"
    "mini_sobjectizer_config.cmake"
    INSTALL_DESTINATION lib/cmake/mini_sobjectizer
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mini_sobjectizer_config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mini_sobjectizer_config_version.cmake"
    DESTINATION lib/cmake/mini_sobjectizer
)

# Note: SDK example files were removed during cleanup

# Print configuration summary
message(STATUS "")
message(STATUS "Mini SObjectizer v${PROJECT_VERSION} Configuration Summary:")
message(STATUS "----------------------------------------")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build Tests: ${MINI_SO_BUILD_TESTS}")
message(STATUS "Build Examples: ${MINI_SO_BUILD_EXAMPLES}")
message(STATUS "Enable Metrics: ${MINI_SO_ENABLE_METRICS}")
message(STATUS "Enable Validation: ${MINI_SO_ENABLE_VALIDATION}")
message(STATUS "Max Agents: 16")
message(STATUS "Max Queue Size: 64")
message(STATUS "Max Message Size: 128")
message(STATUS "")