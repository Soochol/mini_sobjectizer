#include "../include/mini_sobjectizer/system_agents.h"
#include <cstring>

namespace mini_so {

// ============================================================================
// SystemErrorAgent Implementation
// ============================================================================

void SystemErrorAgent::report_error(ErrorCode code, const char* context) {
    // 내부 상태 업데이트 (원자적)
    error_count_.fetch_add(1, std::memory_order_relaxed);
    last_error_.store(code, std::memory_order_release);
    last_error_timestamp_.store(now(), std::memory_order_release);
    
    // 메시지로 자신에게 보고 (비동기 처리)
    internal_messages::ErrorReport error;
    error.code = code;
    error.source_agent = INVALID_AGENT_ID; // 외부에서 호출
    error.timestamp = now();
    error.context = context;
    
    // 자신에게 메시지 전송하여 로그에 저장 (Agent가 등록된 경우에만)
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), error);
    }
}

ErrorCode SystemErrorAgent::get_last_error() const {
    return last_error_.load(std::memory_order_acquire);
}

uint32_t SystemErrorAgent::get_error_count() const {
    return error_count_.load(std::memory_order_relaxed);
}

uint32_t SystemErrorAgent::get_last_error_timestamp() const {
    return last_error_timestamp_.load(std::memory_order_acquire);
}

SystemHealth SystemErrorAgent::get_system_health() const {
    uint32_t count = error_count_.load(std::memory_order_relaxed);
    if (count == 0) {
        return SystemHealth::HEALTHY;
    } else if (count < 10) {
        return SystemHealth::WARNING;
    } else if (count < 100) {
        return SystemHealth::CRITICAL;
    } else {
        return SystemHealth::FAILED;
    }
}

void SystemErrorAgent::reset_error_state() {
    error_count_.store(0, std::memory_order_relaxed);
    last_error_.store(ErrorCode::SUCCESS, std::memory_order_release);
    last_error_timestamp_.store(0, std::memory_order_release);
}

void SystemErrorAgent::handle_error_report(const Message<internal_messages::ErrorReport>& msg) {
    store_error(msg.data);
    
    // Critical 오류의 경우 복구 Agent에 알림
    if (msg.data.code == ErrorCode::SYSTEM_OVERLOAD || 
        msg.data.code == ErrorCode::MEMORY_ALLOCATION_FAILED) {
        
        SystemServiceManager& manager = SystemServiceManager::instance();
        if (manager.is_initialized()) {
            internal_messages::RecoveryRequest recovery_req;
            recovery_req.action = internal_messages::RecoveryRequest::RESET_AGENTS;
            recovery_req.requester = id();
            recovery_req.timestamp = now();
            
            send_message(manager.get_recovery_agent_id(), recovery_req);
        }
    }
}

void SystemErrorAgent::handle_error_query(const Message<internal_messages::ErrorQuery>& msg) {
    internal_messages::ErrorStatus status;
    status.request_id = msg.data.request_id;
    status.error_count = error_count_.load(std::memory_order_relaxed);
    status.last_error = last_error_.load(std::memory_order_acquire);
    status.last_error_timestamp = last_error_timestamp_.load(std::memory_order_acquire);
    status.health = get_system_health();
    
    send_message(msg.data.requester, status);
}

void SystemErrorAgent::store_error(const internal_messages::ErrorReport& error) {
    uint32_t current_count = error_count_.load(std::memory_order_relaxed);
    if (current_count < error_log_.size()) {
        ErrorEntry& entry = error_log_[current_count % error_log_.size()];
        entry.code = error.code;
        entry.source_agent = error.source_agent;
        entry.timestamp = error.timestamp;
        entry.context = error.context;
    }
}

// ============================================================================
// SystemMetricsAgent Implementation
// ============================================================================

void SystemMetricsAgent::increment_messages_sent() {
    total_messages_sent_.fetch_add(1, std::memory_order_relaxed);
    
    // 메시지로 자신에게 보고
    internal_messages::MetricReport report;
    report.type = internal_messages::MetricReport::MESSAGE_SENT;
    report.source_agent = INVALID_AGENT_ID;
    report.value = 1;
    report.timestamp = now();
    
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), report);
    }
}

