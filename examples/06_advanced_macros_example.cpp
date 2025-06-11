// 06_advanced_macros_example.cpp - 새로운 고급 매크로 사용법 데모
// 이 예제는 Mini SObjectizer의 새로운 고급 매크로들을 보여줍니다

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>

using namespace mini_so;

// ============================================================================
// 메시지 정의
// ============================================================================
struct TaskMessage {
    const char* task_name;
    uint32_t priority;
    float processing_time;
};

struct DelayedMessage {
    const char* message;
    uint32_t delay_cycles;
};

struct StatusRequest {
    uint32_t request_id;
    AgentId requester;
};

struct StatusResponse {
    uint32_t request_id;
    const char* status;
    uint32_t processed_tasks;
    bool busy;
};

struct ConditionalMessage {
    uint32_t value;
    uint32_t threshold;
    const char* action;
};

// ============================================================================
// 새 매크로를 사용한 Worker Agent
// ============================================================================
class WorkerAgent : public Agent {
private:
    uint32_t processed_tasks_ = 0;
    bool busy_ = false;
    uint32_t cycle_count_ = 0;
    
public:
    // 🚀 새 매크로: 메시지 핸들러 선언 간소화
    BEGIN_MESSAGE_HANDLER()
        // 🚀 새 매크로: 자동 반환 타입 감지
        HANDLE_MESSAGE_AUTO(TaskMessage, process_task);
        HANDLE_MESSAGE_AUTO(DelayedMessage, handle_delayed_message);
        HANDLE_MESSAGE_AUTO(StatusRequest, handle_status_request);
        
        // 🚀 새 매크로: 조건부 메시지 처리
        HANDLE_MESSAGE_IF(ConditionalMessage, handle_conditional, 
                         typed_msg.data.value > typed_msg.data.threshold);
    END_MESSAGE_HANDLER()
    
    void run_cycle() {
        cycle_count_++;
        
        // 주기적으로 자신에게 작업 전송 (자체 스케줄링)
        if (cycle_count_ % 10 == 0) {
            TaskMessage self_task{
                "Self-scheduled task",
                1,
                2.5f
            };
            
            // 🚀 새 매크로: 자신에게 메시지 전송
            SEND_TO_SELF(self_task);
            printf("📤 WorkerAgent: 자신에게 작업 전송 (사이클 %u)\n", cycle_count_);
        }
        
        // 지연 메시지 테스트
        if (cycle_count_ == 5) {
            DelayedMessage delayed{
                "이 메시지는 5 사이클 후에 처리됩니다",
                5
            };
            SEND_TO_SELF(delayed);
        }
        
        MINI_SO_HEARTBEAT();
    }
    
private:
    // void 반환: HANDLE_MESSAGE_AUTO가 자동으로 true 반환
    void process_task(const TaskMessage& task) {
        processed_tasks_++;
        busy_ = true;
        
        printf("⚙️ WorkerAgent: 작업 처리 중 '%s' (우선순위: %u, 처리시간: %.1fs)\n",
               task.task_name, task.priority, task.processing_time);
        
        // 작업 완료 후 상태 변경
        busy_ = false;
        
        MINI_SO_RECORD_PERFORMANCE(static_cast<uint32_t>(task.processing_time * 1000), 1);
    }
    
    // void 반환: HANDLE_MESSAGE_AUTO가 자동으로 true 반환
    void handle_delayed_message(const DelayedMessage& delayed) {
        if (delayed.delay_cycles > 0) {
            // 아직 지연 시간이 남음 - 다시 자신에게 전송
            DelayedMessage next_delayed{
                delayed.message,
                delayed.delay_cycles - 1
            };
            SEND_TO_SELF(next_delayed);
            printf("⏱️ WorkerAgent: 지연 메시지 대기 중... (남은 사이클: %u)\n", 
                   next_delayed.delay_cycles);
        } else {
            // 지연 완료 - 실제 처리
            printf("✅ WorkerAgent: 지연 메시지 처리 완료: '%s'\n", delayed.message);
        }
    }
    
    // void 반환: HANDLE_MESSAGE_AUTO가 자동으로 true 반환
    void handle_status_request(const StatusRequest& request) {
        printf("📊 WorkerAgent: 상태 요청 받음 (ID: %u)\n", request.request_id);
        
        StatusResponse response{
            request.request_id,
            busy_ ? "Working" : "Idle",
            processed_tasks_,
            busy_
        };
        
        // 요청자에게 응답 전송
        Environment& env = Environment::instance();
        env.send_message(id(), request.requester, response);
    }
    
    // void 반환: HANDLE_MESSAGE_AUTO가 자동으로 true 반환
    void handle_conditional(const ConditionalMessage& msg) {
        printf("🎯 WorkerAgent: 조건부 메시지 처리 (값: %u > 임계값: %u) - 액션: %s\n",
               msg.value, msg.threshold, msg.action);
    }
};

