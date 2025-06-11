#pragma once

#include "mini_sobjectizer_v2.h"

namespace mini_so {

// ============================================================================
// System Service Messages - 내부 통신용
// ============================================================================

namespace internal_messages {
    // 오류 보고 메시지
    struct ErrorReport {
        ErrorCode code;
        AgentId source_agent;
        uint32_t timestamp;
        const char* context;
    };
    
    // 오류 상태 조회
    struct ErrorQuery {
        AgentId requester;
        uint32_t request_id;
    };
    
    struct ErrorStatus {
        uint32_t request_id;
        uint32_t error_count;
        ErrorCode last_error;
        uint32_t last_error_timestamp;
        SystemHealth health;
    };
    
    // 메트릭 보고 메시지
    struct MetricReport {
        enum Type : uint8_t {
            MESSAGE_SENT,
            MESSAGE_PROCESSED,
            TIMER_OVERRUN,
            QUEUE_DEPTH_UPDATE
        } type;
        
        AgentId source_agent;
        uint32_t value;
        uint32_t timestamp;
    };
    
    // 메트릭 조회
    struct MetricQuery {
        AgentId requester;
        uint32_t request_id;
    };
    
    struct MetricStatus {
        uint32_t request_id;
        uint32_t total_messages_sent;
        uint32_t total_messages_processed;
        uint32_t max_queue_depth;
        uint32_t timer_overruns;
    };
    
    // 성능 모니터링 메시지
    struct PerformanceReport {
        AgentId source_agent;
        uint32_t processing_time_ms;
        uint32_t message_latency_ms;
        uint32_t timestamp;
    };
    
    struct PerformanceQuery {
        AgentId requester;
        uint32_t request_id;
    };
    
    struct PerformanceStatus {
        uint32_t request_id;
        uint32_t max_processing_time;
        uint32_t max_message_latency;
        uint32_t loop_count;
    };
    
    // Watchdog 메시지
    struct WatchdogKick {
        AgentId source_agent;
        uint32_t timestamp;
    };
    
    struct WatchdogTimeout {
        AgentId failed_agent;
        uint32_t last_kick_time;
        uint32_t timeout_duration;
    };
    
    // 복구 메시지
    struct RecoveryRequest {
        enum Action : uint8_t {
            RESET_AGENTS = 1,
            CLEAR_QUEUES = 2,
            RESTART_TIMERS = 4,
            FULL_RESET = 7
        } action;
        
        AgentId requester;
        uint32_t timestamp;
    };
    
    struct RecoveryStatus {
        AgentId requester;
        bool success;
        uint32_t actions_performed;
    };
}

// ============================================================================
// System Error Agent - ErrorHandler를 대체
// ============================================================================

class SystemErrorAgent : public Agent {
private:
    struct ErrorEntry {
        ErrorCode code;
        AgentId source_agent;
        uint32_t timestamp;
        const char* context;
    };
    
    std::array<ErrorEntry, 32> error_log_;
    std::atomic<uint32_t> error_count_{0};
    std::atomic<ErrorCode> last_error_{ErrorCode::SUCCESS};
    std::atomic<uint32_t> last_error_timestamp_{0};
    
public:
    void so_define_states() override {
        // 단순한 상태 - 항상 활성
        StateId active_state = define_state("Active");
        transition_to(active_state);
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::ErrorReport)) {
            handle_error_report(static_cast<const Message<internal_messages::ErrorReport>&>(msg));
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::ErrorQuery)) {
            handle_error_query(static_cast<const Message<internal_messages::ErrorQuery>&>(msg));
            return true;
        }
        
        return false;
    }
    
    // 기존 ErrorHandler와 호환되는 인터페이스
    void report_error(ErrorCode code, const char* context = nullptr);
    ErrorCode get_last_error() const;
    uint32_t get_error_count() const;
    uint32_t get_last_error_timestamp() const;
    SystemHealth get_system_health() const;
    void reset_error_state();
    
