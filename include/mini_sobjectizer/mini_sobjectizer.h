#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <array>
#include <type_traits>
#include <atomic>  // CAS 기반 lock-free 구현을 위해 추가

// FreeRTOS includes or mock definitions for testing
#ifdef UNIT_TEST
// Mock FreeRTOS types for unit testing
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;

#ifndef portMAX_DELAY
#define portMAX_DELAY 0xFFFFFFFF
#endif

#ifndef pdTRUE
#define pdTRUE 1
#define pdFALSE 0
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) (x)
#endif

#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000
#endif

// Mock FreeRTOS function declarations for testing
extern "C" {
    SemaphoreHandle_t xSemaphoreCreateMutex(void);
    BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
    BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);
    void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);
    TickType_t xTaskGetTickCount(void);
    void taskDISABLE_INTERRUPTS(void);
}

#else
// Real FreeRTOS includes for embedded target
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#endif

// ============================================================================
// Configuration - Phase 3: Zero-overhead 설정
// ============================================================================
#ifndef MINI_SO_MAX_AGENTS
#define MINI_SO_MAX_AGENTS 16
#endif

#ifndef MINI_SO_MAX_QUEUE_SIZE
#define MINI_SO_MAX_QUEUE_SIZE 64
#endif

#ifndef MINI_SO_MAX_MESSAGE_SIZE
#define MINI_SO_MAX_MESSAGE_SIZE 128
#endif

// Phase 3: 성능 최적화 설정
#ifndef MINI_SO_ENABLE_METRICS
#define MINI_SO_ENABLE_METRICS 1
#endif

#ifndef MINI_SO_ENABLE_VALIDATION
#define MINI_SO_ENABLE_VALIDATION 1
#endif

namespace mini_so {

// ============================================================================
// Core Types - Phase 3: Zero-overhead 타입 시스템
// ============================================================================
using AgentId = uint16_t;
using MessageId = uint16_t;
using Duration = uint32_t;
using TimePoint = uint32_t;

constexpr AgentId INVALID_AGENT_ID = 0xFFFF;
constexpr MessageId INVALID_MESSAGE_ID = 0xFFFF;

// Phase 3: Zero-overhead 시간 함수
inline TimePoint now() noexcept {
#ifdef UNIT_TEST
    static uint32_t test_counter = 1000;
    return test_counter += 10;
#else
    return xTaskGetTickCount();
#endif
}

// ============================================================================
// Message System - Phase 3: Zero-overhead 메시지 시스템
// ============================================================================

// Phase 3: 최적화된 메시지 헤더 (8바이트로 최소화)
struct alignas(8) MessageHeader {
    MessageId type_id;
    AgentId sender_id;
    uint32_t timestamp;
    
    constexpr MessageHeader(MessageId id, AgentId sender = INVALID_AGENT_ID) noexcept
        : type_id(id), sender_id(sender), timestamp(0) {}
        
    void set_timestamp() noexcept { timestamp = now(); }
};

// Phase 3: Zero-overhead 메시지 기본 클래스
class MessageBase {
public:
    MessageHeader header;
    
    constexpr MessageBase(MessageId id, AgentId sender = INVALID_AGENT_ID) noexcept
        : header(id, sender) {}
    
    // virtual ~MessageBase() = default;  // 제거: memcpy 안전성을 위해
    
    // Zero-overhead 접근자 (inline)
    constexpr MessageId type_id() const noexcept { return header.type_id; }
    constexpr AgentId sender_id() const noexcept { return header.sender_id; }
    constexpr TimePoint timestamp() const noexcept { return header.timestamp; }
    
    void mark_sent() noexcept { header.set_timestamp(); }
};

// Phase 3: 컴파일 타임 메시지 타입 ID 생성 (충돌 방지)
namespace detail {
    // constexpr 문자열 해시 함수 (FNV-1a 기반)
    constexpr uint32_t fnv1a_hash(const char* str, std::size_t len) noexcept {
        uint32_t hash = 2166136261u;  // FNV offset basis
        for (std::size_t i = 0; i < len; ++i) {
            hash ^= static_cast<uint32_t>(str[i]);
            hash *= 16777619u;  // FNV prime
        }
        return hash;
    }
    
    // 컴파일 타임 문자열 길이 계산
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
            // 타입명 기반 고유 ID 생성 (__PRETTY_FUNCTION__ 사용)
            constexpr const char* type_name = __PRETTY_FUNCTION__;
            constexpr std::size_t name_len = const_strlen(type_name);
            constexpr uint32_t hash = fnv1a_hash(type_name, name_len);
            
            // 추가 엔트로피: 타입 크기와 특성 조합
            constexpr uint32_t size_factor = static_cast<uint32_t>(sizeof(T));
            constexpr uint32_t trait_factor = std::is_trivially_copyable_v<T> ? 1u : 0u;
            constexpr uint32_t alignment_factor = static_cast<uint32_t>(alignof(T));
            
            // 개선된 해시 조합 (더 나은 엔트로피 분산)
            constexpr uint32_t hash1 = hash ^ (size_factor * 0x9E3779B9u);
            constexpr uint32_t hash2 = hash1 ^ (trait_factor * 0x85EBCA6Bu);
            constexpr uint32_t final_hash = hash2 ^ (alignment_factor * 0xC2B2AE3Du);
            
            // 개선된 16비트 압축 (더 나은 엔트로피 보존)
            // XOR 대신 해시 믹싱 사용
            constexpr uint32_t mixed = ((final_hash >> 16) * 0x85EBCA6Bu) ^ 
                                      ((final_hash & 0xFFFF) * 0x9E3779B9u);
            constexpr uint16_t compressed = static_cast<uint16_t>((mixed >> 16) ^ (mixed & 0xFFFF));
            
