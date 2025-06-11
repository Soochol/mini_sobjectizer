/**
 * @file message_types.h
 * @brief Compile-time message type ID generation and collision detection
 * 
 * Advanced type ID system with collision detection for message safety.
 */

#pragma once

#include <type_traits>
#include <cstddef>
#include <cstdio>
#include "../types.h"
#include "../config.h"

namespace mini_so {

namespace detail {
    // constexpr string hash function (FNV-1a based)
    constexpr uint32_t fnv1a_hash(const char* str, std::size_t len) noexcept {
        uint32_t hash = 2166136261u;  // FNV offset basis
        for (std::size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint32_t>(str[i]);
            hash *= 16777619u;  // FNV prime
        }
        return hash;
    }
    
    // Compile-time string length calculation
    constexpr std::size_t const_strlen(const char* str) noexcept {
        std::size_t len = 0;
        while (str[len] != '\0') {
            ++len;
        }
        return len;
    }
    
    template<typename T>
    struct MessageTypeRegistry {
        static constexpr MessageId id() noexcept {
            // Generate unique ID based on type name (__PRETTY_FUNCTION__)
            constexpr const char* type_name = __PRETTY_FUNCTION__;
            constexpr std::size_t name_len = const_strlen(type_name);
            constexpr uint32_t hash = fnv1a_hash(type_name, name_len);
            
            // Additional entropy: type size and traits combination
            constexpr uint32_t size_factor = static_cast<uint32_t>(sizeof(T));
            constexpr uint32_t trait_factor = std::is_trivially_copyable_v<T> ? 1u : 0u;
            constexpr uint32_t alignment_factor = static_cast<uint32_t>(alignof(T));
            
            // Improved hash combination (better entropy distribution)
            constexpr uint32_t hash1 = hash ^ (size_factor * 0x9E3779B9u);
            constexpr uint32_t hash2 = hash1 ^ (trait_factor * 0x85EBCA6Bu);
            constexpr uint32_t final_hash = hash2 ^ (alignment_factor * 0xC2B2AE3Du);
            
            // Improved 16-bit compression (better entropy preservation)
            // Using hash mixing instead of XOR
            constexpr uint32_t mixed = ((final_hash >> 16) * 0x85EBCA6Bu) ^ 
                                      ((final_hash & 0xFFFF) * 0x9E3779B9u);
            constexpr uint16_t compressed = static_cast<uint16_t>((mixed >> 16) ^ (mixed & 0xFFFF));
            
            // Reserve 0 and 0xFFFF as special values (INVALID_MESSAGE_ID etc.)
            return compressed == 0 ? 1 : (compressed == 0xFFFF ? 0xFFFE : compressed);
        }
    };
    
    // Compile-time type ID collision check (for development use)
    template<typename T1, typename T2>
    constexpr bool type_ids_collide() noexcept {
        return MessageTypeRegistry<T1>::id() == MessageTypeRegistry<T2>::id();
    }
    
    // Systematic collision verification for type lists
    template<typename... Types>
    struct TypeCollisionDetector {
        // Check all type pairs for collisions
        static constexpr bool has_collisions() noexcept {
            return check_all_pairs<Types...>();
        }
        
    private:
        template<typename T1, typename T2, typename... Rest>
        static constexpr bool check_pairs_with_first() noexcept {
            if constexpr (sizeof...(Rest) == 0) {
                return type_ids_collide<T1, T2>();
            } else {
                return type_ids_collide<T1, T2>() || check_pairs_with_first<T1, Rest...>();
            }
        }
        
        template<typename First, typename... Rest>
        static constexpr bool check_all_pairs() noexcept {
            if constexpr (sizeof...(Rest) == 0) {
                return false; // No collision if only one type
            } else {
                return check_pairs_with_first<First, Rest...>() || check_all_pairs<Rest...>();
            }
        }
    };
    
    // Runtime collision verification ID registry
    class TypeIdRegistry {
    private:
        static constexpr std::size_t MAX_REGISTERED_TYPES = 256;
        
        struct TypeInfo {
            MessageId id;
            const char* type_name;
            std::size_t type_size;
            bool valid;
            
            constexpr TypeInfo() noexcept : id(0), type_name(nullptr), type_size(0), valid(false) {}
            constexpr TypeInfo(MessageId id_, const char* name, std::size_t size) noexcept 
                : id(id_), type_name(name), type_size(size), valid(true) {}
        };
        