void SystemMetricsAgent::increment_messages_processed() {
    total_messages_processed_.fetch_add(1, std::memory_order_relaxed);
    
    internal_messages::MetricReport report;
    report.type = internal_messages::MetricReport::MESSAGE_PROCESSED;
    report.source_agent = INVALID_AGENT_ID;
    report.value = 1;
    report.timestamp = now();
    
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), report);
    }
}

void SystemMetricsAgent::report_timer_overrun() {
    timer_overruns_.fetch_add(1, std::memory_order_relaxed);
    
    internal_messages::MetricReport report;
    report.type = internal_messages::MetricReport::TIMER_OVERRUN;
    report.source_agent = INVALID_AGENT_ID;
    report.value = 1;
    report.timestamp = now();
    
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), report);
    }
}

void SystemMetricsAgent::update_max_queue_depth(uint32_t depth) {
    // 원자적 최댓값 업데이트
    uint32_t current_max = max_queue_depth_.load(std::memory_order_relaxed);
    while (depth > current_max && 
           !max_queue_depth_.compare_exchange_weak(current_max, depth, 
                                                  std::memory_order_relaxed)) {
        // 재시도
    }
    
    internal_messages::MetricReport report;
    report.type = internal_messages::MetricReport::QUEUE_DEPTH_UPDATE;
    report.source_agent = INVALID_AGENT_ID;
    report.value = depth;
    report.timestamp = now();
    
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), report);
    }
}

void SystemMetricsAgent::reset_metrics() {
    total_messages_sent_.store(0, std::memory_order_relaxed);
    total_messages_processed_.store(0, std::memory_order_relaxed);
    active_timers_.store(0, std::memory_order_relaxed);
    timer_overruns_.store(0, std::memory_order_relaxed);
    max_queue_depth_.store(0, std::memory_order_relaxed);
}

uint32_t SystemMetricsAgent::get_messages_sent() const {
    return total_messages_sent_.load(std::memory_order_relaxed);
}

uint32_t SystemMetricsAgent::get_messages_processed() const {
    return total_messages_processed_.load(std::memory_order_relaxed);
}

uint32_t SystemMetricsAgent::get_timer_overruns() const {
    return timer_overruns_.load(std::memory_order_relaxed);
}

uint32_t SystemMetricsAgent::get_max_queue_depth() const {
    return max_queue_depth_.load(std::memory_order_relaxed);
}

uint32_t SystemMetricsAgent::get_pending_messages() const {
    uint32_t sent = total_messages_sent_.load(std::memory_order_relaxed);
    uint32_t processed = total_messages_processed_.load(std::memory_order_relaxed);
    return sent >= processed ? sent - processed : 0;
}

void SystemMetricsAgent::handle_metric_report(const Message<internal_messages::MetricReport>& msg) {
    // 추가적인 메트릭 처리 로직 (필요시)
    // 현재는 atomic 변수에서 이미 처리됨
}

void SystemMetricsAgent::handle_metric_query(const Message<internal_messages::MetricQuery>& msg) {
    internal_messages::MetricStatus status;
    status.request_id = msg.data.request_id;
    status.total_messages_sent = get_messages_sent();
    status.total_messages_processed = get_messages_processed();
    status.max_queue_depth = get_max_queue_depth();
    status.timer_overruns = get_timer_overruns();
    
    send_message(msg.data.requester, status);
}

// ============================================================================
// PerformanceMonitorAgent Implementation
// ============================================================================

void PerformanceMonitorAgent::record_message_latency(uint32_t latency_ms) {
    // 원자적 최댓값 업데이트
    uint32_t current_max = max_message_latency_ms_.load(std::memory_order_relaxed);
    while (latency_ms > current_max && 
           !max_message_latency_ms_.compare_exchange_weak(current_max, latency_ms, 
                                                         std::memory_order_relaxed)) {
        // 재시도
    }
    
    internal_messages::PerformanceReport report;
    report.source_agent = INVALID_AGENT_ID;
    report.processing_time_ms = 0;
    report.message_latency_ms = latency_ms;
    report.timestamp = now();
    
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), report);
    }
}

void PerformanceMonitorAgent::record_processing_time(uint32_t processing_time_ms) {
    uint32_t current_max = max_processing_time_ms_.load(std::memory_order_relaxed);
    while (processing_time_ms > current_max && 
           !max_processing_time_ms_.compare_exchange_weak(current_max, processing_time_ms, 
                                                         std::memory_order_relaxed)) {
        // 재시도
    }
    
    internal_messages::PerformanceReport report;
    report.source_agent = INVALID_AGENT_ID;
    report.processing_time_ms = processing_time_ms;
    report.message_latency_ms = 0;
    report.timestamp = now();
    
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), report);
    }
}

