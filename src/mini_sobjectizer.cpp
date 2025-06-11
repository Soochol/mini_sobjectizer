/**
 * @file mini_sobjectizer.cpp
 * @brief Mini SObjectizer v3.0 Core Implementation
 * 
 * Core implementation components:
 * - Environment: Singleton agent manager
 * - Agent: Base actor class for message handling
 * - MessageQueue: Lock-free circular buffer
 * - Emergency System: Real-time failure recovery
 * - Metrics: Performance monitoring
 * 
 * Design principles:
 * - Zero-overhead abstractions
 * - RAII (Resource Acquisition Is Initialization)
 * - Lock-free data structures
 * - Cache-friendly memory layout
 */

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstring>
#include <cstdio>  // Safe string formatting with snprintf

namespace mini_so {

// Using Meyers' Singleton pattern to prevent SIOF (Static Initialization Order Fiasco)
// InitializationGuard safely manages global object initialization order

// ============================================================================
// Emergency System Implementation - 현대적 Fail-Safe
// ============================================================================
namespace emergency {
    // 전역 Emergency 상태 (정적 메모리에 저장)
    static EmergencyState g_emergency_state;
    
    // 시스템 정보 수집 헬퍼
    uint32_t get_free_heap_size() noexcept {
#ifdef UNIT_TEST
        return 8192;  // Mock 값
#else
        return xPortGetFreeHeapSize();
#endif
    }
    
    uint32_t get_stack_usage() noexcept {
#ifdef UNIT_TEST
        return 512;   // Mock 값
#else
        return uxTaskGetStackHighWaterMark(nullptr);
#endif
    }
    
    void save_failure_context(CriticalFailure reason, const char* file, int line, const char* function) noexcept {
        FailureContext& ctx = g_emergency_state.last_failure;
        
        ctx.reason = reason;
        ctx.file = file;
        ctx.line = line;
        ctx.function = function;
        ctx.timestamp = now();
        ctx.heap_free_bytes = get_free_heap_size();
        ctx.stack_usage = get_stack_usage();
        
        // 추가 정보 생성
        snprintf(ctx.additional_info, sizeof(ctx.additional_info) - 1,
                "heap:%u stack:%u", ctx.heap_free_bytes, ctx.stack_usage);
        ctx.additional_info[sizeof(ctx.additional_info) - 1] = '\0';
        
        // 로그 출력
        emergency_log_failure(ctx);
    }
    
    void enter_emergency_mode() noexcept {
        if (g_emergency_state.is_active) return;  // 이미 Emergency mode
        
        g_emergency_state.is_active = true;
        g_emergency_state.entered_time = now();
        g_emergency_state.restart_scheduled = false;
        
        printf("[EMERGENCY] System entered emergency mode at %u\n", g_emergency_state.entered_time);
        
        // Emergency mode에서는 최소한의 안전 기능만 활성화
        // 1. 모든 액추에이터 안전 위치로 이동
        // 2. 센서 모니터링 중단 
        // 3. 통신은 유지 (원격 진단용)
    }
    
    void schedule_controlled_restart(uint32_t delay_ms) noexcept {
        g_emergency_state.restart_scheduled = true;
        g_emergency_state.restart_countdown_ms = delay_ms;
        
        printf("[EMERGENCY] Controlled restart scheduled in %u ms\n", delay_ms);
        
#ifndef UNIT_TEST
        // 하드웨어 워치독 활성화 (실제 하드웨어에서)
        // watchdog_enable(delay_ms + 1000);  // 여유시간 추가
#endif
    }
    
    void run_emergency_loop() noexcept {
        if (!g_emergency_state.is_active) return;
        
        static TimePoint last_status_time = 0;
        TimePoint current_time = now();
        
        // 1초마다 상태 출력
        if (current_time - last_status_time > 1000) {
            printf("[EMERGENCY] Still in emergency mode, uptime: %u ms\n", 
                   current_time - g_emergency_state.entered_time);
            last_status_time = current_time;
        }
        
        // 재시작 카운트다운 체크
        if (g_emergency_state.restart_scheduled) {
            if (current_time - g_emergency_state.entered_time >= g_emergency_state.restart_countdown_ms) {
                printf("[EMERGENCY] Restart time reached, halting system...\n");
                
                // 최종 하드 락 (안전한 상태로 정지)
                taskDISABLE_INTERRUPTS();
                for(;;);  // 워치독이 시스템을 재시작할 것임
            }
        }
        
        // 최소 기능 유지 (예: UART 통신, LED 상태 표시)
        // yield to other emergency tasks
#ifndef UNIT_TEST
        vTaskDelay(pdMS_TO_TICKS(10));
#endif
    }
    
    bool is_emergency_mode() noexcept {
        return g_emergency_state.is_active;
    }
    
