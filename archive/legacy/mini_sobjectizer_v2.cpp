#include "mini_sobjectizer/mini_sobjectizer_v2.h"
#include "mini_sobjectizer/system_agents.h"
#include <cstring>

namespace mini_so {

// Static member initialization
namespace detail {
    uint16_t MessageIdGenerator::counter_ = 1;
}

Environment* Environment::instance_ = nullptr;

// ============================================================================
// Error Handler Implementation - Delegated to SystemErrorAgent
// ============================================================================

SystemErrorAgent* ErrorHandler::get_error_agent() {
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.is_initialized()) {
        return manager.get_error_agent();
    }
    return nullptr;
}

void ErrorHandler::report_error(ErrorCode code, const char* context) {
    SystemErrorAgent* agent = get_error_agent();
    if (agent) {
        agent->report_error(code, context);
    }
    // Fallback: do nothing if system services not initialized
}

ErrorCode ErrorHandler::get_last_error() {
    SystemErrorAgent* agent = get_error_agent();
    if (agent) {
        return agent->get_last_error();
    }
    return ErrorCode::SUCCESS;
}

uint32_t ErrorHandler::get_error_count() {
    SystemErrorAgent* agent = get_error_agent();
    if (agent) {
        return agent->get_error_count();
    }
    return 0;
}

uint32_t ErrorHandler::get_last_error_timestamp() {
    SystemErrorAgent* agent = get_error_agent();
    if (agent) {
        return agent->get_last_error_timestamp();
    }
    return 0;
}

SystemHealth ErrorHandler::get_system_health() {
    SystemErrorAgent* agent = get_error_agent();
    if (agent) {
        return agent->get_system_health();
    }
    return SystemHealth::HEALTHY;
}

void ErrorHandler::reset_error_state() {
    SystemErrorAgent* agent = get_error_agent();
    if (agent) {
        agent->reset_error_state();
    }
}

const char* ErrorHandler::error_code_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        case ErrorCode::MUTEX_CREATION_FAILED: return "MUTEX_CREATION_FAILED";
        case ErrorCode::QUEUE_OVERFLOW: return "QUEUE_OVERFLOW";
        case ErrorCode::MESSAGE_TOO_LARGE: return "MESSAGE_TOO_LARGE";
        case ErrorCode::INVALID_MESSAGE: return "INVALID_MESSAGE";
        case ErrorCode::TIMER_ALLOCATION_FAILED: return "TIMER_ALLOCATION_FAILED";
        case ErrorCode::INVALID_STATE_TRANSITION: return "INVALID_STATE_TRANSITION";
        case ErrorCode::AGENT_REGISTRATION_FAILED: return "AGENT_REGISTRATION_FAILED";
        case ErrorCode::SYSTEM_OVERLOAD: return "SYSTEM_OVERLOAD";
        case ErrorCode::MEMORY_ALLOCATION_FAILED: return "MEMORY_ALLOCATION_FAILED";
        case ErrorCode::MESSAGE_CORRUPTION_DETECTED: return "MESSAGE_CORRUPTION_DETECTED";
        case ErrorCode::MESSAGE_CHECKSUM_FAILED: return "MESSAGE_CHECKSUM_FAILED";
        case ErrorCode::MESSAGE_SEQUENCE_ERROR: return "MESSAGE_SEQUENCE_ERROR";
        case ErrorCode::MESSAGE_FORMAT_INVALID: return "MESSAGE_FORMAT_INVALID";
        default: return "UNKNOWN_ERROR";
    }
}

// ============================================================================
// System Metrics Implementation - Delegated to SystemMetricsAgent
// ============================================================================

SystemMetricsAgent* SystemMetrics::get_metrics_agent() {
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.is_initialized()) {
        return manager.get_metrics_agent();
    }
    return nullptr;
}

void SystemMetrics::increment_messages_sent() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        agent->increment_messages_sent();
    }
}

void SystemMetrics::increment_messages_processed() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        agent->increment_messages_processed();
    }
}

void SystemMetrics::report_timer_overrun() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        agent->report_timer_overrun();
    }
}

void SystemMetrics::update_max_queue_depth(uint32_t depth) {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        agent->update_max_queue_depth(depth);
    }
}

void SystemMetrics::reset_metrics() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        agent->reset_metrics();
    }
}

uint32_t SystemMetrics::get_messages_sent() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        return agent->get_messages_sent();
    }
    return 0;
}

uint32_t SystemMetrics::get_messages_processed() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        return agent->get_messages_processed();
    }
    return 0;
}

uint32_t SystemMetrics::get_timer_overruns() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        return agent->get_timer_overruns();
    }
    return 0;
}

uint32_t SystemMetrics::get_max_queue_depth() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        return agent->get_max_queue_depth();
    }
    return 0;
}

uint32_t SystemMetrics::get_pending_messages() {
    SystemMetricsAgent* agent = get_metrics_agent();
    if (agent) {
        return agent->get_pending_messages();
    }
    return 0;
}

// ============================================================================
// Message Validator Implementation
// ============================================================================
std::atomic<uint32_t> MessageValidator::message_sequence_counter_{1};
std::atomic<uint32_t> MessageValidator::corrupted_messages_detected_{0};
std::atomic<uint32_t> MessageValidator::checksum_failures_{0};
bool MessageValidator::crc32_table_initialized_ = false;