void PerformanceMonitorAgent::increment_loop_count() {
    loop_count_.fetch_add(1, std::memory_order_relaxed);
}

void PerformanceMonitorAgent::reset_stats() {
    max_message_latency_ms_.store(0, std::memory_order_relaxed);
    max_processing_time_ms_.store(0, std::memory_order_relaxed);
    loop_count_.store(0, std::memory_order_relaxed);
}

uint32_t PerformanceMonitorAgent::get_max_message_latency_ms() const {
    return max_message_latency_ms_.load(std::memory_order_relaxed);
}

uint32_t PerformanceMonitorAgent::get_max_processing_time_ms() const {
    return max_processing_time_ms_.load(std::memory_order_relaxed);
}

uint32_t PerformanceMonitorAgent::get_loop_count() const {
    return loop_count_.load(std::memory_order_relaxed);
}

void PerformanceMonitorAgent::handle_performance_report(const Message<internal_messages::PerformanceReport>& msg) {
    // 추가적인 성능 데이터 분석 (필요시)
}

void PerformanceMonitorAgent::handle_performance_query(const Message<internal_messages::PerformanceQuery>& msg) {
    internal_messages::PerformanceStatus status;
    status.request_id = msg.data.request_id;
    status.max_processing_time = get_max_processing_time_ms();
    status.max_message_latency = get_max_message_latency_ms();
    status.loop_count = get_loop_count();
    
    send_message(msg.data.requester, status);
}

// ============================================================================
// SystemWatchdogAgent Implementation
// ============================================================================

bool SystemWatchdogAgent::init(uint32_t timeout_ms) {
    global_timeout_ms_.store(timeout_ms, std::memory_order_release);
    enabled_.store(true, std::memory_order_release);
    
    return true;
}

void SystemWatchdogAgent::kick() {
    if (enabled_.load(std::memory_order_acquire)) {
        internal_messages::WatchdogKick kick_msg;
        kick_msg.source_agent = INVALID_AGENT_ID; // 외부에서 호출
        kick_msg.timestamp = now();
        
        if (id() != INVALID_AGENT_ID) {
            send_message(id(), kick_msg);
        }
    }
}

void SystemWatchdogAgent::enable(bool state) {
    enabled_.store(state, std::memory_order_release);
}

bool SystemWatchdogAgent::is_expired() {
    // 글로벌 watchdog의 경우 전체 시스템 상태 확인
    uint32_t current_time = now();
    uint32_t timeout = global_timeout_ms_.load(std::memory_order_acquire);
    
    // 최근에 kick된 Agent가 있는지 확인
    for (size_t i = 0; i < monitored_count_.load(std::memory_order_acquire); ++i) {
        if (monitored_agents_[i].active) {
            uint32_t elapsed = current_time - monitored_agents_[i].last_kick_time;
            if (elapsed <= monitored_agents_[i].timeout_ms) {
                return false; // 적어도 하나는 활성 상태
            }
        }
    }
    
    return true; // 모든 Agent가 타임아웃됨
}

uint32_t SystemWatchdogAgent::time_since_last_kick() {
    uint32_t current_time = now();
    uint32_t min_elapsed = UINT32_MAX;
    
    for (size_t i = 0; i < monitored_count_.load(std::memory_order_acquire); ++i) {
        if (monitored_agents_[i].active) {
            uint32_t elapsed = current_time - monitored_agents_[i].last_kick_time;
            if (elapsed < min_elapsed) {
                min_elapsed = elapsed;
            }
        }
    }
    
    return min_elapsed == UINT32_MAX ? 0 : min_elapsed;
}

void SystemWatchdogAgent::trigger_system_recovery() {
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.is_initialized()) {
        internal_messages::RecoveryRequest recovery_req;
        recovery_req.action = internal_messages::RecoveryRequest::RESET_AGENTS;
        recovery_req.requester = id();
        recovery_req.timestamp = now();
        
        send_message(manager.get_recovery_agent_id(), recovery_req);
    }
}