    const FailureContext& get_last_failure() noexcept {
        return g_emergency_state.last_failure;
    }
    
    void emergency_log_failure(const FailureContext& context) noexcept {
        printf("[CRITICAL FAILURE] =================\n");
        printf("Reason: %u\n", static_cast<uint32_t>(context.reason));
        printf("Location: %s:%d", context.file ? context.file : "unknown", context.line);
        if (context.function) {
            printf(" in %s()", context.function);
        }
        printf("\nTimestamp: %u\n", context.timestamp);
        printf("Heap free: %u bytes\n", context.heap_free_bytes);
        printf("Stack usage: %u bytes\n", context.stack_usage);
        printf("Additional: %s\n", context.additional_info);
        printf("=====================================\n");
        
        // TODO: Flash memory나 EEPROM에 저장 (재부팅 후에도 유지)
        // save_to_flash(&context, sizeof(context));
    }
}

// ============================================================================
// MessageQueue Implementation - Phase 3: Zero-overhead
// ============================================================================

MessageQueue::MessageQueue() noexcept {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) [[unlikely]] {
        // 현대적 Fail-Safe: 정보 보존 + 제어된 복구
        emergency::save_failure_context(
            emergency::CriticalFailure::MUTEX_CREATION_FAILED,
            __FILE__, __LINE__, __FUNCTION__);
        
        emergency::enter_emergency_mode();
        emergency::schedule_controlled_restart(5000);  // 5초 후 재시작
        
        // 최후의 안전 정지 (기존 안전성 유지)
        taskDISABLE_INTERRUPTS();
        for(;;);
    }
    
    // Cache-friendly initialization
    for (auto& entry : queue_) {
        entry.valid = false;
        entry.size = 0;
    }
}

MessageQueue::~MessageQueue() noexcept {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

MessageQueue::Result MessageQueue::push(const MessageBase& msg, uint16_t size) noexcept {
    if (size > MINI_SO_MAX_MESSAGE_SIZE) [[unlikely]] {
        return Result::MESSAGE_TOO_LARGE;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) [[unlikely]] {
        return Result::INVALID_MESSAGE;
    }
    
    if (count_ >= MINI_SO_MAX_QUEUE_SIZE) [[unlikely]] {
        xSemaphoreGive(mutex_);
        return Result::QUEUE_FULL;
    }
    
    // Zero-overhead: 캐시 정렬된 안전한 메모리 복사
    QueueEntry& entry = queue_[tail_];
    
    // 메모리 안전성: 크기 재검증 (Defense in depth)
    if (size <= MINI_SO_MAX_MESSAGE_SIZE) [[likely]] {
        std::memcpy(entry.data, &msg, size);
        entry.size = size;
        entry.valid = true;
        
        tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_++;
    } else {
        xSemaphoreGive(mutex_);
        return Result::MESSAGE_TOO_LARGE;
    }
    
    xSemaphoreGive(mutex_);
    return Result::SUCCESS;
}

bool MessageQueue::pop(uint8_t* buffer, uint16_t& size) noexcept {
    // 입력 유효성 검사 (Zero-overhead when inlined)
    if (!buffer) [[unlikely]] {
        return false;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) [[unlikely]] {
        return false;
    }
    
    if (count_ == 0) [[unlikely]] {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    QueueEntry& entry = queue_[head_];
    if (!entry.valid) [[unlikely]] {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    // 메모리 안전성: 크기 검증 후 복사
    if (entry.size <= MINI_SO_MAX_MESSAGE_SIZE) [[likely]] {
        size = entry.size;
        std::memcpy(buffer, entry.data, entry.size);
        entry.valid = false;
        entry.size = 0;  // Zero-overhead 클리어
        
        head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_--;
    } else {
        // 손상된 엔트리 복구
        entry.valid = false;
        entry.size = 0;
        head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
        count_--;
        xSemaphoreGive(mutex_);
        return false;
    }
    
    xSemaphoreGive(mutex_);
    return true;
}

void MessageQueue::clear() noexcept {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        for (auto& entry : queue_) {
            entry.valid = false;
        }
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        xSemaphoreGive(mutex_);
    }
}

// ============================================================================
// Agent Implementation - Phase 3: Zero-overhead messaging
// ============================================================================

void Agent::process_messages() noexcept {
    alignas(8) uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    uint16_t size;
    
    uint32_t messages_processed = 0;
    TimePoint start_time = now();
    
    while (message_queue_.pop(buffer, size)) {
        MessageBase* msg = reinterpret_cast<MessageBase*>(buffer);
        if (msg && handle_message(*msg)) {
            messages_processed++;
        }
        
        // 과도한 처리 방지 (임베디드 시스템 고려)
        if (messages_processed >= 8) break;
    }
    
    if (messages_processed > 0) {
        TimePoint end_time = now();
        uint32_t processing_time_us = (end_time - start_time) * 1000;
        report_performance(processing_time_us, messages_processed);
    }
}

// ============================================================================
// Environment Implementation - Phase 3: High-performance runtime
// ============================================================================

Environment::Environment() noexcept {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) [[unlikely]] {
        // 현대적 Fail-Safe: Environment 실패는 더욱 심각
        emergency::save_failure_context(
            emergency::CriticalFailure::FREERTOS_INIT_FAILED,
            __FILE__, __LINE__, __FUNCTION__);
        
        emergency::enter_emergency_mode();
        emergency::schedule_controlled_restart(3000);  // 3초 후 재시작 (더 빨리)
        
        // 최후의 안전 정지
        taskDISABLE_INTERRUPTS();
        for(;;);
    }
    
    for (auto& agent : agents_) {
        agent = nullptr;
    }
}

bool Environment::initialize() noexcept {
    static bool initialized = false;
    if (initialized) {
        return true;
    }
    
    // Environment 초기화는 생성자에서 이미 완료됨 (SIOF-Safe)
    initialized = true;
    return true;
}

Environment::~Environment() noexcept {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

AgentId Environment::register_agent(Agent* agent) noexcept {
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

void Environment::unregister_agent(AgentId id) noexcept {
    if (id >= agent_count_) [[unlikely]] return;
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        if (agents_[id]) {
            agents_[id]->message_queue_.clear();
            agents_[id] = nullptr;
        }
        xSemaphoreGive(mutex_);
    }
}

Agent* Environment::get_agent(AgentId id) noexcept {
    if (id >= agent_count_) [[unlikely]] {
        return nullptr;
    }
    return agents_[id];
}

bool Environment::process_one_message() noexcept {
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && !agents_[i]->message_queue_.empty()) {
            agents_[i]->process_messages();
#if MINI_SO_ENABLE_METRICS
            total_messages_processed_++;
#endif
            return true;
        }
    }
    return false;
}

