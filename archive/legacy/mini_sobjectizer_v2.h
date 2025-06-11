#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <array>
#include <type_traits>
#include <atomic>
#include <mutex>

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

#ifdef STM32F1
#include "stm32f1xx_hal.h"
#elif defined(STM32F103xE)
#include "stm32f1xx_hal.h"
#endif

// Configuration macros
#ifndef MINI_SO_MAX_AGENTS
#define MINI_SO_MAX_AGENTS 16
#endif

#ifndef MINI_SO_MAX_QUEUE_SIZE
#define MINI_SO_MAX_QUEUE_SIZE 64
#endif

#ifndef MINI_SO_MAX_MESSAGE_SIZE
#define MINI_SO_MAX_MESSAGE_SIZE 128
#endif

#ifndef MINI_SO_MAX_STATES
#define MINI_SO_MAX_STATES 16
#endif

#ifndef MINI_SO_MAX_TIMERS
#define MINI_SO_MAX_TIMERS 8
#endif

namespace mini_so {

// Forward declarations for system agents
class SystemServiceManager;

// ============================================================================
// Error Handling System
// ============================================================================
enum class ErrorCode : uint8_t {
    SUCCESS = 0,
    MUTEX_CREATION_FAILED,
    QUEUE_OVERFLOW,
    MESSAGE_TOO_LARGE,
    INVALID_MESSAGE,
    TIMER_ALLOCATION_FAILED,
    INVALID_STATE_TRANSITION,
    AGENT_REGISTRATION_FAILED,
    SYSTEM_OVERLOAD,
    MEMORY_ALLOCATION_FAILED,
    MESSAGE_CORRUPTION_DETECTED,
    MESSAGE_CHECKSUM_FAILED,
    MESSAGE_SEQUENCE_ERROR,
    MESSAGE_FORMAT_INVALID
};

enum class SystemHealth : uint8_t {
    HEALTHY = 0,
    WARNING,
    CRITICAL,
    FAILED
};

// Forward declaration for compatibility layer
class SystemErrorAgent;
class SystemServiceManager;

class ErrorHandler {
public:
    // 하위 호환성을 위한 static 인터페이스 - 내부적으로 SystemErrorAgent에 위임
    static void report_error(ErrorCode code, const char* context = nullptr);
    static ErrorCode get_last_error();
    static uint32_t get_error_count();
    static uint32_t get_last_error_timestamp();
    static SystemHealth get_system_health();
    static void reset_error_state();
    static const char* error_code_to_string(ErrorCode code);
    
private:
    // Agent 기반 구현에 위임하는 헬퍼 함수들
    static SystemErrorAgent* get_error_agent();
};

// ============================================================================
// System Metrics
// ============================================================================
// Forward declaration for compatibility layer
class SystemMetricsAgent;

class SystemMetrics {
public:
    // 하위 호환성을 위한 static 인터페이스 - 내부적으로 SystemMetricsAgent에 위임
    static void increment_messages_sent();
    static void increment_messages_processed();
    static void report_timer_overrun();
    static void update_max_queue_depth(uint32_t depth);
    static void reset_metrics();
    
    static uint32_t get_messages_sent();
    static uint32_t get_messages_processed();
    static uint32_t get_timer_overruns();
    static uint32_t get_max_queue_depth();
    static uint32_t get_pending_messages();
    
private:
    static SystemMetricsAgent* get_metrics_agent();
};

// Forward declaration for MessageValidator
class MessageValidator;

// Forward declarations
class Agent;
class Environment;
class MessageBase;
template<typename T> class Message;

// Basic types
using AgentId = uint16_t;
using MessageId = uint16_t;
using StateId = uint8_t;
using TimerId = uint8_t;
using MessageSize = uint16_t;

constexpr AgentId INVALID_AGENT_ID = 0xFFFF;
constexpr MessageId INVALID_MESSAGE_ID = 0xFFFF;
constexpr StateId INVALID_STATE_ID = 0xFF;

// Time types
using TimePoint = uint32_t;
using Duration = uint32_t;

inline TimePoint now() {
#if defined(STM32F1) || defined(STM32F103xE)
    return HAL_GetTick();
#elif defined(UNIT_TEST)
    // For unit testing, return incrementing counter (thread-safe)
    static std::atomic<uint32_t> test_counter{1000};
    return test_counter.fetch_add(1, std::memory_order_relaxed);
#else
    return xTaskGetTickCount();
#endif
}

// ============================================================================
// Message Type Registration System
// ============================================================================
namespace detail {
    class MessageIdGenerator {
        static uint16_t counter_;
        
