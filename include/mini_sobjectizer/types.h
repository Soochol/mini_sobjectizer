/**
 * @file types.h
 * @brief Core type definitions for Mini SObjectizer
 * 
 * Fundamental types used throughout the framework.
 */

#pragma once

#include <cstdint>
#include "platform.h"

namespace mini_so {

// ============================================================================
// Core Types - Zero-overhead type system
// ============================================================================

using AgentId = uint16_t;
using MessageId = uint16_t;
using Duration = uint32_t;
using TimePoint = uint32_t;

constexpr AgentId INVALID_AGENT_ID = 0xFFFF;
constexpr MessageId INVALID_MESSAGE_ID = 0xFFFF;

// Zero-overhead time function
inline TimePoint now() noexcept {
#ifdef UNIT_TEST
    static uint32_t test_counter = 1000;
    return test_counter += 10;
#else
    return xTaskGetTickCount();
#endif
}

} // namespace mini_so