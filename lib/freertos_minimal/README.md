# FreeRTOS Minimal Library

This directory contains a minimal FreeRTOS implementation for Mini SObjectizer.

## Structure

### Core Files
- **`FreeRTOSConfig.h`** - FreeRTOS configuration optimized for Mini SObjectizer
- **`FreeRTOS.h`** - Main FreeRTOS header
- **`task.h`**, **`queue.h`**, **`semphr.h`** - Core FreeRTOS APIs
- **`portable.h`** - Platform portability layer

### Implementation Files
- **`tasks.c`** - Task management implementation
- **`queue.c`** - Queue and semaphore implementation  
- **`timers.c`** - Software timer implementation
- **`heap_4.c`** - Memory management (heap_4 algorithm)
- **`portable/ARM_CM3/`** - ARM Cortex-M3 port

## Configuration

The `FreeRTOSConfig.h` is specifically tuned for:
- **STM32F103RC** (Cortex-M3) target
- **Mini SObjectizer** integration
- **16KB heap** for embedded constraints
- **Optimized for real-time performance**

## Usage

This FreeRTOS configuration provides the RTOS services needed by Mini SObjectizer:
- Mutex/semaphore synchronization
- Task management
- Time services
- Memory management

For embedded builds, this provides real FreeRTOS.
For host testing (UNIT_TEST), the mock in `src/freertos_mock.cpp` is used instead.