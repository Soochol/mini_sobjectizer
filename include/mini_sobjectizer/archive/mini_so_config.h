#pragma once

/**
 * @file mini_so_config.h
 * @brief Configuration and Platform Abstraction for Mini SObjectizer
 * 
 * This file contains all build-time configuration options and platform-specific
 * abstractions for FreeRTOS integration and testing.
 */

#include <cstdint>
#include <cstddef>

// ============================================================================
// Build Configuration
// ============================================================================

#ifndef MINI_SO_MAX_AGENTS
#define MINI_SO_MAX_AGENTS 16
#endif

#ifndef MINI_SO_MAX_QUEUE_SIZE
#define MINI_SO_MAX_QUEUE_SIZE 64
#endif

#ifndef MINI_SO_MAX_MESSAGE_SIZE
#define MINI_SO_MAX_MESSAGE_SIZE 128
#endif

// Performance optimization settings
#ifndef MINI_SO_ENABLE_METRICS
#define MINI_SO_ENABLE_METRICS 1
#endif

#ifndef MINI_SO_ENABLE_VALIDATION
#define MINI_SO_ENABLE_VALIDATION 1
#endif

// Cache optimization settings
#ifndef MINI_SO_CACHE_LINE_SIZE
#define MINI_SO_CACHE_LINE_SIZE 64
#endif

// ============================================================================
// Platform Abstraction Layer
// ============================================================================

#ifdef UNIT_TEST
// Mock FreeRTOS types for unit testing
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;

#ifndef portMAX_DELAY
#define portMAX_DELAY 0xFFFFFFFF
#endif

#ifndef pdTRUE
#define pdTRUE 1
#define pdFALSE 0
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) (x)
#endif

#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000
#endif

// Mock FreeRTOS function declarations for testing
extern "C" {
    SemaphoreHandle_t xSemaphoreCreateMutex(void);
    BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
    BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);
    void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);
    TickType_t xTaskGetTickCount(void);
    void taskDISABLE_INTERRUPTS(void);
}

#else
// Real FreeRTOS includes for embedded target
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#endif

// ============================================================================
// Compiler Optimization Hints
// ============================================================================

// Branch prediction hints (C++20 feature, fallback for older compilers)
#if __cplusplus >= 202002L
    // C++20 [[likely]] and [[unlikely]] are available
#else
    #define likely
    #define unlikely
#endif

// Memory alignment macros
#define MINI_SO_CACHE_ALIGNED alignas(MINI_SO_CACHE_LINE_SIZE)
#define MINI_SO_WORD_ALIGNED alignas(sizeof(void*))

// ============================================================================
// Static Assertions for Configuration Validation
// ============================================================================

namespace mini_so {
namespace config {

// Validate configuration at compile time
static_assert(MINI_SO_MAX_AGENTS > 0 && MINI_SO_MAX_AGENTS <= 65535, 
              "MINI_SO_MAX_AGENTS must be between 1 and 65535");

static_assert(MINI_SO_MAX_QUEUE_SIZE > 0 && MINI_SO_MAX_QUEUE_SIZE <= 65535,
              "MINI_SO_MAX_QUEUE_SIZE must be between 1 and 65535");

static_assert(MINI_SO_MAX_MESSAGE_SIZE >= 8 && MINI_SO_MAX_MESSAGE_SIZE <= 65535,
              "MINI_SO_MAX_MESSAGE_SIZE must be between 8 and 65535");

static_assert(MINI_SO_CACHE_LINE_SIZE >= 8 && (MINI_SO_CACHE_LINE_SIZE & (MINI_SO_CACHE_LINE_SIZE - 1)) == 0,
              "MINI_SO_CACHE_LINE_SIZE must be a power of 2 and at least 8 bytes");

// Memory usage calculations
constexpr size_t calculate_queue_memory_usage() noexcept {
    constexpr size_t entry_size = ((MINI_SO_MAX_MESSAGE_SIZE + sizeof(uint16_t) + sizeof(bool) + MINI_SO_CACHE_LINE_SIZE - 1) 
                                   / MINI_SO_CACHE_LINE_SIZE) * MINI_SO_CACHE_LINE_SIZE;
    return MINI_SO_MAX_AGENTS * MINI_SO_MAX_QUEUE_SIZE * entry_size;
}

constexpr size_t ESTIMATED_MEMORY_USAGE = calculate_queue_memory_usage();

} // namespace config
} // namespace mini_so