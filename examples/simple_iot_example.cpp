// simple_iot_example.cpp - Simple IoT demonstration using Mini SObjectizer
// This example shows basic IoT sensor monitoring and control

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>

using namespace mini_so;

// Simple IoT messages
struct TemperatureReading {
    float celsius;
    uint32_t sensor_id;
};

struct ControlCommand {
    enum Type { TURN_ON_HEATER, TURN_OFF_HEATER, TURN_ON_FAN, TURN_OFF_FAN } command;
    uint32_t device_id;
};

// Simple Temperature Sensor Agent
class TemperatureSensor : public Agent {
private:
    uint32_t sensor_id_;
    uint32_t reading_count_ = 0;
    
public:
    TemperatureSensor(uint32_t id) : sensor_id_(id) {}
    
    bool handle_message(const MessageBase& msg) noexcept override {
        (void)msg;  // 경고 방지
        // This sensor doesn't handle incoming messages in this simple example
        return false;
    }
    
    void take_reading() {
        reading_count_++;
        
        // Simulate temperature reading (varies between 18-32°C)
        float temperature = 25.0f + ((reading_count_ % 10) - 5.0f) + 
                           ((sensor_id_ * 3) % 5) - 2.0f;
        
        TemperatureReading reading;
        reading.celsius = temperature;
        reading.sensor_id = sensor_id_;
        
        printf("Sensor %u: Temperature = %.1f°C\n", sensor_id_, temperature);
        
        // Send temperature reading to all agents
        MINI_SO_BROADCAST(reading);
        
        // Send heartbeat to system
        MINI_SO_HEARTBEAT();
    }
};

// Simple Thermostat Control Agent
class ThermostatController : public Agent {
private:
    float target_temperature_ = 22.0f;
    bool heater_on_ = false;
    bool fan_on_ = false;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(TemperatureReading, process_temperature);
        return false;
    }
    
private:
    void process_temperature(const TemperatureReading& reading) {
        printf("Thermostat: Received temperature %.1f°C from sensor %u\n", 
               reading.celsius, reading.sensor_id);
        
        // Simple control logic
        if (reading.celsius < target_temperature_ - 1.0f && !heater_on_) {
            // Turn on heater
            ControlCommand cmd;
            cmd.command = ControlCommand::TURN_ON_HEATER;
            cmd.device_id = 1;
            MINI_SO_BROADCAST(cmd);
            heater_on_ = true;
            printf("Thermostat: Turning ON heater (temp %.1f°C < target %.1f°C)\n", 
                   reading.celsius, target_temperature_);
        }
        else if (reading.celsius > target_temperature_ + 1.0f && heater_on_) {
            // Turn off heater
            ControlCommand cmd;
            cmd.command = ControlCommand::TURN_OFF_HEATER;
            cmd.device_id = 1;
            MINI_SO_BROADCAST(cmd);
            heater_on_ = false;
            printf("Thermostat: Turning OFF heater (temp %.1f°C > target %.1f°C)\n", 
                   reading.celsius, target_temperature_);
        }
        
        // Fan control for cooling
        if (reading.celsius > target_temperature_ + 2.0f && !fan_on_) {
            ControlCommand cmd;
            cmd.command = ControlCommand::TURN_ON_FAN;
            cmd.device_id = 2;
            MINI_SO_BROADCAST(cmd);
            fan_on_ = true;
            printf("Thermostat: Turning ON fan for cooling\n");
        }
        else if (reading.celsius < target_temperature_ + 0.5f && fan_on_) {
            ControlCommand cmd;
            cmd.command = ControlCommand::TURN_OFF_FAN;
            cmd.device_id = 2;
            MINI_SO_BROADCAST(cmd);
            fan_on_ = false;
            printf("Thermostat: Turning OFF fan\n");
        }
        
        // Send heartbeat
        System::instance().heartbeat(id());
    }
};

