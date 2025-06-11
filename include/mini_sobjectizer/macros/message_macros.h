/**
 * @file message_macros.h
 * @brief User-friendly macros for message handling
 * 
 * Simplified macros for common message operations and agent definitions.
 */

#pragma once

#include "../message/message_types.h"

namespace mini_so {

// Forward declarations
class MessageBase;
template<typename T> class Message;

} // namespace mini_so

// ============================================================================
// Message Handling Macros
// ============================================================================

// 1. Message handling simplification macros
#define MINI_SO_HANDLE_MESSAGE(Type, handler) \
    if (msg.type_id() == MESSAGE_TYPE_ID(Type)) { \
        const auto& typed_msg = static_cast<const mini_so::Message<Type>&>(msg); \
        return handler(typed_msg.data); \
    }

#define HANDLE_MESSAGE(Type, handler) MINI_SO_HANDLE_MESSAGE(Type, handler)

// Message handling with automatic return true (for void handlers)
#define MINI_SO_HANDLE_MESSAGE_VOID(Type, handler) \
    if (msg.type_id() == MESSAGE_TYPE_ID(Type)) { \
        const auto& typed_msg = static_cast<const mini_so::Message<Type>&>(msg); \
        handler(typed_msg.data); \
        return true; \
    }

#define HANDLE_MESSAGE_VOID(Type, handler) MINI_SO_HANDLE_MESSAGE_VOID(Type, handler)

// ============================================================================
// Short and Intuitive Alias Macros
// ============================================================================

#define MSG_ID(T) MESSAGE_TYPE_ID(T)
#define CHECK_NO_COLLISIONS(...) ASSERT_NO_TYPE_ID_COLLISIONS(__VA_ARGS__)
#define VERIFY_TYPES() VERIFY_NO_RUNTIME_COLLISIONS()