#pragma once

/**
 * @file mini_so_queue.h
 * @brief Lock-safe Message Queue for Mini SObjectizer
 * 
 * This file contains the implementation of a cache-aligned, FreeRTOS mutex-protected
 * circular buffer for efficient message passing between agents.
 */

#include "mini_so_message.h"
#include <array>
#include <cstring>

namespace mini_so {

// ============================================================================
// Message Queue Implementation
// ============================================================================

/**
 * @brief Thread-safe circular message queue
 * 
 * High-performance message queue optimized for embedded systems:
 * - Cache-aligned data structures for optimal memory access
 * - FreeRTOS mutex protection for thread safety
 * - Fixed-size circular buffer for predictable memory usage
 * - Zero-overhead design with minimal runtime costs
 */
class MessageQueue {
public:
    /**
     * @brief Result codes for queue operations
     */
    enum class Result : uint8_t {
        SUCCESS = 0,        ///< Operation completed successfully
        QUEUE_FULL,         ///< Queue has reached maximum capacity
        QUEUE_EMPTY,        ///< Queue contains no messages
        MESSAGE_TOO_LARGE,  ///< Message exceeds size limit
        INVALID_MESSAGE,    ///< Message validation failed
        MUTEX_ERROR         ///< Mutex operation failed
    };

private:
    /**
     * @brief Cache-aligned queue entry
     * 
     * Each entry is aligned to CPU cache line size (typically 64 bytes)
     * to prevent false sharing and optimize memory access patterns.
     */
    struct alignas(MINI_SO_CACHE_LINE_SIZE) QueueEntry {
        alignas(8) uint8_t data[MINI_SO_MAX_MESSAGE_SIZE];  ///< Message data buffer
        uint16_t size;      ///< Actual message size
        bool valid;         ///< Entry validity flag
        
        // Calculate safe padding size
        static constexpr std::size_t USED_SIZE = MINI_SO_MAX_MESSAGE_SIZE + sizeof(uint16_t) + sizeof(bool);
        static constexpr std::size_t PADDING_SIZE = (USED_SIZE >= MINI_SO_CACHE_LINE_SIZE) ? 0 : 
                                                    (MINI_SO_CACHE_LINE_SIZE - USED_SIZE);
        uint8_t padding[PADDING_SIZE];
    };
    
    // Cache-aligned data members to prevent false sharing
    MINI_SO_CACHE_ALIGNED std::array<QueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    MINI_SO_CACHE_ALIGNED std::size_t head_{0};     ///< Read position
    MINI_SO_CACHE_ALIGNED std::size_t tail_{0};     ///< Write position  
    MINI_SO_CACHE_ALIGNED std::size_t count_{0};    ///< Current message count
    
    SemaphoreHandle_t mutex_;  ///< FreeRTOS mutex for thread safety

public:
    /**
     * @brief Construct message queue
     * 
     * Initializes the circular buffer and creates FreeRTOS mutex.
     * Will trigger system halt if mutex creation fails.
     */
    MessageQueue() noexcept;
    
    /**
     * @brief Destroy message queue
     * 
     * Properly releases FreeRTOS mutex resources.
     */
    ~MessageQueue() noexcept;
    
    // Disable copy operations (queue is not copyable)
    MessageQueue(const MessageQueue&) = delete;
    MessageQueue& operator=(const MessageQueue&) = delete;
    
    // Enable move operations
    MessageQueue(MessageQueue&& other) noexcept;
    MessageQueue& operator=(MessageQueue&& other) noexcept;
    
    /**
     * @brief Add message to queue
     * @param msg Message to add
     * @param size Message size in bytes
     * @return Result code indicating success or failure
     * 
     * Thread-safe operation that adds a message to the tail of the queue.
     * Will fail if queue is full or message is too large.
     */
    Result push(const MessageBase& msg, uint16_t size) noexcept;
    
    /**
     * @brief Remove message from queue
     * @param buffer Buffer to store message data
     * @param size Reference to store actual message size
     * @return true if message was retrieved, false if queue is empty
     * 
     * Thread-safe operation that removes a message from the head of the queue.
     * The buffer must be large enough to hold any possible message.
     */
    bool pop(uint8_t* buffer, uint16_t& size) noexcept;
    
    /**
     * @brief Check if queue is empty
     * @return true if queue contains no messages
     * 
     * Note: This is a snapshot view and may change immediately after return
     * in a multi-threaded environment.
     */
    bool empty() const noexcept { 
        return count_ == 0; 
    }
    