        static TypeInfo* get_registered_types() noexcept {
            static TypeInfo registered_types[MAX_REGISTERED_TYPES] = {};
            return registered_types;
        }
        
        static std::size_t& get_registered_count() noexcept {
            static std::size_t registered_count = 0;
            return registered_count;
        }
        
    public:
        // Type registration (runtime)
        template<typename T>
        static bool register_type() noexcept {
            std::size_t& count = get_registered_count();
            TypeInfo* types = get_registered_types();
            
            if (count >= MAX_REGISTERED_TYPES) {
                return false; // Registration space exhausted
            }
            
            MessageId new_id = MessageTypeRegistry<T>::id();
            const char* type_name = __PRETTY_FUNCTION__; // Approximation
            
            // Check for collisions with existing registered types
            for (std::size_t i = 0; i < count; ++i) {
                if (types[i].valid && types[i].id == new_id) {
                    // Collision detected!
                    return false;
                }
            }
            
            // No collision - register
            types[count] = TypeInfo(new_id, type_name, sizeof(T));
            ++count;
            return true;
        }
        
        // Find collision IDs
        static std::size_t find_collisions(MessageId* collision_ids, std::size_t max_collisions) noexcept {
            std::size_t collision_count = 0;
            std::size_t count = get_registered_count();
            TypeInfo* types = get_registered_types();
            
            for (std::size_t i = 0; i < count && collision_count < max_collisions; ++i) {
                if (!types[i].valid) continue;
                
                for (std::size_t j = i + 1; j < count; ++j) {
                    if (!types[j].valid) continue;
                    
                    if (types[i].id == types[j].id) {
                        collision_ids[collision_count++] = types[i].id;
                        break;
                    }
                }
            }
            
            return collision_count;
        }
        
        // Statistics
        static std::size_t registered_count() noexcept { return get_registered_count(); }
        static constexpr std::size_t max_capacity() noexcept { return MAX_REGISTERED_TYPES; }
        
        // Reset registration info (for testing)
        static void reset() noexcept {
            TypeInfo* types = get_registered_types();
            for (std::size_t i = 0; i < MAX_REGISTERED_TYPES; ++i) {
                types[i] = TypeInfo{};
            }
            get_registered_count() = 0;
        }
    };
}

#define MESSAGE_TYPE_ID(T) (mini_so::detail::MessageTypeRegistry<T>::id())

// Development-time type ID collision check macros
#define ASSERT_NO_TYPE_ID_COLLISION(T1, T2) \
    static_assert(!mini_so::detail::type_ids_collide<T1, T2>(), \
                  "Message type ID collision detected between " #T1 " and " #T2)

// Systematic collision verification macro (check multiple types at once)
#define ASSERT_NO_TYPE_ID_COLLISIONS(...) \
    static_assert(!mini_so::detail::TypeCollisionDetector<__VA_ARGS__>::has_collisions(), \
                  "Message type ID collision detected in type list: " #__VA_ARGS__)

// Runtime type registration and collision verification macros
#define REGISTER_MESSAGE_TYPE(T) \
    do { \
        bool success = mini_so::detail::TypeIdRegistry::register_type<T>(); \
        if (!success) { \
            printf("WARNING: Type ID collision or registration failure for type: %s (ID: %u)\n", \
                   #T, static_cast<unsigned>(MESSAGE_TYPE_ID(T))); \
        } \
    } while(0)

// System-wide collision verification macro  
#define VERIFY_NO_RUNTIME_COLLISIONS() \
    do { \
        constexpr std::size_t MAX_COLLISIONS = 32; \
        mini_so::MessageId collision_ids[MAX_COLLISIONS]; \
        std::size_t collision_count = mini_so::detail::TypeIdRegistry::find_collisions(collision_ids, MAX_COLLISIONS); \
        if (collision_count > 0) { \
            printf("CRITICAL: %zu type ID collisions detected!\n", collision_count); \
            for (std::size_t i = 0; i < collision_count; ++i) { \
                printf("  Collision ID: %u\n", collision_ids[i]); \
            } \
        } else { \
            printf("✓ No type ID collisions detected (%zu types registered)\n", \
                   mini_so::detail::TypeIdRegistry::registered_count()); \
        } \
    } while(0)

} // namespace mini_so