            // 0과 0xFFFF는 특수 값으로 예약 (INVALID_MESSAGE_ID 등)
            return compressed == 0 ? 1 : (compressed == 0xFFFF ? 0xFFFE : compressed);
        }
    };
    
    // 컴파일 타임 타입 ID 충돌 검사 (개발 시 사용)
    template<typename T1, typename T2>
    constexpr bool type_ids_collide() noexcept {
        return MessageTypeRegistry<T1>::id() == MessageTypeRegistry<T2>::id();
    }
    
    // 체계적 충돌 검증을 위한 타입 리스트 처리
    template<typename... Types>
    struct TypeCollisionDetector {
        // 모든 타입 쌍에 대해 충돌 검사
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
                return false; // 타입이 하나뿐이면 충돌 없음
            } else {
                return check_pairs_with_first<First, Rest...>() || check_all_pairs<Rest...>();
            }
        }
    };
    
    // 런타임 충돌 검증을 위한 ID 레지스트리
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
        // 타입 등록 (런타임)
        template<typename T>
        static bool register_type() noexcept {
            std::size_t& count = get_registered_count();
            TypeInfo* types = get_registered_types();
            
            if (count >= MAX_REGISTERED_TYPES) {
                return false; // 등록 공간 부족
            }
            
            MessageId new_id = MessageTypeRegistry<T>::id();
            const char* type_name = __PRETTY_FUNCTION__; // 근사치
            
            // 기존 등록된 타입들과 충돌 검사
            for (std::size_t i = 0; i < count; ++i) {
                if (types[i].valid && types[i].id == new_id) {
                    // 충돌 발견!
                    return false;
                }
            }
            
            // 충돌 없음 - 등록
            types[count] = TypeInfo(new_id, type_name, sizeof(T));
            ++count;
            return true;
        }
        
        // 충돌 발생한 타입들 조회
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
        
        // 통계 정보
        static std::size_t registered_count() noexcept { return get_registered_count(); }
        static constexpr std::size_t max_capacity() noexcept { return MAX_REGISTERED_TYPES; }
        
        // 등록 정보 초기화 (테스트용)
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