// CRC32 polynomial: 0x04C11DB7 (standard IEEE 802.3)
const uint32_t MessageValidator::crc32_table_[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

void MessageValidator::initialize() {
    crc32_table_initialized_ = true;
    message_sequence_counter_.store(1, std::memory_order_release);
    corrupted_messages_detected_.store(0, std::memory_order_release);
    checksum_failures_.store(0, std::memory_order_release);
}

uint32_t MessageValidator::calculate_crc32(const void* data, size_t length) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; ++i) {
        uint8_t table_index = (crc ^ bytes[i]) & 0xFF;
        crc = (crc >> 8) ^ crc32_table_[table_index];
    }
    
    return crc ^ 0xFFFFFFFF;
}

uint32_t MessageValidator::calculate_message_checksum(const MessageBase& msg, MessageSize size) {
    // Calculate checksum of everything except the checksum field itself
    uint32_t saved_checksum = msg.header.checksum;
    
    // Temporarily zero out the checksum field
    const_cast<MessageBase&>(msg).header.checksum = 0;
    
    uint32_t calculated_checksum = calculate_crc32(&msg, size);
    
    // Restore the original checksum
    const_cast<MessageBase&>(msg).header.checksum = saved_checksum;
    
    return calculated_checksum;
}

bool MessageValidator::verify_message_integrity(const MessageBase& msg, MessageSize size, uint32_t expected_checksum) {
    // Basic format validation
    if (!validate_message_format(msg, size)) {
        corrupted_messages_detected_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    
    // Calculate and verify checksum
    uint32_t calculated_checksum = calculate_message_checksum(msg, size);
    
    if (calculated_checksum != expected_checksum) {
        checksum_failures_.fetch_add(1, std::memory_order_relaxed);
        ErrorHandler::report_error(ErrorCode::MESSAGE_CHECKSUM_FAILED, "MessageValidator::verify_message_integrity");
        return false;
    }
    
    return true;
}

bool MessageValidator::validate_message_format(const MessageBase& msg, MessageSize size) {
    // Check minimum size
    if (size < sizeof(MessageHeader)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageValidator::validate_message_format - size too small");
        return false;
    }
    
    // Check maximum size
    if (size > MINI_SO_MAX_MESSAGE_SIZE) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageValidator::validate_message_format - size too large");
        return false;
    }
    
    // Check version
    if (msg.header.version == 0 || msg.header.version > 1) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageValidator::validate_message_format - invalid version");
        return false;
    }
    
    // Check payload size consistency
    if (msg.header.payload_size != (size - sizeof(MessageHeader))) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageValidator::validate_message_format - payload size mismatch");
        return false;
    }
    
    // Check timestamp reasonableness (not too far in future or past)
    TimePoint current_time = now();
    TimePoint msg_time = msg.header.timestamp;
    
    // Allow 1 minute tolerance for clock differences
    const uint32_t TIME_TOLERANCE_MS = 60000;
    
    if (msg_time > current_time + TIME_TOLERANCE_MS) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageValidator::validate_message_format - timestamp too far in future");
        return false;
    }
    
    // Don't reject old messages as they might be legitimate delayed messages
    
    return true;
}

bool MessageValidator::validate_message_type(MessageId type_id) {
    // Check for reserved/invalid message types
    if (type_id == INVALID_MESSAGE_ID || type_id == 0) {
        return false;
    }
    
    return true;
}

bool MessageValidator::validate_message_size(MessageSize size) {
    return size >= sizeof(MessageHeader) && size <= MINI_SO_MAX_MESSAGE_SIZE;
}

uint32_t MessageValidator::get_next_sequence_number() {
    uint32_t seq = message_sequence_counter_.fetch_add(1, std::memory_order_acq_rel);
    
    // Handle wrap-around (avoid 0 as it might be used as invalid)
    if (seq == 0) {
        message_sequence_counter_.compare_exchange_strong(seq, 1, std::memory_order_acq_rel);
        return 1;
    }
    
    return seq;
}

bool MessageValidator::validate_message_sequence(uint32_t sequence_number) {
    // For now, just check that sequence number is not zero
    // In a more sophisticated implementation, we could track per-sender sequence numbers
    // and detect gaps or duplicates
    
    if (sequence_number == 0) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_SEQUENCE_ERROR, "MessageValidator::validate_message_sequence - zero sequence");
        return false;
    }
    
    return true;
}

void MessageValidator::reset_validation_stats() {
    corrupted_messages_detected_.store(0, std::memory_order_release);
    checksum_failures_.store(0, std::memory_order_release);
}

// ============================================================================
// MessageBase Implementation
// ============================================================================

MessageBase::MessageBase(MessageId id, AgentId sender) : header(id, sender) {
    header.sequence_number = MessageValidator::get_next_sequence_number();
}

void MessageBase::calculate_and_set_checksum(MessageSize total_size) {
    header.payload_size = total_size - sizeof(MessageHeader);
    header.checksum = MessageValidator::calculate_message_checksum(*this, total_size);
}

bool MessageBase::verify_integrity(MessageSize total_size) const {
    return MessageValidator::verify_message_integrity(*this, total_size, header.checksum);
}

// ============================================================================
// Minimal Watchdog System Implementation - Delegated to SystemWatchdogAgent
// ============================================================================

SystemWatchdogAgent* MinimalWatchdog::get_watchdog_agent() {
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.is_initialized()) {
        return manager.get_watchdog_agent();
    }
    return nullptr;
}

bool MinimalWatchdog::init(uint32_t timeout_ms) {
    SystemWatchdogAgent* agent = get_watchdog_agent();
    if (agent) {
        return agent->init(timeout_ms);
    }
    return false;
}

void MinimalWatchdog::kick() {
    SystemWatchdogAgent* agent = get_watchdog_agent();
    if (agent) {
        agent->kick();
    }
}

void MinimalWatchdog::enable(bool state) {
    SystemWatchdogAgent* agent = get_watchdog_agent();
    if (agent) {
        agent->enable(state);
    }
}

