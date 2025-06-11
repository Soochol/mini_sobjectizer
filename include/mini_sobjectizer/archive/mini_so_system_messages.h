#pragma once

/**
 * @file mini_so_system_messages.h
 * @brief System Message Definitions for Mini SObjectizer
 * 
 * This file contains all system-level message types used for error reporting,
 * performance monitoring, heartbeat tracking, and system coordination.
 */

#include "mini_so_message.h"

namespace mini_so {
namespace system_messages {

// ============================================================================
// Error Reporting Messages
// ============================================================================

/**
 * @brief Error report message
 * 
 * Used to report errors from agents to the system error handler.
 * Optimized structure to minimize memory usage.
 */
struct ErrorReport {
    /**
     * @brief Error severity levels
     */
    enum Level : uint8_t { 
        INFO = 0,     ///< Informational message
        WARNING = 1,  ///< Warning condition
        CRITICAL = 2  ///< Critical error requiring attention
    };
    
    Level level;             ///< Error severity level
    uint32_t error_code;     ///< Application-specific error code
    AgentId source_agent;    ///< Agent that reported the error
    
    /**
     * @brief Construct error report
     * @param lvl Error level
     * @param code Error code  
     * @param source Source agent ID
     */
    constexpr ErrorReport(Level lvl, uint32_t code, AgentId source) noexcept
        : level(lvl), error_code(code), source_agent(source) {}
};

// ============================================================================
// Performance Monitoring Messages
// ============================================================================

/**
 * @brief Performance metric report
 * 
 * Contains performance data from agents for system monitoring.
 */
struct PerformanceMetric {
    AgentId source_agent;        ///< Agent reporting the metric
    uint32_t processing_time_us; ///< Processing time in microseconds
    uint32_t message_count;      ///< Number of messages processed
    uint32_t queue_utilization;  ///< Queue usage percentage (0-100)
    
    /**
     * @brief Construct performance metric
     * @param source Source agent ID
     * @param proc_time Processing time in microseconds
     * @param msg_count Number of messages processed
     * @param queue_util Queue utilization percentage
     */
    constexpr PerformanceMetric(AgentId source, uint32_t proc_time, 
                               uint32_t msg_count, uint32_t queue_util) noexcept
        : source_agent(source), processing_time_us(proc_time), 
          message_count(msg_count), queue_utilization(queue_util) {}
};

// ============================================================================
// System Control Messages
// ============================================================================

/**
 * @brief Heartbeat message
 * 
 * Sent periodically by agents to indicate they are alive and functioning.
 */
struct Heartbeat {
    AgentId source_agent;  ///< Agent sending heartbeat
    TimePoint timestamp;   ///< When heartbeat was generated
    uint32_t sequence;     ///< Sequence number for tracking missed beats
    
    /**
     * @brief Construct heartbeat
     * @param source Source agent ID
     * @param seq Sequence number
     */
    constexpr Heartbeat(AgentId source, uint32_t seq) noexcept
        : source_agent(source), timestamp(0), sequence(seq) {}
};

/**
 * @brief System status request
 * 
 * Request for system status information.
 */
struct StatusRequest {
    AgentId requesting_agent;  ///< Agent requesting status
    uint32_t request_id;       ///< Unique request identifier
    
    /**
     * @brief Construct status request
     * @param requester Requesting agent ID
     * @param req_id Request identifier
     */
    constexpr StatusRequest(AgentId requester, uint32_t req_id) noexcept
        : requesting_agent(requester), request_id(req_id) {}
};

/**
 * @brief System status response
 * 
 * Response containing system status information.
 */
struct StatusResponse {
    uint32_t request_id;           ///< Matching request identifier
    uint32_t total_agents;         ///< Number of active agents
    uint32_t total_messages;       ///< Total messages processed
    uint32_t memory_usage_bytes;   ///< Current memory usage
    uint32_t uptime_ms;           ///< System uptime in milliseconds
    
