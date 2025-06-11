// macro_demo_example.cpp - Demonstration of User-Friendly Macros
// This example showcases all the new user-friendly macros

#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstdio>

using namespace mini_so;

// FreeRTOS mock functions are in freertos_mock.cpp

// Demo message types
struct DemoMessage {
    uint32_t value;
    const char* text;
};

struct StatusMessage {
    uint32_t agent_id;
    bool active;
};

// Demo Agent using new user-friendly macros
class DemoAgent : public Agent {
public:
    DemoAgent(const char* name) : name_(name) {}
    
    bool handle_message(const MessageBase& msg) noexcept override {
        printf("🎯 %s processing message (ID: %u)\n", name_, msg.type_id());
        
        // Using user-friendly message handling macros
        HANDLE_MESSAGE_VOID(DemoMessage, process_demo);
        HANDLE_MESSAGE_VOID(StatusMessage, process_status);
        
        return false;
    }
    
    void send_demo_message() {
        DemoMessage msg{42, "Hello from user-friendly macros!"};
        printf("📤 %s sending demo message\n", name_);
        
        // Using user-friendly broadcast macro
        MINI_SO_BROADCAST(msg);
        
        // Using user-friendly heartbeat macro
        MINI_SO_HEARTBEAT();
    }
    
private:
    const char* name_;
    
    void process_demo(const DemoMessage& msg) {
        printf("✅ %s received: value=%u, text='%s'\n", name_, msg.value, msg.text);
        
        // Using user-friendly performance recording macro
        MINI_SO_RECORD_PERFORMANCE(25, 1);
    }
    
    void process_status(const StatusMessage& msg) {
        printf("📊 %s status: agent_id=%u, active=%s\n", 
               name_, msg.agent_id, msg.active ? "true" : "false");
    }
};

int main() {
    printf("🚀 User-Friendly Macros Demo - Mini SObjectizer v3.0 🚀\n\n");
    
    // 1. System initialization with user-friendly macro
    printf("=== 1. System Initialization ===\n");
    MINI_SO_INIT();  // One-liner initialization!
    
    if (!sys.initialize()) {
        printf("❌ System initialization failed!\n");
        return 1;
    }
    printf("✅ System initialized with MINI_SO_INIT() macro\n\n");
    
    // 2. Agent creation and registration
    printf("=== 2. Agent Registration ===\n");
    DemoAgent agent1("Agent1");
    DemoAgent agent2("Agent2");
    DemoAgent agent3("Agent3");
    
    // Using user-friendly registration macros
    AgentId id1 = MINI_SO_REGISTER(agent1);
    AgentId id2 = MINI_SO_REGISTER(agent2);
    AgentId id3 = MINI_SO_REGISTER(agent3);
    
    printf("✅ Registered agents with MINI_SO_REGISTER() macros:\n");
    printf("   - Agent1: ID %u\n", id1);
    printf("   - Agent2: ID %u\n", id2);
    printf("   - Agent3: ID %u\n\n", id3);
    
    // 3. Type registration and collision checking
    printf("=== 3. Type Safety Macros ===\n");
    
    // Check for type ID collisions using user-friendly macro
    CHECK_NO_COLLISIONS(DemoMessage, StatusMessage);
    printf("✅ No type ID collisions detected with CHECK_NO_COLLISIONS()\n");
    
    // Show type IDs using short alias
    printf("📋 Type IDs:\n");
    printf("   - DemoMessage: %u (using MSG_ID macro)\n", MSG_ID(DemoMessage));
    printf("   - StatusMessage: %u (using MSG_ID macro)\n\n", MSG_ID(StatusMessage));
    
    // 4. Message sending demonstration
    printf("=== 4. Message Sending Demo ===\n");
    agent1.send_demo_message();
    agent2.send_demo_message();
    
    // Send status messages using traditional way for comparison
    StatusMessage status{id1, true};
    env.send_message(id1, id3, status);
    
    printf("\n");
    
    // 5. Message processing with user-friendly macro
    printf("=== 5. Message Processing ===\n");
    MINI_SO_RUN();  // Process all messages with one macro!
    printf("✅ All messages processed with MINI_SO_RUN() macro\n\n");
    
    // 6. System status check
    printf("=== 6. System Status ===\n");
    printf("📊 System Health: %s\n", sys.is_healthy() ? "HEALTHY" : "UNHEALTHY");
    printf("📈 Total Messages: %lu\n", (unsigned long)sys.performance().total_messages());
    printf("⚡ Max Processing Time: %u μs\n", sys.performance().max_processing_time_us());
    printf("🔍 Error Count: %zu\n\n", sys.error().error_count());
    
    // 7. Macro comparison summary
    printf("=== 7. Macro Comparison Summary ===\n");
    printf("✨ User-Friendly Improvements:\n");
    printf("   🎯 Message Handling: 5 lines → 1 line (HANDLE_MESSAGE)\n");
    printf("   🚀 System Init: 3 lines → 1 line (MINI_SO_INIT)\n");
    printf("   📝 Agent Registration: verbose → MINI_SO_REGISTER()\n");
    printf("   📤 Message Broadcasting: verbose → MINI_SO_BROADCAST()\n");
    printf("   💓 Heartbeat: verbose → MINI_SO_HEARTBEAT()\n");
    printf("   🔍 Type ID: MESSAGE_TYPE_ID() → MSG_ID()\n");
    printf("   ✅ Type Checking: ASSERT_NO_TYPE_ID_COLLISIONS() → CHECK_NO_COLLISIONS()\n\n");
    
    printf("🎉 User-Friendly Macros Demo Complete! 🎉\n");
    printf("💡 Code is now more readable and maintainable!\n");
    
    return 0;
}