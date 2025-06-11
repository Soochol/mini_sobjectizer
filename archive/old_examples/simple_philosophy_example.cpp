// simple_philosophy_example.cpp - SObjectizer 철학에 부합하는 예제
#include "../include/mini_sobjectizer/simple_sobjectizer.h"
#include <cstdio>

using namespace mini_so;

// Mock FreeRTOS functions
extern "C" {
    SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
    BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) { return pdTRUE; }
    BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) { return pdTRUE; }
    void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {}
    TickType_t xTaskGetTickCount(void) { 
        static uint32_t tick = 1000;
        return tick += 10;
    }
}

// ============================================================================
// Business Logic Messages - 도메인에 집중
// ============================================================================
struct SensorReading {
    float temperature;
    float humidity;
    uint16_t sensor_id;
};

struct MotorCommand {
    enum Action { START, STOP, SET_SPEED } action;
    uint16_t speed;
};

struct SystemStatus {
    bool motor_running;
    uint16_t current_speed;
    float last_temperature;
};

// ============================================================================
// Pure Business Logic Agents - 단순하고 명확
// ============================================================================

class SensorAgent : public SimpleAgent {
private:
    uint16_t sensor_id_;
    uint32_t reading_count_ = 0;
    
public:
    SensorAgent(uint16_t sensor_id) : sensor_id_(sensor_id) {}
    
    bool handle_message(const MessageBase& msg) override {
        // 센서는 외부 요청에만 응답 (단순한 구조)
        if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::StatusRequest)) {
            handle_status_request(static_cast<const Message<system_messages::StatusRequest>&>(msg));
            return true;
        }
        return false;
    }
    
    // 주기적으로 호출되는 센서 읽기
    void take_reading() {
        reading_count_++;
        
        SensorReading reading;
        reading.temperature = 20.0f + (reading_count_ % 20);
        reading.humidity = 50.0f + (reading_count_ % 30);
        reading.sensor_id = sensor_id_;
        
        // 모든 관심있는 Agent들에게 브로드캐스트
        broadcast_message(reading);
        
        // 성능 메트릭 보고 (메시지로!)
        system_messages::PerformanceMetric metric;
        metric.source_agent = id();
        metric.processing_time_ms = 1; // 센서 읽기는 빠름
        metric.message_count = 1;
        metric.timestamp = now();
        broadcast_message(metric);
        
        // Watchdog에 생존 신호 (메시지로!)
        system_messages::HeartbeatSignal heartbeat;
        heartbeat.source_agent = id();
        heartbeat.timestamp = now();
        broadcast_message(heartbeat);
    }
    
private:
    void handle_status_request(const Message<system_messages::StatusRequest>& msg) {
        SystemStatus status;
        status.motor_running = false; // 센서는 모터 상태 모름
        status.current_speed = 0;
        status.last_temperature = 20.0f + (reading_count_ % 20);
        
        // 상태를 메시지로 응답
        send_message(msg.sender_id(), status);
    }
};

class MotorAgent : public SimpleAgent {
private:
    bool running_ = false;
    uint16_t current_speed_ = 0;
    float last_temperature_ = 0.0f;
    
public:
    bool handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(MotorCommand)) {
            handle_motor_command(static_cast<const Message<MotorCommand>&>(msg));
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(SensorReading)) {
            handle_sensor_reading(static_cast<const Message<SensorReading>&>(msg));
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::StatusRequest)) {
            handle_status_request(static_cast<const Message<system_messages::StatusRequest>&>(msg));
            return true;
        }
        
        return false;
    }
    
private:
    void handle_motor_command(const Message<MotorCommand>& msg) {
        printf("Motor: Received command %d from Agent %u\n", 
               msg.data.action, msg.sender_id());
        
        switch (msg.data.action) {
            case MotorCommand::START:
                running_ = true;
                current_speed_ = msg.data.speed > 0 ? msg.data.speed : 500;
                printf("Motor: Started at speed %u\n", current_speed_);
                break;
                
            case MotorCommand::STOP:
                running_ = false;
                current_speed_ = 0;
                printf("Motor: Stopped\n");
                break;
                
            case MotorCommand::SET_SPEED:
                if (running_) {
                    current_speed_ = msg.data.speed;
                    printf("Motor: Speed set to %u\n", current_speed_);
                }
                break;
        }
        
        // 성능 메트릭 보고
        system_messages::PerformanceMetric metric;
        metric.source_agent = id();
        metric.processing_time_ms = 2; // 모터 제어 시간
        metric.message_count = 1;
        metric.timestamp = now();
        broadcast_message(metric);
        
        // Heartbeat 신호
        system_messages::HeartbeatSignal heartbeat;
        heartbeat.source_agent = id();
        heartbeat.timestamp = now();
        broadcast_message(heartbeat);
    }
    
    void handle_sensor_reading(const Message<SensorReading>& msg) {
        last_temperature_ = msg.data.temperature;
        
        // 과열 보호 (85도 이상)
        if (msg.data.temperature > 85.0f && running_) {
            printf("Motor: Overheating detected (%.1f°C), emergency stop!\n", 
                   msg.data.temperature);
            
            running_ = false;
            current_speed_ = 0;
            
            // 오류 보고 (메시지로!)
            system_messages::ErrorReport error;
            error.level = system_messages::ErrorReport::CRITICAL;
            error.source_agent = id();
            error.error_code = 1001; // OVERHEATING
            error.context = "Motor overheating";
            broadcast_message(error);
        }
    }
    
    void handle_status_request(const Message<system_messages::StatusRequest>& msg) {
        SystemStatus status;
        status.motor_running = running_;
        status.current_speed = current_speed_;
        status.last_temperature = last_temperature_;
        
        send_message(msg.sender_id(), status);
    }
};

