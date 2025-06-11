/**
 * @file agent_base.h
 * @brief Base agent class and core actor functionality
 * 
 * Fundamental agent implementation for the actor model.
 */

#pragma once

#include "../types.h"
#include "../message/message_base.h"
#include "../queue/message_queue.h"

namespace mini_so {

// ============================================================================
// Agent - Zero-overhead Agent system
// ============================================================================
class Agent {
protected:
    AgentId id_ = INVALID_AGENT_ID;
    
public:
    MessageQueue message_queue_;
    
    Agent() = default;
    virtual ~Agent() = default;
    
    // Pure virtual function - performance optimized
    virtual bool handle_message(const MessageBase& msg) noexcept = 0;
    
    // Agent lifecycle - noexcept guaranteed
    void initialize(AgentId id) noexcept { id_ = id; }
    void process_messages() noexcept;
    
    // Zero-overhead message sending
    template<typename T>
    void send_message(AgentId target_id, const T& message) noexcept;
    
    template<typename T>
    void broadcast_message(const T& message) noexcept;
    
    // Pooled message sending (Agent convenience methods)
    template<typename T>
    void send_pooled_message(AgentId target_id, const T& message) noexcept {
        send_message(target_id, message); // Simplified for now
    }
    
    template<typename T>
    void broadcast_pooled_message(const T& message) noexcept {
        broadcast_message(message); // Simplified for now
    }
    
    // Inline accessors (noexcept guaranteed)
    bool has_messages() const noexcept { return !message_queue_.empty(); }
    constexpr AgentId id() const noexcept { return id_; }
    
    // Performance metrics auto-reporting
    void report_performance(uint32_t processing_time_us, uint32_t message_count = 1) noexcept;
    void heartbeat() noexcept;
};

} // namespace mini_so