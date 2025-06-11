// phase2_simple_example.cpp - Phase 2 단순화된 API 사용 예제
#include "mini_sobjectizer/mini_sobjectizer_v3.h"
#include <cstdio>

// Mock FreeRTOS functions for testing
extern "C" {
    SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
    BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) { return pdTRUE; }
    BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) { return pdTRUE; }
    void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {}
    TickType_t xTaskGetTickCount(void) { 
        static uint32_t tick = 1000;
        return tick += 10;
    }
    void taskDISABLE_INTERRUPTS(void) {}
}

using namespace mini_so;

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

struct SystemAlert {
    enum Level { INFO, WARNING, CRITICAL } level;
    const char* message;
};

// ============================================================================
// Simple Business Agents - Phase 2의 깔끔한 구조
// ============================================================================

class SensorAgent : public Agent {
private:
    uint16_t sensor_id_;
    uint32_t reading_count_ = 0;
    
public:
    SensorAgent(uint16_t sensor_id) : sensor_id_(sensor_id) {}
    
    bool handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::StatusRequest)) {
            // 상태 요청 처리
            printf("Sensor %u: Status requested by Agent %u\n", sensor_id_, msg.sender_id());
            
            system_messages::SystemStatus status;
            status.request_id = 0; // 간소화
            status.total_agents = 1;
            status.total_messages = reading_count_;
            status.uptime_ms = now();
            
            send_message(msg.sender_id(), status);
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
        
        // 간단한 브로드캐스트
        broadcast_message(reading);
        
        // Phase 2: 직접적인 시스템 서비스 접근
        SystemManager::instance().heartbeat(id());
        
        printf("Sensor %u: Reading #%u - Temp: %.1f°C, Humidity: %.1f%%\n", 
               sensor_id_, reading_count_, reading.temperature, reading.humidity);
    }
};

class MotorAgent : public Agent {
private:
    bool running_ = false;
    uint16_t current_speed_ = 0;
    float last_temperature_ = 0.0f;
    
public:
    bool handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(MotorCommand)) {
            const auto& cmd_msg = static_cast<const Message<MotorCommand>&>(msg);
            handle_motor_command(cmd_msg.data);
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(SensorReading)) {
            const auto& sensor_msg = static_cast<const Message<SensorReading>&>(msg);
            handle_sensor_reading(sensor_msg.data);
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::StatusRequest)) {
            printf("Motor: Status requested - Running: %s, Speed: %u\n", 
                   running_ ? "YES" : "NO", current_speed_);
            return true;
        }
        
        return false;
    }
    
private:
    void handle_motor_command(const MotorCommand& cmd) {
        printf("Motor: Received command %d\n", cmd.action);
        
        switch (cmd.action) {
            case MotorCommand::START:
                running_ = true;
                current_speed_ = cmd.speed > 0 ? cmd.speed : 500;
                printf("Motor: Started at speed %u\n", current_speed_);
                break;
                
            case MotorCommand::STOP:
                running_ = false;
                current_speed_ = 0;
                printf("Motor: Stopped\n");
                break;
                
            case MotorCommand::SET_SPEED:
                if (running_) {
                    current_speed_ = cmd.speed;
                    printf("Motor: Speed set to %u\n", current_speed_);
                }
                break;
        }
        
        // Phase 2: 간단한 시스템 서비스 호출
        SystemManager::instance().heartbeat(id());
    }
    
    void handle_sensor_reading(const SensorReading& reading) {
        last_temperature_ = reading.temperature;
        
        // 과열 보호 (85도 이상)
        if (reading.temperature > 85.0f && running_) {
            printf("Motor: Overheating detected (%.1f°C), emergency stop!\n", reading.temperature);
            
            running_ = false;
            current_speed_ = 0;
            
            // Phase 2: 직접적인 오류 보고
            SystemManager::instance().report_error(
                system_messages::ErrorReport::CRITICAL, 
                1001, // OVERHEATING
                "Motor overheating detected"
            );
            
            // 알람 메시지 브로드캐스트
            SystemAlert alert = {SystemAlert::CRITICAL, "Motor emergency stop due to overheating"};
            broadcast_message(alert);
        }
    }
};

class ControllerAgent : public Agent {
private:
    uint32_t cycle_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) override {
        if (msg.type_id() == MESSAGE_TYPE_ID(SensorReading)) {
            const auto& sensor_msg = static_cast<const Message<SensorReading>&>(msg);
            handle_sensor_data(sensor_msg.data);
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(SystemAlert)) {
            const auto& alert_msg = static_cast<const Message<SystemAlert>&>(msg);
            handle_system_alert(alert_msg.data);
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::SystemStatus)) {
            const auto& status_msg = static_cast<const Message<system_messages::SystemStatus>&>(msg);
            printf("Controller: Received system status - Agents: %u, Messages: %u, Uptime: %u ms\n",
                   status_msg.data.total_agents, status_msg.data.total_messages, status_msg.data.uptime_ms);
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
        
        // 주기적 상태 요청
        if (cycle_count_ % 5 == 0) {
            system_messages::StatusRequest status_req;
            status_req.requester = id();
            status_req.request_id = cycle_count_;
            broadcast_message(status_req);
        }
        
        // Phase 2: 간단한 heartbeat
        SystemManager::instance().heartbeat(id());
    }
    
