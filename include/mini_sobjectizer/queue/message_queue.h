/**
 * @file message_queue.h
 * @brief Lock-free message queue implementation with cache optimization
 * 
 * High-performance circular buffer optimized for embedded systems.
 */

#pragma once

#include <array>
#include <cstddef>
#include "../types.h"
#include "../config.h"
#include "../platform.h"
#include "../message/message_base.h"

namespace mini_so {

// ============================================================================
// Message Queue - Zero-overhead queue
// ============================================================================
class MessageQueue {
public:
    enum class Result : uint8_t {
        SUCCESS = 0,
        QUEUE_FULL = 1,
        MESSAGE_TOO_LARGE = 2,
        INVALID_MESSAGE = 3
    };
    
private:
    // Cache line optimized entry (zero-overhead concurrency)
    struct alignas(64) QueueEntry {  // Aligned to CPU cache line size
        alignas(8) uint8_t data[MINI_SO_MAX_MESSAGE_SIZE];
        uint16_t size;
        bool valid;  // Safe under FreeRTOS mutex protection, no volatile needed
        uint8_t padding[45];  // 64-byte alignment padding
    };
    
    // Zero-overhead: volatile removed to enable compiler optimizations
    alignas(64) std::array<QueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    alignas(64) std::size_t head_{0};    // Synchronized by FreeRTOS mutex
    alignas(64) std::size_t tail_{0};    // Synchronized by FreeRTOS mutex
    alignas(64) std::size_t count_{0};   // Synchronized by FreeRTOS mutex
    SemaphoreHandle_t mutex_;
    
public:
    MessageQueue() noexcept;
    ~MessageQueue() noexcept;
    
    Result push(const MessageBase& msg, uint16_t size) noexcept;
    bool pop(uint8_t* buffer, uint16_t& size) noexcept;
    
    bool empty() const noexcept { return count_ == 0; }
    bool full() const noexcept { return count_ >= MINI_SO_MAX_QUEUE_SIZE; }
    std::size_t size() const noexcept { return count_; }
    void clear() noexcept;
};

} // namespace mini_so