    public:
        static MessageId generate() {
            return counter_++;
        }
        
        static void reset() {
            counter_ = 1;
        }
    };
    
    // Compile-time message type ID
    template<typename T>
    struct MessageTypeRegistry {
        static MessageId id() {
            static MessageId msg_id = []() -> MessageId {
                // Generate ID based on type characteristics
                std::size_t hash = sizeof(T) * 31 + 
                                  (std::is_trivially_copyable<T>::value ? 1 : 0) * 7;
                return static_cast<MessageId>(hash & 0x7FFF);
            }();
            return msg_id;
        }
    };
}

#define MESSAGE_TYPE_ID(T) (mini_so::detail::MessageTypeRegistry<T>::id())

// Enhanced message header with integrity verification
struct MessageHeader {
    MessageId type_id;
    AgentId sender_id;
    TimePoint timestamp;
    uint32_t sequence_number;
    uint32_t checksum;
    MessageSize payload_size;
    uint8_t version;
    uint8_t flags;
    
    MessageHeader(MessageId id, AgentId sender = INVALID_AGENT_ID) 
        : type_id(id), sender_id(sender), timestamp(now()),
          sequence_number(0), checksum(0), payload_size(0),
          version(1), flags(0) {}
};

// Base message class with integrity verification
class MessageBase {
public:
    MessageHeader header;
    
    MessageBase(MessageId id, AgentId sender = INVALID_AGENT_ID);
    
    virtual ~MessageBase() = default;
    
    // Backward compatibility accessors
    MessageId type_id() const { return header.type_id; }
    AgentId sender_id() const { return header.sender_id; }
    TimePoint timestamp() const { return header.timestamp; }
    uint32_t sequence_number() const { return header.sequence_number; }
    uint32_t checksum() const { return header.checksum; }
    
    // Integrity verification
    void calculate_and_set_checksum(MessageSize total_size);
    bool verify_integrity(MessageSize total_size) const;
};

// Typed message wrapper
template<typename T>
class Message : public MessageBase {
public:
    T data;
    
    Message(const T& msg_data, AgentId sender = INVALID_AGENT_ID)
        : MessageBase(MESSAGE_TYPE_ID(T), sender), data(msg_data) {}
};

// ============================================================================
// Message Integrity Verification System
// ============================================================================

class MessageValidator {
private:
    static std::atomic<uint32_t> message_sequence_counter_;
    static std::atomic<uint32_t> corrupted_messages_detected_;
    static std::atomic<uint32_t> checksum_failures_;
    
    // CRC32 lookup table for fast checksum calculation
    static const uint32_t crc32_table_[256];
    
    // Initialize CRC32 table
    static void init_crc32_table();
    static bool crc32_table_initialized_;
    
public:
    // Message integrity verification
    static uint32_t calculate_crc32(const void* data, size_t length);
    static uint32_t calculate_message_checksum(const MessageBase& msg, MessageSize size);
    static bool verify_message_integrity(const MessageBase& msg, MessageSize size, uint32_t expected_checksum);
    
    // Message format validation
    static bool validate_message_format(const MessageBase& msg, MessageSize size);
    static bool validate_message_type(MessageId type_id);
    static bool validate_message_size(MessageSize size);
    
    // Sequence validation
    static uint32_t get_next_sequence_number();
    static bool validate_message_sequence(uint32_t sequence_number);
    
    // Statistics
    static uint32_t get_corrupted_message_count() { 
        return corrupted_messages_detected_.load(std::memory_order_relaxed); 
    }
    static uint32_t get_checksum_failure_count() { 
        return checksum_failures_.load(std::memory_order_relaxed); 
    }
    static void reset_validation_stats();
    
    // System initialization
    static void initialize();
};

// ============================================================================
// Minimal Watchdog System (Phase 1-4)
// ============================================================================

class MinimalWatchdog {
public:
    // 최소 인터페이스 - 메모리 사용량 최소화 - SystemWatchdogAgent로 위임
    static bool init(uint32_t timeout_ms);
    static void kick();
    static void enable(bool state);
    static bool is_expired();
    static uint32_t time_since_last_kick();
    