// 개발 시 타입 ID 충돌 검사 매크로
#define ASSERT_NO_TYPE_ID_COLLISION(T1, T2) \
    static_assert(!mini_so::detail::type_ids_collide<T1, T2>(), \
                  "Message type ID collision detected between " #T1 " and " #T2)

// 체계적 충돌 검증 매크로 (여러 타입을 한번에 검사)
#define ASSERT_NO_TYPE_ID_COLLISIONS(...) \
    static_assert(!mini_so::detail::TypeCollisionDetector<__VA_ARGS__>::has_collisions(), \
                  "Message type ID collision detected in type list: " #__VA_ARGS__)

// 런타임 타입 등록 및 충돌 검증 매크로
#define REGISTER_MESSAGE_TYPE(T) \
    do { \
        bool success = mini_so::detail::TypeIdRegistry::register_type<T>(); \
        if (!success) { \
            printf("WARNING: Type ID collision or registration failure for type: %s (ID: %u)\n", \
                   #T, static_cast<unsigned>(MESSAGE_TYPE_ID(T))); \
        } \
    } while(0)

// 전체 시스템 충돌 검증 매크로  
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

// ============================================================================
// User-Friendly Macros - 사용자 친화적 매크로
// ============================================================================

// 1. 메시지 핸들링 간소화 매크로
#define MINI_SO_HANDLE_MESSAGE(Type, handler) \
    if (msg.type_id() == MESSAGE_TYPE_ID(Type)) { \
        const auto& typed_msg = static_cast<const mini_so::Message<Type>&>(msg); \
        return handler(typed_msg.data); \
    }

#define HANDLE_MESSAGE(Type, handler) MINI_SO_HANDLE_MESSAGE(Type, handler)

// 메시지 핸들링 with 자동 return true (handler가 void인 경우)
#define MINI_SO_HANDLE_MESSAGE_VOID(Type, handler) \
    if (msg.type_id() == MESSAGE_TYPE_ID(Type)) { \
        const auto& typed_msg = static_cast<const mini_so::Message<Type>&>(msg); \
        handler(typed_msg.data); \
        return true; \
    }

#define HANDLE_MESSAGE_VOID(Type, handler) MINI_SO_HANDLE_MESSAGE_VOID(Type, handler)

// 2. 짧고 직관적인 별칭 매크로
#define MSG_ID(T) MESSAGE_TYPE_ID(T)
#define CHECK_NO_COLLISIONS(...) ASSERT_NO_TYPE_ID_COLLISIONS(__VA_ARGS__)
#define VERIFY_TYPES() VERIFY_NO_RUNTIME_COLLISIONS()

// 3. Agent 정의 간소화 매크로
#define MINI_SO_AGENT_BEGIN(ClassName) \
    class ClassName : public mini_so::Agent { \
    public: \
        bool handle_message(const mini_so::MessageBase& msg) noexcept override {

#define MINI_SO_AGENT_END() \
            return false; \
        } \
    };

// 더 간단한 버전
#define DEFINE_AGENT(ClassName) MINI_SO_AGENT_BEGIN(ClassName)
#define END_AGENT() MINI_SO_AGENT_END()

// 4. 시스템 초기화 one-liner 매크로
#define MINI_SO_INIT() \
    mini_so::Environment& env = mini_so::Environment::instance(); \
    mini_so::System& sys = mini_so::System::instance(); \
    sys.initialize()

#define MINI_SO_REGISTER(agent) env.register_agent(&agent)
#define MINI_SO_RUN() env.run()
#define MINI_SO_PROCESS_ALL() env.process_all_messages()

// 5. 메시지 전송 간소화 매크로
#define MINI_SO_SEND(target_id, msg_data) send_message(target_id, msg_data)
#define MINI_SO_BROADCAST(msg_data) broadcast_message(msg_data)
#define MINI_SO_SEND_POOLED(target_id, msg_data) send_pooled_message(target_id, msg_data)
#define MINI_SO_BROADCAST_POOLED(msg_data) broadcast_pooled_message(msg_data)

// 6. 타입 안전성 convenience 매크로
#define MINI_SO_REGISTER_TYPES(...) \
    do { \
        (REGISTER_MESSAGE_TYPE(__VA_ARGS__), ...); \
        VERIFY_NO_RUNTIME_COLLISIONS(); \
    } while(0)

// 7. 디버깅/개발 helper 매크로 (optional)
#ifdef MINI_SO_ENABLE_DEBUG_MACROS
#define MINI_SO_DEBUG_MSG(Type, data) \
    printf("[DEBUG] Sending %s\n", #Type)

#define MINI_SO_TRACE_AGENT(agent_name) \
    printf("[TRACE] Agent %s processing\n", agent_name)
#else
#define MINI_SO_DEBUG_MSG(Type, data) ((void)0)
#define MINI_SO_TRACE_AGENT(agent_name) ((void)0)
#endif

// 8. 시스템 서비스 간소화 매크로
#define MINI_SO_REPORT_ERROR(level, code) \
    mini_so::System::instance().report_error(level, code, id())

#define MINI_SO_HEARTBEAT() \
    mini_so::System::instance().heartbeat(id())

#define MINI_SO_RECORD_PERFORMANCE(time_us, msg_count) \
    mini_so::System::instance().record_performance(time_us, msg_count)

// 9. 새로운 고급 User-Friendly 매크로들
// 자신에게 메시지 전송
#define MINI_SO_SEND_TO_SELF(msg_data) \
    do { \
        mini_so::Environment& env = mini_so::Environment::instance(); \
        env.send_message(id(), id(), msg_data); \
    } while(0)

// 메시지 핸들러 선언 간소화
#define BEGIN_MESSAGE_HANDLER() \
    bool handle_message(const mini_so::MessageBase& msg) noexcept override {

#define END_MESSAGE_HANDLER() \
        return false; \
    }

// 자동 반환 타입 감지 메시지 핸들링 - SFINAE 기반 (더 안정적)
#define HANDLE_MESSAGE_AUTO(Type, handler) \
    if (msg.type_id() == MESSAGE_TYPE_ID(Type)) { \
        const auto& typed_msg = static_cast<const mini_so::Message<Type>&>(msg); \
        handler(typed_msg.data); \
        return true; \
    }

// 조건부 메시지 핸들링
#define HANDLE_MESSAGE_IF(Type, handler, condition) \
    if (msg.type_id() == MESSAGE_TYPE_ID(Type)) { \
        const auto& typed_msg = static_cast<const mini_so::Message<Type>&>(msg); \
        if (condition) { \
            handler(typed_msg.data); \
            return true; \
        } \
    }

// 짧은 별칭들
#define SEND_TO_SELF(msg_data) MINI_SO_SEND_TO_SELF(msg_data)
#define MESSAGE_HANDLER_BEGIN() BEGIN_MESSAGE_HANDLER()
#define MESSAGE_HANDLER_END() END_MESSAGE_HANDLER()

// Phase 3: Zero-overhead 타입이 지정된 메시지
template<typename T>
class Message : public MessageBase {
public:
    T data;
    
    constexpr Message(const T& msg_data, AgentId sender = INVALID_AGENT_ID) noexcept
        : MessageBase(MESSAGE_TYPE_ID(T), sender), data(msg_data) {}
        
    template<typename... Args>
    constexpr Message(AgentId sender, Args&&... args) noexcept
        : MessageBase(MESSAGE_TYPE_ID(T), sender), data(std::forward<Args>(args)...) {}
};

// ============================================================================
// System Messages - Phase 3: 완전한 메시지 기반 시스템
// ============================================================================
namespace system_messages {
    // 오류 보고 - 단순화된 구조
    struct ErrorReport {
        enum Level : uint8_t { INFO = 0, WARNING = 1, CRITICAL = 2 };
        Level level;
        uint32_t error_code;
        AgentId source_agent;
        // context는 성능상 제거, error_code로 충분
    };
    
    // 성능 메트릭 - 핵심만
    struct PerformanceMetric {
        AgentId source_agent;
        uint32_t processing_time_us;  // 마이크로초 정밀도
        uint32_t message_count;
    };
    
    // Heartbeat - 최소화
    struct Heartbeat {
        AgentId source_agent;
    };
    
    // 상태 요청/응답 - 효율적 구조
    struct StatusRequest {
        AgentId requester;
        uint32_t request_id;
    };
    
    struct StatusResponse {
        uint32_t request_id;
        uint32_t agent_count;
        uint32_t message_count;
        uint32_t uptime_ms;
        uint8_t health_level;  // 0=healthy, 1=warning, 2=critical
    };
    
    // 시스템 명령 - 일반화된 명령 구조
    struct SystemCommand {
        enum Type : uint8_t { 
            SHUTDOWN = 0, 
            RESET = 1, 
            SUSPEND = 2, 
            RESUME = 3,
            COLLECT_GARBAGE = 4
        };
        Type command;
        AgentId target_agent;  // INVALID_AGENT_ID면 전체 시스템
        uint32_t parameter;
    };
}

// ============================================================================
// Message Queue - Phase 3: Zero-overhead 큐
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
    // Phase 3: 캐시 라인 최적화된 엔트리 (Zero-overhead 동시성)
    struct alignas(64) QueueEntry {  // CPU 캐시 라인 크기에 맞춤
        alignas(8) uint8_t data[MINI_SO_MAX_MESSAGE_SIZE];
        uint16_t size;
        bool valid;  // FreeRTOS 뮤텍스 보호하에 안전, volatile 불필요
        uint8_t padding[45];  // 64바이트 정렬 패딩
    };
    
    // Zero-overhead: volatile 제거로 컴파일러 최적화 활성화
    alignas(64) std::array<QueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    alignas(64) std::size_t head_{0};    // FreeRTOS 뮤텍스로 동기화
    alignas(64) std::size_t tail_{0};    // FreeRTOS 뮤텍스로 동기화
    alignas(64) std::size_t count_{0};   // FreeRTOS 뮤텍스로 동기화
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

// ============================================================================
// Agent - Phase 3: Zero-overhead Agent 시스템
// ============================================================================
class Agent {
protected:
    AgentId id_ = INVALID_AGENT_ID;
    
public:
    MessageQueue message_queue_;
    
    Agent() = default;
    virtual ~Agent() = default;
    
    // Phase 3: 순수 가상 함수 - 성능 최적화
    virtual bool handle_message(const MessageBase& msg) noexcept = 0;
    
    // Agent 생명주기 - noexcept 보장
    void initialize(AgentId id) noexcept { id_ = id; }
    void process_messages() noexcept;
    
    // Phase 3: Zero-overhead 메시지 전송
    template<typename T>
    void send_message(AgentId target_id, const T& message) noexcept;
    
    template<typename T>
    void broadcast_message(const T& message) noexcept;
    
    // Phase 2.2: 풀링된 메시지 전송 (Agent 편의 메서드)
    template<typename T>
    void send_pooled_message(AgentId target_id, const T& message) noexcept;
    
    template<typename T>
    void broadcast_pooled_message(const T& message) noexcept;
    
    // Phase 3: inline 접근자 (noexcept 보장)
    bool has_messages() const noexcept { return !message_queue_.empty(); }
    constexpr AgentId id() const noexcept { return id_; }
    
    // Phase 3: 성능 메트릭 자동 보고
    void report_performance(uint32_t processing_time_us, uint32_t message_count = 1) noexcept;
    void heartbeat() noexcept;
};

// ============================================================================
// Message Pool - Phase 3: Zero-overhead 메시지 생명주기 최적화
// ============================================================================
namespace detail {
// Lock-free 메시지 풀 (고정 크기)
template<typename T, std::size_t PoolSize>
class MessagePool {
    // 완화된 타입 제약 - virtual 상속을 지원하기 위해 제약 완화
    static_assert(std::is_destructible_v<T>, "Message type must be destructible");
    static_assert(PoolSize > 0 && PoolSize <= 256, "Pool size must be between 1 and 256");
    
private:
    struct PoolEntry {
        alignas(T) uint8_t data[sizeof(T)];
        std::atomic<bool> available;  // CAS를 위한 atomic bool
        
        PoolEntry() noexcept : available(true) {}  // 생성자에서 초기화
    };
    
    alignas(64) std::array<PoolEntry, PoolSize> pool_;
    alignas(64) std::atomic<std::size_t> next_index_{0};  // atomic index
    
public:
    MessagePool() noexcept {
        // PoolEntry 생성자에서 available = true로 이미 초기화됨
        // atomic 변수들도 기본 초기화됨
    }
    
    // 메시지 할당 (True Lock-free with CAS)
    T* allocate() noexcept {
        for (std::size_t attempts = 0; attempts < PoolSize * 2; ++attempts) {
            // 원자적으로 현재 인덱스 읽기
            std::size_t index = next_index_.load(std::memory_order_acquire);
            
            // CAS: available을 true→false로 원자적 변경 시도
            bool expected = true;
            if (pool_[index].available.compare_exchange_weak(
                    expected, false,
                    std::memory_order_acq_rel,    // 성공 시: acquire-release
                    std::memory_order_relaxed)) { // 실패 시: relaxed
                
                // 성공! 다음 인덱스 업데이트 (다른 스레드를 위해)
                std::size_t next_idx = (index + 1) % PoolSize;
                next_index_.store(next_idx, std::memory_order_release);
                
                return reinterpret_cast<T*>(pool_[index].data);
            }
            
            // 실패 시 다음 인덱스로 이동 (다른 스레드와 겹치지 않도록)
            std::size_t next_idx = (index + 1) % PoolSize;
            next_index_.compare_exchange_weak(index, next_idx, std::memory_order_relaxed);
        }
        return nullptr;  // 풀 고갈
    }
    
    // 메시지 해제 (원자적)
    void deallocate(T* ptr) noexcept {
        if (!ptr) return;
        
        for (auto& entry : pool_) {
            if (reinterpret_cast<T*>(entry.data) == ptr) {
                // 원자적으로 available을 true로 설정
                entry.available.store(true, std::memory_order_release);
                return;
            }
        }
    }
    
    // 풀 상태 조회
    constexpr std::size_t capacity() const noexcept { return PoolSize; }
    
    std::size_t available_count() const noexcept {
        std::size_t count = 0;
        for (const auto& entry : pool_) {
            // 원자적으로 available 상태 읽기
            if (entry.available.load(std::memory_order_relaxed)) ++count;
        }
        return count;
    }
};

// 전역 메시지 풀 관리자
template<typename T>
class GlobalMessagePool {
private:
    static constexpr std::size_t DEFAULT_POOL_SIZE = 32;
    static inline MessagePool<Message<T>, DEFAULT_POOL_SIZE> pool_;
    
public:
    static Message<T>* allocate() noexcept {
        return pool_.allocate();
    }
    
    static void deallocate(Message<T>* msg) noexcept {
        pool_.deallocate(msg);
    }
    
    static std::size_t available_count() noexcept {
        return pool_.available_count();
    }
    
    static constexpr std::size_t capacity() noexcept {
        return pool_.capacity();
    }
};
}   // namespace detail

// 풀링된 메시지 래퍼
template<typename T>
class PooledMessage {
private:
    Message<T>* msg_;
    bool owns_message_;
    
public:
    // 풀에서 할당
    static PooledMessage<T> create(const T& data, AgentId sender = INVALID_AGENT_ID) noexcept {
        Message<T>* msg = detail::GlobalMessagePool<T>::allocate();
        if (msg) [[likely]] {
            // Placement new로 메시지 생성
            new (msg) Message<T>(data, sender);
            return PooledMessage<T>(msg, true);
        } else {
            // 풀 고갈 시 스택 할당 (fallback)
            static thread_local Message<T> fallback_msg(data, sender);
            fallback_msg = Message<T>(data, sender);
            return PooledMessage<T>(&fallback_msg, false);
        }
    }
    
    // 이동 생성자
    PooledMessage(PooledMessage&& other) noexcept 
        : msg_(other.msg_), owns_message_(other.owns_message_) {
        other.msg_ = nullptr;
        other.owns_message_ = false;
    }
    
    // 복사 금지
    PooledMessage(const PooledMessage&) = delete;
    PooledMessage& operator=(const PooledMessage&) = delete;
    PooledMessage& operator=(PooledMessage&&) = delete;
    
    // 소멸자에서 자동 해제
    ~PooledMessage() noexcept {
        if (msg_ && owns_message_) {
            detail::GlobalMessagePool<T>::deallocate(msg_);
        }
    }
    
    // 메시지 접근
    const Message<T>& get() const noexcept { return *msg_; }
    Message<T>& get() noexcept { return *msg_; }
    
    const Message<T>* operator->() const noexcept { return msg_; }
    Message<T>* operator->() noexcept { return msg_; }
    
    const Message<T>& operator*() const noexcept { return *msg_; }
    Message<T>& operator*() noexcept { return *msg_; }
    
    bool is_pooled() const noexcept { return owns_message_; }
    
private:
    explicit PooledMessage(Message<T>* msg, bool owns) noexcept 
        : msg_(msg), owns_message_(owns) {}
};

// ============================================================================
// Static Environment - Phase 3: 컴파일 타임 Agent 등록 최적화
// ============================================================================
namespace detail {
    // 컴파일 타임 Agent 카운터
    template<typename... Agents>
    struct AgentCounter {
        static constexpr std::size_t count = sizeof...(Agents);
    };
    
    // 컴파일 타임 Agent 타입 검증
    template<typename T>
    constexpr bool is_valid_agent() noexcept {
        return std::is_base_of_v<Agent, T> && !std::is_abstract_v<T>;
    }
    
    // 컴파일 타임 Agent 리스트 유효성 검사
    template<typename... Agents>
    constexpr bool all_valid_agents() noexcept {
        return (is_valid_agent<Agents>() && ...);
    }
}

// Static Environment for compile-time Agent registration
template<typename... StaticAgents>
class StaticEnvironment {
    static_assert(detail::all_valid_agents<StaticAgents...>(), 
                  "All template parameters must be valid Agent types");
    static_assert(sizeof...(StaticAgents) <= MINI_SO_MAX_AGENTS,
                  "Too many static agents defined");
    
private:
    alignas(64) std::array<Agent*, MINI_SO_MAX_AGENTS> agents_;
    std::size_t agent_count_ = 0;
    SemaphoreHandle_t mutex_;
    static inline bool env_initialized_ = false;
    
public:
    StaticEnvironment() noexcept {
        mutex_ = xSemaphoreCreateMutex();
        if (!mutex_) [[unlikely]] {
            // 현대적 Fail-Safe: 헤더에서는 간단히 처리
            taskDISABLE_INTERRUPTS();
            for(;;);
        }
        
        for (auto& agent : agents_) {
            agent = nullptr;
        }
    }
    
    ~StaticEnvironment() noexcept {
        if (mutex_) {
            vSemaphoreDelete(mutex_);
        }
    }
    
    // 컴파일 타임 Agent 등록
    template<typename T>
    constexpr AgentId register_static_agent(T& agent) noexcept {
        static_assert(detail::is_valid_agent<T>(), "T must be a valid Agent type");
        
        // 컴파일 타임에 Agent 타입이 리스트에 있는지 확인
        static_assert(((std::is_same_v<T, StaticAgents>) || ...), 
                      "Agent type must be in the StaticAgents list");
        
        // 런타임 등록 (컴파일 타임 검증 완료 후)
        return register_agent_impl(&agent);
    }
    
    // 동적 Agent 등록 (기존 호환성)
    AgentId register_agent(Agent* agent) noexcept {
        return register_agent_impl(agent);
    }
    
    // 나머지 메서드들은 기존과 동일 (인라인 구현)
    void unregister_agent(AgentId id) noexcept {
        if (id >= agent_count_) [[unlikely]] return;
        
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            if (agents_[id]) {
                agents_[id]->message_queue_.clear();
                agents_[id] = nullptr;
            }
            xSemaphoreGive(mutex_);
        }
    }
    
    Agent* get_agent(AgentId id) noexcept {
        if (id >= agent_count_) [[unlikely]] {
            return nullptr;
        }
        return agents_[id];
    }
    
    template<typename T>
    bool send_message(AgentId sender_id, AgentId target_id, const T& message) noexcept {
        if (target_id >= agent_count_ || !agents_[target_id]) [[unlikely]] {
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
    
    // Phase 2.2: 풀링된 메시지 전송 (Zero-allocation)
    template<typename T>
    bool send_pooled_message(AgentId sender_id, AgentId target_id, const T& message) noexcept {
        if (target_id >= agent_count_ || !agents_[target_id]) [[unlikely]] {
            return false;
        }
        
        // 풀에서 메시지 할당
        auto pooled_msg = PooledMessage<T>::create(message, sender_id);
        pooled_msg->mark_sent();
        
        constexpr uint16_t msg_size = sizeof(Message<T>);
        static_assert(msg_size <= MINI_SO_MAX_MESSAGE_SIZE, "Message too large");
        
        return agents_[target_id]->message_queue_.push(pooled_msg.get(), msg_size) == MessageQueue::Result::SUCCESS;
    }
    
    template<typename T>
    void broadcast_pooled_message(AgentId sender_id, const T& message) noexcept {
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i] && i != sender_id) {
                send_pooled_message(sender_id, static_cast<AgentId>(i), message);
            }
        }
    }
    
    bool process_one_message() noexcept {
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i] && !agents_[i]->message_queue_.empty()) {
                agents_[i]->process_messages();
                return true;
            }
        }
        return false;
    }
    
    void process_all_messages() noexcept {
        bool has_messages = true;
        while (has_messages) {
            has_messages = false;
            for (std::size_t i = 0; i < agent_count_; ++i) {
                if (agents_[i] && !agents_[i]->message_queue_.empty()) {
                    agents_[i]->process_messages();
                    has_messages = true;
                }
            }
        }
    }
    
    void run() noexcept {
        process_all_messages();
    }
    
    bool initialize() noexcept {
        if (env_initialized_) {
            return true;
        }
        env_initialized_ = true;
        return true;
    }
    
    constexpr std::size_t agent_count() const noexcept { return agent_count_; }
    
    std::size_t total_pending_messages() const noexcept {
        std::size_t total = 0;
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i]) {
                total += agents_[i]->message_queue_.size();
            }
        }
        return total;
    }
    
