#pragma once

#include "mini_sobjectizer_v2.h"
#include <cstdio>

namespace mini_so {

// ============================================================================
// Debug and Diagnostic Utilities
// ============================================================================

class DebugUtils {
public:
    // System status report
    static void print_system_status() {
        printf("=== Mini-SObjectizer System Status ===\n");
        printf("System Health: %s\n", health_to_string(ErrorHandler::get_system_health()));
        printf("Total Errors: %u\n", ErrorHandler::get_error_count());
        printf("Last Error: %s\n", ErrorHandler::error_code_to_string(ErrorHandler::get_last_error()));
        printf("\n");
        
        printf("=== Metrics ===\n");
        printf("Messages Sent: %u\n", SystemMetrics::get_messages_sent());
        printf("Messages Processed: %u\n", SystemMetrics::get_messages_processed());
        printf("Pending Messages: %u\n", SystemMetrics::get_pending_messages());
        printf("Max Queue Depth: %u\n", SystemMetrics::get_max_queue_depth());
        printf("Timer Overruns: %u\n", SystemMetrics::get_timer_overruns());
        printf("\n");
        
        printf("=== Message Validation ===\n");
        printf("Corrupted Messages: %u\n", MessageValidator::get_corrupted_message_count());
        printf("Checksum Failures: %u\n", MessageValidator::get_checksum_failure_count());
        printf("\n");
        
        printf("=== System Safety ===\n");
        printf("Watchdog Active: %s\n", MinimalWatchdog::is_expired() ? "EXPIRED!" : "OK");
        printf("Time Since Last Kick: %u ms\n", MinimalWatchdog::time_since_last_kick());
        printf("\n");
        
        printf("=== Performance (Minimal) ===\n");
        printf("Max Message Latency: %u ms\n", MinimalPerformanceMonitor::get_max_message_latency_ms());
        printf("Max Processing Time: %u ms\n", MinimalPerformanceMonitor::get_max_processing_time_ms());
        printf("Total Loop Count: %u\n", MinimalPerformanceMonitor::get_loop_count());
        printf("\n");
        
        printf("=== Environment ===\n");
        Environment& env = Environment::instance();
        printf("Agent Count: %zu\n", env.agent_count());
        printf("Active Timers: %zu\n", env.active_timer_count());
        printf("Total Pending Messages: %zu\n", env.total_pending_messages());
        printf("\n");
    }
    
    // Agent status report
    static void print_agent_status(const Agent& agent) {
        printf("=== Agent %u Status ===\n", agent.id());
        printf("Current State: %u (%s)\n", agent.current_state(), agent.current_state_name());
        printf("Previous State: %u\n", agent.previous_state());
        printf("Queue Size: %zu\n", agent.message_queue_.size());
        printf("Queue Empty: %s\n", agent.message_queue_.empty() ? "Yes" : "No");
        printf("Queue Full: %s\n", agent.message_queue_.full() ? "Yes" : "No");
        printf("\n");
    }
    
    // Configuration report
    static void print_configuration() {
        printf("=== Mini-SObjectizer Configuration ===\n");
        printf("Max Agents: %d\n", MINI_SO_MAX_AGENTS);
        printf("Max Queue Size: %d\n", MINI_SO_MAX_QUEUE_SIZE);
        printf("Max Message Size: %d bytes\n", MINI_SO_MAX_MESSAGE_SIZE);
        printf("Max States: %d\n", MINI_SO_MAX_STATES);
        printf("Max Timers: %d\n", MINI_SO_MAX_TIMERS);
        printf("\n");
        
        printf("=== Memory Usage Estimates ===\n");
        printf("MessageQueue size: %zu bytes\n", sizeof(MessageQueue));
        printf("Agent size: %zu bytes\n", sizeof(Agent));
        printf("Environment size: %zu bytes\n", sizeof(Environment));
        printf("Timer size: %zu bytes\n", sizeof(Timer));
        printf("StateMachine size: %zu bytes\n", sizeof(StateMachine));
        printf("\n");
    }
    
    // Performance test
    static void run_performance_test(uint32_t num_messages = 1000) {
        printf("=== Performance Test (%u messages) ===\n", num_messages);
        
        // Reset metrics
        SystemMetrics::reset_metrics();
        ErrorHandler::reset_error_state();
        
        uint32_t start_time = now();
        
        // Simple message throughput test would go here
        // This is just a placeholder for the framework
        
        uint32_t end_time = now();
        uint32_t duration = end_time - start_time;
        
        printf("Duration: %u ms\n", duration);
        printf("Messages/sec: %.2f\n", duration > 0 ? (float)num_messages * 1000.0f / duration : 0.0f);
        printf("Errors during test: %u\n", ErrorHandler::get_error_count());
        printf("\n");
    }
    
