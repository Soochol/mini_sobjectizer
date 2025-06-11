#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <array>
#include <type_traits>

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
// Configuration - 단순화된 설정
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

namespace mini_so {

// ============================================================================
// Core Types - 단순화된 타입 정의
// ============================================================================
using AgentId = uint16_t;
using MessageId = uint16_t;
using Duration = uint32_t;
using TimePoint = uint32_t;

constexpr AgentId INVALID_AGENT_ID = 0xFFFF;
constexpr MessageId INVALID_MESSAGE_ID = 0xFFFF;

// 시간 함수 - 단순화
inline TimePoint now() {
#ifdef UNIT_TEST
    static uint32_t test_counter = 1000;
    return test_counter += 10;
#else
    return xTaskGetTickCount();
#endif
}

// ============================================================================
// Message System - 단순화된 메시지 시스템
// ============================================================================

// 단순화된 메시지 헤더
struct MessageHeader {
    MessageId type_id;
    AgentId sender_id;
    TimePoint timestamp;
    
    MessageHeader(MessageId id, AgentId sender = INVALID_AGENT_ID) 
        : type_id(id), sender_id(sender), timestamp(now()) {}
};

// 기본 메시지 클래스
class MessageBase {
public:
    MessageHeader header;
    
    MessageBase(MessageId id, AgentId sender = INVALID_AGENT_ID)
        : header(id, sender) {}
    
    virtual ~MessageBase() = default;
    
    // 접근자
    MessageId type_id() const { return header.type_id; }
    AgentId sender_id() const { return header.sender_id; }
    TimePoint timestamp() const { return header.timestamp; }
};

// 메시지 타입 ID 생성 - 단순화
namespace detail {
    template<typename T>
    struct MessageTypeRegistry {
        static MessageId id() {
            static MessageId msg_id = []() -> MessageId {
                std::size_t hash = sizeof(T) * 31 + 
                                  (std::is_trivially_copyable<T>::value ? 1 : 0) * 7;
                return static_cast<MessageId>(hash & 0x7FFF);
            }();
            return msg_id;
        }
    };
}

#define MESSAGE_TYPE_ID(T) (mini_so::detail::MessageTypeRegistry<T>::id())

// 타입이 지정된 메시지
template<typename T>
class Message : public MessageBase {
public:
    T data;
    
    Message(const T& msg_data, AgentId sender = INVALID_AGENT_ID)
        : MessageBase(MESSAGE_TYPE_ID(T), sender), data(msg_data) {}
};

// ============================================================================
// System Messages - 시스템 메시지 단순화
// ============================================================================
namespace system_messages {
    // 오류 보고
    struct ErrorReport {
        enum Level : uint8_t { INFO, WARNING, CRITICAL };
        Level level;
        uint32_t error_code;
        const char* context;
    };
    
    // 성능 메트릭
    struct PerformanceMetric {
        AgentId source_agent;
        uint32_t processing_time_ms;
        uint32_t message_count;
        TimePoint timestamp;
    };
    
    // Heartbeat 신호
    struct HeartbeatSignal {
        AgentId source_agent;
        TimePoint timestamp;
    };
    
    // 상태 요청/응답
    struct StatusRequest {
        AgentId requester;
        uint32_t request_id;
    };
    
    struct SystemStatus {
        uint32_t request_id;
        uint32_t total_agents;
        uint32_t total_messages;
        uint32_t uptime_ms;
    };
}

// ============================================================================
// Message Queue - 단순화된 메시지 큐
// ============================================================================
class MessageQueue {
public:
    enum class Result {
        SUCCESS,
        QUEUE_FULL,
        MESSAGE_TOO_LARGE,
        INVALID_MESSAGE
    };
    
private:
    struct QueueEntry {
        alignas(4) uint8_t data[MINI_SO_MAX_MESSAGE_SIZE];
        uint16_t size;
        bool valid;
    };
    
    std::array<QueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;
    SemaphoreHandle_t mutex_;
    
public:
    MessageQueue();
    ~MessageQueue();
    
    Result push(const MessageBase& msg, uint16_t size);
    bool pop(uint8_t* buffer, uint16_t& size);
    
    bool empty() const { return count_ == 0; }
    bool full() const { return count_ >= MINI_SO_MAX_QUEUE_SIZE; }
    std::size_t size() const { return count_; }
    void clear();
};

// ============================================================================
// Agent - 단순화된 Agent 클래스
// ============================================================================
class Agent {
protected:
    AgentId id_ = INVALID_AGENT_ID;
    
public:
    MessageQueue message_queue_;
    
    Agent() = default;
    virtual ~Agent() = default;
    
    // 핵심 가상 함수 - 단순화
    virtual bool handle_message(const MessageBase& msg) = 0;
    
    // Agent 생명주기
    void initialize(AgentId id) { id_ = id; }
    void process_messages();
    
    // 메시지 전송 - 단순화
    template<typename T>
    void send_message(AgentId target_id, const T& message);
    
    template<typename T>
    void broadcast_message(const T& message);
    