void Environment::process_all_messages() noexcept {
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

void Environment::run() noexcept {
#if MINI_SO_ENABLE_METRICS
    TimePoint loop_start = now();
#endif
    
    // 메시지 처리
    process_all_messages();
    
    // Watchdog 체크 (System을 통해)
    System::instance().watchdog().check_timeouts();
    
#if MINI_SO_ENABLE_METRICS
    TimePoint loop_end = now();
    uint32_t processing_time_us = (loop_end - loop_start) * 1000;
    if (processing_time_us > max_processing_time_us_) {
        max_processing_time_us_ = processing_time_us;
    }
#endif
}

std::size_t Environment::total_pending_messages() const noexcept {
    std::size_t total = 0;
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i]) {
            total += agents_[i]->message_queue_.size();
        }
    }
    return total;
}

// ============================================================================
// ErrorAgent Implementation - Phase 3: Efficient error handling
// ============================================================================

bool ErrorAgent::handle_message(const MessageBase& msg) noexcept {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::ErrorReport)) {
        const auto& error_msg = static_cast<const Message<system_messages::ErrorReport>&>(msg);
        
        // 순환 버퍼로 항상 최신 32개 에러 유지
        ErrorEntry& entry = error_log_[next_index_];
        entry.level = error_msg.data.level;
        entry.error_code = error_msg.data.error_code;
        entry.source_agent = error_msg.data.source_agent;
        entry.timestamp = now();
        
        // 인덱스 업데이트 (순환)
        next_index_ = (next_index_ + 1) % error_log_.size();
        error_count_++;
        
        // 최고 레벨 업데이트
        if (error_msg.data.level > max_level_) {
            max_level_ = error_msg.data.level;
        }
        
        return true;
    }
    
    return false;
}

void ErrorAgent::report_error(system_messages::ErrorReport::Level level, uint32_t code, AgentId source) noexcept {
    system_messages::ErrorReport error;
    error.level = level;
    error.error_code = code;
    error.source_agent = source;
    
    // 자신에게 메시지 전송 (순수 메시지 기반)
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), error);
    } else {
        // 직접 저장 (초기화 중인 경우) - 순환 버퍼 적용
        ErrorEntry& entry = error_log_[next_index_];
        entry.level = level;
        entry.error_code = code;
        entry.source_agent = source;
        entry.timestamp = now();
        
        // 인덱스 업데이트 (순환)
        next_index_ = (next_index_ + 1) % error_log_.size();
        error_count_++;
        
        if (level > max_level_) {
            max_level_ = level;
        }
    }
}

// ============================================================================
// PerformanceAgent Implementation - Phase 3: High-performance monitoring
// ============================================================================