bool MinimalWatchdog::is_expired() {
    SystemWatchdogAgent* agent = get_watchdog_agent();
    if (agent) {
        return agent->is_expired();
    }
    return false;
}

uint32_t MinimalWatchdog::time_since_last_kick() {
    SystemWatchdogAgent* agent = get_watchdog_agent();
    if (agent) {
        return agent->time_since_last_kick();
    }
    return 0;
}

void MinimalWatchdog::watchdog_callback() {
    SystemWatchdogAgent* agent = get_watchdog_agent();
    if (agent && agent->is_expired()) {
        ErrorHandler::report_error(ErrorCode::SYSTEM_OVERLOAD, "Watchdog timeout");
        trigger_system_recovery();
    }
}

void MinimalWatchdog::trigger_system_recovery() {
    SystemWatchdogAgent* agent = get_watchdog_agent();
    if (agent) {
        agent->trigger_system_recovery();
    }
}

// ============================================================================
// Basic System Recovery Implementation (Phase 1-5)  
// ============================================================================

std::atomic<uint8_t> BasicRecovery::recovery_pending_{0};

void BasicRecovery::trigger_recovery(RecoveryAction action) {
    // 원자적으로 복구 액션 설정 (비트 OR)
    uint8_t current = recovery_pending_.load(std::memory_order_acquire);
    while (!recovery_pending_.compare_exchange_weak(
        current, current | static_cast<uint8_t>(action), 
        std::memory_order_acq_rel)) {
        // 재시도
    }
    
    ErrorHandler::report_error(ErrorCode::SYSTEM_OVERLOAD, "Recovery triggered");
}

bool BasicRecovery::execute_pending_recovery() {
    uint8_t pending = recovery_pending_.exchange(0, std::memory_order_acq_rel);
    
    if (pending == 0) {
        return false; // 복구할 것이 없음
    }
    
    // 단계별 복구 실행 (중요도 순)
    if (pending & CLEAR_QUEUES) {
        clear_all_queues();
    }
    
    if (pending & RESET_AGENTS) {
        reset_all_agents();
    }
    
    if (pending & RESTART_TIMERS) {
        restart_system_timers();
    }
    
    return true;
}

void BasicRecovery::reset_all_agents() {
    Environment& env = Environment::instance();
    
    // 모든 에이전트의 메시지 큐 초기화
    for (size_t i = 0; i < env.agent_count(); ++i) {
        Agent* agent = env.get_agent(static_cast<AgentId>(i));
        if (agent) {
            agent->message_queue_.clear();
            // 에이전트를 초기 상태로 재초기화
            agent->initialize(static_cast<AgentId>(i));
        }
    }
}

void BasicRecovery::clear_all_queues() {
    Environment& env = Environment::instance();
    
    // 모든 메시지 큐 비우기
    for (size_t i = 0; i < env.agent_count(); ++i) {
        Agent* agent = env.get_agent(static_cast<AgentId>(i));
        if (agent) {
            agent->message_queue_.clear();
        }
    }
}

void BasicRecovery::restart_system_timers() {
    Environment& env = Environment::instance();
    
    // 모든 타이머 정리 후 재시작
    env.timer_manager().cleanup_all_timers();
    
    // 시스템 서비스의 타이머들 재시작
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.is_initialized() && manager.get_watchdog_agent()) {
        manager.get_watchdog_agent()->start_periodic_checking();
    }
}

// ============================================================================
// Minimal Performance Monitor Implementation - Delegated to PerformanceMonitorAgent
// ============================================================================

PerformanceMonitorAgent* MinimalPerformanceMonitor::get_performance_agent() {
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.is_initialized()) {
        return manager.get_performance_agent();
    }
    return nullptr;
}

void MinimalPerformanceMonitor::record_message_latency(uint32_t latency_ms) {
    PerformanceMonitorAgent* agent = get_performance_agent();
    if (agent) {
        agent->record_message_latency(latency_ms);
    }
}

void MinimalPerformanceMonitor::record_processing_time(uint32_t processing_time_ms) {
    PerformanceMonitorAgent* agent = get_performance_agent();
    if (agent) {
        agent->record_processing_time(processing_time_ms);
    }
}

void MinimalPerformanceMonitor::increment_loop_count() {
    PerformanceMonitorAgent* agent = get_performance_agent();
    if (agent) {
        agent->increment_loop_count();
    }
}

uint32_t MinimalPerformanceMonitor::get_max_message_latency_ms() {
    PerformanceMonitorAgent* agent = get_performance_agent();
    if (agent) {
        return agent->get_max_message_latency_ms();
    }
    return 0;
}

uint32_t MinimalPerformanceMonitor::get_max_processing_time_ms() {
    PerformanceMonitorAgent* agent = get_performance_agent();
    if (agent) {
        return agent->get_max_processing_time_ms();
    }
    return 0;
}

uint32_t MinimalPerformanceMonitor::get_loop_count() {
    PerformanceMonitorAgent* agent = get_performance_agent();
    if (agent) {
        return agent->get_loop_count();
    }
    return 0;
}

void MinimalPerformanceMonitor::reset_stats() {
    PerformanceMonitorAgent* agent = get_performance_agent();
    if (agent) {
        agent->reset_stats();
    }
}

// ============================================================================
// TimerManager Implementation (Phase 1)
// ============================================================================

TimerManager::TimerManager() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ErrorHandler::report_error(ErrorCode::MUTEX_CREATION_FAILED, "TimerManager");
        // Try to continue without mutex - degraded operation mode
        // In a real system, might trigger system reset or safe mode
    }
    
    // Initialize all timers to inactive state
    for (auto& timer : timers_) {
        timer = Timer{};
    }
}

