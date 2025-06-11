/**
 * @file environment.h
 * @brief Environment singleton for agent management and message routing
 * 
 * Core system manager implementing SIOF-safe singleton pattern.
 */

#pragma once

#include <array>
#include <cstddef>
#include "../types.h"
#include "../config.h"
#include "../platform.h"
#include "../agent/agent_base.h"
#include "../message/message_base.h"
#include "../queue/message_queue.h"

namespace mini_so {

// Forward declarations
template<typename T> class PooledMessage;

// ============================================================================
// Environment - Zero-overhead environment management (SIOF-Safe)
// ============================================================================
class Environment {
private:
    alignas(64) std::array<Agent*, MINI_SO_MAX_AGENTS> agents_;
    std::size_t agent_count_ = 0;
    SemaphoreHandle_t mutex_;
    
    // Performance statistics (conditional compilation)
#if MINI_SO_ENABLE_METRICS
    uint64_t total_messages_sent_ = 0;     // 64-bit for long-term operation
    uint64_t total_messages_processed_ = 0; // 64-bit for long-term operation
    uint32_t max_processing_time_us_ = 0;
#endif
    
    Environment() noexcept;
    ~Environment() noexcept;
    
public:
    // SIOF-Safe Pure Meyers' Singleton
    static Environment& instance() noexcept {
        static Environment env;
        return env;
    }
    
    // SIOF-Safe initialization
    bool initialize() noexcept;
    
    // noexcept Agent management
    AgentId register_agent(Agent* agent) noexcept;
    void unregister_agent(AgentId id) noexcept;
    Agent* get_agent(AgentId id) noexcept;
    
    // Zero-overhead message routing
    template<typename T>
    bool send_message(AgentId sender_id, AgentId target_id, const T& message) noexcept {
        if (target_id >= agent_count_ || !agents_[target_id]) {
            return false;
        }
        
        Message<T> typed_msg(message, sender_id);
        typed_msg.mark_sent();
        
        constexpr uint16_t msg_size = sizeof(Message<T>);
        static_assert(msg_size <= MINI_SO_MAX_MESSAGE_SIZE, "Message too large");
        
        return agents_[target_id]->message_queue_.push(typed_msg, msg_size) == MessageQueue::Result::SUCCESS;
    }
    
    template<typename T>
    void broadcast_message(AgentId sender_id, const T& message) noexcept {
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i] && i != sender_id) {
                send_message(sender_id, static_cast<AgentId>(i), message);
            }
        }
    }
    
    // Pooled message sending (Zero-allocation)
    template<typename T>
    bool send_pooled_message(AgentId sender_id, AgentId target_id, const T& message) noexcept {
        return send_message(sender_id, target_id, message); // Simplified for now
    }
    
    template<typename T>
    void broadcast_pooled_message(AgentId sender_id, const T& message) noexcept {
        broadcast_message(sender_id, message); // Simplified for now
    }
    
    // Optimized system execution
    bool process_one_message() noexcept;
    void process_all_messages() noexcept;
    void run() noexcept;  // Integrated high-performance loop
    
    // constexpr state queries
    constexpr std::size_t agent_count() const noexcept { return agent_count_; }
    std::size_t total_pending_messages() const noexcept;
    
    // Public metrics access (for monitoring)
#if MINI_SO_ENABLE_METRICS
    constexpr uint64_t total_messages_sent() const noexcept { return total_messages_sent_; }
    constexpr uint64_t total_messages_processed() const noexcept { return total_messages_processed_; }
    constexpr uint32_t max_processing_time_us() const noexcept { return max_processing_time_us_; }
#endif
};

} // namespace mini_so