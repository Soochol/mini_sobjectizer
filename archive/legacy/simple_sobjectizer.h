#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <array>
#include <type_traits>

// FreeRTOS includes
#ifdef UNIT_TEST
// Mock FreeRTOS types for unit testing
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;
typedef int32_t BaseType_t;

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

extern "C" {
    SemaphoreHandle_t xSemaphoreCreateMutex(void);
    BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait);
    BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore);
    void vSemaphoreDelete(SemaphoreHandle_t xSemaphore);
    TickType_t xTaskGetTickCount(void);
}

#else
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#endif

// Configuration macros - 단순화
#ifndef MINI_SO_MAX_AGENTS
#define MINI_SO_MAX_AGENTS 8
#endif

#ifndef MINI_SO_MAX_QUEUE_SIZE  
#define MINI_SO_MAX_QUEUE_SIZE 32
#endif

#ifndef MINI_SO_MAX_MESSAGE_SIZE
#define MINI_SO_MAX_MESSAGE_SIZE 64
#endif

namespace mini_so {

// ============================================================================
// Pure SObjectizer Design - 최소한의 타입들만
// ============================================================================

// Basic types
using AgentId = uint8_t;
using MessageId = uint16_t;
using MessageSize = uint16_t;

constexpr AgentId INVALID_AGENT_ID = 0xFF;
constexpr MessageId INVALID_MESSAGE_ID = 0xFFFF;

// Time types - 단순화
using TimePoint = uint32_t;

inline TimePoint now() {
#ifdef UNIT_TEST
    static uint32_t test_counter = 1000;
    return test_counter += 1;
#else
    return xTaskGetTickCount();
#endif
}

// ============================================================================
// Message Type System - 단순화
// ============================================================================
namespace detail {
    class MessageIdGenerator {
        static uint16_t counter_;
    public:
        static MessageId generate() { return counter_++; }
    };
    
    template<typename T>
    struct MessageTypeRegistry {
        static MessageId id() {
            static MessageId msg_id = MessageIdGenerator::generate();
            return msg_id;
        }
    };
}

#define MESSAGE_TYPE_ID(T) (mini_so::detail::MessageTypeRegistry<T>::id())

// ============================================================================
// Simple Message System - 글로벌 상태 제거
// ============================================================================

// 단순한 메시지 헤더 - 최소한만
struct MessageHeader {
    MessageId type_id;
    AgentId sender_id;
    TimePoint timestamp;
    
    MessageHeader(MessageId id, AgentId sender = INVALID_AGENT_ID) 
        : type_id(id), sender_id(sender), timestamp(now()) {}
};

// 기본 메시지 클래스 - 단순화
class MessageBase {
public:
    MessageHeader header;
    
    MessageBase(MessageId id, AgentId sender = INVALID_AGENT_ID)
        : header(id, sender) {}
    
    virtual ~MessageBase() = default;
    
    // 단순한 접근자들만
    MessageId type_id() const { return header.type_id; }
    AgentId sender_id() const { return header.sender_id; }
    TimePoint timestamp() const { return header.timestamp; }
};

// 타입 안전한 메시지
template<typename T>
class Message : public MessageBase {
public:
    T data;
    
    Message(const T& msg_data, AgentId sender = INVALID_AGENT_ID)
        : MessageBase(MESSAGE_TYPE_ID(T), sender), data(msg_data) {}
};

// ============================================================================
// Simple Message Queue - 복잡성 제거
// ============================================================================
class SimpleMessageQueue {
private:
    struct QueueEntry {
        alignas(4) uint8_t data[MINI_SO_MAX_MESSAGE_SIZE];
        MessageSize size;
        bool valid;
    };
    
    std::array<QueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    SemaphoreHandle_t mutex_;
    
public:
    SimpleMessageQueue();
    ~SimpleMessageQueue();
    
    // 단순한 인터페이스만
    bool push(const MessageBase& msg, MessageSize size);
    bool pop(uint8_t* buffer, MessageSize& size);
    void clear();
    
    bool empty() const { return count_ == 0; }
    bool full() const { return count_ >= MINI_SO_MAX_QUEUE_SIZE; }
    size_t size() const { return count_; }
};

// ============================================================================
// Simple Agent - 핵심 기능만
// ============================================================================
class SimpleAgent {
protected:
    AgentId id_ = INVALID_AGENT_ID;
    
public:
    SimpleMessageQueue message_queue_;
    
    SimpleAgent() = default;
    virtual ~SimpleAgent() = default;
    
    // 핵심 가상 함수 - 단순화
    virtual bool handle_message(const MessageBase& msg) = 0;
    
    // Agent 생명주기
    void initialize(AgentId id) { id_ = id; }
    void process_messages();
    
    // 단순한 메시지 전송
    template<typename T>
    void send_message(AgentId target_id, const T& message);
    
    template<typename T>
    void broadcast_message(const T& message);
    
    bool has_messages() const { return !message_queue_.empty(); }
    AgentId id() const { return id_; }
};

// ============================================================================
// Simple Environment - 글로벌 상태 최소화
// ============================================================================
class SimpleEnvironment {
private:
    std::array<SimpleAgent*, MINI_SO_MAX_AGENTS> agents_;
    size_t agent_count_ = 0;
    static SimpleEnvironment* instance_;
    SemaphoreHandle_t mutex_;
    
