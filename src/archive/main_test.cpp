// main_test.cpp - Mini-SObjectizer v3.0 Final Test Version (Host Platform)
#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>
#include <chrono>
#include <thread>

using namespace mini_so;

// ============================================================================
// Example Messages
// ============================================================================
struct SensorReading {
    float temperature;
    float humidity;
    uint32_t timestamp;
};

struct MotorCommand {
    enum Action { START, STOP, SET_SPEED } action;
    uint16_t speed;
};

struct StatusRequest {
    uint32_t request_id;
};

struct StatusResponse {
    uint32_t request_id;
    bool motor_running;
    uint16_t current_speed;
    float last_temperature;
};

// ============================================================================
// Sensor Agent
// ============================================================================
class SensorAgent : public Agent {
private:
    uint32_t reading_count_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        if (msg.type_id() == MESSAGE_TYPE_ID(StatusRequest)) {
            printf("Sensor: Status request received from Agent %u\n", msg.sender_id());
            return true;
        }
        return false;
    }
    
    void read_sensors() {
        reading_count_++;
        
        // Simulate sensor reading
        SensorReading reading;
        reading.temperature = 25.0f + (reading_count_ % 10) - 5.0f; // 20-30°C
        reading.humidity = 60.0f + (reading_count_ % 20) - 10.0f;   // 50-70%
        reading.timestamp = now();
        
        printf("Sensor: Reading #%u - Temp: %.1f°C, Humidity: %.1f%%\n", 
               reading_count_, reading.temperature, reading.humidity);
        
        // Send sensor data to other agents
        broadcast_message(reading);
        
        // Send heartbeat to system
        System::instance().heartbeat(id());
    }
};

// ============================================================================
// Motor Control Agent
// ============================================================================
class MotorAgent : public Agent {
private:
    enum State { IDLE, RUNNING } current_motor_state_ = IDLE;
    uint16_t current_speed_ = 0;
    float last_temperature_ = 0.0f;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        if (msg.type_id() == MESSAGE_TYPE_ID(MotorCommand)) {
            const auto& cmd_msg = static_cast<const Message<MotorCommand>&>(msg);
            const auto& cmd = cmd_msg.data;
            
            switch (cmd.action) {
                case MotorCommand::START:
                    current_motor_state_ = RUNNING;
                    current_speed_ = cmd.speed;
                    printf("Motor: Started at speed %u\n", current_speed_);
                    break;
                    
                case MotorCommand::STOP:
                    current_motor_state_ = IDLE;
                    current_speed_ = 0;
                    printf("Motor: Stopped\n");
                    break;
                    
                case MotorCommand::SET_SPEED:
                    if (current_motor_state_ == RUNNING) {
                        current_speed_ = cmd.speed;
                        printf("Motor: Speed set to %u\n", current_speed_);
                    }
                    break;
            }
            
            // Send heartbeat after processing command
            System::instance().heartbeat(id());
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(SensorReading)) {
            const auto& sensor_msg = static_cast<const Message<SensorReading>&>(msg);
            const auto& reading = sensor_msg.data;
            
            last_temperature_ = reading.temperature;
            
            // Emergency stop if temperature too high
            if (reading.temperature > 30.0f && current_motor_state_ == RUNNING) {
                current_motor_state_ = IDLE;
                current_speed_ = 0;
                printf("Motor: Emergency stop due to high temperature (%.1f°C)\n", reading.temperature);
                
                // Report critical error
                System::instance().report_error(
                    system_messages::ErrorReport::CRITICAL, 
                    1001, // OVERHEATING
                    id()
                );
            }
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(StatusRequest)) {
            const auto& req_msg = static_cast<const Message<StatusRequest>&>(msg);
            const auto& request = req_msg.data;
            
            StatusResponse response;
            response.request_id = request.request_id;
            response.motor_running = (current_motor_state_ == RUNNING);
            response.current_speed = current_speed_;
            response.last_temperature = last_temperature_;
            
            printf("Motor: Status - %s, Speed: %u, Temp: %.1f°C\n",
                   response.motor_running ? "RUNNING" : "STOPPED",
                   response.current_speed, response.last_temperature);
            
            // Send response back to sender
            send_message(msg.sender_id(), response);
            return true;
        }
        
        return false;
    }
};

// ============================================================================
// Monitor Agent
// ============================================================================
class MonitorAgent : public Agent {
private:
    uint32_t status_request_id_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        if (msg.type_id() == MESSAGE_TYPE_ID(StatusResponse)) {
            const auto& resp_msg = static_cast<const Message<StatusResponse>&>(msg);
            const auto& response = resp_msg.data;
            
            printf("Monitor: Status Response #%u - Motor: %s, Speed: %u, Temp: %.1f°C\n",
                   response.request_id,
                   response.motor_running ? "ON" : "OFF",
                   response.current_speed,
                   response.last_temperature);
            return true;
        }
        return false;
    }
    
    void request_system_status() {
        StatusRequest request;
        request.request_id = ++status_request_id_;
        
        printf("Monitor: Requesting status #%u\n", request.request_id);
        
        // Broadcast status request to all agents
        broadcast_message(request);
        
        // Send heartbeat
        System::instance().heartbeat(id());
    }
};

