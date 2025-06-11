// enhanced_example.cpp - Enhanced Mini-SObjectizer Example with Error Handling and Metrics
#include "mini_sobjectizer/mini_sobjectizer_v2.h"
#include "mini_sobjectizer/debug_utils.h"
#include <cstdio>

using namespace mini_so;

// ============================================================================
// Enhanced Message Types
// ============================================================================
struct SensorReading {
    float temperature;
    float humidity;
    uint32_t timestamp;
    uint16_t sensor_id;
};

struct MotorCommand {
    enum Action { START, STOP, SET_SPEED, EMERGENCY_STOP } action;
    uint16_t speed;
    uint32_t command_id;
};

struct StatusRequest {
    uint32_t request_id;
    AgentId requester_id;
};

struct StatusResponse {
    uint32_t request_id;
    bool motor_running;
    uint16_t current_speed;
    float last_temperature;
    SystemHealth system_health;
    uint32_t error_count;
};

struct SystemAlert {
    enum AlertLevel { INFO, WARNING, CRITICAL } level;
    uint32_t alert_id;
    const char* message;
};

// ============================================================================
// Enhanced Sensor Agent with Error Handling
// ============================================================================
class EnhancedSensorAgent : public Agent {
private:
    uint32_t reading_count_ = 0;
    uint32_t error_count_ = 0;
    float last_temperature_ = 0.0f;
    float last_humidity_ = 0.0f;
    uint16_t sensor_id_;
    
    // State machine states
    StateId initializing_state_;
    StateId normal_operation_state_;
    StateId error_state_;
    StateId calibrating_state_;
    
public:
    EnhancedSensorAgent(uint16_t sensor_id) : sensor_id_(sensor_id) {}
    
    void so_define_states() override {
        // Define hierarchical states
        initializing_state_ = define_state("Initializing");
        normal_operation_state_ = define_state("NormalOperation");
        error_state_ = define_state("Error");
        calibrating_state_ = define_state("Calibrating", normal_operation_state_); // Child of normal operation
        
        // Set up state callbacks
        on_state_enter(initializing_state_, [this]() {
            printf("Sensor %u: Entering initialization state\n", sensor_id_);
            // Simulate initialization delay
            after(2000, [this]() {
                transition_to(normal_operation_state_);
            });
        });
        
        on_state_enter(normal_operation_state_, [this]() {
            printf("Sensor %u: Entering normal operation\n", sensor_id_);
            start_periodic_readings();
        });
        
        on_state_enter(error_state_, [this]() {
            printf("Sensor %u: Entering error state\n", sensor_id_);
            send_system_alert(SystemAlert::CRITICAL, "Sensor entered error state");
            
            // Try to recover after 5 seconds
            after(5000, [this]() {
                if (error_count_ < 3) {
                    transition_to(initializing_state_);
                } else {
                    send_system_alert(SystemAlert::CRITICAL, "Sensor failed permanently");
                }
            });
        });
        
        on_state_enter(calibrating_state_, [this]() {
            printf("Sensor %u: Starting calibration\n", sensor_id_);
            after(3000, [this]() {
                printf("Sensor %u: Calibration complete\n", sensor_id_);
                transition_to(normal_operation_state_);
            });
        });
        
        // Start in initializing state
        transition_to(initializing_state_);
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id == MESSAGE_TYPE_ID(StatusRequest)) {
            handle_status_request(static_cast<const Message<StatusRequest>&>(msg));
            return true;
        }
        
        if (msg.type_id == MESSAGE_TYPE_ID(SystemAlert)) {
            auto& alert = static_cast<const Message<SystemAlert>&>(msg);
            if (alert.data.level == SystemAlert::CRITICAL && 
                in_state(normal_operation_state_)) {
                // React to critical system alerts
                transition_to(error_state_);
            }
            return true;
        }
        
        return false;
    }
    