void SystemWatchdogAgent::register_agent_for_monitoring(AgentId agent_id, uint32_t timeout_ms) {
    uint32_t count = monitored_count_.load(std::memory_order_acquire);
    if (count < monitored_agents_.size()) {
        AgentInfo& info = monitored_agents_[count];
        info.agent_id = agent_id;
        info.last_kick_time = now();
        info.timeout_ms = timeout_ms;
        info.active = true;
        
        monitored_count_.fetch_add(1, std::memory_order_acq_rel);
    }
}

void SystemWatchdogAgent::handle_watchdog_kick(const Message<internal_messages::WatchdogKick>& msg) {
    uint32_t current_time = msg.data.timestamp;
    
    // 해당 Agent의 kick 시간 업데이트
    for (size_t i = 0; i < monitored_count_.load(std::memory_order_acquire); ++i) {
        if (monitored_agents_[i].agent_id == msg.data.source_agent && monitored_agents_[i].active) {
            monitored_agents_[i].last_kick_time = current_time;
            break;
        }
    }
}

void SystemWatchdogAgent::start_periodic_checking() {
    // 주기적으로 타임아웃 체크
    uint32_t check_interval = global_timeout_ms_.load(std::memory_order_acquire) / 4;
    if (check_interval < 100) check_interval = 100;
    
    check_timer_id_ = start_periodic_timer(check_interval, [this]() {
        check_all_timeouts();
    });
}

void SystemWatchdogAgent::check_all_timeouts() {
    if (!enabled_.load(std::memory_order_acquire)) {
        return;
    }
    
    uint32_t current_time = now();
    
    for (size_t i = 0; i < monitored_count_.load(std::memory_order_acquire); ++i) {
        AgentInfo& info = monitored_agents_[i];
        if (info.active) {
            uint32_t elapsed = current_time - info.last_kick_time;
            if (elapsed > info.timeout_ms) {
                report_timeout(info.agent_id);
            }
        }
    }
}

void SystemWatchdogAgent::report_timeout(AgentId agent_id) {
    internal_messages::WatchdogTimeout timeout_msg;
    timeout_msg.failed_agent = agent_id;
    timeout_msg.last_kick_time = 0; // 필요시 저장된 시간 사용
    timeout_msg.timeout_duration = global_timeout_ms_.load(std::memory_order_acquire);
    
    // 복구 Agent에게 알림
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.is_initialized()) {
        send_message(manager.get_recovery_agent_id(), timeout_msg);
    }
}

// ============================================================================
// SystemRecoveryAgent Implementation
// ============================================================================

void SystemRecoveryAgent::trigger_recovery(uint8_t action) {
    // 원자적으로 복구 액션 설정
    uint8_t current = recovery_pending_.load(std::memory_order_acquire);
    while (!recovery_pending_.compare_exchange_weak(
        current, current | action, std::memory_order_acq_rel)) {
        // 재시도
    }
    
    internal_messages::RecoveryRequest recovery_req;
    recovery_req.action = static_cast<internal_messages::RecoveryRequest::Action>(action);
    recovery_req.requester = INVALID_AGENT_ID;
    recovery_req.timestamp = now();
    
    if (id() != INVALID_AGENT_ID) {
        send_message(id(), recovery_req);
    }
}

bool SystemRecoveryAgent::execute_pending_recovery() {
    uint8_t pending = recovery_pending_.exchange(0, std::memory_order_acq_rel);
    
    if (pending == 0) {
        return false;
    }
    
    perform_recovery_action(pending);
    return true;
}

void SystemRecoveryAgent::reset_all_agents() {
    Environment& env = Environment::instance();
    
    // 모든 Agent의 메시지 큐 초기화
    for (size_t i = 0; i < env.agent_count(); ++i) {
        Agent* agent = env.get_agent(static_cast<AgentId>(i));
        if (agent) {
            agent->message_queue_.clear();
            agent->initialize(static_cast<AgentId>(i));
        }
    }
}

void SystemRecoveryAgent::clear_all_queues() {
    Environment& env = Environment::instance();
    
    for (size_t i = 0; i < env.agent_count(); ++i) {
        Agent* agent = env.get_agent(static_cast<AgentId>(i));
        if (agent) {
            agent->message_queue_.clear();
        }
    }
}

void SystemRecoveryAgent::restart_system_timers() {
    Environment& env = Environment::instance();
    env.timer_manager().cleanup_all_timers();
    
    // 시스템 서비스들의 타이머 재시작
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.is_initialized() && manager.get_watchdog_agent()) {
        manager.get_watchdog_agent()->start_periodic_checking();
    }
}