private:
    void handle_sensor_data(const SensorReading& reading) {
        printf("Controller: Sensor data - Temp: %.1f°C, Humidity: %.1f%%\n",
               reading.temperature, reading.humidity);
    }
    
    void handle_system_alert(const SystemAlert& alert) {
        const char* level_str = alert.level == SystemAlert::INFO ? "INFO" :
                               alert.level == SystemAlert::WARNING ? "WARNING" : "CRITICAL";
        printf("Controller: ALERT [%s] - %s\n", level_str, alert.message);
        
        if (alert.level == SystemAlert::CRITICAL) {
            // Phase 2: 간단한 오류 보고
            SystemManager::instance().report_error(
                system_messages::ErrorReport::CRITICAL, 
                2001, // CRITICAL_ALERT_RECEIVED
                alert.message
            );
        }
    }
};

// ============================================================================
// Main Application - Phase 2의 단순함
// ============================================================================

int main() {
    printf("=== Phase 2 Simple API Example ===\n\n");
    
    // 1. 단순한 Environment 초기화
    Environment& env = Environment::instance();
    
    // 2. 간단한 시스템 서비스 초기화
    printf("Initializing system services...\n");
    SystemManager& system = SystemManager::instance();
    if (!system.initialize()) {
        printf("ERROR: Failed to initialize system services!\n");
        return 1;
    }
    printf("System services initialized.\n\n");
    
    // 3. 비즈니스 Agent들 생성 및 등록
    SensorAgent sensor(1);
    MotorAgent motor;
    ControllerAgent controller;
    
    AgentId sensor_id = env.register_agent(&sensor);
    AgentId motor_id = env.register_agent(&motor);
    AgentId controller_id = env.register_agent(&controller);
    
    printf("Agents registered: Sensor=%u, Motor=%u, Controller=%u\n\n", 
           sensor_id, motor_id, controller_id);
    
    // 4. Watchdog 등록 (Phase 2의 간단함)
    system.watchdog().register_for_monitoring(sensor_id, 3000);     // 3초 타임아웃
    system.watchdog().register_for_monitoring(motor_id, 3000);      // 3초 타임아웃
    system.watchdog().register_for_monitoring(controller_id, 3000); // 3초 타임아웃
    
    // 5. 간단한 시뮬레이션 루프
    printf("Starting Phase 2 simulation...\n\n");
    
    for (int cycle = 0; cycle < 15; ++cycle) {
        printf("=== Cycle %d ===\n", cycle + 1);
        
        // 센서 읽기
        sensor.take_reading();
        
        // 제어 로직
        controller.run_control_cycle();
        
        // Phase 2: 단순한 시스템 실행
        env.run();
        
        // 시스템 상태 출력 (주기적)
        if (cycle % 5 == 4) {
            printf("System Status:\n");
            printf("  - Total pending messages: %zu\n", env.total_pending_messages());
            printf("  - Error count: %zu\n", system.error().get_error_count());
            printf("  - Max processing time: %u ms\n", system.performance().get_max_processing_time());
            printf("  - Total messages: %u\n", system.performance().get_total_messages());
            printf("  - System healthy: %s\n", system.watchdog().is_healthy() ? "YES" : "NO");
        }
        
        printf("\n");
    }
    
    // 6. 최종 시스템 상태
    printf("=== Final System Status ===\n");
    printf("Error count: %zu\n", system.error().get_error_count());
    printf("Total messages processed: %u\n", system.performance().get_total_messages());
    printf("Loop count: %u\n", system.performance().get_loop_count());
    printf("System healthy: %s\n", system.watchdog().is_healthy() ? "YES" : "NO");
    
    printf("\n=== Phase 2 Advantages Demonstrated ===\n");
    printf("✓ Simplified API - Direct access to system services\n");
    printf("✓ Reduced complexity - No static wrapper classes\n");
    printf("✓ Message-based - Pure actor model implementation\n");
    printf("✓ Intuitive interface - SystemManager.error(), .performance(), .watchdog()\n");
    printf("✓ Cleaner code - Less boilerplate, more focus on business logic\n");
    
    // 시스템 종료
    system.shutdown();
    
    printf("\nPhase 2 simulation completed successfully!\n");
    return 0;
}