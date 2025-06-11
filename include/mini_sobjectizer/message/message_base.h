/**
 * @file message_base.h
 * @brief Base message classes and header definitions
 * 
 * Core message infrastructure including optimized 8-byte headers
 * and zero-overhead message base class.
 */

#pragma once

#include "../types.h"
#include "../config.h"
#include "message_types.h"
#include <type_traits>
#include <utility>

namespace mini_so {

// ============================================================================
// Message System - Zero-overhead message system
// ============================================================================

// Optimized message header (minimized to 8 bytes)
struct alignas(8) MessageHeader {
    MessageId type_id;
    AgentId sender_id;
    uint32_t timestamp;
    
    constexpr MessageHeader(MessageId id, AgentId sender = INVALID_AGENT_ID) noexcept
        : type_id(id), sender_id(sender), timestamp(0) {}
        
    void set_timestamp() noexcept { timestamp = now(); }
};

// Zero-overhead message base class
class MessageBase {
public:
    MessageHeader header;
    
    constexpr MessageBase(MessageId id, AgentId sender = INVALID_AGENT_ID) noexcept
        : header(id, sender) {}
    
    // No virtual destructor for memcpy safety
    
    // Zero-overhead accessors (inline)
    constexpr MessageId type_id() const noexcept { return header.type_id; }
    constexpr AgentId sender_id() const noexcept { return header.sender_id; }
    constexpr TimePoint timestamp() const noexcept { return header.timestamp; }
    
    void mark_sent() noexcept { header.set_timestamp(); }
};

// Strongly-typed message template
template<typename T>
class Message : public MessageBase {
public:
    static_assert(std::is_trivially_copyable_v<T>, "Message data must be trivially copyable");
    static_assert(sizeof(T) <= MINI_SO_MAX_MESSAGE_SIZE, "Message data exceeds maximum size");
    
    T data;
    
    constexpr Message(const T& payload, AgentId sender = INVALID_AGENT_ID) noexcept
        : MessageBase(MESSAGE_TYPE_ID(T), sender), data(payload) {}
    
    constexpr Message(T&& payload, AgentId sender = INVALID_AGENT_ID) noexcept
        : MessageBase(MESSAGE_TYPE_ID(T), sender), data(std::move(payload)) {}
    
    // Direct data access
    const T& get_data() const noexcept { return data; }
    T& get_data() noexcept { return data; }
};

} // namespace mini_so