private:
    AgentId register_agent_impl(Agent* agent) noexcept {
        if (!agent) [[unlikely]] {
            return INVALID_AGENT_ID;
        }
        
        if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) [[unlikely]] {
            return INVALID_AGENT_ID;
        }
        
        if (agent_count_ >= MINI_SO_MAX_AGENTS) [[unlikely]] {
            xSemaphoreGive(mutex_);
            return INVALID_AGENT_ID;
        }
        
        AgentId id = static_cast<AgentId>(agent_count_);
        agents_[agent_count_] = agent;
        agent_count_++;
        
        xSemaphoreGive(mutex_);
        
        agent->initialize(id);
        return id;
    }
};

// ============================================================================
// Environment - Phase 3: Zero-overhead 환경 관리 (SIOF-Safe)
// ============================================================================
class Environment {
private:
    alignas(64) std::array<Agent*, MINI_SO_MAX_AGENTS> agents_;
    std::size_t agent_count_ = 0;
    SemaphoreHandle_t mutex_;
    
    // Phase 3: 성능 통계 (조건부 컴파일)
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
    
    // SIOF-Safe 초기화
    bool initialize() noexcept;
    
    // Phase 3: noexcept Agent 관리
    AgentId register_agent(Agent* agent) noexcept;
    void unregister_agent(AgentId id) noexcept;
    Agent* get_agent(AgentId id) noexcept;
    
