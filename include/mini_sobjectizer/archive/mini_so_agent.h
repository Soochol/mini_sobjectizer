#pragma once

/**
 * @file mini_so_agent.h
 * @brief Agent Framework for Mini SObjectizer
 * 
 * This file contains the Agent base class and related utilities for building
 * actor-based systems. Agents are autonomous entities that communicate through
 * message passing and maintain their own message queues.
 */

#include "mini_so_queue.h"
#include <functional>

namespace mini_so {

// ============================================================================
// Agent Base Class
// ============================================================================

/**
 * @brief Base class for all agents in the system
 * 
 * Agents are the fundamental building blocks of the actor model:
 * - Each agent has a unique ID and dedicated message queue
 * - Agents process messages by implementing handle_message()
 * - Message processing is cooperative and non-preemptive
 * - Agents cannot directly access each other's state
 */
class Agent {
protected:
    AgentId id_ = INVALID_AGENT_ID;  ///< Unique agent identifier

public:
    MessageQueue message_queue_;  ///< Dedicated message queue for this agent
    
    /**
     * @brief Default constructor
     */
    Agent() = default;
    
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~Agent() = default;
    
    // Agents are not copyable but can be movable
    Agent(const Agent&) = delete;
    Agent& operator=(const Agent&) = delete;
    Agent(Agent&&) = default;
    Agent& operator=(Agent&&) = default;
    
    /**
     * @brief Initialize agent with assigned ID
     * @param id Unique identifier assigned by environment
     * 
     * Called by the environment when the agent is registered.
     * Override this method to perform agent-specific initialization.
     */
    virtual void initialize(AgentId id) noexcept {
        id_ = id;
    }
    
    /**
     * @brief Get agent's unique identifier
     * @return Agent ID or INVALID_AGENT_ID if not initialized
     */
    AgentId id() const noexcept { 
        return id_; 
    }
    
    /**
     * @brief Check if agent is properly initialized
     * @return true if agent has valid ID
     */
    bool is_initialized() const noexcept {
        return id_ != INVALID_AGENT_ID;
    }
    
    /**
     * @brief Pure virtual message handler
     * @param msg Message to process
     * @return true if message was handled successfully
     * 
     * This is the core method that defines agent behavior. Implementations
     * should:
     * - Check message type and cast appropriately
     * - Process the message content
     * - Return true for successful processing
     * - Return false for unhandled or invalid messages
     * 
     * Note: This method should be efficient and non-blocking to maintain
     * system responsiveness.
     */
    virtual bool handle_message(const MessageBase& msg) noexcept = 0;
    
    /**
     * @brief Process all pending messages
     * @return Number of messages processed
     * 
     * Processes messages from the queue until empty or maximum limit reached.
     * This method is called by the environment during the main loop.
     */
    std::size_t process_messages() noexcept;
    
    /**
     * @brief Process a single message if available
     * @return true if a message was processed
     * 
     * Attempts to process one message from the queue. Useful for
     * fine-grained control over message processing.
     */
    bool process_one_message() noexcept;
    
    /**
     * @brief Get current message queue statistics
     * @return MessageQueueStats structure with current statistics
     */
    struct MessageQueueStats {
        std::size_t current_size;     ///< Current number of messages
        std::size_t capacity;         ///< Maximum queue capacity
        std::size_t total_processed;  ///< Total messages processed (if metrics enabled)
        std::size_t total_dropped;    ///< Total messages dropped (if metrics enabled)
    };
    
    MessageQueueStats get_queue_stats() const noexcept;
    
    /**
     * @brief Agent lifecycle hook - called when agent starts
     * 
     * Override this method to perform initialization that requires
     * the agent to be fully registered in the environment.
     */
    virtual void on_start() noexcept {}
    
    /**
     * @brief Agent lifecycle hook - called when agent stops
     * 
     * Override this method to perform cleanup before the agent
     * is removed from the environment.
     */
    virtual void on_stop() noexcept {}
    
