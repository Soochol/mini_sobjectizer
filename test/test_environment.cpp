#include <unity.h>
#include "mini_sobjectizer/mini_sobjectizer.h"

using namespace mini_so;

// Mock FreeRTOS functions for testing
extern "C" {
    SemaphoreHandle_t xSemaphoreCreateMutex(void) {
        return (SemaphoreHandle_t)1;
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
    uint32_t id;
    uint32_t value;
};

struct ResponseMessage {
    uint32_t original_id;
    bool success;
};

// Simple test agent
class SimpleTestAgent : public Agent {
private:
    uint32_t received_messages_ = 0;
    TestMessage last_message_ = {0, 0};
    
public:
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id == MESSAGE_TYPE_ID(TestMessage)) {
            auto& typed_msg = static_cast<const Message<TestMessage>&>(msg);
            received_messages_++;
            last_message_ = typed_msg.data;
            
            // Auto-respond with a ResponseMessage
            ResponseMessage response = {typed_msg.data.id, true};
            send_message(msg.sender_id, response);
            
            return true;
        }
        
        if (msg.type_id == MESSAGE_TYPE_ID(ResponseMessage)) {
            received_messages_++;
            return true;
        }
        
        return false;
    }
    
    uint32_t get_received_count() const { return received_messages_; }
    const TestMessage& get_last_message() const { return last_message_; }
    void reset() { received_messages_ = 0; last_message_ = {0, 0}; }
};

// Broadcast test agent
class BroadcastTestAgent : public Agent {
private:
    uint32_t broadcast_count_ = 0;
    
public:
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id == MESSAGE_TYPE_ID(TestMessage)) {
            broadcast_count_++;
            return true;
        }
        return false;
    }
    
    uint32_t get_broadcast_count() const { return broadcast_count_; }
    void reset() { broadcast_count_ = 0; }
};

void setUp(void) {
    ErrorHandler::reset_error_state();
    SystemMetrics::reset_metrics();
    
    // Reset Environment singleton for testing
    // Note: In a real test environment, we'd need a way to reset the singleton
    // For now, we'll work with the existing instance
}

void tearDown(void) {
    // Clean up
}

void test_environment_singleton(void) {
    Environment& env1 = Environment::instance();
    Environment& env2 = Environment::instance();
    
    // Should be the same instance
    TEST_ASSERT_EQUAL_PTR(&env1, &env2);
}

void test_environment_agent_registration(void) {
    Environment& env = Environment::instance();
    SimpleTestAgent agent1, agent2;
    
    // Register agents
    AgentId id1 = env.register_agent(&agent1);
    AgentId id2 = env.register_agent(&agent2);
    
    // Should get valid, different IDs
    TEST_ASSERT_NOT_EQUAL(INVALID_AGENT_ID, id1);
    TEST_ASSERT_NOT_EQUAL(INVALID_AGENT_ID, id2);
    TEST_ASSERT_NOT_EQUAL(id1, id2);
    
    // Verify agents can be retrieved
    Agent* retrieved1 = env.get_agent(id1);
    Agent* retrieved2 = env.get_agent(id2);
    
    TEST_ASSERT_EQUAL_PTR(&agent1, retrieved1);
    TEST_ASSERT_EQUAL_PTR(&agent2, retrieved2);
    
    // Test agent count
    TEST_ASSERT_TRUE(env.agent_count() >= 2);
}

void test_environment_invalid_agent_registration(void) {
    Environment& env = Environment::instance();
    
    // Try to register null agent
    AgentId invalid_id = env.register_agent(nullptr);
    TEST_ASSERT_EQUAL(INVALID_AGENT_ID, invalid_id);
    
    // Verify error was reported
    TEST_ASSERT_EQUAL(ErrorCode::AGENT_REGISTRATION_FAILED, ErrorHandler::get_last_error());
}

void test_environment_message_sending(void) {
    Environment& env = Environment::instance();
    SimpleTestAgent sender, receiver;
    
    AgentId sender_id = env.register_agent(&sender);
    AgentId receiver_id = env.register_agent(&receiver);
    
    // Send a message
    TestMessage test_msg = {123, 456};
    bool result = env.send_message(sender_id, receiver_id, test_msg);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(receiver.message_queue_.empty());
    
    // Process the message
    receiver.process_messages();
    
    // Verify message was received
    TEST_ASSERT_EQUAL(1, receiver.get_received_count());
    TEST_ASSERT_EQUAL(123, receiver.get_last_message().id);
    TEST_ASSERT_EQUAL(456, receiver.get_last_message().value);
    
    // Verify metrics
    TEST_ASSERT_EQUAL(1, SystemMetrics::get_messages_sent());
    TEST_ASSERT_EQUAL(1, SystemMetrics::get_messages_processed());
}

void test_environment_message_sending_invalid_target(void) {
    Environment& env = Environment::instance();
    SimpleTestAgent sender;
    
    AgentId sender_id = env.register_agent(&sender);
    AgentId invalid_target = 9999; // Assuming this is invalid
    
    TestMessage test_msg = {123, 456};
    bool result = env.send_message(sender_id, invalid_target, test_msg);
    
    TEST_ASSERT_FALSE(result);
    
    // Verify error was reported
    TEST_ASSERT_EQUAL(ErrorCode::AGENT_REGISTRATION_FAILED, ErrorHandler::get_last_error());
}