void SystemRecoveryAgent::handle_recovery_request(const Message<internal_messages::RecoveryRequest>& msg) {
    perform_recovery_action(static_cast<uint8_t>(msg.data.action));
    
    // 요청자에게 결과 응답
    internal_messages::RecoveryStatus status;
    status.requester = msg.data.requester;
    status.success = true;
    status.actions_performed = static_cast<uint8_t>(msg.data.action);
    
    if (msg.data.requester != INVALID_AGENT_ID) {
        send_message(msg.data.requester, status);
    }
}

void SystemRecoveryAgent::handle_watchdog_timeout(const Message<internal_messages::WatchdogTimeout>& msg) {
    // Watchdog 타임아웃 시 자동 복구
    trigger_recovery(static_cast<uint8_t>(internal_messages::RecoveryRequest::RESET_AGENTS));
}

void SystemRecoveryAgent::perform_recovery_action(uint8_t action) {
    if (action & static_cast<uint8_t>(internal_messages::RecoveryRequest::CLEAR_QUEUES)) {
        clear_all_queues();
    }
    
    if (action & static_cast<uint8_t>(internal_messages::RecoveryRequest::RESET_AGENTS)) {
        reset_all_agents();
    }
    
    if (action & static_cast<uint8_t>(internal_messages::RecoveryRequest::RESTART_TIMERS)) {
        restart_system_timers();
    }
}

// ============================================================================
// SystemServiceManager Implementation
// ============================================================================

SystemServiceManager::SystemServiceManager() 
    : error_agent_(nullptr), metrics_agent_(nullptr), performance_agent_(nullptr),
      watchdog_agent_(nullptr), recovery_agent_(nullptr), initialized_(false) {
}

SystemServiceManager::~SystemServiceManager() {
    shutdown();
}

bool SystemServiceManager::initialize() {
    if (initialized_) {
        return true;
    }
    
    Environment& env = Environment::instance();
    
    // System Agent들 생성
    error_agent_ = new SystemErrorAgent();
    metrics_agent_ = new SystemMetricsAgent();
    performance_agent_ = new PerformanceMonitorAgent();
    watchdog_agent_ = new SystemWatchdogAgent();
    recovery_agent_ = new SystemRecoveryAgent();
    
    // Environment에 등록
    error_agent_id_ = env.register_agent(error_agent_);
    metrics_agent_id_ = env.register_agent(metrics_agent_);
    performance_agent_id_ = env.register_agent(performance_agent_);
    watchdog_agent_id_ = env.register_agent(watchdog_agent_);
    recovery_agent_id_ = env.register_agent(recovery_agent_);
    
    if (error_agent_id_ == INVALID_AGENT_ID || 
        metrics_agent_id_ == INVALID_AGENT_ID ||
        performance_agent_id_ == INVALID_AGENT_ID ||
        watchdog_agent_id_ == INVALID_AGENT_ID ||
        recovery_agent_id_ == INVALID_AGENT_ID) {
        return false;
    }
    
    // Watchdog 초기화
    watchdog_agent_->init(5000); // 5초 타임아웃
    
    initialized_ = true;
    return true;
}

void SystemServiceManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    Environment& env = Environment::instance();
    
    // Agent들 등록 해제
    if (error_agent_id_ != INVALID_AGENT_ID) {
        env.unregister_agent(error_agent_id_);
    }
    if (metrics_agent_id_ != INVALID_AGENT_ID) {
        env.unregister_agent(metrics_agent_id_);
    }
    if (performance_agent_id_ != INVALID_AGENT_ID) {
        env.unregister_agent(performance_agent_id_);
    }
    if (watchdog_agent_id_ != INVALID_AGENT_ID) {
        env.unregister_agent(watchdog_agent_id_);
    }
    if (recovery_agent_id_ != INVALID_AGENT_ID) {
        env.unregister_agent(recovery_agent_id_);
    }
    
    // Agent들 삭제
    delete error_agent_;
    delete metrics_agent_;
    delete performance_agent_;
    delete watchdog_agent_;
    delete recovery_agent_;
    
    error_agent_ = nullptr;
    metrics_agent_ = nullptr;
    performance_agent_ = nullptr;
    watchdog_agent_ = nullptr;
    recovery_agent_ = nullptr;
    
    initialized_ = false;
}

} // namespace mini_so