    /**
     * @brief Check if queue is full
     * @return true if queue has reached maximum capacity
     * 
     * Note: This is a snapshot view and may change immediately after return
     * in a multi-threaded environment.
     */
    bool full() const noexcept { 
        return count_ >= MINI_SO_MAX_QUEUE_SIZE; 
    }
    
    /**
     * @brief Get current message count
     * @return Number of messages currently in queue
     * 
     * Note: This is a snapshot view and may change immediately after return
     * in a multi-threaded environment.
     */
    std::size_t size() const noexcept { 
        return count_; 
    }
    
    /**
     * @brief Get maximum queue capacity
     * @return Maximum number of messages the queue can hold
     */
    constexpr std::size_t capacity() const noexcept { 
        return MINI_SO_MAX_QUEUE_SIZE; 
    }
    
    /**
     * @brief Clear all messages from queue
     * 
     * Thread-safe operation that removes all messages and resets
     * the queue to empty state.
     */
    void clear() noexcept;
    
    /**
     * @brief Get memory usage statistics
     * @return Total memory used by this queue in bytes
     */
    constexpr std::size_t memory_usage() const noexcept {
        return sizeof(MessageQueue);
    }

private:
    /**
     * @brief Initialize queue entry to invalid state
     * @param entry Entry to initialize
     */
    void initialize_entry(QueueEntry& entry) noexcept {
        entry.valid = false;
        entry.size = 0;
        std::memset(entry.data, 0, MINI_SO_MAX_MESSAGE_SIZE);
    }
    
    /**
     * @brief Validate queue invariants (debug builds only)
     * @return true if queue state is consistent
     */
    bool validate_invariants() const noexcept {
#ifdef DEBUG
        // Check that count matches actual entries
        std::size_t actual_count = 0;
        for (const auto& entry : queue_) {
            if (entry.valid) {
                actual_count++;
            }
        }
        
        if (actual_count != count_) {
            return false;
        }
        
        // Check head/tail bounds
        if (head_ >= MINI_SO_MAX_QUEUE_SIZE || tail_ >= MINI_SO_MAX_QUEUE_SIZE) {
            return false;
        }
        
        return true;
#else
        return true;
#endif
    }
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Convert queue result to string representation
 * @param result Result code to convert
 * @return Human-readable string
 */
inline const char* result_to_string(MessageQueue::Result result) noexcept {
    switch (result) {
        case MessageQueue::Result::SUCCESS:           return "SUCCESS";
        case MessageQueue::Result::QUEUE_FULL:       return "QUEUE_FULL";
        case MessageQueue::Result::QUEUE_EMPTY:      return "QUEUE_EMPTY";
        case MessageQueue::Result::MESSAGE_TOO_LARGE: return "MESSAGE_TOO_LARGE";
        case MessageQueue::Result::INVALID_MESSAGE:  return "INVALID_MESSAGE";
        case MessageQueue::Result::MUTEX_ERROR:      return "MUTEX_ERROR";
        default:                                     return "UNKNOWN";
    }
}

/**
 * @brief Helper function to push typed messages
 * @tparam T Message data type
 * @param queue Queue to push to
 * @param message Typed message to push
 * @return Result code
 */
template<typename T>
MessageQueue::Result push_message(MessageQueue& queue, const Message<T>& message) noexcept {
    constexpr uint16_t msg_size = sizeof(Message<T>);
    static_assert(msg_size <= MINI_SO_MAX_MESSAGE_SIZE, 
                  "Message size exceeds queue capacity");
    
    return queue.push(message, msg_size);
}

/**
 * @brief Helper function to pop typed messages
 * @tparam T Expected message data type
 * @param queue Queue to pop from
 * @param message Reference to store the popped message
 * @return true if message was retrieved and type matches
 */
template<typename T>
bool pop_message(MessageQueue& queue, Message<T>& message) noexcept {
    alignas(8) uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    uint16_t size;
    
    if (!queue.pop(buffer, size)) {
        return false;
    }
    
    // Verify size matches expected message type
    if (size != sizeof(Message<T>)) {
        return false;
    }
    
    // Safe to cast - size has been verified
    std::memcpy(&message, buffer, size);
    
    // Verify type ID matches (additional safety check)
    return message.type_id() == MESSAGE_TYPE_ID(T);
}

} // namespace mini_so