    /**
     * @brief Agent lifecycle hook - called on periodic basis
     * 
     * Override this method to perform periodic tasks like heartbeat
     * generation or maintenance operations.
     */
    virtual void on_tick() noexcept {}

#if MINI_SO_ENABLE_METRICS
private:
    std::size_t total_messages_processed_ = 0;  ///< Performance counter
    std::size_t total_messages_dropped_ = 0;    ///< Error counter
    TimePoint last_activity_time_ = 0;          ///< Last message processing time
    
public:
    /**
     * @brief Get total messages processed by this agent
     * @return Number of successfully processed messages
     */
    std::size_t total_messages_processed() const noexcept {
        return total_messages_processed_;
    }
    
    /**
     * @brief Get total messages dropped by this agent
     * @return Number of dropped messages (queue full, etc.)
     */
    std::size_t total_messages_dropped() const noexcept {
        return total_messages_dropped_;
    }
    
    /**
     * @brief Get time of last activity
     * @return Timestamp of last message processing
     */
    TimePoint last_activity_time() const noexcept {
        return last_activity_time_;
    }
#endif
};

// ============================================================================
// Agent Utilities
// ============================================================================

/**
 * @brief CRTP base class for typed message handling
 * 
 * Provides type-safe message handling with automatic casting.
 * Derive from this class to get convenient typed message handlers.
 * 
 * @tparam Derived The derived agent class
 */
template<typename Derived>
class TypedAgent : public Agent {
public:
    /**
     * @brief Type-safe message handler dispatcher
     * @param msg Message to process
     * @return true if message was handled
     * 
     * Automatically dispatches messages to type-specific handlers.
     */
    bool handle_message(const MessageBase& msg) noexcept override {
        return static_cast<Derived*>(this)->dispatch_message(msg);
    }

protected:
    /**
     * @brief Handle typed message
     * @tparam T Message data type
     * @param msg Typed message
     * @return true if handled successfully
     * 
     * Override this method in derived classes for specific message types.
     * Default implementation returns false (unhandled).
     */
    template<typename T>
    bool handle_typed_message(const Message<T>& msg) noexcept {
        (void)msg;  // Suppress unused parameter warning
        return false;  // Default: message not handled
    }
    
    /**
     * @brief Dispatch message to appropriate handler
     * @param msg Message to dispatch
     * @return true if message was handled
     * 
     * This method should be implemented by derived classes to route
     * messages to appropriate type-specific handlers.
     */
    bool dispatch_message(const MessageBase& msg) noexcept {
        // Default implementation - derived class should override
        return handle_typed_message(static_cast<const Message<int>&>(msg));
    }
};

/**
 * @brief Simple agent implementation for function-based behavior
 * 
 * Allows creating agents with lambda functions or function pointers
 * without defining new classes.
 */
class FunctionAgent : public Agent {
public:
    using HandlerFunction = std::function<bool(const MessageBase&)>;
    
    /**
     * @brief Construct agent with handler function
     * @param handler Function to call for message processing
     */
    explicit FunctionAgent(HandlerFunction handler) noexcept
        : handler_(std::move(handler)) {}
    
    /**
     * @brief Handle message using provided function
     * @param msg Message to process
     * @return Result from handler function
     */
    bool handle_message(const MessageBase& msg) noexcept override {
        if (handler_) {
            return handler_(msg);
        }
        return false;
    }

private:
    HandlerFunction handler_;
};

// ============================================================================
// Helper Macros
// ============================================================================

/**
 * @brief Macro to declare typed message handler
 * @param MessageType The message data type
 * 
 * Use this in your agent class declaration to declare a handler
 * for a specific message type.
 */
#define DECLARE_MESSAGE_HANDLER(MessageType) \
    bool handle_##MessageType(const Message<MessageType>& msg) noexcept

/**
 * @brief Macro to implement message dispatcher
 * 
 * Use this in your agent implementation to create a dispatcher
 * that routes messages to appropriate handlers based on type ID.
 */
#define IMPLEMENT_MESSAGE_DISPATCHER() \
    bool dispatch_message(const MessageBase& msg) noexcept override { \
        switch (msg.type_id()) {

/**
 * @brief Macro to add case for message type in dispatcher
 * @param MessageType The message data type
 */
#define HANDLE_MESSAGE_TYPE(MessageType) \
        case MESSAGE_TYPE_ID(MessageType): \
            return handle_##MessageType(static_cast<const Message<MessageType>&>(msg));

/**
 * @brief Macro to end message dispatcher implementation
 */
#define END_MESSAGE_DISPATCHER() \
        default: \
            return false; \
        } \
    }

} // namespace mini_so