    // 내부 콜백 - TimerManager에서 호출
    static void watchdog_callback();
    
    // 시스템 복구 인터페이스 (최소)
    static void trigger_system_recovery();
    
    // BasicRecovery에서 접근 허용
    friend class BasicRecovery;
    
private:
    static class SystemWatchdogAgent* get_watchdog_agent();
};

// ============================================================================
// Basic System Recovery (Phase 1-5)
// ============================================================================

class BasicRecovery {
public:
    enum RecoveryAction : uint8_t {
        RESET_AGENTS = 1,
        CLEAR_QUEUES = 2, 
        RESTART_TIMERS = 4,
        FULL_RESET = 7  // 위 3개 모두
    };
    
private:
    static std::atomic<uint8_t> recovery_pending_;
    
public:
    static void trigger_recovery(RecoveryAction action);
    static bool execute_pending_recovery(); // 메인 루프에서 호출
    static void reset_all_agents();
    static void clear_all_queues(); 
    static void restart_system_timers();
};

// ============================================================================
// Minimal Performance Monitor (Phase 1-6)
// ============================================================================

class MinimalPerformanceMonitor {
public:
    // 최소 성능 메트릭만 추적 - PerformanceMonitorAgent로 위임
    static void record_message_latency(uint32_t latency_ms);
    static void record_processing_time(uint32_t processing_time_ms);
    static void increment_loop_count();
    
    // 조회 인터페이스
    static uint32_t get_max_message_latency_ms();
    static uint32_t get_max_processing_time_ms();
    static uint32_t get_loop_count();
    
    // 통계 리셋
    static void reset_stats();
    
private:
    static class PerformanceMonitorAgent* get_performance_agent();
};

// ============================================================================
// Timer System (Phase 1)
// ============================================================================

// Timer callback type for better type safety
using TimerCallback = std::function<void()>;

// Timer state enum for clear state management
enum class TimerState : uint8_t {
    INACTIVE = 0,
    ACTIVE = 1,
    EXPIRED = 2,
    CANCELLED = 3
};

// Timer structure with overflow-safe tick handling
struct Timer {
    TimerId id;
    TickType_t start_tick;      // FreeRTOS tick when started
    TickType_t period_ticks;    // Period in FreeRTOS ticks
    TimerState state;
    bool periodic;
    TimerCallback callback;
    
    Timer() : id(0), start_tick(0), period_ticks(0), 
              state(TimerState::INACTIVE), periodic(false) {}
    
    Timer(TimerId timer_id, TickType_t period, bool is_periodic, TimerCallback cb)
        : id(timer_id), start_tick(0), period_ticks(period), 
          state(TimerState::INACTIVE), periodic(is_periodic), callback(cb) {}
    
    // Overflow-safe timer expiration check using FreeRTOS tick arithmetic
    bool is_expired(TickType_t current_tick) const {
        if (state != TimerState::ACTIVE) return false;
        
        // Handle 32-bit tick overflow correctly
        TickType_t elapsed = current_tick - start_tick;
        return elapsed >= period_ticks;
    }
    
    void start(TickType_t current_tick) {
        start_tick = current_tick;
        state = TimerState::ACTIVE;
    }
    
    void stop() {
        state = TimerState::CANCELLED;
    }
    
    void expire() {
        state = TimerState::EXPIRED;
    }
    
    void reset_for_periodic(TickType_t current_tick) {
        if (periodic && state == TimerState::EXPIRED) {
            start_tick = current_tick;
            state = TimerState::ACTIVE;
        }
    }
    
    // Convert milliseconds to FreeRTOS ticks safely
    static TickType_t ms_to_ticks(Duration ms) {
        // Protect against overflow
        if (ms > (portMAX_DELAY / configTICK_RATE_HZ * 1000)) {
            return portMAX_DELAY;
        }
        return pdMS_TO_TICKS(ms);
    }
};

// Timer Manager - Thread-safe timer management
class TimerManager {
private:
    std::array<Timer, MINI_SO_MAX_TIMERS> timers_;
    std::atomic<TimerId> next_timer_id_{1};
    SemaphoreHandle_t mutex_;
    std::atomic<std::size_t> active_timer_count_{0};
    
