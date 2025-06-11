#include "mini_sobjectizer/mini_sobjectizer_v3.h"
#include <cstring>

namespace mini_so {

// Static member initialization
Environment* Environment::instance_ = nullptr;
SystemManager* SystemManager::instance_ = nullptr;

// ============================================================================
// MessageQueue Implementation
// ============================================================================

MessageQueue::MessageQueue() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        // 임베디드 시스템에서는 halt
        taskDISABLE_INTERRUPTS();
        for(;;);
    }
    
    for (auto& entry : queue_) {
        entry.valid = false;
        entry.size = 0;
    }
}

MessageQueue::~MessageQueue() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

MessageQueue::Result MessageQueue::push(const MessageBase& msg, uint16_t size) {
    if (size > MINI_SO_MAX_MESSAGE_SIZE) {
        return Result::MESSAGE_TOO_LARGE;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return Result::INVALID_MESSAGE;
    }
    
    if (count_ >= MINI_SO_MAX_QUEUE_SIZE) {
        xSemaphoreGive(mutex_);
        return Result::QUEUE_FULL;
    }
    
    QueueEntry& entry = queue_[tail_];
    std::memcpy(entry.data, &msg, size);
    entry.size = size;
    entry.valid = true;
    
    tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
    count_++;
    
    xSemaphoreGive(mutex_);
    return Result::SUCCESS;
}

bool MessageQueue::pop(uint8_t* buffer, uint16_t& size) {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    
    if (count_ == 0) {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    QueueEntry& entry = queue_[head_];
    if (!entry.valid) {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    size = entry.size;
    std::memcpy(buffer, entry.data, entry.size);
    entry.valid = false;
    
    head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
    count_--;
    
    xSemaphoreGive(mutex_);
    return true;
}

void MessageQueue::clear() {
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
// Agent Implementation
// ============================================================================

void Agent::process_messages() {
    uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    uint16_t size;
    
    while (message_queue_.pop(buffer, size)) {
        MessageBase* msg = reinterpret_cast<MessageBase*>(buffer);
        if (msg) {
            handle_message(*msg);
            
            // 성능 메트릭 보고
            SystemManager::instance().record_performance(1, 1);
        }
    }
}

// ============================================================================
// Environment Implementation
// ============================================================================

Environment::Environment() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        taskDISABLE_INTERRUPTS();
        for(;;);
    }
    
    for (auto& agent : agents_) {
        agent = nullptr;
    }
}

Environment::~Environment() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

AgentId Environment::register_agent(Agent* agent) {
    if (!agent) {
        return INVALID_AGENT_ID;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return INVALID_AGENT_ID;
    }
    
    if (agent_count_ >= MINI_SO_MAX_AGENTS) {
        xSemaphoreGive(mutex_);
        return INVALID_AGENT_ID;
    }
    
    AgentId id = static_cast<AgentId>(agent_count_);
    agents_[agent_count_] = agent;
    agent_count_++;
    
    xSemaphoreGive(mutex_);
    
    // Agent 초기화
    agent->initialize(id);
    
    return id;
}

void Environment::unregister_agent(AgentId id) {
    if (id >= agent_count_) return;
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        if (agents_[id]) {
            agents_[id]->message_queue_.clear();
            agents_[id] = nullptr;
        }
        xSemaphoreGive(mutex_);
    }
}

Agent* Environment::get_agent(AgentId id) {
    if (id >= agent_count_) {
        return nullptr;
    }
    return agents_[id];
}

bool Environment::process_one_message() {
    bool processed = false;
    
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && !agents_[i]->message_queue_.empty()) {
            agents_[i]->process_messages();
            processed = true;
            break;
        }
    }
    
    return processed;
}

void Environment::process_all_messages() {
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

void Environment::run() {
    TimePoint loop_start = now();
    
    // 메시지 처리
    process_all_messages();
    
    // Watchdog 체크
    SystemManager::instance().watchdog().check_timeouts();
    
    // 성능 측정
    TimePoint loop_end = now();
    uint32_t processing_time = loop_end - loop_start;
    SystemManager::instance().record_performance(processing_time, 0);
}

std::size_t Environment::total_pending_messages() const {
    std::size_t total = 0;
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i]) {
            total += agents_[i]->message_queue_.size();
        }
    }
    return total;
}

// ============================================================================
// ErrorAgent Implementation
// ============================================================================

bool ErrorAgent::handle_message(const MessageBase& msg) {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::ErrorReport)) {
        const auto& error_msg = static_cast<const Message<system_messages::ErrorReport>&>(msg);
        
        if (error_count_ < error_log_.size()) {
            ErrorEntry& entry = error_log_[error_count_];
            entry.level = error_msg.data.level;
            entry.error_code = error_msg.data.error_code;
            entry.context = error_msg.data.context;
            entry.timestamp = now();
            error_count_++;
        }
        
        return true;
    }
    
    return false;
}

