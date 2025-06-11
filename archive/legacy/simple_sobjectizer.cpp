#include "../include/mini_sobjectizer/simple_sobjectizer.h"
#include <cstring>

namespace mini_so {

// Static member initialization
namespace detail {
    uint16_t MessageIdGenerator::counter_ = 1;
}

SimpleEnvironment* SimpleEnvironment::instance_ = nullptr;

// ============================================================================
// SimpleMessageQueue Implementation
// ============================================================================

SimpleMessageQueue::SimpleMessageQueue() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        // 실패시에도 계속 진행 (degraded mode)
    }
}

SimpleMessageQueue::~SimpleMessageQueue() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

bool SimpleMessageQueue::push(const MessageBase& msg, MessageSize size) {
    if (size > MINI_SO_MAX_MESSAGE_SIZE || size < sizeof(MessageBase)) {
        return false;
    }
    
    if (!mutex_ || xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    if (count_ >= MINI_SO_MAX_QUEUE_SIZE) {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    // 메시지 복사
    std::memcpy(queue_[tail_].data, &msg, size);
    queue_[tail_].size = size;
    queue_[tail_].valid = true;
    
    tail_ = (tail_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
    count_++;
    
    xSemaphoreGive(mutex_);
    return true;
}

bool SimpleMessageQueue::pop(uint8_t* buffer, MessageSize& size) {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    
    if (count_ == 0) {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    if (!queue_[head_].valid) {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    size = queue_[head_].size;
    std::memcpy(buffer, queue_[head_].data, size);
    queue_[head_].valid = false;
    
    head_ = (head_ + 1) % MINI_SO_MAX_QUEUE_SIZE;
    count_--;
    
    xSemaphoreGive(mutex_);
    return true;
}

void SimpleMessageQueue::clear() {
    if (!mutex_) {
        return;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        head_ = 0;
        tail_ = 0;
        count_ = 0;
        
        for (auto& entry : queue_) {
            entry.valid = false;
        }
        
        xSemaphoreGive(mutex_);
    }
}

// ============================================================================
// SimpleAgent Implementation
// ============================================================================

void SimpleAgent::process_messages() {
    uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    MessageSize size;
    
    while (message_queue_.pop(buffer, size)) {
        if (size >= sizeof(MessageBase)) {
            const MessageBase* msg = reinterpret_cast<const MessageBase*>(buffer);
            handle_message(*msg);
        }
    }
}

// ============================================================================
// SimpleEnvironment Implementation
// ============================================================================

SimpleEnvironment::SimpleEnvironment() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        // Critical failure in embedded system
        // 하지만 복잡한 처리 대신 단순하게
        for(;;);
    }
    
    for (auto& agent : agents_) {
        agent = nullptr;
    }
}

SimpleEnvironment::~SimpleEnvironment() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

AgentId SimpleEnvironment::register_agent(SimpleAgent* agent) {
    if (!agent || !mutex_) {
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
    
    agent->initialize(id);
    return id;
}

void SimpleEnvironment::unregister_agent(AgentId id) {
    if (id >= agent_count_ || !mutex_) {
        return;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        agents_[id] = nullptr;
        xSemaphoreGive(mutex_);
    }
}

SimpleAgent* SimpleEnvironment::get_agent(AgentId id) {
    if (id >= agent_count_ || !mutex_) {
        return nullptr;
    }
    
    SimpleAgent* agent = nullptr;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        agent = agents_[id];
        xSemaphoreGive(mutex_);
    }
    
    return agent;
}

bool SimpleEnvironment::process_one_message() {
    for (size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && agents_[i]->has_messages()) {
            uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
            MessageSize size;
            
            if (agents_[i]->message_queue_.pop(buffer, size)) {
                if (size >= sizeof(MessageBase)) {
                    const MessageBase* msg = reinterpret_cast<const MessageBase*>(buffer);
                    agents_[i]->handle_message(*msg);
                    return true;
                }
            }
        }
    }
    return false;
}

void SimpleEnvironment::process_all_messages() {
    while (process_one_message()) {
        // Process all pending messages
    }
}

void SimpleEnvironment::run_main_loop() {
    // 단순한 메시지 처리만 - 복잡한 기능 없음
    process_all_messages();
}

size_t SimpleEnvironment::total_pending_messages() const {
    size_t total = 0;
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        for (size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i]) {
                total += agents_[i]->message_queue_.size();
            }
        }
        xSemaphoreGive(mutex_);
    }
    
    return total;
}