// Simple Device Control Agent
class DeviceController : public Agent {
private:
    bool heater_status_ = false;
    bool fan_status_ = false;
    
public:
    bool handle_message(const MessageBase& msg) noexcept override {
        HANDLE_MESSAGE_VOID(ControlCommand, execute_command);
        return false;
    }
    
private:
    void execute_command(const ControlCommand& cmd) {
        switch (cmd.command) {
            case ControlCommand::TURN_ON_HEATER:
                heater_status_ = true;
                printf("Device Controller: Heater %u turned ON\n", cmd.device_id);
                break;
                
            case ControlCommand::TURN_OFF_HEATER:
                heater_status_ = false;
                printf("Device Controller: Heater %u turned OFF\n", cmd.device_id);
                break;
                
            case ControlCommand::TURN_ON_FAN:
                fan_status_ = true;
                printf("Device Controller: Fan %u turned ON\n", cmd.device_id);
                break;
                
            case ControlCommand::TURN_OFF_FAN:
                fan_status_ = false;
                printf("Device Controller: Fan %u turned OFF\n", cmd.device_id);
                break;
        }
        
        // Send heartbeat after executing command
        MINI_SO_HEARTBEAT();
    }
};

int main() {
    printf("🌡️ Simple IoT Thermostat Example - Mini SObjectizer v3.0 🌡️\n\n");
    
    // Initialize Mini SObjectizer with user-friendly macro
    MINI_SO_INIT();
    
    if (!sys.initialize()) {
        printf("Failed to initialize system!\n");
        return 1;
    }
    printf("✓ System initialized\n");
    
    // Create agents
    TemperatureSensor sensor1(1);
    TemperatureSensor sensor2(2);
    ThermostatController thermostat;
    DeviceController device_controller;
    
    // Register agents with user-friendly macros
    AgentId sensor1_id = MINI_SO_REGISTER(sensor1);
    AgentId sensor2_id = MINI_SO_REGISTER(sensor2);
    AgentId thermostat_id = MINI_SO_REGISTER(thermostat);
    AgentId controller_id = MINI_SO_REGISTER(device_controller);
    
    printf("✓ Registered agents: Sensor1=%u, Sensor2=%u, Thermostat=%u, Controller=%u\n\n",
           sensor1_id, sensor2_id, thermostat_id, controller_id);
    
    // Setup watchdog
    sys.watchdog().register_for_monitoring(sensor1_id, 3000);
    sys.watchdog().register_for_monitoring(sensor2_id, 3000);
    sys.watchdog().register_for_monitoring(thermostat_id, 2000);
    sys.watchdog().register_for_monitoring(controller_id, 2000);
    
    // Run simulation
    printf("=== IoT Thermostat Simulation ===\n");
    for (int cycle = 0; cycle < 20; ++cycle) {
        printf("\n--- Cycle %d ---\n", cycle + 1);
        
        // Take sensor readings
        sensor1.take_reading();
        if (cycle % 2 == 0) {  // Sensor 2 reads every other cycle
            sensor2.take_reading();
        }
        
        // Process all messages with user-friendly macro
        MINI_SO_RUN();
        
        // Show system status every 5 cycles
        if (cycle % 5 == 4) {
            printf("\n📊 System Status:\n");
            printf("  - System health: %s\n", sys.is_healthy() ? "HEALTHY" : "UNHEALTHY");
            printf("  - Total messages: %lu\n", (unsigned long)sys.performance().total_messages());
            printf("  - Error count: %zu\n", sys.error().error_count());
        }
    }
    
    // Final status
    printf("\n=== Final Status ===\n");
    printf("System health: %s\n", sys.is_healthy() ? "✅ HEALTHY" : "❌ UNHEALTHY");
    printf("Total messages processed: %lu\n", (unsigned long)sys.performance().total_messages());
    
    sys.shutdown();
    printf("\n✨ IoT Thermostat simulation complete! ✨\n");
    
    return 0;
}