void test_environment_broadcast_message(void) {
    Environment& env = Environment::instance();
    BroadcastTestAgent sender, receiver1, receiver2, receiver3;
    
    AgentId sender_id = env.register_agent(&sender);
    AgentId receiver1_id = env.register_agent(&receiver1);
    AgentId receiver2_id = env.register_agent(&receiver2);
    AgentId receiver3_id = env.register_agent(&receiver3);
    
    // Send broadcast message
    TestMessage broadcast_msg = {999, 777};
    env.broadcast_message(sender_id, broadcast_msg);
    
    // Verify all receivers got the message (but not sender)
    TEST_ASSERT_FALSE(sender.message_queue_.empty() || sender.message_queue_.empty()); // Sender shouldn't receive
    TEST_ASSERT_FALSE(receiver1.message_queue_.empty());
    TEST_ASSERT_FALSE(receiver2.message_queue_.empty());
    TEST_ASSERT_FALSE(receiver3.message_queue_.empty());
    
    // Process messages
    receiver1.process_messages();
    receiver2.process_messages();
    receiver3.process_messages();
    
    // Verify all received the broadcast
    TEST_ASSERT_EQUAL(1, receiver1.get_broadcast_count());
    TEST_ASSERT_EQUAL(1, receiver2.get_broadcast_count());
    TEST_ASSERT_EQUAL(1, receiver3.get_broadcast_count());
    
    // Sender should not have received its own broadcast
    sender.process_messages();
    TEST_ASSERT_EQUAL(0, sender.get_broadcast_count());
}

void test_environment_process_all_messages(void) {
    Environment& env = Environment::instance();
    SimpleTestAgent agent1, agent2;
    
    AgentId id1 = env.register_agent(&agent1);
    AgentId id2 = env.register_agent(&agent2);
    
    // Send multiple messages
    for (uint32_t i = 0; i < 5; ++i) {
        TestMessage msg = {i, i * 10};
        env.send_message(id1, id2, msg);
        env.send_message(id2, id1, msg);
    }
    
    // Process all messages at once
    env.process_all_messages();
    
    // Both agents should have processed their messages
    TEST_ASSERT_EQUAL(5, agent1.get_received_count());
    TEST_ASSERT_EQUAL(5, agent2.get_received_count());
    
    // All queues should be empty
    TEST_ASSERT_TRUE(agent1.message_queue_.empty());
    TEST_ASSERT_TRUE(agent2.message_queue_.empty());
}

void test_environment_process_one_message(void) {
    Environment& env = Environment::instance();
    SimpleTestAgent agent;
    
    AgentId id = env.register_agent(&agent);
    
    // Send multiple messages
    for (uint32_t i = 0; i < 3; ++i) {
        TestMessage msg = {i, i};
        env.send_message(id, id, msg); // Self-send for testing
    }
    
    agent.reset(); // Reset counter
    
    // Process one message at a time
    bool result1 = env.process_one_message();
    TEST_ASSERT_TRUE(result1);
    TEST_ASSERT_EQUAL(1, agent.get_received_count());
    
    bool result2 = env.process_one_message();
    TEST_ASSERT_TRUE(result2);
    TEST_ASSERT_EQUAL(2, agent.get_received_count());
    
    bool result3 = env.process_one_message();
    TEST_ASSERT_TRUE(result3);
    TEST_ASSERT_EQUAL(3, agent.get_received_count());
    
    // No more messages to process
    bool result4 = env.process_one_message();
    TEST_ASSERT_FALSE(result4);
}

void test_environment_total_pending_messages(void) {
    Environment& env = Environment::instance();
    SimpleTestAgent agent1, agent2;
    
    AgentId id1 = env.register_agent(&agent1);
    AgentId id2 = env.register_agent(&agent2);
    
    // Send some messages without processing
    TestMessage msg = {1, 1};
    env.send_message(id1, id2, msg);
    env.send_message(id1, id2, msg);
    env.send_message(id2, id1, msg);
    
    // Should have 3 pending messages
    size_t pending = env.total_pending_messages();
    TEST_ASSERT_EQUAL(3, pending);
    
    // Process one message
    env.process_one_message();
    
    // Should have 2 pending messages
    pending = env.total_pending_messages();
    TEST_ASSERT_EQUAL(2, pending);
    
    // Process all remaining
    env.process_all_messages();
    
    // Should have 0 pending messages
    pending = env.total_pending_messages();
    TEST_ASSERT_EQUAL(0, pending);
}

void test_environment_oversized_message(void) {
    Environment& env = Environment::instance();
    SimpleTestAgent agent;
    
    AgentId id = env.register_agent(&agent);
    
    // Create an oversized message type
    struct OversizedMessage {
        uint8_t data[MINI_SO_MAX_MESSAGE_SIZE + 100]; // Too large
    };
    
    OversizedMessage big_msg;
    bool result = env.send_message(id, id, big_msg);
    
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(ErrorCode::MESSAGE_TOO_LARGE, ErrorHandler::get_last_error());
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_environment_singleton);
    RUN_TEST(test_environment_agent_registration);
    RUN_TEST(test_environment_invalid_agent_registration);
    RUN_TEST(test_environment_message_sending);
    RUN_TEST(test_environment_message_sending_invalid_target);
    RUN_TEST(test_environment_broadcast_message);
    RUN_TEST(test_environment_process_all_messages);
    RUN_TEST(test_environment_process_one_message);
    RUN_TEST(test_environment_total_pending_messages);
    RUN_TEST(test_environment_oversized_message);
    
    return UNITY_END();
}