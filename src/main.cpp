/**
 * @file main.cpp
 * @brief Mini SObjectizer v3.0 Production IoT Example
 * 
 * Complete IoT application example for real embedded systems.
 * 
 * System architecture:
 * - IoTSensorAgent: Temperature/humidity/pressure sensor data collection and broadcasting
 * - ActuatorAgent: Motor/valve control and emergency stop handling
 * - ControlAgent: Automatic control logic and sensor data-based decision making
 * 
 * Key features:
 * - 100% message-based communication (no direct calls between agents)
 * - Real-time safety monitoring (automatic stop on overheating detection)
 * - Watchdog-based liveness monitoring
 * - FreeRTOS task-based multithreading (embedded environment)
 * - Host simulation mode (UNIT_TEST=1)
 * 
 * @note Supports both actual hardware (STM32, ESP32, etc.) and host test environments
 */

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>

using namespace mini_so;

// ============================================================================
// Application Messages - Production IoT Example
// ============================================================================
struct SensorReading {
    float temperature;
    float humidity;
    float pressure;
    uint32_t timestamp;
};

struct ActuatorCommand {
    enum Action { START_MOTOR, STOP_MOTOR, OPEN_VALVE, CLOSE_VALVE } action;
    uint16_t parameter;  // speed, angle, etc.
};

struct SystemAlert {
    enum Level { INFO, WARNING, CRITICAL } level;
    uint32_t alert_code;
    float sensor_value;
};

// ============================================================================
// Production IoT Sensor Agent
// ============================================================================
class IoTSensorAgent : public Agent {
private:
    uint32_t reading_count_ = 0;
    float last_temperature_ = 25.0f;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        // Handle system status requests
        if (msg.type_id() == MESSAGE_TYPE_ID(system_messages::StatusRequest)) {
            printf("IoT Sensor: Status request received\n");
            return true;
        }
        return false;
    }
    
    void take_reading() {
        reading_count_++;
        
        // Simulate real sensor readings
        SensorReading reading;
        reading.temperature = last_temperature_ + ((reading_count_ % 5) - 2.0f);
        reading.humidity = 45.0f + (reading_count_ % 30);
        reading.pressure = 1013.25f + ((reading_count_ % 10) - 5.0f);
        reading.timestamp = now();
        
        last_temperature_ = reading.temperature;
        
        printf("IoT Sensor: T=%.1f°C, H=%.1f%%, P=%.1fhPa [#%lu]\n",
               reading.temperature, reading.humidity, reading.pressure, (unsigned long)reading_count_);
        
        // Broadcast sensor data
        MINI_SO_BROADCAST(reading);
        
        // Check for safety conditions and generate alerts
        if (reading.temperature > 80.0f) {
            SystemAlert alert;
            alert.level = SystemAlert::CRITICAL;
            alert.alert_code = 1001; // OVERHEATING
            alert.sensor_value = reading.temperature;
            
            MINI_SO_BROADCAST(alert);
            printf("IoT Sensor: CRITICAL ALERT - Overheating detected!\n");
        }
        
        // Send heartbeat to system
        MINI_SO_HEARTBEAT();
    }
};

// ============================================================================
// Production Actuator Control Agent
// ============================================================================
class ActuatorAgent : public Agent {
private:
    bool motor_running_ = false;
    bool valve_open_ = false;
    uint16_t motor_speed_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        if (msg.type_id() == MESSAGE_TYPE_ID(ActuatorCommand)) {
            const auto& cmd_msg = static_cast<const Message<ActuatorCommand>&>(msg);
            handle_actuator_command(cmd_msg.data);
            return true;
        }
        
        if (msg.type_id() == MESSAGE_TYPE_ID(SystemAlert)) {
            const auto& alert_msg = static_cast<const Message<SystemAlert>&>(msg);
            handle_system_alert(alert_msg.data);
            return true;
        }
        
        return false;
    }
    