    SimpleEnvironment();
    ~SimpleEnvironment();
    
public:
    static SimpleEnvironment& instance() {
        if (!instance_) {
            static SimpleEnvironment env;
            instance_ = &env;
        }
        return *instance_;
    }
    
    // Agent 관리 - 단순화
    AgentId register_agent(SimpleAgent* agent);
    void unregister_agent(AgentId id);
    SimpleAgent* get_agent(AgentId id);
    
    // 순수 메시지 라우팅만
    template<typename T>
    bool send_message(AgentId sender_id, AgentId target_id, const T& message);
    
    template<typename T>
    void broadcast_message(AgentId sender_id, const T& message);
    
    // 단순한 처리 루프
    bool process_one_message();
    void process_all_messages();
    void run_main_loop(); // 복잡한 기능 없이 순수 메시지 처리만
    
    // 단순한 정보만
    size_t agent_count() const { return agent_count_; }
    size_t total_pending_messages() const;
};

// ============================================================================
// System Service Messages - 모든 것을 메시지로
// ============================================================================
namespace system_messages {
    // 오류 보고 메시지
    struct ErrorReport {
        enum ErrorLevel : uint8_t { INFO, WARNING, ERROR, CRITICAL };
        
        ErrorLevel level;
        AgentId source_agent;
        uint32_t error_code;
        const char* context;
    };
    
    // 성능 메트릭 메시지
    struct PerformanceMetric {
        AgentId source_agent;
        uint32_t processing_time_ms;
        uint32_t message_count;
        uint32_t timestamp;
    };
    
    // 시스템 상태 요청/응답
    struct StatusRequest {
        AgentId requester;
        uint32_t request_id;
    };
    
    struct StatusResponse {
        uint32_t request_id;
        uint32_t uptime_ms;
        uint32_t total_messages;
        uint32_t error_count;
    };
    
    // Watchdog 메시지
    struct HeartbeatSignal {
        AgentId source_agent;
        uint32_t timestamp;
    };
    
    struct WatchdogTimeout {
        AgentId failed_agent;
        uint32_t last_heartbeat;
        uint32_t timeout_duration;
    };
}

// ============================================================================
// System Service Agents - 모든 기능을 Agent로
// ============================================================================

// 오류 처리 Agent
class ErrorHandlerAgent : public SimpleAgent {
private:
    struct ErrorEntry {
        system_messages::ErrorReport::ErrorLevel level;
        AgentId source;
        uint32_t timestamp;
        uint32_t error_code;
    };
    
    std::array<ErrorEntry, 16> error_log_;
    size_t error_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) override;
    
private:
    void handle_error_report(const Message<system_messages::ErrorReport>& msg);
    void handle_status_request(const Message<system_messages::StatusRequest>& msg);
};

// 성능 모니터링 Agent
class PerformanceMonitorAgent : public SimpleAgent {
private:
    uint32_t max_processing_time_ = 0;
    uint32_t total_messages_ = 0;
    uint32_t start_time_;
    
public:
    bool handle_message(const MessageBase& msg) override;
    
private:
    void handle_performance_metric(const Message<system_messages::PerformanceMetric>& msg);
    void handle_status_request(const Message<system_messages::StatusRequest>& msg);
};

// Watchdog Agent
class WatchdogAgent : public SimpleAgent {
private:
    struct AgentHeartbeat {
        AgentId agent_id;
        uint32_t last_heartbeat;
        uint32_t timeout_ms;
        bool active;
    };
    
    std::array<AgentHeartbeat, MINI_SO_MAX_AGENTS> heartbeats_;
    size_t monitored_agents_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) override;
    
    // 주기적으로 호출되어야 함
    void check_timeouts();
    
private:
    void handle_heartbeat(const Message<system_messages::HeartbeatSignal>& msg);
    void register_agent_for_monitoring(AgentId agent_id, uint32_t timeout_ms);
};

// ============================================================================
// Template implementations
// ============================================================================

template<typename T>
void SimpleAgent::send_message(AgentId target_id, const T& message) {
    SimpleEnvironment::instance().send_message(id_, target_id, message);
}

template<typename T>
void SimpleAgent::broadcast_message(const T& message) {
    SimpleEnvironment::instance().broadcast_message(id_, message);
}

template<typename T>
bool SimpleEnvironment::send_message(AgentId sender_id, AgentId target_id, const T& message) {
    if (target_id >= agent_count_ || !agents_[target_id]) {
        return false;
    }
    
    Message<T> typed_msg(message, sender_id);
    MessageSize msg_size = sizeof(Message<T>);
    
    if (msg_size > MINI_SO_MAX_MESSAGE_SIZE) {
        return false;
    }
    
    return agents_[target_id]->message_queue_.push(typed_msg, msg_size);
}

template<typename T>
void SimpleEnvironment::broadcast_message(AgentId sender_id, const T& message) {
    for (size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && i != sender_id) {
            send_message(sender_id, static_cast<AgentId>(i), message);
        }
    }
}

} // namespace mini_so