    // Internal helper methods
    TimerId allocate_timer_id();
    Timer* find_timer_slot();
    Timer* find_timer_by_id(TimerId id);
    void cleanup_expired_timers();
    
public:
    TimerManager();
    ~TimerManager();
    
    // Public API - Thread-safe timer operations
    TimerId start_timer(Duration duration_ms, TimerCallback callback, bool periodic = false);
    bool cancel_timer(TimerId timer_id);
    void update_timers();  // Called from FreeRTOS tick or main loop
    
    // Timer information
    std::size_t active_timer_count() const { 
        return active_timer_count_.load(std::memory_order_acquire); 
    }
    bool is_timer_active(TimerId timer_id) const;
    
    // System maintenance
    void cleanup_all_timers();
    
    // Disable copy/move for singleton-like behavior
    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;
};

// ============================================================================
// State Machine System (Phase 1)
// ============================================================================

// State transition callback types
using StateEntryCallback = std::function<void()>;
using StateExitCallback = std::function<void()>;
using StateMessageHandler = std::function<bool(const MessageBase&)>;

// State information structure
struct StateInfo {
    StateId id;
    StateId parent_id;
    const char* name;
    StateEntryCallback on_enter;
    StateExitCallback on_exit;
    StateMessageHandler message_handler;
    bool active;
    
    StateInfo() : id(INVALID_STATE_ID), parent_id(INVALID_STATE_ID), 
                  name(nullptr), active(false) {}
    
    StateInfo(StateId state_id, const char* state_name, StateId parent = INVALID_STATE_ID)
        : id(state_id), parent_id(parent), name(state_name), active(false) {}
};

// State Machine Manager - handles hierarchical states
class StateMachine {
private:
    std::array<StateInfo, MINI_SO_MAX_STATES> states_;
    std::size_t state_count_;
    std::atomic<StateId> current_state_{INVALID_STATE_ID};
    std::atomic<StateId> previous_state_{INVALID_STATE_ID};
    std::atomic<bool> in_transition_{false};
    
public:
    StateMachine();
    
    // State definition methods
    StateId define_state(StateId id, const char* name, StateId parent = INVALID_STATE_ID);
    bool set_state_entry_handler(StateId state_id, StateEntryCallback handler);
    bool set_state_exit_handler(StateId state_id, StateExitCallback handler);
    bool set_state_message_handler(StateId state_id, StateMessageHandler handler);
    
    // State transition methods
    bool transition_to(StateId new_state);
    bool is_in_state(StateId state_id) const;
    bool is_child_of(StateId child_state, StateId parent_state) const;
    
    // Message handling
    bool handle_message_in_current_state(const MessageBase& msg);
    
    // State information
    StateId current_state() const { 
        return current_state_.load(std::memory_order_acquire); 
    }
    StateId previous_state() const { 
        return previous_state_.load(std::memory_order_acquire); 
    }
    bool is_transitioning() const { 
        return in_transition_.load(std::memory_order_acquire); 
    }
    const char* current_state_name() const;
    
    // Debugging and maintenance
    std::size_t state_count() const { return state_count_; }
    bool validate_state_machine() const;
    
private:
    // Internal helper methods
    StateInfo* find_state(StateId id);
    const StateInfo* find_state(StateId id) const;
    bool is_valid_state(StateId id) const;
    void execute_entry_chain(StateId target_state);
    void execute_exit_chain(StateId current_state, StateId target_state);
    StateId find_common_parent(StateId state1, StateId state2) const;
};

// ============================================================================
// Message Queue
// ============================================================================
class MessageQueue {
public:
    enum class PushResult {
        SUCCESS,
        QUEUE_FULL,
        MESSAGE_TOO_LARGE,
        INVALID_MESSAGE,
        MUTEX_TIMEOUT,
        MEMORY_ERROR
    };
    
private:
    struct QueueEntry {
        alignas(4) uint8_t data[MINI_SO_MAX_MESSAGE_SIZE];
        MessageSize size;
        bool valid;
    };
    
    std::array<QueueEntry, MINI_SO_MAX_QUEUE_SIZE> queue_;
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
    std::atomic<std::size_t> count_{0};
    SemaphoreHandle_t mutex_;
    
public:
    MessageQueue();
    ~MessageQueue();
    
