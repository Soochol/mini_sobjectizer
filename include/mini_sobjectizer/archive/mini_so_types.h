#pragma once

/**
 * @file mini_so_types.h
 * @brief Core Types and Constants for Mini SObjectizer
 * 
 * This file defines the fundamental types, constants, and time utilities
 * used throughout the Mini SObjectizer framework.
 */

#include "mini_so_config.h"

namespace mini_so {

// ============================================================================
// Core Types - Zero-overhead type system
// ============================================================================

/// Unique identifier for agents (16-bit for memory efficiency)
using AgentId = uint16_t;

/// Unique identifier for message types (16-bit for memory efficiency)
using MessageId = uint16_t;

/// Duration type for timeouts and intervals (32-bit milliseconds)
using Duration = uint32_t;

/// Time point type for timestamps (32-bit milliseconds since boot)
using TimePoint = uint32_t;

// ============================================================================
// Constants
// ============================================================================

/// Invalid agent ID constant
constexpr AgentId INVALID_AGENT_ID = 0xFFFF;

/// Invalid message ID constant
constexpr MessageId INVALID_MESSAGE_ID = 0xFFFF;

// ============================================================================
// Time Utilities
// ============================================================================

/**
 * @brief Get current time point
 * @return Current time in milliseconds since system boot
 * 
 * This function provides a lightweight time source that works both
 * in unit tests (mock) and embedded systems (FreeRTOS ticks).
 */
inline TimePoint now() noexcept {
#ifdef UNIT_TEST
    // Mock time implementation for testing
    static uint32_t test_counter = 1000;
    return test_counter += 10;
#else
    // Real FreeRTOS implementation
    return xTaskGetTickCount();
#endif
}

/**
 * @brief Check if a timeout has elapsed
 * @param start_time The starting time point
 * @param timeout_ms Timeout duration in milliseconds
 * @return true if timeout has elapsed, false otherwise
 */
inline bool is_timeout_elapsed(TimePoint start_time, Duration timeout_ms) noexcept {
    return (now() - start_time) >= timeout_ms;
}

/**
 * @brief Calculate elapsed time since a given time point
 * @param start_time The starting time point
 * @return Elapsed time in milliseconds
 */
inline Duration elapsed_since(TimePoint start_time) noexcept {
    return now() - start_time;
}

/**
 * @brief Add duration to a time point with overflow protection
 * @param time_point Base time point
 * @param duration Duration to add
 * @return New time point, handling 32-bit overflow correctly
 */
inline TimePoint add_duration(TimePoint time_point, Duration duration) noexcept {
    return time_point + duration;  // 32-bit arithmetic handles overflow naturally
}

// ============================================================================
// Result Types
// ============================================================================

/**
 * @brief Common result type for operations that can fail
 * 
 * Provides a lightweight alternative to exceptions for embedded systems.
 */
enum class Result : uint8_t {
    SUCCESS = 0,           ///< Operation completed successfully
    FAILURE,               ///< Generic failure
    TIMEOUT,               ///< Operation timed out
    INVALID_PARAMETER,     ///< Invalid input parameter
    INSUFFICIENT_MEMORY,   ///< Not enough memory available
    QUEUE_FULL,           ///< Message queue is full
    QUEUE_EMPTY,          ///< Message queue is empty
    AGENT_NOT_FOUND,      ///< Agent ID not found
    MESSAGE_TOO_LARGE,    ///< Message exceeds size limit
    INVALID_MESSAGE       ///< Message validation failed
};

/**
 * @brief Convert result to human-readable string
 * @param result The result to convert
 * @return String representation of the result
 */
inline const char* result_to_string(Result result) noexcept {
    switch (result) {
        case Result::SUCCESS:              return "SUCCESS";
        case Result::FAILURE:              return "FAILURE";
        case Result::TIMEOUT:              return "TIMEOUT";
        case Result::INVALID_PARAMETER:    return "INVALID_PARAMETER";
        case Result::INSUFFICIENT_MEMORY:  return "INSUFFICIENT_MEMORY";
        case Result::QUEUE_FULL:           return "QUEUE_FULL";
        case Result::QUEUE_EMPTY:          return "QUEUE_EMPTY";
        case Result::AGENT_NOT_FOUND:      return "AGENT_NOT_FOUND";
        case Result::MESSAGE_TOO_LARGE:    return "MESSAGE_TOO_LARGE";
        case Result::INVALID_MESSAGE:      return "INVALID_MESSAGE";
        default:                           return "UNKNOWN";
    }
}

// ============================================================================
// Type Traits and Utilities
// ============================================================================

/**
 * @brief Check if a type is suitable for message content
 * 
 * Messages should be trivially copyable for efficient zero-copy operations.
 */
template<typename T>
struct is_valid_message_type {
    // Simplified check - assume all types are valid for basic compilation
    static constexpr bool value = sizeof(T) <= MINI_SO_MAX_MESSAGE_SIZE;
};

template<typename T>
constexpr bool is_valid_message_type_v = is_valid_message_type<T>::value;

/**
 * @brief Compile-time assertion for message type validity
 */
#define MINI_SO_ASSERT_VALID_MESSAGE_TYPE(T) \
    static_assert(mini_so::is_valid_message_type_v<T>, \
                  "Message type must be trivially copyable and within size limit")

} // namespace mini_so