TimerManager::~TimerManager() {
    cleanup_all_timers();
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

TimerId TimerManager::start_timer(Duration duration_ms, TimerCallback callback, bool periodic) {
    if (!callback || duration_ms == 0) {
        ErrorHandler::report_error(ErrorCode::INVALID_MESSAGE, "TimerManager::start_timer - invalid parameters");
        return 0;  // Invalid timer ID
    }
    
    if (!mutex_) {
        ErrorHandler::report_error(ErrorCode::MUTEX_CREATION_FAILED, "TimerManager::start_timer - no mutex");
        return 0;
    }
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        ErrorHandler::report_error(ErrorCode::SYSTEM_OVERLOAD, "TimerManager::start_timer - mutex timeout");
        return 0;  // Could not acquire mutex
    }
    
    // Find available timer slot
    Timer* timer_slot = find_timer_slot();
    if (!timer_slot) {
        ErrorHandler::report_error(ErrorCode::TIMER_ALLOCATION_FAILED, "TimerManager::start_timer - no slots");
        xSemaphoreGive(mutex_);
        return 0;  // No available timer slots
    }
    
    // Allocate unique timer ID (atomic operation)
    TimerId timer_id = allocate_timer_id();
    if (timer_id == 0) {
        ErrorHandler::report_error(ErrorCode::TIMER_ALLOCATION_FAILED, "TimerManager::start_timer - ID allocation failed");
        xSemaphoreGive(mutex_);
        return 0;  // Failed to allocate ID
    }
    
    // Initialize timer
    TickType_t period_ticks = Timer::ms_to_ticks(duration_ms);
    *timer_slot = Timer(timer_id, period_ticks, periodic, callback);
    timer_slot->start(xTaskGetTickCount());
    
    active_timer_count_.fetch_add(1, std::memory_order_acq_rel);
    
    xSemaphoreGive(mutex_);
    return timer_id;
}

bool TimerManager::cancel_timer(TimerId timer_id) {
    if (timer_id == 0) {
        return false;
    }
    
    if (!mutex_) {
        ErrorHandler::report_error(ErrorCode::MUTEX_CREATION_FAILED, "TimerManager::cancel_timer - no mutex");
        return false;
    }
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        ErrorHandler::report_error(ErrorCode::SYSTEM_OVERLOAD, "TimerManager::cancel_timer - mutex timeout");
        return false;
    }
    
    Timer* timer = find_timer_by_id(timer_id);
    if (timer && timer->state == TimerState::ACTIVE) {
        timer->stop();
        active_timer_count_.fetch_sub(1, std::memory_order_acq_rel);
        xSemaphoreGive(mutex_);
        return true;
    }
    
    xSemaphoreGive(mutex_);
    return false;
}

void TimerManager::update_timers() {
    if (!mutex_ || xSemaphoreTake(mutex_, 0) != pdTRUE) {
        return;  // Skip this update if mutex not immediately available
    }
    
    TickType_t current_tick = xTaskGetTickCount();
    
    // Collect expired timers to execute callbacks safely
    struct ExpiredTimer {
        TimerCallback callback;
        TimerId timer_id;
        bool periodic;
        bool valid;
    };
    
    std::array<ExpiredTimer, MINI_SO_MAX_TIMERS> expired_timers;
    size_t expired_count = 0;
    
    // Phase 1: Find all expired timers under mutex protection
    for (auto& timer : timers_) {
        if (timer.state == TimerState::ACTIVE && timer.is_expired(current_tick)) {
            if (expired_count < MINI_SO_MAX_TIMERS) {
                expired_timers[expired_count] = {
                    timer.callback,
                    timer.id,
                    timer.periodic,
                    true
                };
                expired_count++;
                
                // Mark timer as expired but don't reset yet
                timer.expire();
            }
        }
    }
    
    xSemaphoreGive(mutex_);
    
    // Phase 2: Execute callbacks outside of mutex
    for (size_t i = 0; i < expired_count; ++i) {
        const auto& expired = expired_timers[i];
        if (expired.valid && expired.callback) {
            try {
                expired.callback();
            } catch (...) {
                // Catch any exceptions from timer callbacks
                ErrorHandler::report_error(ErrorCode::SYSTEM_OVERLOAD, "Timer callback exception");
            }
        }
    }
    
    // Phase 3: Handle periodic timer reset and cleanup under mutex
    if (expired_count > 0 && mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        TickType_t new_current_tick = xTaskGetTickCount();
        
        for (size_t i = 0; i < expired_count; ++i) {
            const auto& expired = expired_timers[i];
            Timer* timer_ptr = find_timer_by_id(expired.timer_id);
            
            if (timer_ptr && timer_ptr->state == TimerState::EXPIRED) {
                if (expired.periodic) {
                    // Reset periodic timer
                    timer_ptr->reset_for_periodic(new_current_tick);
                } else {
                    // Clean up one-shot timer
                    active_timer_count_.fetch_sub(1, std::memory_order_acq_rel);
                    *timer_ptr = Timer{};  // Reset to inactive
                }
            }
        }
        
        xSemaphoreGive(mutex_);
    }
}

TimerId TimerManager::allocate_timer_id() {
    TimerId id = next_timer_id_.fetch_add(1, std::memory_order_acq_rel);
    
    // Handle wrap-around case atomically
    if (id == 0) {
        // We wrapped around from UINT16_MAX to 0, reset to 1
        next_timer_id_.compare_exchange_strong(id, 1, std::memory_order_acq_rel);
        return 1;
    }
    
    return id;
}