void ErrorAgent::report_error(system_messages::ErrorReport::Level level, uint32_t code, const char* context) {
    system_messages::ErrorReport error;
    error.level = level;
    error.error_code = code;
    error.context = context;
    
    // 자신에게 메시지 전송
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), error);
    } else {
        // Agent가 등록되지 않은 경우 직접 저장
        if (error_count_ < error_log_.size()) {
            ErrorEntry& entry = error_log_[error_count_];
            entry.level = level;
            entry.error_code = code;
            entry.context = context;
            entry.timestamp = now();
            error_count_++;
        }
    }
}

// ============================================================================
// PerformanceAgent Implementation
// ============================================================================

bool PerformanceAgent::handle_message(const MessageBase& msg) {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::PerformanceMetric)) {
        const auto& perf_msg = static_cast<const Message<system_messages::PerformanceMetric>&>(msg);
        
        if (perf_msg.data.processing_time_ms > max_processing_time_ms_) {
            max_processing_time_ms_ = perf_msg.data.processing_time_ms;
        }
        
        total_messages_ += perf_msg.data.message_count;
        
        return true;
    }
    
    return false;
}

void PerformanceAgent::record_performance(uint32_t processing_time_ms, uint32_t message_count) {
    // 직접 업데이트 (간소화)
    if (processing_time_ms > max_processing_time_ms_) {
        max_processing_time_ms_ = processing_time_ms;
    }
    
    total_messages_ += message_count;
    loop_count_++;
    
    // 선택적으로 메시지 전송 (필요시)
    if (id() != INVALID_AGENT_ID) {
        system_messages::PerformanceMetric metric;
        metric.source_agent = INVALID_AGENT_ID;
        metric.processing_time_ms = processing_time_ms;
        metric.message_count = message_count;
        metric.timestamp = now();
        
        send_message(id(), metric);
    }
}

// ============================================================================
// WatchdogAgent Implementation
// ============================================================================

bool WatchdogAgent::handle_message(const MessageBase& msg) {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::HeartbeatSignal)) {
        const auto& heartbeat_msg = static_cast<const Message<system_messages::HeartbeatSignal>&>(msg);
        
        // 해당 Agent의 heartbeat 업데이트
        for (std::size_t i = 0; i < monitored_count_; ++i) {
            if (monitored_[i].agent_id == heartbeat_msg.data.source_agent && monitored_[i].active) {
                monitored_[i].last_heartbeat = heartbeat_msg.data.timestamp;
                break;
            }
        }
        
        return true;
    }
    
    return false;
}

void WatchdogAgent::register_for_monitoring(AgentId agent_id, Duration timeout_ms) {
    if (monitored_count_ < monitored_.size()) {
        MonitoredAgent& agent = monitored_[monitored_count_];
        agent.agent_id = agent_id;
        agent.last_heartbeat = now();
        agent.timeout_ms = timeout_ms > 0 ? timeout_ms : default_timeout_ms_;
        agent.active = true;
        monitored_count_++;
    }
}

void WatchdogAgent::check_timeouts() {
    TimePoint current_time = now();
    
    for (std::size_t i = 0; i < monitored_count_; ++i) {
        MonitoredAgent& agent = monitored_[i];
        if (agent.active) {
            Duration elapsed = current_time - agent.last_heartbeat;
            if (elapsed > agent.timeout_ms) {
                // 타임아웃 발생 - 오류 보고
                SystemManager::instance().report_error(
                    system_messages::ErrorReport::CRITICAL, 
                    1001, // AGENT_TIMEOUT
                    "Watchdog timeout detected"
                );
                
                // Agent를 비활성 상태로 설정
                agent.active = false;
            }
        }
    }
}

bool WatchdogAgent::is_healthy() const {
    for (std::size_t i = 0; i < monitored_count_; ++i) {
        if (monitored_[i].active) {
            return true; // 적어도 하나의 Agent가 활성 상태
        }
    }
    return monitored_count_ == 0; // 모니터링 중인 Agent가 없으면 건강한 것으로 간주
}

// ============================================================================
// SystemManager Implementation
// ============================================================================

bool SystemManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    Environment& env = Environment::instance();
    
    // System Agent 등록
    error_agent_id_ = env.register_agent(&error_agent_);
    performance_agent_id_ = env.register_agent(&performance_agent_);
    watchdog_agent_id_ = env.register_agent(&watchdog_agent_);
    
    if (error_agent_id_ == INVALID_AGENT_ID || 
        performance_agent_id_ == INVALID_AGENT_ID ||
        watchdog_agent_id_ == INVALID_AGENT_ID) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

void SystemManager::shutdown() {
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