    PushResult push(const MessageBase& msg, MessageSize size);
    PushResult push_with_validation(const MessageBase& msg, MessageSize size);
    bool pop(uint8_t* buffer, MessageSize& size);
    bool pop_with_validation(uint8_t* buffer, MessageSize& size);
    
    bool empty() const { 
        return count_.load(std::memory_order_acquire) == 0;
    }
    
    bool full() const { 
        return count_.load(std::memory_order_acquire) >= MINI_SO_MAX_QUEUE_SIZE;
    }
    
    std::size_t size() const { 
        return count_.load(std::memory_order_acquire);
    }
    
    void clear();
};

// ============================================================================
// Agent class
// ============================================================================
class Agent {
protected:
    AgentId id_ = INVALID_AGENT_ID;
    TimerManager* timer_manager_;  // Shared timer manager instance
    StateMachine state_machine_;   // Integrated state machine
    
public:
    MessageQueue message_queue_;
    
    Agent() = default;
    virtual ~Agent() = default;
    
    // Core virtual functions - Enhanced for state machine
    virtual void so_define_states() {}
    virtual void so_on_state_enter(StateId state) { (void)state; }
    virtual void so_on_state_exit(StateId state) { (void)state; }
    virtual bool so_handle_message(const MessageBase& msg) = 0;
    
    // Agent lifecycle
    void initialize(AgentId id);
    void process_messages();
    void process_messages_validated();
    
    // Enhanced State management with state machine
    StateId define_state(const char* name, StateId parent = INVALID_STATE_ID);
    StateId define_state(StateId id, const char* name, StateId parent = INVALID_STATE_ID);
    
    bool transition_to(StateId new_state);
    StateId current_state() const { return state_machine_.current_state(); }
    StateId previous_state() const { return state_machine_.previous_state(); }
    bool in_state(StateId state) const { return state_machine_.is_in_state(state); }
    bool is_child_of(StateId child_state, StateId parent_state) const;
    const char* current_state_name() const;
    
    // State callback registration (user-friendly API)
    template<typename Func>
    bool on_state_enter(StateId state_id, Func&& callback) {
        return state_machine_.set_state_entry_handler(state_id, std::forward<Func>(callback));
    }
    
    template<typename Func>
    bool on_state_exit(StateId state_id, Func&& callback) {
        return state_machine_.set_state_exit_handler(state_id, std::forward<Func>(callback));
    }
    
    template<typename Func>
    bool on_state_message(StateId state_id, Func&& handler) {
        return state_machine_.set_state_message_handler(state_id, std::forward<Func>(handler));
    }
    
    // Timer management - User-friendly API
    TimerId start_timer(Duration duration_ms, TimerCallback callback, bool periodic = false);
    TimerId start_periodic_timer(Duration duration_ms, TimerCallback callback);
    TimerId start_oneshot_timer(Duration duration_ms, TimerCallback callback);
    bool cancel_timer(TimerId timer_id);
    
    // Convenience timer methods with lambda support
    template<typename Func>
    TimerId after(Duration duration_ms, Func&& func) {
        return start_oneshot_timer(duration_ms, std::forward<Func>(func));
    }
    
    template<typename Func>
    TimerId every(Duration duration_ms, Func&& func) {
        return start_periodic_timer(duration_ms, std::forward<Func>(func));
    }
    
    // Message handling
    template<typename T>
    void send_message(AgentId target_id, const T& message);
    
    template<typename T>
    void send_message_validated(AgentId target_id, const T& message);
    
    template<typename T>
    void broadcast_message(const T& message);
    
    template<typename T>
    void broadcast_message_validated(const T& message);
    
    bool has_messages() const { return !message_queue_.empty(); }
    AgentId id() const { return id_; }
    
private:
    // Internal timer manager access
    void set_timer_manager(TimerManager* manager) { timer_manager_ = manager; }
    friend class Environment;  // Allow Environment to set timer manager
};

// Environment - singleton message dispatcher with timer support
class Environment {
    std::array<Agent*, MINI_SO_MAX_AGENTS> agents_;
    std::size_t agent_count_ = 0;
    static Environment* instance_;
    SemaphoreHandle_t mutex_;
    TimerManager timer_manager_;  // Integrated timer management
    bool system_services_initialized_ = false;
    
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
    
