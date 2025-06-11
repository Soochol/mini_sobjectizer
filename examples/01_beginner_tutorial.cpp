// 01_beginner_tutorial.cpp - Mini SObjectizer 초보자 튜토리얼
// 이 예제는 Mini SObjectizer의 기본 개념을 단계별로 설명합니다

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>

using namespace mini_so;

// ============================================================================
// 1단계: 간단한 메시지 정의
// ============================================================================
struct HelloMessage {
    const char* text;
    uint32_t number;
};

struct CounterMessage {
    uint32_t count;
};

// ============================================================================
// 2단계: 첫 번째 Agent 만들기 (새 매크로 사용!)
// ============================================================================
class HelloAgent : public Agent {
private:
    uint32_t message_count_ = 0;
    
public:
    // 🚀 새 매크로: 메시지 핸들러 선언 간소화!
    BEGIN_MESSAGE_HANDLER()
        // 기본 매크로 사용 (안정성을 위해)
        HANDLE_MESSAGE_VOID(HelloMessage, process_hello);
        HANDLE_MESSAGE_VOID(CounterMessage, process_counter);
    END_MESSAGE_HANDLER()
    
private:
    void process_hello(const HelloMessage& hello) {
        message_count_++;
        printf("👋 HelloAgent received: '%s' with number %u (총 %u번째 메시지)\n", 
               hello.text, hello.number, message_count_);
        
        // 🚀 간편한 heartbeat
        MINI_SO_HEARTBEAT();
    }
    
    void process_counter(const CounterMessage& counter) {
        printf("🔢 HelloAgent received counter: %u\n", counter.count);
        
        // 조건부로 새 메시지 전송
        if (counter.count < 3) {
            HelloMessage reply{"Reply from HelloAgent", counter.count + 1};
            MINI_SO_BROADCAST(reply);  // 🚀 간편한 브로드캐스트
        }
        
        // 🚀 새 매크로: 자신에게 메시지 전송 데모
        if (counter.count == 2) {
            CounterMessage self_msg{999};
            SEND_TO_SELF(self_msg);  // 나 자신에게 특별한 메시지 전송
            printf("📮 HelloAgent: 자신에게 특별한 메시지 전송!\n");
        }
    }
};

// ============================================================================
// 3단계: 응답하는 Agent 만들기 (더욱 간소화!)
// ============================================================================
class EchoAgent : public Agent {
private:
    uint32_t echo_count_ = 0;
    
public:
    // 🚀 새 매크로로 더욱 간소화!
    BEGIN_MESSAGE_HANDLER()
        HANDLE_MESSAGE_VOID(HelloMessage, echo_hello);
    END_MESSAGE_HANDLER()
    
private:
    void echo_hello(const HelloMessage& hello) {
        echo_count_++;
        printf("🔄 EchoAgent echoing: '%s' (echo #%u)\n", hello.text, echo_count_);
        
        // 카운터 메시지로 응답
        CounterMessage counter{echo_count_};
        MINI_SO_BROADCAST(counter);
        
        MINI_SO_HEARTBEAT();
    }
};

// ============================================================================
// 4단계: 모니터링 Agent 만들기 (일반 클래스로 변경)
// ============================================================================
class MonitorAgent : public Agent {
private:
    uint32_t hello_messages_ = 0;
    uint32_t counter_messages_ = 0;
    
public:
    // 🚀 새 매크로: 메시지 핸들러 선언 간소화!
    BEGIN_MESSAGE_HANDLER()
        HANDLE_MESSAGE_VOID(HelloMessage, monitor_hello);
        HANDLE_MESSAGE_VOID(CounterMessage, monitor_counter);
    END_MESSAGE_HANDLER()
    
    void print_statistics() {
        printf("📊 MonitorAgent Statistics:\n");
        printf("   - Hello messages: %u\n", hello_messages_);
        printf("   - Counter messages: %u\n", counter_messages_);
        printf("   - Total messages: %u\n", hello_messages_ + counter_messages_);
    }
    
private:
    void monitor_hello(const HelloMessage& hello) {
        (void)hello;  // 경고 방지
        hello_messages_++;
        printf("📈 Monitor: Hello message #%u detected\n", hello_messages_);
    }
    