    // 상태 조회
    bool has_messages() const { return !message_queue_.empty(); }
    AgentId id() const { return id_; }
};

// ============================================================================
// Environment - 단순화된 환경 관리
// ============================================================================
class Environment {
private:
    std::array<Agent*, MINI_SO_MAX_AGENTS> agents_;
    std::size_t agent_count_ = 0;
    static Environment* instance_;
    SemaphoreHandle_t mutex_;
    
    Environment();
    ~Environment();
    
public:
    static Environment& instance() {
        if (!instance_) {
            static Environment env;
            instance_ = &env;
        }
        return *instance_;
    }
    
    // Agent 관리 - 단순화
    AgentId register_agent(Agent* agent);
    void unregister_agent(AgentId id);
    Agent* get_agent(AgentId id);
    
    // 메시지 라우팅 - 단순화
    template<typename T>
    bool send_message(AgentId sender_id, AgentId target_id, const T& message);
    
    template<typename T>
    void broadcast_message(AgentId sender_id, const T& message);
    
    // 시스템 실행 - 단순화
    bool process_one_message();
    void process_all_messages();
    void run();  // 메인 루프
    
    // 시스템 상태
    std::size_t agent_count() const { return agent_count_; }
    std::size_t total_pending_messages() const;
};

// ============================================================================
// System Services - 단순화된 시스템 서비스
// ============================================================================

// 오류 처리 Agent
class ErrorAgent : public Agent {
private:
    struct ErrorEntry {
        system_messages::ErrorReport::Level level;
        uint32_t error_code;
        const char* context;
        TimePoint timestamp;
    };
    
    std::array<ErrorEntry, 32> error_log_;
    std::size_t error_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) override;
    void report_error(system_messages::ErrorReport::Level level, uint32_t code, const char* context = nullptr);
    std::size_t get_error_count() const { return error_count_; }
};

// 성능 모니터링 Agent
class PerformanceAgent : public Agent {
private:
    uint32_t max_processing_time_ms_ = 0;
    uint32_t total_messages_ = 0;
    uint32_t loop_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) override;
    void record_performance(uint32_t processing_time_ms, uint32_t message_count);
    uint32_t get_max_processing_time() const { return max_processing_time_ms_; }
    uint32_t get_total_messages() const { return total_messages_; }
    uint32_t get_loop_count() const { return loop_count_; }
};

// Watchdog Agent
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
    
public:
    bool handle_message(const MessageBase& msg) override;
    void register_for_monitoring(AgentId agent_id, Duration timeout_ms = 0);
    void check_timeouts();
    bool is_healthy() const;
};

// ============================================================================
// System Manager - 단순화된 시스템 관리
// ============================================================================
class SystemManager {
private:
    ErrorAgent error_agent_;
    PerformanceAgent performance_agent_;
    WatchdogAgent watchdog_agent_;
    
    AgentId error_agent_id_ = INVALID_AGENT_ID;
    AgentId performance_agent_id_ = INVALID_AGENT_ID;
    AgentId watchdog_agent_id_ = INVALID_AGENT_ID;
    
    bool initialized_ = false;
    static SystemManager* instance_;
    
    SystemManager() = default;
    
public:
    static SystemManager& instance() {
        if (!instance_) {
            static SystemManager manager;
            instance_ = &manager;
        }
        return *instance_;
    }
    
    bool initialize();
    void shutdown();
    bool is_initialized() const { return initialized_; }
    
    // 직접 접근 - Phase 2의 핵심
    ErrorAgent& error() { return error_agent_; }
    PerformanceAgent& performance() { return performance_agent_; }
    WatchdogAgent& watchdog() { return watchdog_agent_; }
    
    // 편의 메서드
    void report_error(system_messages::ErrorReport::Level level, uint32_t code, const char* context = nullptr) {
        error_agent_.report_error(level, code, context);
    }
    
    void record_performance(uint32_t processing_time_ms, uint32_t message_count = 1) {
        performance_agent_.record_performance(processing_time_ms, message_count);
    }
    
    void heartbeat(AgentId agent_id) {
        system_messages::HeartbeatSignal signal{agent_id, now()};
        Environment::instance().send_message(agent_id, watchdog_agent_id_, signal);
    }
};

// ============================================================================
// Template Implementations
// ============================================================================

template<typename T>
void Agent::send_message(AgentId target_id, const T& message) {
    Environment::instance().send_message(id_, target_id, message);
}

template<typename T>
void Agent::broadcast_message(const T& message) {
    Environment::instance().broadcast_message(id_, message);
}

template<typename T>
bool Environment::send_message(AgentId sender_id, AgentId target_id, const T& message) {
    if (target_id >= agent_count_ || !agents_[target_id]) {
        return false;
    }
    
    Message<T> typed_msg(message, sender_id);
    uint16_t msg_size = sizeof(Message<T>);
    
    if (msg_size > MINI_SO_MAX_MESSAGE_SIZE) {
        return false;
    }
    
    return agents_[target_id]->message_queue_.push(typed_msg, msg_size) == MessageQueue::Result::SUCCESS;
}

template<typename T>
void Environment::broadcast_message(AgentId sender_id, const T& message) {
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && i != sender_id) {
            send_message(sender_id, static_cast<AgentId>(i), message);
        }
    }
}

} // namespace mini_so