private:
    void start_periodic_readings() {
        // Start periodic sensor readings every 1 second
        every(1000, [this]() {
            if (in_state(normal_operation_state_)) {
                take_reading();
            }
        });
    }
    
    void take_reading() {
        reading_count_++;
        
        // Simulate sensor reading with occasional errors
        if (reading_count_ % 20 == 0) {
            // Simulate sensor error every 20 readings
            error_count_++;
            ErrorHandler::report_error(ErrorCode::SYSTEM_OVERLOAD, "Sensor reading failed");
            
            if (error_count_ >= 3) {
                transition_to(error_state_);
                return;
            }
        }
        
        // Generate realistic sensor data
        last_temperature_ = 20.0f + (reading_count_ % 20) - 10.0f; // 10-30°C
        last_humidity_ = 50.0f + (reading_count_ % 30) - 15.0f;    // 35-65%
        
        SensorReading reading;
        reading.temperature = last_temperature_;
        reading.humidity = last_humidity_;
        reading.timestamp = now();
        reading.sensor_id = sensor_id_;
        
        // Broadcast sensor data
        broadcast_message(reading);
        
        // Trigger calibration occasionally
        if (reading_count_ % 50 == 0 && in_state(normal_operation_state_)) {
            transition_to(calibrating_state_);
        }
    }
    
    void handle_status_request(const Message<StatusRequest>& request) {
        StatusResponse response;
        response.request_id = request.data.request_id;
        response.motor_running = false; // Not applicable for sensor
        response.current_speed = 0;
        response.last_temperature = last_temperature_;
        response.system_health = ErrorHandler::get_system_health();
        response.error_count = error_count_;
        
        send_message(request.data.requester_id, response);
    }
    
    void send_system_alert(SystemAlert::AlertLevel level, const char* message) {
        SystemAlert alert;
        alert.level = level;
        alert.alert_id = now();
        alert.message = message;
        
        broadcast_message(alert);
    }
};

// ============================================================================
// Enhanced Motor Agent with Advanced State Machine
// ============================================================================
class EnhancedMotorAgent : public Agent {
private:
    // Motor states
    StateId idle_state_;
    StateId starting_state_;
    StateId running_state_;
    StateId stopping_state_;
    StateId emergency_stop_state_;
    StateId maintenance_state_;
    
    uint16_t current_speed_ = 0;
    uint16_t target_speed_ = 0;
    float last_temperature_ = 0.0f;
    uint32_t run_time_ms_ = 0;
    uint32_t start_time_ = 0;
    bool emergency_stop_active_ = false;
    
public:
    void so_define_states() override {
        // Define motor states
        idle_state_ = define_state("Idle");
        starting_state_ = define_state("Starting");
        running_state_ = define_state("Running");
        stopping_state_ = define_state("Stopping");
        emergency_stop_state_ = define_state("EmergencyStop");
        maintenance_state_ = define_state("Maintenance");
        
        // Idle state
        on_state_enter(idle_state_, [this]() {
            printf("Motor: Entering idle state\n");
            current_speed_ = 0;
            emergency_stop_active_ = false;
        });
        
        // Starting state
        on_state_enter(starting_state_, [this]() {
            printf("Motor: Starting up...\n");
            // Simulate gradual speed increase
            after(500, [this]() {
                current_speed_ = target_speed_ / 4;
                after(500, [this]() {
                    current_speed_ = target_speed_ / 2;
                    after(500, [this]() {
                        current_speed_ = target_speed_;
                        start_time_ = now();
                        transition_to(running_state_);
                    });
                });
            });
        });
        
        // Running state
        on_state_enter(running_state_, [this]() {
            printf("Motor: Running at speed %u\n", current_speed_);
            // Monitor for overheating
            every(2000, [this]() {
                if (in_state(running_state_) && last_temperature_ > 80.0f) {
                    printf("Motor: Overheating detected! Emergency stop.\n");
                    transition_to(emergency_stop_state_);
                }
            });
        });
        
        on_state_exit(running_state_, [this]() {
            run_time_ms_ += now() - start_time_;
        });
        
        // Stopping state
        on_state_enter(stopping_state_, [this]() {
            printf("Motor: Stopping...\n");
            // Gradual speed decrease
            after(300, [this]() {
                current_speed_ = current_speed_ / 2;
                after(300, [this]() {
                    current_speed_ = current_speed_ / 2;
                    after(300, [this]() {
                        current_speed_ = 0;
                        transition_to(idle_state_);
                    });
                });
            });
        });
        
        // Emergency stop state
        on_state_enter(emergency_stop_state_, [this]() {
            printf("Motor: EMERGENCY STOP!\n");
            current_speed_ = 0;
            emergency_stop_active_ = true;
            
            SystemAlert alert;
            alert.level = SystemAlert::CRITICAL;
            alert.alert_id = now();
            alert.message = "Motor emergency stop activated";
            broadcast_message(alert);
            
            // Require manual reset after 10 seconds
            after(10000, [this]() {
                if (in_state(emergency_stop_state_)) {
                    transition_to(maintenance_state_);
                }
            });
        });
        
        // Maintenance state
        on_state_enter(maintenance_state_, [this]() {
            printf("Motor: Entering maintenance mode\n");
            // Simulate maintenance check
            after(5000, [this]() {
                printf("Motor: Maintenance complete\n");
                transition_to(idle_state_);
            });
        });
        
        // Start in idle state
        transition_to(idle_state_);
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id == MESSAGE_TYPE_ID(MotorCommand)) {
            handle_motor_command(static_cast<const Message<MotorCommand>&>(msg));
            return true;
        }
        