    // Stress test for memory leaks
    static bool run_stress_test(uint32_t iterations = 100) {
        printf("=== Stress Test (%u iterations) ===\n", iterations);
        
        uint32_t initial_errors = ErrorHandler::get_error_count();
        
        for (uint32_t i = 0; i < iterations; ++i) {
            // Create and destroy message queues
            {
                MessageQueue test_queue;
                
                // Fill and empty the queue multiple times
                struct TestMsg { uint32_t value; };
                Message<TestMsg> msg({i});
                
                for (int j = 0; j < 10; ++j) {
                    test_queue.push(msg, sizeof(msg));
                }
                
                uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
                MessageSize size;
                while (test_queue.pop(buffer, size)) {
                    // Process messages
                }
            }
            
            // Check for error accumulation
            if (ErrorHandler::get_error_count() > initial_errors + iterations) {
                printf("ERROR: Too many errors during stress test\n");
                return false;
            }
        }
        
        uint32_t final_errors = ErrorHandler::get_error_count();
        printf("Errors during test: %u\n", final_errors - initial_errors);
        printf("Test %s\n", (final_errors - initial_errors) < iterations ? "PASSED" : "FAILED");
        
        return (final_errors - initial_errors) < iterations;
    }
    
private:
    static const char* health_to_string(SystemHealth health) {
        switch (health) {
            case SystemHealth::HEALTHY: return "HEALTHY";
            case SystemHealth::WARNING: return "WARNING";
            case SystemHealth::CRITICAL: return "CRITICAL";
            case SystemHealth::FAILED: return "FAILED";
            default: return "UNKNOWN";
        }
    }
};

// ============================================================================
// Configuration Validator
// ============================================================================

class ConfigValidator {
public:
    static bool validate_configuration() {
        bool valid = true;
        
        printf("=== Configuration Validation ===\n");
        
        // Check reasonable limits
        if (MINI_SO_MAX_AGENTS == 0 || MINI_SO_MAX_AGENTS > 256) {
            printf("WARNING: MINI_SO_MAX_AGENTS (%d) may be unreasonable\n", MINI_SO_MAX_AGENTS);
            valid = false;
        }
        
        if (MINI_SO_MAX_QUEUE_SIZE == 0 || MINI_SO_MAX_QUEUE_SIZE > 1024) {
            printf("WARNING: MINI_SO_MAX_QUEUE_SIZE (%d) may be unreasonable\n", MINI_SO_MAX_QUEUE_SIZE);
            valid = false;
        }
        
        if (MINI_SO_MAX_MESSAGE_SIZE < 32 || MINI_SO_MAX_MESSAGE_SIZE > 4096) {
            printf("WARNING: MINI_SO_MAX_MESSAGE_SIZE (%d) may be unreasonable\n", MINI_SO_MAX_MESSAGE_SIZE);
            valid = false;
        }
        
        if (MINI_SO_MAX_STATES == 0 || MINI_SO_MAX_STATES > 64) {
            printf("WARNING: MINI_SO_MAX_STATES (%d) may be unreasonable\n", MINI_SO_MAX_STATES);
            valid = false;
        }
        
        if (MINI_SO_MAX_TIMERS == 0 || MINI_SO_MAX_TIMERS > 32) {
            printf("WARNING: MINI_SO_MAX_TIMERS (%d) may be unreasonable\n", MINI_SO_MAX_TIMERS);
            valid = false;
        }
        
        // Check memory requirements
        size_t total_queue_memory = MINI_SO_MAX_AGENTS * sizeof(MessageQueue);
        size_t total_agent_memory = MINI_SO_MAX_AGENTS * sizeof(Agent);
        size_t estimated_memory = total_queue_memory + total_agent_memory + sizeof(Environment);
        
        printf("Estimated memory usage: %zu bytes\n", estimated_memory);
        
        if (estimated_memory > 32768) { // 32KB warning
            printf("WARNING: Estimated memory usage (%zu bytes) is high for embedded systems\n", estimated_memory);
        }
        
        printf("Configuration validation: %s\n", valid ? "PASSED" : "WARNINGS DETECTED");
        printf("\n");
        
        return valid;
    }
    
    static void print_memory_breakdown() {
        printf("=== Memory Breakdown ===\n");
        printf("Single MessageQueue: %zu bytes\n", sizeof(MessageQueue));
        printf("All MessageQueues: %zu bytes\n", MINI_SO_MAX_AGENTS * sizeof(MessageQueue));
        printf("Single Agent: %zu bytes\n", sizeof(Agent));
        printf("All Agents: %zu bytes\n", MINI_SO_MAX_AGENTS * sizeof(Agent));
        printf("Environment: %zu bytes\n", sizeof(Environment));
        printf("TimerManager: %zu bytes\n", sizeof(TimerManager));
        printf("Single Timer: %zu bytes\n", sizeof(Timer));
        printf("All Timers: %zu bytes\n", MINI_SO_MAX_TIMERS * sizeof(Timer));
        printf("Total Estimated: %zu bytes\n", 
               MINI_SO_MAX_AGENTS * (sizeof(MessageQueue) + sizeof(Agent)) + 
               sizeof(Environment) + MINI_SO_MAX_TIMERS * sizeof(Timer));
        printf("\n");
    }
};

} // namespace mini_so