    // Agent management
    AgentId register_agent(Agent* agent);
    void unregister_agent(AgentId id);
    Agent* get_agent(AgentId id);
    
    // Message routing
    template<typename T>
    bool send_message(AgentId sender_id, AgentId target_id, const T& message);
    
    template<typename T>
    bool send_message_validated(AgentId sender_id, AgentId target_id, const T& message);
    
    template<typename T>
    void broadcast_message(AgentId sender_id, const T& message);
    
    template<typename T>
    void broadcast_message_validated(AgentId sender_id, const T& message);
    
    // Main processing loop - now includes timer processing
    bool process_one_message();
    void process_all_messages();
    void update_all_timers();  // Process expired timers
    void run_main_loop();      // Combined message + timer + watchdog processing
    
    // System safety (minimal overhead)
    void kick_watchdog();      // Call this in main loop
    bool check_system_recovery(); // Execute any pending recovery
    
    // Timer management (delegated to TimerManager)
    TimerManager& timer_manager() { return timer_manager_; }
    
    // System status
    std::size_t total_pending_messages() const;
    std::size_t agent_count() const { return agent_count_; }
    std::size_t active_timer_count() const;
    
    // System maintenance
    void cleanup_system();  // Clean up all resources
    
    // System service management
    bool initialize_system_services();
    void shutdown_system_services();
    bool are_system_services_initialized() const { return system_services_initialized_; }
};

// ============================================================================
// Template implementations
// ============================================================================

template<typename T>
void Agent::send_message(AgentId target_id, const T& message) {
    Environment::instance().send_message(id_, target_id, message);
}

template<typename T>
void Agent::send_message_validated(AgentId target_id, const T& message) {
    Environment::instance().send_message_validated(id_, target_id, message);
}

template<typename T>
void Agent::broadcast_message(const T& message) {
    Environment::instance().broadcast_message(id_, message);
}

template<typename T>
void Agent::broadcast_message_validated(const T& message) {
    Environment::instance().broadcast_message_validated(id_, message);
}

template<typename T>
bool Environment::send_message(AgentId sender_id, AgentId target_id, const T& message) {
    if (target_id >= agent_count_ || !agents_[target_id]) {
        ErrorHandler::report_error(ErrorCode::AGENT_REGISTRATION_FAILED, "send_message - invalid target");
        return false;
    }
    
    Message<T> typed_msg(message, sender_id);
    MessageSize msg_size = sizeof(Message<T>);
    
    if (msg_size > MINI_SO_MAX_MESSAGE_SIZE) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_TOO_LARGE, "send_message - message too large");
        return false;
    }
    
    auto result = agents_[target_id]->message_queue_.push(typed_msg, msg_size);
    if (result == MessageQueue::PushResult::SUCCESS) {
        SystemMetrics::increment_messages_sent();
    }
    return result == MessageQueue::PushResult::SUCCESS;
}

template<typename T>
void Environment::broadcast_message(AgentId sender_id, const T& message) {
    for (size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && i != sender_id) {
            send_message(sender_id, static_cast<AgentId>(i), message);
        }
    }
}

template<typename T>
bool Environment::send_message_validated(AgentId sender_id, AgentId target_id, const T& message) {
    if (target_id >= agent_count_ || !agents_[target_id]) {
        ErrorHandler::report_error(ErrorCode::AGENT_REGISTRATION_FAILED, "send_message_validated - invalid target");
        return false;
    }
    
    Message<T> typed_msg(message, sender_id);
    MessageSize msg_size = sizeof(Message<T>);
    
    if (msg_size > MINI_SO_MAX_MESSAGE_SIZE) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_TOO_LARGE, "send_message_validated - message too large");
        return false;
    }
    
    auto result = agents_[target_id]->message_queue_.push_with_validation(typed_msg, msg_size);
    if (result == MessageQueue::PushResult::SUCCESS) {
        SystemMetrics::increment_messages_sent();
    }
    return result == MessageQueue::PushResult::SUCCESS;
}

template<typename T>
void Environment::broadcast_message_validated(AgentId sender_id, const T& message) {
    for (size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && i != sender_id) {
            send_message_validated(sender_id, static_cast<AgentId>(i), message);
        }
    }
}

} // namespace mini_so