private:
    void handle_error_report(const Message<internal_messages::ErrorReport>& msg);
    void handle_error_query(const Message<internal_messages::ErrorQuery>& msg);
    void store_error(const internal_messages::ErrorReport& error);
};

// ============================================================================
// System Metrics Agent - SystemMetrics를 대체
// ============================================================================

class SystemMetricsAgent : public Agent {
private:
    std::atomic<uint32_t> total_messages_sent_{0};
    std::atomic<uint32_t> total_messages_processed_{0};
    std::atomic<uint32_t> active_timers_{0};
    std::atomic<uint32_t> timer_overruns_{0};
    std::atomic<uint32_t> max_queue_depth_{0};
    
public:
    void so_define_states() override {
        StateId active_state = define_state("Active");
        transition_to(active_state);
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::MetricReport)) {
            handle_metric_report(static_cast<const Message<internal_messages::MetricReport>&>(msg));
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::MetricQuery)) {
            handle_metric_query(static_cast<const Message<internal_messages::MetricQuery>&>(msg));
            return true;
        }
        
        return false;
    }
    
    // 기존 SystemMetrics와 호환되는 인터페이스
    void increment_messages_sent();
    void increment_messages_processed();
    void report_timer_overrun();
    void update_max_queue_depth(uint32_t depth);
    void reset_metrics();
    
    uint32_t get_messages_sent() const;
    uint32_t get_messages_processed() const;
    uint32_t get_timer_overruns() const;
    uint32_t get_max_queue_depth() const;
    uint32_t get_pending_messages() const;
    
private:
    void handle_metric_report(const Message<internal_messages::MetricReport>& msg);
    void handle_metric_query(const Message<internal_messages::MetricQuery>& msg);
};

// ============================================================================
// Performance Monitor Agent - MinimalPerformanceMonitor를 대체
// ============================================================================

class PerformanceMonitorAgent : public Agent {
private:
    std::atomic<uint32_t> max_message_latency_ms_{0};
    std::atomic<uint32_t> max_processing_time_ms_{0};
    std::atomic<uint32_t> loop_count_{0};
    
public:
    void so_define_states() override {
        StateId active_state = define_state("Active");
        transition_to(active_state);
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::PerformanceReport)) {
            handle_performance_report(static_cast<const Message<internal_messages::PerformanceReport>&>(msg));
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::PerformanceQuery)) {
            handle_performance_query(static_cast<const Message<internal_messages::PerformanceQuery>&>(msg));
            return true;
        }
        
        return false;
    }
    
    // 기존 MinimalPerformanceMonitor와 호환되는 인터페이스
    void record_message_latency(uint32_t latency_ms);
    void record_processing_time(uint32_t processing_time_ms);
    void increment_loop_count();
    void reset_stats();
    
    uint32_t get_max_message_latency_ms() const;
    uint32_t get_max_processing_time_ms() const;
    uint32_t get_loop_count() const;
    
private:
    void handle_performance_report(const Message<internal_messages::PerformanceReport>& msg);
    void handle_performance_query(const Message<internal_messages::PerformanceQuery>& msg);
};

// ============================================================================
// Watchdog Agent - MinimalWatchdog를 대체
// ============================================================================

class SystemWatchdogAgent : public Agent {
private:
    struct AgentInfo {
        AgentId agent_id;
        uint32_t last_kick_time;
        uint32_t timeout_ms;
        bool active;
    };
    