    // Phase 3: Zero-overhead 메시지 라우팅
    template<typename T>
    bool send_message(AgentId sender_id, AgentId target_id, const T& message) noexcept;
    
    template<typename T>
    void broadcast_message(AgentId sender_id, const T& message) noexcept;
    
    // Phase 2.2: 풀링된 메시지 전송 (Zero-allocation)
    template<typename T>
    bool send_pooled_message(AgentId sender_id, AgentId target_id, const T& message) noexcept {
        if (target_id >= agent_count_ || !agents_[target_id]) [[unlikely]] {
            return false;
        }
        
        // 풀에서 메시지 할당
        auto pooled_msg = PooledMessage<T>::create(message, sender_id);
        pooled_msg->mark_sent();
        
        constexpr uint16_t msg_size = sizeof(Message<T>);
        static_assert(msg_size <= MINI_SO_MAX_MESSAGE_SIZE, "Message too large");
        
        return agents_[target_id]->message_queue_.push(pooled_msg.get(), msg_size) == MessageQueue::Result::SUCCESS;
    }
    
    template<typename T>
    void broadcast_pooled_message(AgentId sender_id, const T& message) noexcept {
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i] && i != sender_id) {
                send_pooled_message(sender_id, static_cast<AgentId>(i), message);
            }
        }
    }
    
    // Phase 3: 최적화된 시스템 실행
    bool process_one_message() noexcept;
    void process_all_messages() noexcept;
    void run() noexcept;  // 통합된 고성능 루프
    
    // Phase 3: constexpr 상태 조회
    constexpr std::size_t agent_count() const noexcept { return agent_count_; }
    std::size_t total_pending_messages() const noexcept;
    
    // Phase 3: 공개 메트릭 접근 (모니터링용)