    void monitor_counter(const CounterMessage& counter) {
        counter_messages_++;
        printf("📈 Monitor: Counter message #%u (value: %u)\n", 
               counter_messages_, counter.count);
    }
};

// ============================================================================
// 메인 함수: 시스템 실행
// ============================================================================
int main() {
    printf("🎓 Mini SObjectizer 초보자 튜토리얼 🎓\n");
    printf("=====================================\n\n");
    
    // 🚀 1단계: 시스템 초기화 (한 줄로!)
    printf("1️⃣ 시스템 초기화...\n");
    MINI_SO_INIT();
    
    if (!sys.initialize()) {
        printf("❌ 시스템 초기화 실패!\n");
        return 1;
    }
    printf("✅ 시스템 초기화 완료\n\n");
    
    // 🚀 2단계: Agent 생성 및 등록
    printf("2️⃣ Agent 생성 및 등록...\n");
    HelloAgent hello_agent;
    EchoAgent echo_agent;
    MonitorAgent monitor_agent;
    
    // 🚀 간편한 등록
    AgentId hello_id = MINI_SO_REGISTER(hello_agent);
    AgentId echo_id = MINI_SO_REGISTER(echo_agent);
    AgentId monitor_id = MINI_SO_REGISTER(monitor_agent);
    
    printf("✅ Agents 등록 완료:\n");
    printf("   - HelloAgent: ID %u\n", hello_id);
    printf("   - EchoAgent: ID %u\n", echo_id);
    printf("   - MonitorAgent: ID %u\n\n", monitor_id);
    
    // 🚀 3단계: 첫 번째 메시지 전송
    printf("3️⃣ 첫 번째 메시지 전송...\n");
    HelloMessage initial_msg{"Hello, Mini SObjectizer!", 1};
    env.send_message(hello_id, hello_id, initial_msg);
    printf("📤 초기 메시지 전송 완료\n\n");
    
    // 🚀 4단계: 메시지 처리 실행
    printf("4️⃣ 메시지 처리 시작...\n");
    printf("==========================================\n");
    
    for (int cycle = 0; cycle < 5; ++cycle) {
        printf("\n--- 처리 사이클 %d ---\n", cycle + 1);
        
        // 🚀 모든 메시지 처리
        MINI_SO_RUN();
        
        // 시스템 상태 확인
        if (cycle == 2) {
            printf("\n📊 중간 상태 확인:\n");
            monitor_agent.print_statistics();
        }
    }
    
    printf("\n==========================================\n");
    printf("5️⃣ 최종 결과\n");
    
    // 🚀 5단계: 최종 통계
    monitor_agent.print_statistics();
    
    printf("\n📊 시스템 통계:\n");
    printf("   - 시스템 상태: %s\n", sys.is_healthy() ? "정상" : "비정상");
    printf("   - 총 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
    printf("   - 최대 처리 시간: %u μs\n", sys.performance().max_processing_time_us());
    printf("   - 에러 개수: %zu\n", sys.error().error_count());
    
    printf("\n🎉 튜토리얼 완료!\n");
    printf("\n💡 학습 포인트 (업데이트!):\n");
    printf("   ✅ MINI_SO_INIT() - 간편한 시스템 초기화\n");
    printf("   ✅ MINI_SO_REGISTER() - 간편한 Agent 등록\n");
    printf("   🆕 BEGIN_MESSAGE_HANDLER() / END_MESSAGE_HANDLER() - 핸들러 선언 간소화\n");
    printf("   ✅ HANDLE_MESSAGE_VOID() - 간편한 메시지 처리\n");
    printf("   🆕 SEND_TO_SELF() - 자신에게 메시지 전송\n");
    printf("   ✅ MINI_SO_BROADCAST() - 간편한 메시지 전송\n");
    printf("   ✅ MINI_SO_HEARTBEAT() - 간편한 상태 보고\n");
    printf("   ✅ MINI_SO_RUN() - 간편한 메시지 처리 실행\n");
    
    printf("\n다음 단계: 다른 예제들을 확인해보세요!\n");
    printf("   - 02_smart_factory_example.cpp\n");
    printf("   - 03_autonomous_vehicle_example.cpp\n");
    printf("   - 04_medical_device_example.cpp\n");
    printf("   - 06_advanced_macros_example.cpp (새 매크로 심화 학습!)\n");
    
    return 0;
}