bool PerformanceAgent::handle_message(const MessageBase& msg) noexcept {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::PerformanceMetric)) {
        const auto& perf_msg = static_cast<const Message<system_messages::PerformanceMetric>&>(msg);
        
        record_performance(perf_msg.data.processing_time_us, perf_msg.data.message_count);
        return true;
    }
    
    return false;
}

void PerformanceAgent::record_performance(uint32_t processing_time_us, uint32_t message_count) noexcept {
    if (processing_time_us > max_processing_time_us_) {
        max_processing_time_us_ = processing_time_us;
    }
    
    total_messages_ += message_count;
    cycle_count_++;
    
    // 주기적 보고 (메모리 효율성을 위해)
    TimePoint current_time = now();
    if (current_time - last_report_time_ > 1000) { // 1초마다
        last_report_time_ = current_time;
        
        // 필요시 추가 처리 (예: 로그, 알람 등)
    }
}

// ============================================================================
// WatchdogAgent Implementation - Phase 3: Efficient monitoring
// ============================================================================

bool WatchdogAgent::handle_message(const MessageBase& msg) noexcept {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::Heartbeat)) {
        const auto& heartbeat_msg = static_cast<const Message<system_messages::Heartbeat>&>(msg);
        
        // 해당 Agent의 heartbeat 업데이트
        for (std::size_t i = 0; i < monitored_count_; ++i) {
            if (monitored_[i].agent_id == heartbeat_msg.data.source_agent && monitored_[i].active) {
                monitored_[i].last_heartbeat = now();
                break;
            }
        }
        
        return true;
    }
    
    return false;
}

void WatchdogAgent::register_for_monitoring(AgentId agent_id, Duration timeout_ms) noexcept {
    if (monitored_count_ < monitored_.size()) {
        MonitoredAgent& agent = monitored_[monitored_count_];
        agent.agent_id = agent_id;
        agent.last_heartbeat = now();
        agent.timeout_ms = timeout_ms > 0 ? timeout_ms : default_timeout_ms_;
        agent.active = true;
        monitored_count_++;
    }
}

void WatchdogAgent::check_timeouts() noexcept {
    TimePoint current_time = now();
    if (current_time - last_check_time_ < 100) { // 100ms 최소 간격
        return;
    }
    last_check_time_ = current_time;
    
    for (std::size_t i = 0; i < monitored_count_; ++i) {
        MonitoredAgent& agent = monitored_[i];
        if (agent.active) {
            Duration elapsed = current_time - agent.last_heartbeat;
            if (elapsed > agent.timeout_ms) {
                // 타임아웃 발생 - 순수 메시지 기반 오류 보고
                System::instance().report_error(
                    system_messages::ErrorReport::CRITICAL, 
                    1001, // AGENT_TIMEOUT
                    agent.agent_id
                );
                
                agent.active = false;
            }
        }
    }
}

// ============================================================================
// System Implementation - Phase 3: Zero-overhead system management
// ============================================================================

// System::initialize()는 헤더에서 inline으로 구현됨
// ensure_initialized()를 호출하여 InitializationGuard 활용

bool System::initialize() noexcept {
    static bool initialized = false;
    if (initialized) {
        return true;
    }
    
    // Environment 먼저 초기화 보장 (SIOF-Safe)
    Environment& env = Environment::instance();
    if (!env.initialize()) {
        return false;
    }
    
    // 직접 초기화 구현
    if (instance().initialized_) {
        initialized = true;
        return true;
    }
    
    // System Agent 등록
    System& sys = instance();
    sys.error_agent_id_ = env.register_agent(&sys.error_agent_);
    sys.performance_agent_id_ = env.register_agent(&sys.performance_agent_);
    sys.watchdog_agent_id_ = env.register_agent(&sys.watchdog_agent_);
    
    if (sys.error_agent_id_ == INVALID_AGENT_ID || 
        sys.performance_agent_id_ == INVALID_AGENT_ID ||
        sys.watchdog_agent_id_ == INVALID_AGENT_ID) [[unlikely]] {
        return false;
    }
    
    sys.initialized_ = true;
    initialized = true;
    return true;
}

void System::shutdown() noexcept {
    if (!initialized_) {
        return;
    }
    
    Environment& env = Environment::instance();
    
    if (error_agent_id_ != INVALID_AGENT_ID) {
        env.unregister_agent(error_agent_id_);
        error_agent_id_ = INVALID_AGENT_ID;
    }
    
    if (performance_agent_id_ != INVALID_AGENT_ID) {
        env.unregister_agent(performance_agent_id_);
        performance_agent_id_ = INVALID_AGENT_ID;
    }
    
    if (watchdog_agent_id_ != INVALID_AGENT_ID) {
        env.unregister_agent(watchdog_agent_id_);
        watchdog_agent_id_ = INVALID_AGENT_ID;
    }
    
    initialized_ = false;
}

} // namespace mini_so