    std::array<AgentInfo, MINI_SO_MAX_AGENTS> monitored_agents_;
    std::atomic<uint32_t> monitored_count_{0};
    std::atomic<uint32_t> global_timeout_ms_{5000};
    std::atomic<bool> enabled_{false};
    TimerId check_timer_id_{0};
    
public:
    void so_define_states() override {
        StateId active_state = define_state("Active");
        transition_to(active_state);
        
        // 주기적 체크 타이머 시작
        start_periodic_checking();
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::WatchdogKick)) {
            handle_watchdog_kick(static_cast<const Message<internal_messages::WatchdogKick>&>(msg));
            return true;
        }
        
        return false;
    }
    
    // 기존 MinimalWatchdog와 호환되는 인터페이스
    bool init(uint32_t timeout_ms);
    void kick();
    void enable(bool state);
    bool is_expired();
    uint32_t time_since_last_kick();
    void trigger_system_recovery();
    
    // Agent 모니터링 등록
    void register_agent_for_monitoring(AgentId agent_id, uint32_t timeout_ms);
    
    // 공개 메서드로 변경 - BasicRecovery에서 호출
    void start_periodic_checking();
    
private:
    void handle_watchdog_kick(const Message<internal_messages::WatchdogKick>& msg);
    void check_all_timeouts();
    void report_timeout(AgentId agent_id);
};

// ============================================================================
// System Recovery Agent - BasicRecovery를 대체  
// ============================================================================

class SystemRecoveryAgent : public Agent {
private:
    std::atomic<uint8_t> recovery_pending_{0};
    
public:
    void so_define_states() override {
        StateId active_state = define_state("Active");
        transition_to(active_state);
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::RecoveryRequest)) {
            handle_recovery_request(static_cast<const Message<internal_messages::RecoveryRequest>&>(msg));
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(internal_messages::WatchdogTimeout)) {
            handle_watchdog_timeout(static_cast<const Message<internal_messages::WatchdogTimeout>&>(msg));
            return true;
        }
        
        return false;
    }
    
    // 기존 BasicRecovery와 호환되는 인터페이스
    void trigger_recovery(uint8_t action);
    bool execute_pending_recovery();
    void reset_all_agents();
    void clear_all_queues();
    void restart_system_timers();
    
private:
    void handle_recovery_request(const Message<internal_messages::RecoveryRequest>& msg);
    void handle_watchdog_timeout(const Message<internal_messages::WatchdogTimeout>& msg);
    void perform_recovery_action(uint8_t action);
};

// ============================================================================
// System Service Manager - 모든 시스템 Agent 관리
// ============================================================================

class SystemServiceManager {
private:
    SystemErrorAgent* error_agent_;
    SystemMetricsAgent* metrics_agent_;
    PerformanceMonitorAgent* performance_agent_;
    SystemWatchdogAgent* watchdog_agent_;
    SystemRecoveryAgent* recovery_agent_;
    
    AgentId error_agent_id_;
    AgentId metrics_agent_id_;
    AgentId performance_agent_id_;
    AgentId watchdog_agent_id_;
    AgentId recovery_agent_id_;
    
    bool initialized_;
    
    SystemServiceManager();
    ~SystemServiceManager();
    
public:
    static SystemServiceManager& instance() {
        static SystemServiceManager manager;
        return manager;
    }
    
    // 시스템 서비스 초기화
    bool initialize();
    void shutdown();
    
    // Agent 접근자들
    SystemErrorAgent* get_error_agent() { return error_agent_; }
    SystemMetricsAgent* get_metrics_agent() { return metrics_agent_; }
    PerformanceMonitorAgent* get_performance_agent() { return performance_agent_; }
    SystemWatchdogAgent* get_watchdog_agent() { return watchdog_agent_; }
    SystemRecoveryAgent* get_recovery_agent() { return recovery_agent_; }
    
    // Agent ID 접근자들
    AgentId get_error_agent_id() const { return error_agent_id_; }
    AgentId get_metrics_agent_id() const { return metrics_agent_id_; }
    AgentId get_performance_agent_id() const { return performance_agent_id_; }
    AgentId get_watchdog_agent_id() const { return watchdog_agent_id_; }
    AgentId get_recovery_agent_id() const { return recovery_agent_id_; }
    
    bool is_initialized() const { return initialized_; }
};

} // namespace mini_so