private:
    void handle_actuator_command(const ActuatorCommand& cmd) {
        switch (cmd.action) {
            case ActuatorCommand::START_MOTOR:
                motor_running_ = true;
                motor_speed_ = cmd.parameter;
                printf("Actuator: Motor started at speed %u\n", motor_speed_);
                break;
                
            case ActuatorCommand::STOP_MOTOR:
                motor_running_ = false;
                motor_speed_ = 0;
                printf("Actuator: Motor stopped\n");
                break;
                
            case ActuatorCommand::OPEN_VALVE:
                valve_open_ = true;
                printf("Actuator: Valve opened\n");
                break;
                
            case ActuatorCommand::CLOSE_VALVE:
                valve_open_ = false;
                printf("Actuator: Valve closed\n");
                break;
        }
        
        // Send heartbeat after processing command
        MINI_SO_HEARTBEAT();
    }
    
    void handle_system_alert(const SystemAlert& alert) {
        if (alert.level == SystemAlert::CRITICAL) {
            // Emergency response
            if (alert.alert_code == 1001 && motor_running_) { // Overheating
                motor_running_ = false;
                motor_speed_ = 0;
                printf("Actuator: EMERGENCY STOP - Motor shutdown due to overheating\n");
                
                // Report emergency action taken
                printf("Actuator: Emergency action logged - Code 2001\n");
            }
        }
    }
};

// ============================================================================
// Production Control Agent
// ============================================================================
class ControlAgent : public Agent {
private:
    uint32_t control_cycle_ = 0;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        if (msg.type_id() == MESSAGE_TYPE_ID(SensorReading)) {
            const auto& sensor_msg = static_cast<const Message<SensorReading>&>(msg);
            process_sensor_data(sensor_msg.data);
            return true;
        }
        
        return false;
    }
    
    void run_control_cycle() {
        control_cycle_++;
        printf("=== Control Cycle %lu ===\n", (unsigned long)control_cycle_);
        
        // Automated control logic
        execute_control_logic();
        
        // Send heartbeat
        MINI_SO_HEARTBEAT();
    }
    
private:
    void process_sensor_data(const SensorReading& reading) {
        printf("Control: Processing sensor data - T=%.1f°C, P=%.1fhPa\n",
               reading.temperature, reading.pressure);
        
        // Temperature-based control
        if (reading.temperature < 20.0f) {
            // Start heating (motor)
            ActuatorCommand heat_cmd;
            heat_cmd.action = ActuatorCommand::START_MOTOR;
            heat_cmd.parameter = 500; // heating speed
            MINI_SO_BROADCAST(heat_cmd);
        } else if (reading.temperature > 30.0f) {
            // Open cooling valve
            ActuatorCommand cool_cmd;
            cool_cmd.action = ActuatorCommand::OPEN_VALVE;
            cool_cmd.parameter = 0;
            MINI_SO_BROADCAST(cool_cmd);
        }
    }
    
    void execute_control_logic() {
        // Periodic system commands based on control cycle
        if (control_cycle_ == 5) {
            printf("Control: Starting main motor\n");
            ActuatorCommand start_cmd;
            start_cmd.action = ActuatorCommand::START_MOTOR;
            start_cmd.parameter = 750;
            MINI_SO_BROADCAST(start_cmd);
        }
        
        if (control_cycle_ == 15) {
            printf("Control: Opening valve for drainage\n");
            ActuatorCommand valve_cmd;
            valve_cmd.action = ActuatorCommand::OPEN_VALVE;
            valve_cmd.parameter = 0;
            MINI_SO_BROADCAST(valve_cmd);
        }
        
        if (control_cycle_ == 20) {
            printf("Control: Stopping motor and closing valve\n");
            ActuatorCommand stop_cmd;
            stop_cmd.action = ActuatorCommand::STOP_MOTOR;
            stop_cmd.parameter = 0;
            MINI_SO_BROADCAST(stop_cmd);
            
            ActuatorCommand close_cmd;
            close_cmd.action = ActuatorCommand::CLOSE_VALVE;
            close_cmd.parameter = 0;
            MINI_SO_BROADCAST(close_cmd);
        }
    }
};

// ============================================================================
// Global Application Agents
// ============================================================================
static IoTSensorAgent sensor_agent;
static ActuatorAgent actuator_agent;
static ControlAgent control_agent;

// ============================================================================
// FreeRTOS Task Functions (for embedded platforms)
// ============================================================================
#ifndef UNIT_TEST