Timer* TimerManager::find_timer_slot() {
    for (auto& timer : timers_) {
        if (timer.state == TimerState::INACTIVE) {
            return &timer;
        }
    }
    return nullptr;
}

Timer* TimerManager::find_timer_by_id(TimerId id) {
    for (auto& timer : timers_) {
        if (timer.id == id && timer.state != TimerState::INACTIVE) {
            return &timer;
        }
    }
    return nullptr;
}

void TimerManager::cleanup_expired_timers() {
    for (auto& timer : timers_) {
        if (timer.state == TimerState::EXPIRED || timer.state == TimerState::CANCELLED) {
            timer = Timer{};  // Reset to inactive
        }
    }
}

void TimerManager::cleanup_all_timers() {
    if (!mutex_) {
        return;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        for (auto& timer : timers_) {
            timer = Timer{};
        }
        active_timer_count_.store(0, std::memory_order_release);
        xSemaphoreGive(mutex_);
    }
}

bool TimerManager::is_timer_active(TimerId timer_id) const {
    if (timer_id == 0 || !mutex_) {
        return false;
    }
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        for (const auto& timer : timers_) {
            if (timer.id == timer_id && timer.state == TimerState::ACTIVE) {
                xSemaphoreGive(mutex_);
                return true;
            }
        }
        xSemaphoreGive(mutex_);
    }
    return false;
}

// ============================================================================
// MessageQueue Implementation
// ============================================================================

MessageQueue::MessageQueue() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        ErrorHandler::report_error(ErrorCode::MUTEX_CREATION_FAILED, "MessageQueue");
        // Try to continue - will be detected in operations
    }
}

MessageQueue::~MessageQueue() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

MessageQueue::PushResult MessageQueue::push(const MessageBase& msg, MessageSize size) {
    if (size > MINI_SO_MAX_MESSAGE_SIZE) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_TOO_LARGE, "MessageQueue::push");
        return PushResult::MESSAGE_TOO_LARGE;
    }
    
    if (size < sizeof(MessageBase)) {
        ErrorHandler::report_error(ErrorCode::INVALID_MESSAGE, "MessageQueue::push - size too small");
        return PushResult::INVALID_MESSAGE;
    }
    
    if (!mutex_) {
        ErrorHandler::report_error(ErrorCode::MUTEX_CREATION_FAILED, "MessageQueue::push - no mutex");
        return PushResult::MUTEX_TIMEOUT;
    }
    
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(100)) != pdTRUE) {
        ErrorHandler::report_error(ErrorCode::SYSTEM_OVERLOAD, "MessageQueue::push - mutex timeout");
        return PushResult::MUTEX_TIMEOUT;
    }
    
    if (count_ >= MINI_SO_MAX_QUEUE_SIZE) {
        ErrorHandler::report_error(ErrorCode::QUEUE_OVERFLOW, "MessageQueue::push");
        xSemaphoreGive(mutex_);
        return PushResult::QUEUE_FULL;
    }
    
    // Copy message data with bounds checking
    std::size_t current_tail = tail_.load(std::memory_order_relaxed);
    std::memcpy(queue_[current_tail].data, &msg, size);
    queue_[current_tail].size = size;
    queue_[current_tail].valid = true;
    
    // Atomic update of circular buffer indices
    std::size_t new_tail = (current_tail + 1) % MINI_SO_MAX_QUEUE_SIZE;
    tail_.store(new_tail, std::memory_order_release);
    
    std::size_t new_count = count_.fetch_add(1, std::memory_order_acq_rel) + 1;
    
    // Update metrics
    SystemMetrics::update_max_queue_depth(static_cast<uint32_t>(new_count));
    
    xSemaphoreGive(mutex_);
    return PushResult::SUCCESS;
}

bool MessageQueue::pop(uint8_t* buffer, MessageSize& size) {
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    
    if (count_.load(std::memory_order_acquire) == 0) {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    std::size_t current_head = head_.load(std::memory_order_relaxed);
    if (!queue_[current_head].valid) {
        xSemaphoreGive(mutex_);
        return false;
    }
    
    size = queue_[current_head].size;
    std::memcpy(buffer, queue_[current_head].data, size);
    queue_[current_head].valid = false;
    
    // Atomic update of circular buffer indices
    std::size_t new_head = (current_head + 1) % MINI_SO_MAX_QUEUE_SIZE;
    head_.store(new_head, std::memory_order_release);
    
    count_.fetch_sub(1, std::memory_order_acq_rel);
    
    xSemaphoreGive(mutex_);
    return true;
}

void MessageQueue::clear() {
    if (!mutex_) {
        return;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
        count_.store(0, std::memory_order_release);
        
        for (auto& entry : queue_) {
            entry.valid = false;
        }
        
        xSemaphoreGive(mutex_);
    }
}

// ============================================================================
// MessageQueue Validation Implementation (Phase 1-3)
// ============================================================================

MessageQueue::PushResult MessageQueue::push_with_validation(const MessageBase& msg, MessageSize size) {
    // First validate message format and size
    if (!MessageValidator::validate_message_size(size)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageQueue::push_with_validation - invalid size");
        return PushResult::INVALID_MESSAGE;
    }
    
    if (!MessageValidator::validate_message_format(msg, size)) {
        return PushResult::INVALID_MESSAGE;
    }
    
    if (!MessageValidator::validate_message_type(msg.header.type_id)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageQueue::push_with_validation - invalid type");
        return PushResult::INVALID_MESSAGE;
    }
    
    // First verify the message integrity if it already has a checksum
    if (msg.header.checksum != 0) {
        if (!MessageValidator::verify_message_integrity(msg, size, msg.header.checksum)) {
            ErrorHandler::report_error(ErrorCode::MESSAGE_CORRUPTION_DETECTED, "MessageQueue::push_with_validation - pre-validation integrity check failed");
            return PushResult::INVALID_MESSAGE;
        }
    }
    
    // Create a copy of the message for checksum calculation
    uint8_t temp_buffer[MINI_SO_MAX_MESSAGE_SIZE];
    if (size > sizeof(temp_buffer)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_TOO_LARGE, "MessageQueue::push_with_validation - message too large for validation");
        return PushResult::MESSAGE_TOO_LARGE;
    }
    
    std::memcpy(temp_buffer, &msg, size);
    MessageBase* validated_msg = reinterpret_cast<MessageBase*>(temp_buffer);
    
    // Calculate and set checksum (this will overwrite any existing checksum)
    validated_msg->calculate_and_set_checksum(size);
    
    // Verify the checksum was calculated correctly
    if (!validated_msg->verify_integrity(size)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_CHECKSUM_FAILED, "MessageQueue::push_with_validation - checksum verification failed");
        return PushResult::INVALID_MESSAGE;
    }
    
    // Push the validated message with checksum
    return push(*validated_msg, size);
}