        if (msg.type_id == MESSAGE_TYPE_ID(SensorReading)) {
            handle_sensor_reading(static_cast<const Message<SensorReading>&>(msg));
            return true;
        }
        
        if (msg.type_id == MESSAGE_TYPE_ID(StatusRequest)) {
            handle_status_request(static_cast<const Message<StatusRequest>&>(msg));
            return true;
        }
        
        return false;
    }
    
private:
    void handle_motor_command(const Message<MotorCommand>& cmd_msg) {
        const auto& cmd = cmd_msg.data;
        printf("Motor: Received command %d (ID: %u)\n", cmd.action, cmd.command_id);
        
        switch (cmd.action) {
            case MotorCommand::START:
                if (in_state(idle_state_) && !emergency_stop_active_) {
                    target_speed_ = cmd.speed > 0 ? cmd.speed : 500;
                    transition_to(starting_state_);
                } else {
                    printf("Motor: Cannot start - not in idle state or emergency stop active\n");
                }
                break;
                
            case MotorCommand::STOP:
                if (in_state(running_state_) || in_state(starting_state_)) {
                    transition_to(stopping_state_);
                }
                break;
                
            case MotorCommand::SET_SPEED:
                if (in_state(running_state_)) {
                    target_speed_ = cmd.speed;
                    current_speed_ = cmd.speed;
                    printf("Motor: Speed set to %u\n", current_speed_);
                }
                break;
                
            case MotorCommand::EMERGENCY_STOP:
                transition_to(emergency_stop_state_);
                break;
        }
    }
    
    void handle_sensor_reading(const Message<SensorReading>& sensor_msg) {
        const auto& reading = sensor_msg.data;
        last_temperature_ = reading.temperature;
        
        // Safety check for temperature
        if (reading.temperature > 85.0f && in_state(running_state_)) {
            printf("Motor: Critical temperature detected: %.1f°C\n", reading.temperature);
            transition_to(emergency_stop_state_);
        }
    }
    
    void handle_status_request(const Message<StatusRequest>& request) {
        StatusResponse response;
        response.request_id = request.data.request_id;
        response.motor_running = in_state(running_state_);
        response.current_speed = current_speed_;
        response.last_temperature = last_temperature_;
        response.system_health = ErrorHandler::get_system_health();
        response.error_count = ErrorHandler::get_error_count();
        
        send_message(request.data.requester_id, response);
    }
};

// ============================================================================
// System Monitor Agent
// ============================================================================
class SystemMonitorAgent : public Agent {
private:
    uint32_t status_request_id_ = 0;
    uint32_t last_health_check_ = 0;
    
public:
    void so_define_states() override {
        StateId monitoring_state = define_state("Monitoring");
        transition_to(monitoring_state);
        
        // Start periodic monitoring
        every(5000, [this]() {
            perform_system_health_check();
        });
        
        every(10000, [this]() {
            request_system_status();
        });
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id == MESSAGE_TYPE_ID(StatusResponse)) {
            handle_status_response(static_cast<const Message<StatusResponse>&>(msg));
            return true;
        }
        
        if (msg.type_id == MESSAGE_TYPE_ID(SystemAlert)) {
            handle_system_alert(static_cast<const Message<SystemAlert>&>(msg));
            return true;
        }
        
        return false;
    }
    
