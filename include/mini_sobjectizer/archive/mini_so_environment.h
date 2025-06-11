#pragma once

/**
 * @file mini_so_environment.h
 * @brief Environment Management for Mini SObjectizer
 * 
 * This file contains the Environment class responsible for agent lifecycle
 * management, message routing, and system coordination.
 */

#include "mini_so_agent.h"
#include <array>

namespace mini_so {

// ============================================================================
// Environment Class - Agent Lifecycle and Message Routing
// ============================================================================

/**
 * @brief Central environment for agent management and message routing
 * 
 * The Environment class provides:
 * - Agent registration and lifecycle management
 * - Message routing between agents
 * - System-wide message processing coordination
 * - Performance monitoring and statistics
 * 
 * Uses the Singleton pattern to ensure single system-wide instance.
 */
class Environment {
private:
    // Cache-aligned agent storage
    MINI_SO_CACHE_ALIGNED std::array<Agent*, MINI_SO_MAX_AGENTS> agents_;
    std::size_t agent_count_ = 0;
    static Environment* instance_;
    SemaphoreHandle_t mutex_;
    
    // Performance statistics (conditional compilation)
#if MINI_SO_ENABLE_METRICS
    uint32_t total_messages_sent_ = 0;
    uint32_t total_messages_processed_ = 0;
    uint32_t max_processing_time_us_ = 0;
#endif
    
    // Private constructor for singleton
    Environment() noexcept;
    ~Environment() noexcept;
    
    // Initialization flag
    static inline bool env_initialized_ = false;

public:
    // Disable copy operations
    Environment(const Environment&) = delete;
    Environment& operator=(const Environment&) = delete;
    
    /**
     * @brief Get singleton instance
     * @return Reference to the singleton Environment instance
     */
    static Environment& instance() noexcept {
        if (!instance_) {
            static Environment env;
            instance_ = &env;
        }
        return *instance_;
    }
    
    /**
     * @brief Initialize the environment
     * @return true if initialization successful
     * 
     * Must be called before any agent operations.
     * Safe to call multiple times (idempotent).
     */
    bool initialize() noexcept;
    
    // ========================================================================
    // Agent Management
    // ========================================================================
    
    /**
     * @brief Register an agent with the environment
     * @param agent Pointer to agent to register
     * @return Agent ID assigned to the agent, or INVALID_AGENT_ID if failed
     * 
     * Thread-safe operation. The agent will be assigned a unique ID
     * and its initialize() method will be called.
     */
    AgentId register_agent(Agent* agent) noexcept;
    
    /**
     * @brief Unregister an agent from the environment
     * @param id Agent ID to unregister
     * 
     * Thread-safe operation. The agent's message queue will be cleared
     * and the agent will be removed from the system.
     */
    void unregister_agent(AgentId id) noexcept;
    
    /**
     * @brief Get agent by ID
     * @param id Agent ID to retrieve
     * @return Pointer to agent or nullptr if not found
     */
    Agent* get_agent(AgentId id) noexcept;
    
    // ========================================================================
    // Message Routing
    // ========================================================================
    
    /**
     * @brief Send message from one agent to another
     * @tparam T Message data type
     * @param sender_id Sender agent ID
     * @param target_id Target agent ID
     * @param message Message data to send
     * @return true if message was sent successfully
     */
    template<typename T>
    bool send_message(AgentId sender_id, AgentId target_id, const T& message) noexcept {
        if (target_id >= agent_count_ || !agents_[target_id]) {
            return false;
        }
        
        Message<T> typed_msg(message, sender_id);
        typed_msg.mark_sent();
        
        constexpr uint16_t msg_size = sizeof(Message<T>);
        static_assert(msg_size <= MINI_SO_MAX_MESSAGE_SIZE, "Message too large");
        
        auto result = agents_[target_id]->message_queue_.push(typed_msg, msg_size);
        
#if MINI_SO_ENABLE_METRICS
        if (result == MessageQueue::Result::SUCCESS) {
            total_messages_sent_++;
        }
#endif
        
        return result == MessageQueue::Result::SUCCESS;
    }
    
    /**
     * @brief Broadcast message to all agents except sender
     * @tparam T Message data type
     * @param sender_id Sender agent ID
     * @param message Message data to broadcast
     */
    template<typename T>
    void broadcast_message(AgentId sender_id, const T& message) noexcept {
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i] && i != sender_id) {
                send_message(sender_id, static_cast<AgentId>(i), message);
            }
        }
    }
    
    // ========================================================================
    // Message Processing
    // ========================================================================
    
    /**
     * @brief Process one message from any agent
     * @return true if a message was processed
     */
    bool process_one_message() noexcept;
    
    /**
     * @brief Process all pending messages from all agents
     */
    void process_all_messages() noexcept;
    
    /**
     * @brief Main system run loop
     * 
     * Processes all messages and runs system services.
     * This is the main coordination point for the entire system.
     */
    void run() noexcept;
    
    // ========================================================================
    // Status and Statistics (PUBLIC ACCESS)
    // ========================================================================
    
    /**
     * @brief Get number of registered agents
     * @return Current agent count
     */
    constexpr std::size_t agent_count() const noexcept { 
        return agent_count_; 
    }
    
    /**
     * @brief Get total pending messages across all agents
     * @return Total number of unprocessed messages
     */
    std::size_t total_pending_messages() const noexcept;
    
    // Conditional metrics access
#if MINI_SO_ENABLE_METRICS
    /**
     * @brief Get total messages sent through environment
     * @return Total message count
     */
    constexpr uint32_t total_messages_sent() const noexcept { 
        return total_messages_sent_; 
    }
    
    /**
     * @brief Get total messages processed
     * @return Total processed message count
     */
    constexpr uint32_t total_messages_processed() const noexcept { 
        return total_messages_processed_; 
    }
    
    /**
     * @brief Get maximum processing time recorded
     * @return Maximum processing time in microseconds
     */
    constexpr uint32_t max_processing_time_us() const noexcept { 
        return max_processing_time_us_; 
    }
#endif
    
    // ========================================================================
    // System Health
    // ========================================================================
    
    /**
     * @brief Check if environment is healthy
     * @return true if all systems are functioning normally
     */
    bool is_healthy() const noexcept;
    
    /**
     * @brief Get environment statistics
     * @return Structure containing detailed statistics
     */
    struct EnvironmentStats {
        std::size_t agent_count;
        std::size_t total_pending_messages;
        uint32_t total_messages_sent;
        uint32_t total_messages_processed;
        uint32_t max_processing_time_us;
        bool is_healthy;
    };
    
    EnvironmentStats get_stats() const noexcept;
};

} // namespace mini_so