bool MessageQueue::pop_with_validation(uint8_t* buffer, MessageSize& size) {
    // First try normal pop
    if (!pop(buffer, size)) {
        return false;
    }
    
    // Validate the popped message
    if (size < sizeof(MessageHeader)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageQueue::pop_with_validation - message too small");
        return false;
    }
    
    const MessageBase* msg = reinterpret_cast<const MessageBase*>(buffer);
    
    // Validate message integrity
    if (!MessageValidator::verify_message_integrity(*msg, size, msg->header.checksum)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_CORRUPTION_DETECTED, "MessageQueue::pop_with_validation - integrity check failed");
        return false;
    }
    
    // Validate message format
    if (!MessageValidator::validate_message_format(*msg, size)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_FORMAT_INVALID, "MessageQueue::pop_with_validation - format validation failed");
        return false;
    }
    
    // Validate sequence number
    if (!MessageValidator::validate_message_sequence(msg->header.sequence_number)) {
        ErrorHandler::report_error(ErrorCode::MESSAGE_SEQUENCE_ERROR, "MessageQueue::pop_with_validation - sequence validation failed");
        return false;
    }
    
    return true;
}

// ============================================================================
// Agent Implementation
// ============================================================================

void Agent::initialize(AgentId id) {
    id_ = id;
    timer_manager_ = nullptr;  // Will be set by Environment
    so_define_states();
}

// ============================================================================
// Agent Timer Management Implementation
// ============================================================================

TimerId Agent::start_timer(Duration duration_ms, TimerCallback callback, bool periodic) {
    if (!timer_manager_) {
        return 0;  // Timer manager not initialized
    }
    return timer_manager_->start_timer(duration_ms, callback, periodic);
}

TimerId Agent::start_periodic_timer(Duration duration_ms, TimerCallback callback) {
    return start_timer(duration_ms, callback, true);
}

TimerId Agent::start_oneshot_timer(Duration duration_ms, TimerCallback callback) {
    return start_timer(duration_ms, callback, false);
}

bool Agent::cancel_timer(TimerId timer_id) {
    if (!timer_manager_) {
        return false;
    }
    return timer_manager_->cancel_timer(timer_id);
}

// ============================================================================
// StateMachine Implementation (Phase 1)
// ============================================================================

StateMachine::StateMachine() : state_count_(0) {
    // Initialize all states to invalid
    for (auto& state : states_) {
        state = StateInfo{};
    }
    
    // Define default initial state (state 0)
    define_state(0, "Initial");
    current_state_.store(0, std::memory_order_release);
}

StateId StateMachine::define_state(StateId id, const char* name, StateId parent) {
    if (state_count_ >= MINI_SO_MAX_STATES) {
        return INVALID_STATE_ID;  // No more state slots
    }
    
    // Check if state ID already exists
    if (find_state(id) != nullptr) {
        return INVALID_STATE_ID;  // State already defined
    }
    
    // Validate parent state if specified
    if (parent != INVALID_STATE_ID && !is_valid_state(parent)) {
        return INVALID_STATE_ID;  // Invalid parent state
    }
    
    // Find available slot
    for (auto& state : states_) {
        if (state.id == INVALID_STATE_ID) {
            state = StateInfo(id, name, parent);
            state_count_++;
            return id;
        }
    }
    
    return INVALID_STATE_ID;  // No available slots
}

bool StateMachine::set_state_entry_handler(StateId state_id, StateEntryCallback handler) {
    StateInfo* state = find_state(state_id);
    if (state && handler) {
        state->on_enter = handler;
        return true;
    }
    return false;
}

bool StateMachine::set_state_exit_handler(StateId state_id, StateExitCallback handler) {
    StateInfo* state = find_state(state_id);
    if (state && handler) {
        state->on_exit = handler;
        return true;
    }
    return false;
}

bool StateMachine::set_state_message_handler(StateId state_id, StateMessageHandler handler) {
    StateInfo* state = find_state(state_id);
    if (state && handler) {
        state->message_handler = handler;
        return true;
    }
    return false;
}

bool StateMachine::transition_to(StateId new_state) {
    // Atomic check and set of transition flag
    bool expected = false;
    if (!in_transition_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false;  // Already in transition
    }
    
    if (!is_valid_state(new_state)) {
        in_transition_.store(false, std::memory_order_release);
        return false;  // Invalid target state
    }
    
    StateId current = current_state_.load(std::memory_order_acquire);
    if (current == new_state) {
        in_transition_.store(false, std::memory_order_release);
        return true;  // Already in target state
    }
    
    // Execute exit chain from current state
    if (current != INVALID_STATE_ID) {
        execute_exit_chain(current, new_state);
    }
    
    // Update state atomically
    previous_state_.store(current, std::memory_order_release);
    current_state_.store(new_state, std::memory_order_release);
    
    // Execute entry chain to new state
    execute_entry_chain(new_state);
    
    in_transition_.store(false, std::memory_order_release);
    return true;
}