private:
    void perform_system_health_check() {
        SystemHealth health = ErrorHandler::get_system_health();
        uint32_t current_time = now();
        
        if (health != SystemHealth::HEALTHY && current_time - last_health_check_ > 30000) {
            printf("MONITOR: System health check - Status: %s, Errors: %u\n",
                   health == SystemHealth::WARNING ? "WARNING" :
                   health == SystemHealth::CRITICAL ? "CRITICAL" : "FAILED",
                   ErrorHandler::get_error_count());
            
            last_health_check_ = current_time;
            
            if (health == SystemHealth::FAILED) {
                // Trigger system recovery
                SystemAlert alert;
                alert.level = SystemAlert::CRITICAL;
                alert.alert_id = current_time;
                alert.message = "System health critical - initiating recovery";
                broadcast_message(alert);
            }
        }
    }
    
    void request_system_status() {
        StatusRequest request;
        request.request_id = ++status_request_id_;
        request.requester_id = id();
        
        // Broadcast status request to all agents
        broadcast_message(request);
    }
    
    void handle_status_response(const Message<StatusResponse>& response) {
        printf("MONITOR: Status response %u - Motor: %s, Speed: %u, Temp: %.1f°C, Health: %s\n",
               response.data.request_id,
               response.data.motor_running ? "Running" : "Stopped",
               response.data.current_speed,
               response.data.last_temperature,
               response.data.system_health == SystemHealth::HEALTHY ? "Healthy" : "Warning");
    }
    
    void handle_system_alert(const Message<SystemAlert>& alert) {
        const char* level_str = alert.data.level == SystemAlert::INFO ? "INFO" :
                               alert.data.level == SystemAlert::WARNING ? "WARNING" : "CRITICAL";
        printf("MONITOR: ALERT [%s] ID:%u - %s\n", level_str, alert.data.alert_id, alert.data.message);
        
        if (alert.data.level == SystemAlert::CRITICAL) {
            // Log critical alerts
            ErrorHandler::report_error(ErrorCode::SYSTEM_OVERLOAD, "Critical system alert received");
        }
    }
};

// ============================================================================
// Main Application
// ============================================================================

#ifdef NATIVE_BUILD
// For testing on native platform
int main() {
    printf("=== Enhanced Mini-SObjectizer Example ===\n\n");
    
    // Validate configuration
    ConfigValidator::validate_configuration();
    ConfigValidator::print_memory_breakdown();
    
    // Initialize environment
    Environment& env = Environment::instance();
    
    // Create agents
    EnhancedSensorAgent sensor(1);
    EnhancedMotorAgent motor;
    SystemMonitorAgent monitor;
    
    // Register agents
    AgentId sensor_id = env.register_agent(&sensor);
    AgentId motor_id = env.register_agent(&motor);
    AgentId monitor_id = env.register_agent(&monitor);
    
    printf("Agents registered - Sensor: %u, Motor: %u, Monitor: %u\n\n", 
           sensor_id, motor_id, monitor_id);
    
    // Simulation loop
    printf("Starting simulation...\n\n");
    
    for (int cycle = 0; cycle < 20; ++cycle) {
        printf("=== Simulation Cycle %d ===\n", cycle + 1);
        
        // Process all pending messages
        env.process_all_messages();
        env.update_all_timers();
        
        // Send some commands
        if (cycle == 3) {
            printf("Sending motor start command...\n");
            MotorCommand start_cmd = {MotorCommand::START, 750, (uint32_t)(cycle + 100)};
            env.send_message(monitor_id, motor_id, start_cmd);
        }
        
        if (cycle == 10) {
            printf("Sending speed change command...\n");
            MotorCommand speed_cmd = {MotorCommand::SET_SPEED, 1000, (uint32_t)(cycle + 100)};
            env.send_message(monitor_id, motor_id, speed_cmd);
        }
        
        if (cycle == 15) {
            printf("Sending motor stop command...\n");
            MotorCommand stop_cmd = {MotorCommand::STOP, 0, (uint32_t)(cycle + 100)};
            env.send_message(monitor_id, motor_id, stop_cmd);
        }
        
        // Print system status every 5 cycles
        if (cycle % 5 == 4) {
            DebugUtils::print_system_status();
        }
        
        // Simulate time passing
        #ifdef UNIT_TEST
        // In test environment, just process messages
        #else
        // In real environment, add delay
        vTaskDelay(pdMS_TO_TICKS(1000));
        #endif
    }
    
    printf("\n=== Final System Status ===\n");
    DebugUtils::print_system_status();
    
    // Run performance and stress tests
    DebugUtils::run_performance_test(100);
    DebugUtils::run_stress_test(50);
    
    printf("Simulation complete.\n");
    return 0;
}

#else
// For STM32 embedded target
extern "C" {
    void enhanced_example_task(void *pvParameters) {
        (void)pvParameters;
        
        // Run the same example as above but in FreeRTOS task
        main();
        
        // Task should never return
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(10000));
        }
    }
}

int main() {
    // Create the example task
    xTaskCreate(enhanced_example_task, "EnhancedExample", 1024, NULL, 2, NULL);
    
    // Start FreeRTOS scheduler
    vTaskStartScheduler();
    
    // Should never reach here
    while (1) {
        // Error - scheduler failed to start
    }
    
    return 0;
}
#endif