#if MINI_SO_ENABLE_METRICS
    constexpr uint64_t total_messages_sent() const noexcept { return total_messages_sent_; }
    constexpr uint64_t total_messages_processed() const noexcept { return total_messages_processed_; }
    constexpr uint32_t max_processing_time_us() const noexcept { return max_processing_time_us_; }
#endif

private:
    // static 포인터 제거 (InitializationGuard가 상태 관리)
};

// ============================================================================
// Emergency System Recovery - 현대적 Fail-Safe 메커니즘
// ============================================================================
namespace emergency {
    // 크리티컬 실패 원인
    enum class CriticalFailure : uint32_t {
        MUTEX_CREATION_FAILED = 0x1001,
        MEMORY_EXHAUSTION = 0x1002,
        HEAP_CORRUPTION = 0x1003,
        FREERTOS_INIT_FAILED = 0x1004,
        SYSTEM_AGENT_FAILURE = 0x1005
    };
    
    // 실패 컨텍스트 정보
    struct FailureContext {
        CriticalFailure reason;
        const char* file;
        int line;
        const char* function;
        TimePoint timestamp;
        uint32_t heap_free_bytes;
        uint32_t stack_usage;
        char additional_info[64];
        
        FailureContext() noexcept 
            : reason(CriticalFailure::MUTEX_CREATION_FAILED)
            , file(nullptr)
            , line(0)
            , function(nullptr)
            , timestamp(0)
            , heap_free_bytes(0)
            , stack_usage(0) {
            additional_info[0] = '\0';
        }
    };
    