bool StateMachine::is_in_state(StateId state_id) const {
    StateId current = current_state_.load(std::memory_order_acquire);
    if (current == state_id) {
        return true;
    }
    
    // Check if current state is a child of the specified state
    return is_child_of(current, state_id);
}

bool StateMachine::is_child_of(StateId child_state, StateId parent_state) const {
    if (child_state == INVALID_STATE_ID || parent_state == INVALID_STATE_ID) {
        return false;
    }
    
    const StateInfo* child = find_state(child_state);
    while (child && child->parent_id != INVALID_STATE_ID) {
        if (child->parent_id == parent_state) {
            return true;
        }
        child = find_state(child->parent_id);
    }
    
    return false;
}

bool StateMachine::handle_message_in_current_state(const MessageBase& msg) {
    StateId current = current_state_.load(std::memory_order_acquire);
    if (current == INVALID_STATE_ID) {
        return false;
    }
    
    // Try current state first
    const StateInfo* state = find_state(current);
    while (state) {
        if (state->message_handler && state->message_handler(msg)) {
            return true;  // Message handled
        }
        
        // Try parent state if current state didn't handle the message
        if (state->parent_id != INVALID_STATE_ID) {
            state = find_state(state->parent_id);
        } else {
            break;
        }
    }
    
    return false;  // Message not handled by any state
}

const char* StateMachine::current_state_name() const {
    StateId current = current_state_.load(std::memory_order_acquire);
    const StateInfo* state = find_state(current);
    return state ? state->name : "INVALID";
}

bool StateMachine::validate_state_machine() const {
    // Check for circular parent relationships
    for (const auto& state : states_) {
        if (state.id == INVALID_STATE_ID) continue;
        
        const StateInfo* current = &state;
        const StateInfo* slow = &state;
        const StateInfo* fast = &state;
        
        // Floyd's cycle detection algorithm
        do {
            if (fast->parent_id == INVALID_STATE_ID) break;
            fast = find_state(fast->parent_id);
            if (!fast || fast->parent_id == INVALID_STATE_ID) break;
            fast = find_state(fast->parent_id);
            
            if (slow->parent_id == INVALID_STATE_ID) break;
            slow = find_state(slow->parent_id);
            
            if (fast && slow && fast == slow) {
                return false;  // Circular parent relationship detected
            }
        } while (fast && slow);
    }
    
    return true;
}

StateInfo* StateMachine::find_state(StateId id) {
    for (auto& state : states_) {
        if (state.id == id) {
            return &state;
        }
    }
    return nullptr;
}

const StateInfo* StateMachine::find_state(StateId id) const {
    for (const auto& state : states_) {
        if (state.id == id) {
            return &state;
        }
    }
    return nullptr;
}

bool StateMachine::is_valid_state(StateId id) const {
    return find_state(id) != nullptr;
}

void StateMachine::execute_entry_chain(StateId target_state) {
    // Build entry chain from root to target
    std::array<StateId, MINI_SO_MAX_STATES> entry_chain;
    size_t chain_length = 0;
    
    StateId current = target_state;
    while (current != INVALID_STATE_ID && chain_length < MINI_SO_MAX_STATES) {
        entry_chain[chain_length++] = current;
        const StateInfo* state = find_state(current);
        current = state ? state->parent_id : INVALID_STATE_ID;
    }
    
    // Execute entry callbacks from root to target
    for (int i = static_cast<int>(chain_length) - 1; i >= 0; --i) {
        StateInfo* state = find_state(entry_chain[i]);
        if (state) {
            state->active = true;
            if (state->on_enter) {
                state->on_enter();
            }
        }
    }
}

void StateMachine::execute_exit_chain(StateId current_state, StateId target_state) {
    // Find common parent to determine exit chain
    StateId common_parent = find_common_parent(current_state, target_state);
    
    // Execute exit callbacks from current state up to (but not including) common parent
    StateId current = current_state;
    while (current != INVALID_STATE_ID && current != common_parent) {
        StateInfo* state = find_state(current);
        if (state) {
            state->active = false;
            if (state->on_exit) {
                state->on_exit();
            }
            current = state->parent_id;
        } else {
            break;
        }
    }
}

StateId StateMachine::find_common_parent(StateId state1, StateId state2) const {
    if (state1 == INVALID_STATE_ID || state2 == INVALID_STATE_ID) {
        return INVALID_STATE_ID;
    }
    
    // Get all ancestors of state1
    std::array<StateId, MINI_SO_MAX_STATES> ancestors1;
    size_t count1 = 0;
    
    StateId current = state1;
    while (current != INVALID_STATE_ID && count1 < MINI_SO_MAX_STATES) {
        ancestors1[count1++] = current;
        const StateInfo* state = find_state(current);
        current = state ? state->parent_id : INVALID_STATE_ID;
    }
    
    // Check ancestors of state2 against ancestors of state1
    current = state2;
    while (current != INVALID_STATE_ID) {
        for (size_t i = 0; i < count1; ++i) {
            if (ancestors1[i] == current) {
                return current;  // Found common ancestor
            }
        }
        const StateInfo* state = find_state(current);
        current = state ? state->parent_id : INVALID_STATE_ID;
    }
    
    return INVALID_STATE_ID;  // No common parent found
}