extern "C" {

void sensor_task(void *pvParameters) {
    (void)pvParameters;
    
    for (;;) {
        sensor_agent.take_reading();
        vTaskDelay(pdMS_TO_TICKS(2000)); // Read every 2 seconds
    }
}

void control_task(void *pvParameters) {
    (void)pvParameters;
    
    for (;;) {
        control_agent.run_control_cycle();
        vTaskDelay(pdMS_TO_TICKS(3000)); // Control every 3 seconds
    }
}

void system_task(void *pvParameters) {
    (void)pvParameters;
    
    Environment& env = Environment::instance();
    
    for (;;) {
        // Process all pending messages
        env.run();
        vTaskDelay(pdMS_TO_TICKS(100)); // System runs every 100ms
    }
}

} // extern "C"

#endif // UNIT_TEST

// ============================================================================
// Main Application Entry Point
// ============================================================================
int main() {
    printf("🚀 Mini SObjectizer v3.0 Final - Production IoT System 🚀\n\n");
    
    // Initialize Mini SObjectizer with user-friendly macro
    printf("=== System Initialization ===\n");
    MINI_SO_INIT();
    if (!sys.initialize()) {
        printf("FATAL: Failed to initialize Mini SObjectizer system!\n");
        return 1;
    }
    printf("✓ Mini SObjectizer system initialized\n");
    
    // Register application agents with user-friendly macros
    AgentId sensor_id = MINI_SO_REGISTER(sensor_agent);
    AgentId actuator_id = MINI_SO_REGISTER(actuator_agent);
    AgentId control_id = MINI_SO_REGISTER(control_agent);
    
    printf("✓ IoT Sensor Agent: ID %u\n", sensor_id);
    printf("✓ Actuator Agent: ID %u\n", actuator_id);
    printf("✓ Control Agent: ID %u\n", control_id);
    
    // Setup watchdog monitoring
    sys.watchdog().register_for_monitoring(sensor_id, 5000);   // 5 second timeout
    sys.watchdog().register_for_monitoring(actuator_id, 3000); // 3 second timeout
    sys.watchdog().register_for_monitoring(control_id, 4000);  // 4 second timeout
    printf("✓ Watchdog monitoring configured\n\n");

#ifdef UNIT_TEST
    // Host platform simulation
    printf("=== Host Platform Simulation ===\n");
    for (int cycle = 0; cycle < 25; ++cycle) {
        printf("\n--- Simulation Cycle %d ---\n", cycle + 1);
        
        // Sensor reading
        sensor_agent.take_reading();
        
        // Control cycle every 3 iterations
        if (cycle % 3 == 0) {
            control_agent.run_control_cycle();
        }
        
        // Process all messages and run system services
        MINI_SO_RUN();
        
        // Show periodic system status
        if (cycle % 5 == 4) {
            printf("\n📊 System Status:\n");
            printf("  - Error count: %zu\n", sys.error().error_count());
            printf("  - Max processing time: %u μs\n", sys.performance().max_processing_time_us());
            printf("  - Total messages: %lu\n", (unsigned long)sys.performance().total_messages());
            printf("  - System health: %s\n", sys.is_healthy() ? "HEALTHY" : "UNHEALTHY");
        }
    }
    
    printf("\n=== Simulation Complete ===\n");
    printf("Final Status: %s\n", sys.is_healthy() ? "✅ HEALTHY" : "❌ UNHEALTHY");
    
#else
    // Real embedded platform with FreeRTOS
    printf("=== Starting FreeRTOS Tasks ===\n");
    
    // Create application tasks
    xTaskCreate(sensor_task, "Sensor", 256, NULL, 2, NULL);
    xTaskCreate(control_task, "Control", 512, NULL, 2, NULL);
    xTaskCreate(system_task, "System", 512, NULL, 3, NULL);
    
    printf("✓ FreeRTOS tasks created\n");
    printf("✓ Starting scheduler...\n\n");
    
    // Start the FreeRTOS scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    printf("FATAL: FreeRTOS scheduler failed to start!\n");
    for (;;) {
        // Error handling - system halt
    }
#endif

    // Clean shutdown (only reached in host simulation)
    sys.shutdown();
    printf("\n✨ Mini SObjectizer shutdown complete ✨\n");
    
    return 0;
}