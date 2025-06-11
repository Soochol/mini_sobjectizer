# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Mini SObjectizer v3.0 is a production-ready, zero-overhead Actor Model framework designed for embedded systems. It provides message-based communication between agents with real-time safety guarantees and minimal memory footprint (~5KB code, 16 bytes BSS).

### Core Architecture

The framework implements a pure Actor Model with three main components:

1. **Environment** - Singleton managing agent lifecycle and message routing
2. **System** - Built-in services (error reporting, performance monitoring, watchdog)  
3. **Agent** - Base class for all actors, handles messages via `handle_message()`

Key design principles:
- 100% message-based communication ("메시지가 모든 것" - Messages are everything)
- Zero-overhead abstractions using C++17 features
- Complete actor independence with no shared state
- Cache-aligned data structures for optimal memory access

### Message System

- **MessageBase** - 8-byte optimized headers (67% reduction from Phase 1)
- **MessageQueue** - Lock-free, cache-friendly circular buffer
- **Broadcast messaging** - Type-safe message dispatch to all agents
- **Point-to-point messaging** - Direct agent-to-agent communication

## Development Commands

### Building with CMake
```bash
# Full build with tests and examples
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Build without tests/examples
cmake .. -DMINI_SO_BUILD_TESTS=OFF -DMINI_SO_BUILD_EXAMPLES=OFF
make -j4
```

### Testing
```bash
# Run all tests
cd build && ctest

# Run specific core functionality test
g++ -std=c++17 -DUNIT_TEST -I include test/test_core_functionality.cpp \
    src/mini_sobjectizer.cpp src/freertos_mock.cpp -o test_runner
./test_runner

# Expected output: "🎉 ALL TESTS PASSED! Mini SObjectizer v3.0 Final is working correctly. 🎉"
```

### PlatformIO (Embedded Development)
```bash
# Build for native (host testing)
pio run -e native_test

# Build for STM32F103
pio run -e stm32f103rc_simple

# Build for ESP32
pio run -e esp32

# Upload to embedded target
pio run -e stm32f103rc_simple -t upload
```

## Build Configuration

### CMake Options
- `MINI_SO_BUILD_TESTS` - Build unit tests (default: ON)
- `MINI_SO_BUILD_EXAMPLES` - Build examples (default: ON)
- `MINI_SO_ENABLE_METRICS` - Enable performance metrics (default: ON)
- `MINI_SO_ENABLE_VALIDATION` - Enable message validation (default: ON)

### Runtime Configuration Defines
- `MINI_SO_MAX_AGENTS=16` - Maximum number of agents
- `MINI_SO_MAX_QUEUE_SIZE=32/64` - Message queue size per agent
- `MINI_SO_MAX_MESSAGE_SIZE=64/128` - Maximum message size in bytes
- `UNIT_TEST=1` - Enable host platform testing with FreeRTOS mocks

## File Structure

### Core Implementation
- `include/mini_sobjectizer/mini_sobjectizer.h` - Main header (production)
- `src/mini_sobjectizer.cpp` - Core implementation
- `src/freertos_mock.cpp` - FreeRTOS mocking for host testing
- `FreeRTOSConfig.h` - FreeRTOS configuration for embedded targets

### Platform Support
- **Host Testing**: Uses `freertos_mock.cpp` with `UNIT_TEST=1` define
- **Embedded**: Real FreeRTOS integration (STM32, ESP32)
- **PlatformIO**: Multi-platform embedded development support

### Testing Approach
The project uses a comprehensive testing strategy:
- Host platform testing with FreeRTOS mocks
- Multiple test environments in `test/` directory
- CMake-based test discovery and execution
- Production readiness score: 99/100

### Legacy Code
The `archive/` directory contains previous implementation phases:
- `legacy/` - Phase 1 and Phase 2 implementations
- `old_builds/` - Historical build artifacts
- `old_examples/` - Previous example implementations

## Performance Targets

- Code Size: < 10KB (currently 4.8KB)
- Message Throughput: > 1,000 msg/sec (currently 2,200+)
- Memory Footprint: < 1KB BSS (currently 16 bytes)
- Timing Precision: < 1ms (currently microseconds)