    // Emergency mode 상태
    struct EmergencyState {
        bool is_active;
        TimePoint entered_time;
        uint32_t restart_countdown_ms;
        bool restart_scheduled;
        FailureContext last_failure;
        
        EmergencyState() noexcept 
            : is_active(false)
            , entered_time(0)
            , restart_countdown_ms(0)
            , restart_scheduled(false) {}
    };
    
    // 실패 컨텍스트 저장 (Flash 메모리 등에 저장)
    void save_failure_context(CriticalFailure reason, const char* file, int line, const char* function = nullptr) noexcept;
    
    // Emergency mode 진입
    void enter_emergency_mode() noexcept;
    
    // 제어된 재시작 스케줄링
    void schedule_controlled_restart(uint32_t delay_ms) noexcept;
    
    // Emergency mode에서 최소 기능 실행
    void run_emergency_loop() noexcept;
    
    // 시스템 상태 체크
    bool is_emergency_mode() noexcept;
    
    // 마지막 실패 정보 조회
    const FailureContext& get_last_failure() noexcept;
    
    // 실패 로그 출력 (UART 등)
    void emergency_log_failure(const FailureContext& context) noexcept;
}

// ============================================================================
// System Services - Phase 3: 순수 Agent 기반 시스템 서비스
// ============================================================================

// Phase 3: 오류 처리 Agent - 순수 메시지 기반
class ErrorAgent : public Agent {
private:
    struct ErrorEntry {
        system_messages::ErrorReport::Level level;
        uint32_t error_code;
        AgentId source_agent;
        TimePoint timestamp;
    };
    
    std::array<ErrorEntry, 32> error_log_;
    std::size_t error_count_ = 0;
    std::size_t next_index_ = 0;  // 순환 버퍼 인덱스
    system_messages::ErrorReport::Level max_level_ = system_messages::ErrorReport::INFO;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override;
    void report_error(system_messages::ErrorReport::Level level, uint32_t code, AgentId source) noexcept;
    
    constexpr std::size_t error_count() const noexcept { return error_count_; }
    constexpr std::size_t logged_errors() const noexcept { 
        // 실제 로그된 에러 수 (최대 32개)
        return std::min(error_count_, error_log_.size()); 
    }
    constexpr system_messages::ErrorReport::Level max_level() const noexcept { return max_level_; }
    constexpr bool is_healthy() const noexcept { return max_level_ <= system_messages::ErrorReport::WARNING; }
};

// Phase 3: 성능 모니터링 Agent - 고성능 메트릭
class PerformanceAgent : public Agent {
private:
    uint32_t max_processing_time_us_ = 0;
    uint64_t total_messages_ = 0;      // 64-bit for long-term operation (584M years)
    uint32_t cycle_count_ = 0;
    TimePoint last_report_time_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override;
    void record_performance(uint32_t processing_time_us, uint32_t message_count) noexcept;
    
    constexpr uint32_t max_processing_time_us() const noexcept { return max_processing_time_us_; }
    constexpr uint64_t total_messages() const noexcept { return total_messages_; }
    constexpr uint32_t cycle_count() const noexcept { return cycle_count_; }
};

// Phase 3: Watchdog Agent - 효율적 모니터링
class WatchdogAgent : public Agent {
private:
    struct MonitoredAgent {
        AgentId agent_id;
        TimePoint last_heartbeat;
        Duration timeout_ms;
        bool active;
    };
    
    std::array<MonitoredAgent, MINI_SO_MAX_AGENTS> monitored_;
    std::size_t monitored_count_ = 0;
    Duration default_timeout_ms_ = 5000;
    TimePoint last_check_time_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override;
    void register_for_monitoring(AgentId agent_id, Duration timeout_ms = 0) noexcept;
    void check_timeouts() noexcept;
    
    constexpr bool is_healthy() const noexcept;
    constexpr std::size_t monitored_count() const noexcept { return monitored_count_; }
};

// ============================================================================
// Initialization Order Management - Phase 3: 컴파일 타임 의존성 해결
// ============================================================================
namespace detail {
    // 초기화 상태 추적
    enum class InitState : uint8_t {
        NOT_INITIALIZED = 0,
        INITIALIZING = 1,
        INITIALIZED = 2,
        FAILED = 3
    };
    
    // CRTP 기반 초기화 순서 보장
    template<typename Derived>
    class InitializationGuard {
    private:
        static inline InitState state_ = InitState::NOT_INITIALIZED;
        static inline uint32_t init_attempt_count_ = 0;
        
    public:
        static bool is_initialized() noexcept {
            return state_ == InitState::INITIALIZED;
        }
        
        static bool is_failed() noexcept {
            return state_ == InitState::FAILED;
        }
        
        static bool ensure_initialized() noexcept {
            // 이미 초기화됨
            if (state_ == InitState::INITIALIZED) [[likely]] {
                return true;
            }
            
            // 실패 상태면 재시도 금지
            if (state_ == InitState::FAILED) [[unlikely]] {
                return false;
            }
            
            // 순환 초기화 방지
            if (state_ == InitState::INITIALIZING) [[unlikely]] {
                state_ = InitState::FAILED;
                return false;
            }
            
            // 너무 많은 재시도 방지
            if (++init_attempt_count_ > 3) [[unlikely]] {
                state_ = InitState::FAILED;
                return false;
            }
            
            // 초기화 시작
            state_ = InitState::INITIALIZING;
            
            bool success = Derived::do_initialize();
            state_ = success ? InitState::INITIALIZED : InitState::FAILED;
            
            return success;
        }
        