// ============================================================================
// Agent State Machine Integration Implementation
// ============================================================================

StateId Agent::define_state(const char* name, StateId parent) {
    // Auto-assign state ID
    StateId id = static_cast<StateId>(state_machine_.state_count());
    return state_machine_.define_state(id, name, parent);
}

StateId Agent::define_state(StateId id, const char* name, StateId parent) {
    return state_machine_.define_state(id, name, parent);
}

bool Agent::transition_to(StateId new_state) {
    return state_machine_.transition_to(new_state);
}

bool Agent::is_child_of(StateId child_state, StateId parent_state) const {
    return state_machine_.is_child_of(child_state, parent_state);
}

const char* Agent::current_state_name() const {
    return state_machine_.current_state_name();
}

void Agent::process_messages() {
    uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    MessageSize size;
    
    while (message_queue_.pop(buffer, size)) {
        if (size >= sizeof(MessageBase)) {
            const MessageBase* msg = reinterpret_cast<const MessageBase*>(buffer);
            
            // 메시지 지연시간 측정 (타임스탬프부터 현재까지)
            uint32_t current_time = now();
            uint32_t message_latency = current_time - msg->timestamp();
            MinimalPerformanceMonitor::record_message_latency(message_latency);
            
            SystemMetrics::increment_messages_processed();
            so_handle_message(*msg);
        }
    }
}

void Agent::process_messages_validated() {
    uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    MessageSize size;
    
    while (message_queue_.pop_with_validation(buffer, size)) {
        if (size >= sizeof(MessageBase)) {
            const MessageBase* msg = reinterpret_cast<const MessageBase*>(buffer);
            SystemMetrics::increment_messages_processed();
            so_handle_message(*msg);
        }
    }
}

// ============================================================================
// Environment Implementation
// ============================================================================

Environment::Environment() {
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        // Handle error - in embedded systems, usually halt
        taskDISABLE_INTERRUPTS();
        for(;;);
    }
    
    for (auto& agent : agents_) {
        agent = nullptr;
    }
    
    // Initialize message validation system
    MessageValidator::initialize();
    
    // Watchdog은 명시적으로 초기화하도록 변경 (정적 초기화 문제 회피)
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
    
    // Initialize the agent and set timer manager
    agent->initialize(id);
    agent->set_timer_manager(&timer_manager_);
    
    return id;
}

void Environment::unregister_agent(AgentId id) {
    if (id >= agent_count_) {
        return;
    }
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        agents_[id] = nullptr;
        xSemaphoreGive(mutex_);
    }
}

Agent* Environment::get_agent(AgentId id) {
    if (id >= agent_count_) {
        return nullptr;
    }
    
    Agent* agent = nullptr;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        agent = agents_[id];
        xSemaphoreGive(mutex_);
    }
    
    return agent;
}

bool Environment::process_one_message() {
    for (std::size_t i = 0; i < agent_count_; ++i) {
        if (agents_[i] && agents_[i]->has_messages()) {
            uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
            MessageSize size;
            
            if (agents_[i]->message_queue_.pop(buffer, size)) {
                if (size >= sizeof(MessageBase)) {
                    const MessageBase* msg = reinterpret_cast<const MessageBase*>(buffer);
                    agents_[i]->so_handle_message(*msg);
                    return true;
                }
            }
        }
    }
    return false;
}

void Environment::process_all_messages() {
    while (process_one_message()) {
        // Process all pending messages
    }
}

std::size_t Environment::total_pending_messages() const {
    std::size_t total = 0;
    
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i]) {
                total += agents_[i]->message_queue_.size();
            }
        }
        xSemaphoreGive(mutex_);
    }
    
    return total;
}

// ============================================================================
// Environment Timer Integration Implementation
// ============================================================================

void Environment::update_all_timers() {
    timer_manager_.update_timers();
}

void Environment::run_main_loop() {
    // 성능 측정 시작
    uint32_t loop_start_time = now();
    
    // Combined processing loop for messages, timers, and system safety
    process_all_messages();
    update_all_timers();
    kick_watchdog();         // 시스템 활성 신호
    check_system_recovery(); // 대기 중인 복구 실행
    
    // 성능 측정 종료
    uint32_t loop_end_time = now();
    uint32_t processing_time = loop_end_time - loop_start_time;
    
    MinimalPerformanceMonitor::record_processing_time(processing_time);
    MinimalPerformanceMonitor::increment_loop_count();
}

void Environment::kick_watchdog() {
    MinimalWatchdog::kick();
}

bool Environment::check_system_recovery() {
    return BasicRecovery::execute_pending_recovery();
}

std::size_t Environment::active_timer_count() const {
    return timer_manager_.active_timer_count();
}

bool Environment::initialize_system_services() {
    if (system_services_initialized_) {
        return true;
    }
    
    SystemServiceManager& manager = SystemServiceManager::instance();
    if (manager.initialize()) {
        system_services_initialized_ = true;
        return true;
    }
    
    return false;
}

void Environment::shutdown_system_services() {
    if (system_services_initialized_) {
        SystemServiceManager& manager = SystemServiceManager::instance();
        manager.shutdown();
        system_services_initialized_ = false;
    }
}

void Environment::cleanup_system() {
    // Shutdown system services first
    shutdown_system_services();
    
    // Cleanup all system resources
    if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
        // Clear all agents
        for (std::size_t i = 0; i < agent_count_; ++i) {
            if (agents_[i]) {
                agents_[i]->message_queue_.clear();
                agents_[i] = nullptr;
            }
        }
        agent_count_ = 0;
        xSemaphoreGive(mutex_);
    }
    
    // Cleanup timers
    timer_manager_.cleanup_all_timers();
}

} // namespace mini_so