class ControllerAgent : public SimpleAgent {
private:
    uint32_t cycle_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(SensorReading)) {
            handle_sensor_reading(static_cast<const Message<SensorReading>&>(msg));
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::WatchdogTimeout)) {
            handle_watchdog_timeout(static_cast<const Message<system_messages::WatchdogTimeout>&>(msg));
            return true;
        }
        
        return false;
    }
    
    // 주기적 제어 로직
    void run_control_cycle() {
        cycle_count_++;
        
        printf("Controller: Control cycle %u\n", cycle_count_);
        
        // 주기적 명령 전송
        if (cycle_count_ == 3) {
            MotorCommand start_cmd = {MotorCommand::START, 750};
            broadcast_message(start_cmd);
        }
        
        if (cycle_count_ == 8) {
            MotorCommand speed_cmd = {MotorCommand::SET_SPEED, 1000};
            broadcast_message(speed_cmd);
        }
        
        if (cycle_count_ == 12) {
            MotorCommand stop_cmd = {MotorCommand::STOP, 0};
            broadcast_message(stop_cmd);
        }
        
        // 상태 요청 (메시지로!)
        if (cycle_count_ % 5 == 0) {
            system_messages::StatusRequest status_req;
            status_req.requester = id();
            status_req.request_id = cycle_count_;
            broadcast_message(status_req);
        }
        
        // Heartbeat
        system_messages::HeartbeatSignal heartbeat;
        heartbeat.source_agent = id();
        heartbeat.timestamp = now();
        broadcast_message(heartbeat);
    }
    
private:
    void handle_sensor_reading(const Message<SensorReading>& msg) {
        printf("Controller: Received sensor data - Temp: %.1f°C, Humidity: %.1f%%\n",
               msg.data.temperature, msg.data.humidity);
    }
    
    void handle_watchdog_timeout(const Message<system_messages::WatchdogTimeout>& msg) {
        printf("Controller: ALERT - Agent %u has timed out!\n", msg.data.failed_agent);
        
        // 복구 조치 - 해당 Agent 재시작 요청 등
        system_messages::ErrorReport error;
        error.level = system_messages::ErrorReport::CRITICAL;
        error.source_agent = id();
        error.error_code = 2001; // AGENT_TIMEOUT
        error.context = "Agent watchdog timeout";
        broadcast_message(error);
    }
};

// ============================================================================
// Simple Main - 복잡한 초기화 없음
// ============================================================================

int main() {
    printf("=== Simple SObjectizer Philosophy Example ===\n\n");
    
    // 1. 단순한 Environment 초기화
    SimpleEnvironment& env = SimpleEnvironment::instance();
    
    // 2. 시스템 서비스 Agent들 (모든 것이 Agent!)
    ErrorHandlerAgent error_handler;
    PerformanceMonitorAgent performance_monitor;
    WatchdogAgent watchdog;
    
    // 3. 비즈니스 로직 Agent들
    SensorAgent sensor(1);
    MotorAgent motor;
    ControllerAgent controller;
    
    // 4. Agent 등록 - 순서 중요하지 않음
    AgentId error_handler_id = env.register_agent(&error_handler);
    AgentId performance_id = env.register_agent(&performance_monitor);
    AgentId watchdog_id = env.register_agent(&watchdog);
    AgentId sensor_id = env.register_agent(&sensor);
    AgentId motor_id = env.register_agent(&motor);
    AgentId controller_id = env.register_agent(&controller);
    
    printf("Agents registered: Error=%u, Perf=%u, Watch=%u, Sensor=%u, Motor=%u, Controller=%u\n\n",
           error_handler_id, performance_id, watchdog_id, sensor_id, motor_id, controller_id);
    
    // 5. 단순한 시뮬레이션 루프
    printf("Starting pure message-based simulation...\n\n");
    
    for (int cycle = 0; cycle < 15; ++cycle) {
        printf("=== Cycle %d ===\n", cycle + 1);
        
        // 센서 읽기 (주기적)
        sensor.take_reading();
        
        // 제어 로직 (주기적)
        controller.run_control_cycle();
        
        // Watchdog 체크 (주기적)
        watchdog.check_timeouts();
        
        // 모든 메시지 처리 - 이것이 전부!
        env.run_main_loop();
        
        printf("\n");
    }
    
    printf("=== Final Status ===\n");
    printf("Total pending messages: %zu\n", env.total_pending_messages());
    printf("All communication was message-based!\n");
    printf("No global state accessed directly!\n");
    printf("Pure Actor Model implementation!\n");
    
    return 0;
}