// ============================================================================
// System Service Agents Implementation
// ============================================================================

bool ErrorHandlerAgent::handle_message(const MessageBase& msg) {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::ErrorReport)) {
        handle_error_report(static_cast<const Message<system_messages::ErrorReport>&>(msg));
        return true;
    }
    
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::StatusRequest)) {
        handle_status_request(static_cast<const Message<system_messages::StatusRequest>&>(msg));
        return true;
    }
    
    return false;
}

void ErrorHandlerAgent::handle_error_report(const Message<system_messages::ErrorReport>& msg) {
    // 오류 로그에 저장
    if (error_count_ < error_log_.size()) {
        ErrorEntry& entry = error_log_[error_count_];
        entry.level = msg.data.level;
        entry.source = msg.data.source_agent;
        entry.timestamp = msg.timestamp();
        entry.error_code = msg.data.error_code;
        error_count_++;
    }
    
    // Critical 오류의 경우 복구 메시지 전송
    if (msg.data.level == system_messages::ErrorReport::CRITICAL) {
        // 복구 Agent가 있다면 복구 요청 메시지 전송
        // (여기서는 단순하게 처리)
    }
}

void ErrorHandlerAgent::handle_status_request(const Message<system_messages::StatusRequest>& msg) {
    system_messages::StatusResponse response;
    response.request_id = msg.data.request_id;
    response.uptime_ms = now();
    response.total_messages = 0; // 단순화
    response.error_count = error_count_;
    
    send_message(msg.data.requester, response);
}

bool PerformanceMonitorAgent::handle_message(const MessageBase& msg) {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::PerformanceMetric)) {
        handle_performance_metric(static_cast<const Message<system_messages::PerformanceMetric>&>(msg));
        return true;
    }
    
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::StatusRequest)) {
        handle_status_request(static_cast<const Message<system_messages::StatusRequest>&>(msg));
        return true;
    }
    
    return false;
}

void PerformanceMonitorAgent::handle_performance_metric(const Message<system_messages::PerformanceMetric>& msg) {
    // 최대 처리 시간 업데이트
    if (msg.data.processing_time_ms > max_processing_time_) {
        max_processing_time_ = msg.data.processing_time_ms;
    }
    
    // 총 메시지 수 업데이트
    total_messages_ += msg.data.message_count;
}

void PerformanceMonitorAgent::handle_status_request(const Message<system_messages::StatusRequest>& msg) {
    system_messages::StatusResponse response;
    response.request_id = msg.data.request_id;
    response.uptime_ms = now() - start_time_;
    response.total_messages = total_messages_;
    response.error_count = 0; // 성능 모니터는 오류 카운트 없음
    
    send_message(msg.data.requester, response);
}

bool WatchdogAgent::handle_message(const MessageBase& msg) {
    if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::HeartbeatSignal)) {
        handle_heartbeat(static_cast<const Message<system_messages::HeartbeatSignal>&>(msg));
        return true;
    }
    
    return false;
}

void WatchdogAgent::handle_heartbeat(const Message<system_messages::HeartbeatSignal>& msg) {
    // 해당 Agent의 heartbeat 업데이트
    for (auto& heartbeat : heartbeats_) {
        if (heartbeat.agent_id == msg.data.source_agent && heartbeat.active) {
            heartbeat.last_heartbeat = msg.data.timestamp;
            break;
        }
    }
}

void WatchdogAgent::check_timeouts() {
    uint32_t current_time = now();
    
    for (const auto& heartbeat : heartbeats_) {
        if (heartbeat.active) {
            uint32_t elapsed = current_time - heartbeat.last_heartbeat;
            if (elapsed > heartbeat.timeout_ms) {
                // 타임아웃 발생 - 메시지로 알림
                system_messages::WatchdogTimeout timeout_msg;
                timeout_msg.failed_agent = heartbeat.agent_id;
                timeout_msg.last_heartbeat = heartbeat.last_heartbeat;
                timeout_msg.timeout_duration = heartbeat.timeout_ms;
                
                broadcast_message(timeout_msg);
            }
        }
    }
}

void WatchdogAgent::register_agent_for_monitoring(AgentId agent_id, uint32_t timeout_ms) {
    if (monitored_agents_ < heartbeats_.size()) {
        AgentHeartbeat& hb = heartbeats_[monitored_agents_];
        hb.agent_id = agent_id;
        hb.last_heartbeat = now();
        hb.timeout_ms = timeout_ms;
        hb.active = true;
        monitored_agents_++;
    }
}

} // namespace mini_so