// ============================================================================
// Manager Agent (기존 스타일과 비교용)
// ============================================================================
class ManagerAgent : public Agent {
private:
    uint32_t request_counter_ = 0;
    uint32_t cycle_count_ = 0;
    
public:
    // 기존 스타일 유지 (비교용)
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(StatusResponse, handle_status_response);
        return false;
    }
    
    void run_management_cycle() {
        cycle_count_++;
        
        // 주기적으로 WorkerAgent에게 상태 요청
        if (cycle_count_ % 15 == 0) {
            request_counter_++;
            StatusRequest status_req{
                request_counter_,
                id()  // 내 ID를 요청자로 설정
            };
            
            printf("📋 ManagerAgent: WorkerAgent에게 상태 요청 전송 (ID: %u)\n", 
                   request_counter_);
            MINI_SO_BROADCAST(status_req);
        }
        
        // 조건부 메시지 테스트
        if (cycle_count_ == 8) {
            ConditionalMessage cond_msg{150, 100, "High value processing"};
            MINI_SO_BROADCAST(cond_msg);
            
            ConditionalMessage cond_msg2{50, 100, "Low value ignored"};
            MINI_SO_BROADCAST(cond_msg2);  // 이건 처리되지 않음 (50 <= 100)
        }
        
        MINI_SO_HEARTBEAT();
    }
    
private:
    void handle_status_response(const StatusResponse& response) {
        printf("📈 ManagerAgent: 상태 응답 받음 (ID: %u) - 상태: %s, 처리된 작업: %u, 바쁨: %s\n",
               response.request_id, response.status, response.processed_tasks,
               response.busy ? "예" : "아니오");
    }
};

// ============================================================================
// 새 매크로로 정의한 Simple Agent (극도로 간소화)
// ============================================================================
class SimpleReporter : public Agent {
public:
    BEGIN_MESSAGE_HANDLER()
        HANDLE_MESSAGE_AUTO(TaskMessage, report_task);
    END_MESSAGE_HANDLER()
    
private:
    void report_task(const TaskMessage& task) {
        printf("📄 SimpleReporter: 작업 보고 - '%s'\n", task.task_name);
    }
};

// ============================================================================
// 메인 함수: 고급 매크로 데모
// ============================================================================
int main() {
    printf("🚀 Mini SObjectizer 고급 매크로 데모 🚀\n");
    printf("=====================================\n\n");
    
    // 시스템 초기화
    MINI_SO_INIT();
    if (!sys.initialize()) {
        printf("❌ 시스템 초기화 실패!\n");
        return 1;
    }
    printf("✅ 시스템 초기화 완료\n\n");
    
    // Agent 생성 및 등록
    WorkerAgent worker;
    ManagerAgent manager;
    SimpleReporter reporter;
    
    AgentId worker_id = MINI_SO_REGISTER(worker);
    AgentId manager_id = MINI_SO_REGISTER(manager);
    AgentId reporter_id = MINI_SO_REGISTER(reporter);
    
    printf("🔧 Agents 등록 완료:\n");
    printf("   - WorkerAgent: ID %u (새 매크로 사용)\n", worker_id);
    printf("   - ManagerAgent: ID %u (기존 스타일)\n", manager_id);
    printf("   - SimpleReporter: ID %u (DEFINE_AGENT 매크로)\n\n", reporter_id);
    
    // 초기 작업 전송
    TaskMessage initial_task{"외부에서 요청된 작업", 3, 1.5f};
    env.send_message(0, worker_id, initial_task);
    
    printf("🚀 고급 매크로 데모 시작!\n");
    printf("==========================================\n");
    
    // 메인 실행 루프
    for (int cycle = 0; cycle < 25; ++cycle) {
        printf("\n--- 사이클 %d ---\n", cycle + 1);
        
        // Agent 실행
        worker.run_cycle();
        manager.run_management_cycle();
        
        // 메시지 처리
        MINI_SO_RUN();
        
        // 중간 상태 확인
        if (cycle == 12) {
            printf("\n📊 중간 상태 점검:\n");
            printf("   - 시스템 상태: %s\n", sys.is_healthy() ? "정상" : "점검 필요");
            printf("   - 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
        }
    }
    
    printf("\n==========================================\n");
    printf("📊 고급 매크로 데모 완료\n");
    printf("==========================================\n");
    
    printf("\n📈 시스템 성능:\n");
    printf("   - 총 처리된 메시지: %lu\n", (unsigned long)sys.performance().total_messages());
    printf("   - 최대 처리 시간: %u μs\n", sys.performance().max_processing_time_us());
    printf("   - 시스템 에러: %zu\n", sys.error().error_count());
    printf("   - 시스템 상태: %s\n", sys.is_healthy() ? "우수" : "점검 필요");
    
    printf("\n🎯 새로운 매크로 데모 완료!\n");
    printf("💡 사용된 새 매크로들:\n");
    printf("   ✅ BEGIN_MESSAGE_HANDLER() / END_MESSAGE_HANDLER()\n");
    printf("   ✅ HANDLE_MESSAGE_AUTO() - 자동 반환 타입 감지\n");
    printf("   ✅ HANDLE_MESSAGE_IF() - 조건부 메시지 처리\n");
    printf("   ✅ SEND_TO_SELF() - 자신에게 메시지 전송\n");
    printf("   ✅ DEFINE_AGENT() / END_AGENT() - Agent 클래스 정의\n");
    
    printf("\n💫 코드가 더 깔끔하고 직관적이 되었습니다!\n");
    
    return 0;
}