    /**
     * @brief Construct status response
     * @param req_id Request identifier
     * @param agents Number of agents
     * @param messages Total messages
     * @param memory Memory usage
     * @param uptime System uptime
     */
    constexpr StatusResponse(uint32_t req_id, uint32_t agents, uint32_t messages,
                            uint32_t memory, uint32_t uptime) noexcept
        : request_id(req_id), total_agents(agents), total_messages(messages),
          memory_usage_bytes(memory), uptime_ms(uptime) {}
};

/**
 * @brief System command message
 * 
 * Commands for system-level operations.
 */
struct SystemCommand {
    /**
     * @brief Command types
     */
    enum Type : uint8_t {
        SHUTDOWN = 0,      ///< Graceful system shutdown
        RESTART = 1,       ///< System restart
        CLEAR_STATS = 2,   ///< Clear performance statistics
        ENABLE_DEBUG = 3,  ///< Enable debug mode
        DISABLE_DEBUG = 4  ///< Disable debug mode
    };
    
    Type command_type;     ///< Command to execute
    AgentId source_agent;  ///< Agent issuing command
    uint32_t parameter;    ///< Optional command parameter
    
    /**
     * @brief Construct system command
     * @param cmd Command type
     * @param source Source agent ID
     * @param param Optional parameter
     */
    constexpr SystemCommand(Type cmd, AgentId source, uint32_t param = 0) noexcept
        : command_type(cmd), source_agent(source), parameter(param) {}
};

// ============================================================================
// Watchdog Messages
// ============================================================================

/**
 * @brief Watchdog registration message
 * 
 * Used by agents to register for watchdog monitoring.
 */
struct WatchdogRegister {
    AgentId agent_id;        ///< Agent to monitor
    Duration timeout_ms;     ///< Timeout period in milliseconds
    
    /**
     * @brief Construct watchdog registration
     * @param agent Agent ID to monitor
     * @param timeout Timeout in milliseconds
     */
    constexpr WatchdogRegister(AgentId agent, Duration timeout) noexcept
        : agent_id(agent), timeout_ms(timeout) {}
};

/**
 * @brief Watchdog timeout notification
 * 
 * Sent when an agent fails to send heartbeat within timeout period.
 */
struct WatchdogTimeout {
    AgentId timed_out_agent;    ///< Agent that timed out
    Duration timeout_duration;  ///< Configured timeout duration
    TimePoint last_heartbeat;   ///< Time of last received heartbeat
    
    /**
     * @brief Construct watchdog timeout
     * @param agent Agent that timed out
     * @param timeout_dur Timeout duration
     * @param last_beat Last heartbeat time
     */
    constexpr WatchdogTimeout(AgentId agent, Duration timeout_dur, TimePoint last_beat) noexcept
        : timed_out_agent(agent), timeout_duration(timeout_dur), last_heartbeat(last_beat) {}
};

// ============================================================================
// Message Type Validation
// ============================================================================

// Compile-time validation that all system messages are valid
static_assert(is_valid_message_type_v<ErrorReport>, 
              "ErrorReport must be valid message type");
static_assert(is_valid_message_type_v<PerformanceMetric>, 
              "PerformanceMetric must be valid message type");
static_assert(is_valid_message_type_v<Heartbeat>, 
              "Heartbeat must be valid message type");
static_assert(is_valid_message_type_v<StatusRequest>, 
              "StatusRequest must be valid message type");
static_assert(is_valid_message_type_v<StatusResponse>, 
              "StatusResponse must be valid message type");
static_assert(is_valid_message_type_v<SystemCommand>, 
              "SystemCommand must be valid message type");
static_assert(is_valid_message_type_v<WatchdogRegister>, 
              "WatchdogRegister must be valid message type");
static_assert(is_valid_message_type_v<WatchdogTimeout>, 
              "WatchdogTimeout must be valid message type");

// ============================================================================
// Convenience Type Aliases
// ============================================================================

using ErrorReportMessage = Message<ErrorReport>;
using PerformanceMetricMessage = Message<PerformanceMetric>;
using HeartbeatMessage = Message<Heartbeat>;
using StatusRequestMessage = Message<StatusRequest>;
using StatusResponseMessage = Message<StatusResponse>;
using SystemCommandMessage = Message<SystemCommand>;
using WatchdogRegisterMessage = Message<WatchdogRegister>;
using WatchdogTimeoutMessage = Message<WatchdogTimeout>;

} // namespace system_messages
} // namespace mini_so