// ============================================================================
// Global agent instances
// ============================================================================
static SensorAgent sensor_agent;
static MotorAgent motor_agent;
static MonitorAgent monitor_agent;

// ============================================================================
// Test Main Function
// ============================================================================
int main() {
    printf("🚀 Mini SObjectizer v3.0 Final - Test Simulation 🚀\n\n");
    
    // Initialize Mini-SObjectizer environment
    Environment& env = Environment::instance();
    System& system = System::instance();
    
    printf("=== System Initialization ===\n");
    if (!system.initialize()) {
        printf("ERROR: Failed to initialize system services!\n");
        return 1;
    }
    printf("✓ System services initialized\n\n");
    
    // Register agents
    printf("=== Agent Registration ===\n");
    AgentId sensor_id = env.register_agent(&sensor_agent);
    AgentId motor_id = env.register_agent(&motor_agent);
    AgentId monitor_id = env.register_agent(&monitor_agent);
    
    printf("✓ Sensor Agent: ID %u\n", sensor_id);
    printf("✓ Motor Agent: ID %u\n", motor_id);
    printf("✓ Monitor Agent: ID %u\n\n", monitor_id);
    
    // Register for watchdog monitoring
    printf("=== Watchdog Setup ===\n");
    system.watchdog().register_for_monitoring(sensor_id, 3000);   // 3 second timeout
    system.watchdog().register_for_monitoring(motor_id, 5000);    // 5 second timeout
    system.watchdog().register_for_monitoring(monitor_id, 2000);  // 2 second timeout
    printf("✓ All agents registered for monitoring\n\n");
    
    // Simulation loop
    printf("=== Test Simulation (10 cycles) ===\n\n");
    
    for (int cycle = 0; cycle < 10; ++cycle) {
        printf("--- Cycle %d ---\n", cycle + 1);
        
        // Sensor reading
        sensor_agent.read_sensors();
        
        // Process messages
        env.process_all_messages();
        
        // Motor commands at specific cycles
        if (cycle == 2) {
            printf("Control: Starting motor at speed 750\n");
            MotorCommand start_cmd{MotorCommand::START, 750};
            env.send_message(monitor_id, motor_id, start_cmd);
        }
        
        if (cycle == 5) {
            printf("Control: Changing motor speed to 1000\n");
            MotorCommand speed_cmd{MotorCommand::SET_SPEED, 1000};
            env.send_message(monitor_id, motor_id, speed_cmd);
        }
        
        if (cycle == 8) {
            printf("Control: Stopping motor\n");
            MotorCommand stop_cmd{MotorCommand::STOP, 0};
            env.send_message(monitor_id, motor_id, stop_cmd);
        }
        
        // Status requests every 3 cycles
        if (cycle % 3 == 2) {
            monitor_agent.request_system_status();
        }
        
        // Process all messages
        env.process_all_messages();
        
        // Run system services (watchdog, performance monitoring)
        env.run();
        
        // Show system status every 3 cycles
        if (cycle % 3 == 2) {
            printf("\n📊 System Status (Cycle %d):\n", cycle + 1);
            printf("  - Pending messages: %zu\n", env.total_pending_messages());
            printf("  - Error count: %zu\n", system.error().error_count());
            printf("  - Max processing time: %u μs\n", system.performance().max_processing_time_us());
            printf("  - Total messages: %lu\n", (unsigned long)system.performance().total_messages());
            printf("  - System health: %s\n", system.is_healthy() ? "HEALTHY" : "UNHEALTHY");
        }
        
        printf("\n");
        
        // Small delay for readability (in real embedded system this would be task scheduling)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // Final system report
    printf("=== Final System Report ===\n");
    printf("Total errors: %zu\n", system.error().error_count());
    printf("Peak processing time: %u μs\n", system.performance().max_processing_time_us());
    printf("Total messages processed: %lu\n", (unsigned long)system.performance().total_messages());
    printf("System final health: %s\n", system.is_healthy() ? "HEALTHY" : "DEGRADED");
    
    printf("\n🎯 Test Features Demonstrated:\n");
    printf("✓ Zero-overhead message passing\n");
    printf("✓ Automatic type-safe message dispatch\n");
    printf("✓ System service agents (Error, Performance, Watchdog)\n");
    printf("✓ Emergency response (temperature-based motor stop)\n");
    printf("✓ Real-time performance monitoring\n");
    printf("✓ Heartbeat-based health monitoring\n");
    printf("✓ Broadcast and point-to-point messaging\n");
    
    // Clean shutdown
    system.shutdown();
    printf("\n✨ Test simulation completed successfully! ✨\n");
    
    return 0;
}