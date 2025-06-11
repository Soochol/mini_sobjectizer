/**
 * @file emergency.h
 * @brief Emergency system recovery and failure handling
 * 
 * Modern fail-safe mechanisms for critical system failures.
 */

#pragma once

#include <cstdint>
#include "../types.h"

namespace mini_so {

// ============================================================================
// Emergency System Recovery - Modern Fail-Safe Mechanism
// ============================================================================
namespace emergency {
    // Critical failure causes
    enum class CriticalFailure : uint32_t {
        MUTEX_CREATION_FAILED = 0x1001,
        MEMORY_EXHAUSTION = 0x1002,
        HEAP_CORRUPTION = 0x1003,
        FREERTOS_INIT_FAILED = 0x1004,
        SYSTEM_AGENT_FAILURE = 0x1005
    };
    
    // Failure context information
    struct FailureContext {
        CriticalFailure reason;
        const char* file;
        int line;
        const char* function;
        TimePoint timestamp;
        uint32_t heap_free_bytes;
        uint32_t stack_usage;
        char additional_info[64];
        
        FailureContext() noexcept 
            : reason(CriticalFailure::MUTEX_CREATION_FAILED)
            , file(nullptr)
            , line(0)
            , function(nullptr)
            , timestamp(0)
            , heap_free_bytes(0)
            , stack_usage(0) {
            additional_info[0] = '\0';
        }
    };
    
    // Emergency state
    struct EmergencyState {
        bool system_halted;
        bool recovery_attempted;
        CriticalFailure last_failure_reason;
        FailureContext last_failure;
        uint32_t failure_count;
        TimePoint last_failure_time;
        
        EmergencyState() noexcept 
            : system_halted(false)
            , recovery_attempted(false)
            , last_failure_reason(CriticalFailure::MUTEX_CREATION_FAILED)
            , failure_count(0)
            , last_failure_time(0) {}
    };
    
    // Emergency functions
    uint32_t get_free_heap_size() noexcept;
    uint32_t get_stack_usage() noexcept;
    void save_failure_context(CriticalFailure reason, const char* file, int line, const char* function) noexcept;
    void trigger_emergency_stop() noexcept;
    bool attempt_recovery() noexcept;
    const EmergencyState& get_emergency_state() noexcept;
    
} // namespace emergency

} // namespace mini_so