        static void reset() noexcept {
            state_ = InitState::NOT_INITIALIZED;
            init_attempt_count_ = 0;
        }
    };
    
    // 초기화 의존성 체크
    template<typename... Dependencies>
    constexpr bool check_dependencies_initialized() noexcept {
        return (Dependencies::is_initialized() && ...);
    }
    
    // 의존성 기반 초기화
    template<typename... Dependencies>
    bool ensure_dependencies_initialized() noexcept {
        return (Dependencies::ensure_initialized() && ...);
    }
}

// ============================================================================
// System - Phase 3: SIOF-Safe 시스템 관리자 (의존성 보장)
// ============================================================================
class System {
private:
    ErrorAgent error_agent_;
    PerformanceAgent performance_agent_;
    WatchdogAgent watchdog_agent_;
    
    AgentId error_agent_id_ = INVALID_AGENT_ID;
    AgentId performance_agent_id_ = INVALID_AGENT_ID;
    AgentId watchdog_agent_id_ = INVALID_AGENT_ID;
    
    bool initialized_ = false;
    
    System() = default;
    
public:
    // SIOF-Safe Pure Meyers' Singleton
    static System& instance() noexcept {
        static System system;
        return system;
    }
    
    // SIOF-Safe 초기화 (의존성 보장)
    bool initialize() noexcept;
    
    void shutdown() noexcept;
    constexpr bool is_initialized() const noexcept { return initialized_; }
    
    // Phase 3: 직접 Agent 접근 (Zero-overhead)
    constexpr ErrorAgent& error() noexcept { return error_agent_; }
    constexpr PerformanceAgent& performance() noexcept { return performance_agent_; }
    constexpr WatchdogAgent& watchdog() noexcept { return watchdog_agent_; }
    
    // Phase 3: 편의 메서드 (inline으로 Zero-overhead)
    void report_error(system_messages::ErrorReport::Level level, uint32_t code, AgentId source = INVALID_AGENT_ID) noexcept {
        error_agent_.report_error(level, code, source);
    }
    
    void record_performance(uint32_t processing_time_us, uint32_t message_count = 1) noexcept {
        performance_agent_.record_performance(processing_time_us, message_count);
    }
    
    void heartbeat(AgentId agent_id) noexcept {
        system_messages::Heartbeat hb{agent_id};
        Environment::instance().send_message(agent_id, watchdog_agent_id_, hb);
    }
    
    // Phase 3: 시스템 상태 조회 (constexpr)
    constexpr bool is_healthy() const noexcept {
        return error_agent_.is_healthy() && watchdog_agent_.is_healthy();
    }
    
private:
    // static 포인터 제거 (InitializationGuard가 상태 관리)
};

// ============================================================================
// Template Implementations - Phase 3: Zero-overhead 구현
// ============================================================================

template<typename T>
inline void Agent::send_message(AgentId target_id, const T& message) noexcept {
    Environment::instance().send_message(id_, target_id, message);
}

template<typename T>
inline void Agent::broadcast_message(const T& message) noexcept {
    Environment::instance().broadcast_message(id_, message);
}

// Phase 2.2: Agent 풀링된 메시지 전송 구현
template<typename T>
inline void Agent::send_pooled_message(AgentId target_id, const T& message) noexcept {
    Environment::instance().send_pooled_message(id_, target_id, message);
}

template<typename T>
inline void Agent::broadcast_pooled_message(const T& message) noexcept {
    Environment::instance().broadcast_pooled_message(id_, message);
}

template<typename T>
inline bool Environment::send_message(AgentId sender_id, AgentId target_id, const T& message) noexcept {
    if (target_id >= agent_count_ || !agents_[target_id]) [[unlikely]] {
        return false;
    }
    
    // Create message with explicit heap allocation to avoid stack issues
    alignas(8) static thread_local uint8_t msg_buffer[MINI_SO_MAX_MESSAGE_SIZE];
    
    // Construct message in static buffer
    Message<T>* typed_msg = new (msg_buffer) Message<T>(message, sender_id);
    typed_msg->mark_sent();  // 타임스탬프 설정
    
    constexpr uint16_t msg_size = sizeof(Message<T>);
    static_assert(msg_size <= MINI_SO_MAX_MESSAGE_SIZE, "Message too large");
    
#if MINI_SO_ENABLE_METRICS
    total_messages_sent_++;
#endif
    
    // Pass the buffer directly to avoid any reference issues
    MessageQueue::Result result = agents_[target_id]->message_queue_.push(*typed_msg, msg_size);
    
    // Explicitly destroy the object (though not strictly necessary for POD types)
    typed_msg->~Message<T>();
    
    return result == MessageQueue::Result::SUCCESS;
}

template<typename T>
inline void Environment::broadcast_message(AgentId sender_id, const T& message) noexcept {
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && i != sender_id) {
            send_message(sender_id, static_cast<AgentId>(i), message);
        }
    }
}

// Phase 3: 인라인 성능 메서드들
inline void Agent::report_performance(uint32_t processing_time_us, uint32_t message_count) noexcept {
    System::instance().record_performance(processing_time_us, message_count);
}

inline void Agent::heartbeat() noexcept {
    System::instance().heartbeat(id_);
}

constexpr bool WatchdogAgent::is_healthy() const noexcept {
    // 모니터링 중인 Agent가 없거나, 적어도 하나가 활성 상태면 건강함
    return monitored_count_ == 0 || 
           [this]() constexpr {
               for (std::size_t i = 0; i < monitored_count_; ++i) {
                   if (monitored_[i].active) return true;
               }
               return false;
           }();
}

} // namespace mini_so