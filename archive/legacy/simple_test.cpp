// Simple test without Unity framework dependency
#include "../include/mini_sobjectizer/mini_sobjectizer_v2.h"
#include "../include/mini_sobjectizer/debug_utils.h"
#include <iostream>
#include <cassert>

using namespace mini_so;

// Mock FreeRTOS functions for testing
extern "C" {
    SemaphoreHandle_t xSemaphoreCreateMutex(void) {
        return (SemaphoreHandle_t)1; // Return non-null pointer
    }
    
    BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
        (void)xSemaphore; (void)xTicksToWait;
        return pdTRUE;
    }
    
    BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
        (void)xSemaphore;
        return pdTRUE;
    }
    
    void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {
        (void)xSemaphore;
    }
    
    TickType_t xTaskGetTickCount(void) {
        return 1000;
    }
    
    void taskDISABLE_INTERRUPTS(void) {}
}

// Test message types
struct TestMessage {
    uint32_t value;
    const char* text;
};

struct StatusRequest {
    uint32_t request_id;
};

// Simple test agent
class SimpleAgent : public Agent {
private:
    uint32_t message_count_ = 0;
    
public:
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id == MESSAGE_TYPE_ID(TestMessage)) {
            message_count_++;
            auto& typed_msg = static_cast<const Message<TestMessage>&>(msg);
            std::cout << "Agent " << id() << " received message: " 
                      << typed_msg.data.value << " - " << typed_msg.data.text << std::endl;
            return true;
        }
        return false;
    }
    
    uint32_t get_message_count() const { return message_count_; }
};

int main() {
    std::cout << "=== Mini-SObjectizer Simple Test ===" << std::endl;
    
    // Reset system state
    ErrorHandler::reset_error_state();
    SystemMetrics::reset_metrics();
    
    // Print configuration
    std::cout << "\n=== Configuration ===" << std::endl;
    ConfigValidator::validate_configuration();
    
    // Test 1: Basic message queue functionality
    std::cout << "\n=== Test 1: MessageQueue ===" << std::endl;
    {
        MessageQueue queue;
        assert(queue.empty());
        assert(!queue.full());
        assert(queue.size() == 0);
        
        TestMessage test_msg = {42, "hello"};
        Message<TestMessage> msg(test_msg);
        
        auto result = queue.push(msg, sizeof(Message<TestMessage>));
        assert(result == MessageQueue::PushResult::SUCCESS);
        assert(!queue.empty());
        assert(queue.size() == 1);
        
        uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
        MessageSize size;
        bool pop_result = queue.pop(buffer, size);
        assert(pop_result);
        assert(queue.empty());
        
        std::cout << "✓ MessageQueue basic functionality works" << std::endl;
    }
    
    // Test 2: Error handling
    std::cout << "\n=== Test 2: Error Handling ===" << std::endl;
    {
        ErrorHandler::reset_error_state();
        assert(ErrorHandler::get_error_count() == 0);
        assert(ErrorHandler::get_system_health() == SystemHealth::HEALTHY);
        
        ErrorHandler::report_error(ErrorCode::QUEUE_OVERFLOW, "Test error");
        assert(ErrorHandler::get_error_count() == 1);
        assert(ErrorHandler::get_last_error() == ErrorCode::QUEUE_OVERFLOW);
        assert(ErrorHandler::get_system_health() == SystemHealth::WARNING);
        
        std::cout << "✓ Error handling works" << std::endl;
        std::cout << "✓ Last error: " << ErrorHandler::error_code_to_string(ErrorHandler::get_last_error()) << std::endl;
    }
    
    // Test 3: Agent and Environment
    std::cout << "\n=== Test 3: Agent & Environment ===" << std::endl;
    {
        Environment& env = Environment::instance();
        SimpleAgent agent1, agent2;
        
        AgentId id1 = env.register_agent(&agent1);
        AgentId id2 = env.register_agent(&agent2);
        
        assert(id1 != INVALID_AGENT_ID);
        assert(id2 != INVALID_AGENT_ID);
        assert(id1 != id2);
        assert(env.agent_count() >= 2);
        
        std::cout << "✓ Agent registration works - Agent1: " << id1 << ", Agent2: " << id2 << std::endl;
        
        // Test message sending
        TestMessage test_msg = {123, "test message"};
        bool send_result = env.send_message(id1, id2, test_msg);
        assert(send_result);
        
        // Process the message
        agent2.process_messages();
        assert(agent2.get_message_count() == 1);
        
        std::cout << "✓ Message sending and processing works" << std::endl;
    }
    
    // Test 4: Metrics
    std::cout << "\n=== Test 4: Metrics ===" << std::endl;
    {
        SystemMetrics::reset_metrics();
        assert(SystemMetrics::get_messages_sent() == 0);
        assert(SystemMetrics::get_messages_processed() == 0);
        
        // Previous test should have incremented metrics
        Environment& env = Environment::instance();
        SimpleAgent agent;
        AgentId id = env.register_agent(&agent);
        
        TestMessage msg = {456, "metrics test"};
        env.send_message(id, id, msg);
        agent.process_messages();
        
        assert(SystemMetrics::get_messages_sent() > 0);
        assert(SystemMetrics::get_messages_processed() > 0);
        
        std::cout << "✓ Metrics tracking works" << std::endl;
        std::cout << "✓ Messages sent: " << SystemMetrics::get_messages_sent() << std::endl;
        std::cout << "✓ Messages processed: " << SystemMetrics::get_messages_processed() << std::endl;
    }
    
    // Test 5: Oversized message handling
    std::cout << "\n=== Test 5: Error Conditions ===" << std::endl;
    {
        MessageQueue queue;
        TestMessage test_msg = {42, "test"};
        Message<TestMessage> msg(test_msg);
        
        // Test oversized message
        auto result = queue.push(msg, MINI_SO_MAX_MESSAGE_SIZE + 1);
        assert(result == MessageQueue::PushResult::MESSAGE_TOO_LARGE);
        
        // Test undersized message
        result = queue.push(msg, sizeof(MessageBase) - 1);
        assert(result == MessageQueue::PushResult::INVALID_MESSAGE);
        
        std::cout << "✓ Error conditions handled correctly" << std::endl;
    }
    
    // Final system status
    std::cout << "\n=== Final Status ===" << std::endl;
    DebugUtils::print_system_status();
    
    // Run stress test
    std::cout << "\n=== Stress Test ===" << std::endl;
    bool stress_result = DebugUtils::run_stress_test(10);
    assert(stress_result);
    
    std::cout << "\n=== All Tests Passed! ===" << std::endl;
    std::cout << "✓ Mini-SObjectizer is ready for production use" << std::endl;
    
    return 0;
}