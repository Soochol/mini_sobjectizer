// 기존 코드에서 안전한 코드로 마이그레이션 예시
#include "mini_sobjectizer/mini_sobjectizer.h"
#include "mini_sobjectizer/message_safety_patch.h"

using namespace mini_so;
using namespace mini_so::safety;

// 기존 Agent를 안전한 버전으로 수정
class SafeTestAgent : public Agent {
private:
    SafeMessageQueuePatch safe_queue_;  // 기존 message_queue_ 대체
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        // 타입별 안전한 처리
        if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::ErrorReport)) {
            // 타입 안전한 캐스팅
            const auto& typed_msg = static_cast<const Message<system_messages::ErrorReport>&>(msg);
            handle_error_report(typed_msg.data);
            return true;
        }
        return false;
    }
    
    // 기존 process_messages()를 안전한 버전으로 교체
    void process_messages_safe() noexcept {
        uint32_t messages_processed = 0;
        TimePoint start_time = now();
        
        // 타입별로 안전하게 처리
        {
            Message<system_messages::ErrorReport> error_msg({}, id());
            while (safe_queue_.pop(error_msg) && messages_processed < 8) {
                handle_error_report(error_msg.data);
                messages_processed++;
            }
        }
        
        {
            Message<system_messages::PerformanceMetric> perf_msg({}, id());
            while (safe_queue_.pop(perf_msg) && messages_processed < 8) {
                handle_performance_metric(perf_msg.data);
                messages_processed++;
            }
        }
        
        if (messages_processed > 0) {
            TimePoint end_time = now();
            uint32_t processing_time_us = (end_time - start_time) * 1000;
            report_performance(processing_time_us, messages_processed);
        }
    }
    
    // 안전한 메시지 전송
    template<typename T>
    void send_safe_message(AgentId target_id, const T& message) noexcept {
        // 컴파일 타임 안전성 검증
        static_assert(is_memcpy_safe_message<T>(), 
                     "Message type is not safe for transmission");
        
        Environment::instance().send_message(id_, target_id, message);
    }
    
private:
    void handle_error_report(const system_messages::ErrorReport& error) noexcept {
        printf("Safe handling of error: level=%d, code=%u\n", 
               static_cast<int>(error.level), error.error_code);
    }
    
    void handle_performance_metric(const system_messages::PerformanceMetric& metric) noexcept {
        printf("Safe handling of performance: agent=%u, time=%u us\n",
               metric.source_agent, metric.processing_time_us);
    }
};

// 마이그레이션 테스트
bool test_safety_migration() {
    printf("\n=== Safety Migration Test ===\n");
    
    Environment& env = Environment::instance();
    System& system = System::instance();
    
    if (!system.initialize()) {
        printf("System initialization failed\n");
        return false;
    }
    
    SafeTestAgent safe_agent;
    AgentId agent_id = env.register_agent(&safe_agent);
    
    if (agent_id == INVALID_AGENT_ID) {
        printf("Agent registration failed\n");
        return false;
    }
    
    // 안전한 메시지 전송 테스트
    system_messages::ErrorReport test_error;
    test_error.level = system_messages::ErrorReport::WARNING;
    test_error.error_code = 1001;
    test_error.source_agent = agent_id;
    
    try {
        safe_agent.send_safe_message(agent_id, test_error);
        safe_agent.process_messages_safe();
        
        printf("Safety migration test: SUCCESS\n");
        return true;
    } catch (...) {
        printf("Safety migration test: FAILED\n");
        return false;
    }
}

int main() {
    printf("🔒 Message Safety Migration Example 🔒\n");
    
    if (test_safety_migration()) {
        printf("✅ All safety tests passed!\n");
        return 0;
    } else {
        printf("❌ Safety tests failed!\n");
        return 1;
    }
}