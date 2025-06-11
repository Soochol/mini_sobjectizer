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
    uint32_t value;
    const char* text;
};

struct StatusRequest {
    uint32_t request_id;
};

// Test Agent implementation
class TestAgent : public Agent {
private:
    uint32_t message_count_ = 0;
    TestMessage last_received_message_ = {0, nullptr};
    bool status_request_received_ = false;
    
public:
    bool so_handle_message(const MessageBase& msg) override {
        message_count_++;
        
        if (msg.type_id == MESSAGE_TYPE_ID(TestMessage)) {
            auto& typed_msg = static_cast<const Message<TestMessage>&>(msg);
            last_received_message_ = typed_msg.data;
            return true;
        }
        
        if (msg.type_id == MESSAGE_TYPE_ID(StatusRequest)) {
            status_request_received_ = true;
            return true;
        }
        
        return false;
    }
    
    // Test accessors
    uint32_t get_message_count() const { return message_count_; }
    const TestMessage& get_last_message() const { return last_received_message_; }
    bool got_status_request() const { return status_request_received_; }
    
    void reset() {
        message_count_ = 0;
        last_received_message_ = {0, nullptr};
        status_request_received_ = false;
    }
};

// Test State Machine Agent
class StateMachineTestAgent : public Agent {
private:
    StateId idle_state_ = INVALID_STATE_ID;
    StateId working_state_ = INVALID_STATE_ID;
    StateId error_state_ = INVALID_STATE_ID;
    
    uint32_t state_enter_count_ = 0;
    uint32_t state_exit_count_ = 0;
    
public:
    void so_define_states() override {
        idle_state_ = define_state("Idle");
        working_state_ = define_state("Working");
        error_state_ = define_state("Error");
        
        // Set up state callbacks
        on_state_enter(idle_state_, [this]() { state_enter_count_++; });
        on_state_exit(idle_state_, [this]() { state_exit_count_++; });
    }
    
    bool so_handle_message(const MessageBase& msg) override {
        if (msg.type_id == MESSAGE_TYPE_ID(TestMessage)) {
            auto& typed_msg = static_cast<const Message<TestMessage>&>(msg);
            
            if (typed_msg.data.value == 1) {
                transition_to(working_state_);
            } else if (typed_msg.data.value == 999) {
                transition_to(error_state_);
            }
            return true;
        }
        return false;
    }
    
    // Test accessors
    StateId get_idle_state() const { return idle_state_; }
    StateId get_working_state() const { return working_state_; }
    StateId get_error_state() const { return error_state_; }
    uint32_t get_enter_count() const { return state_enter_count_; }
    uint32_t get_exit_count() const { return state_exit_count_; }
};

void setUp(void) {
    ErrorHandler::reset_error_state();
    SystemMetrics::reset_metrics();
}

void tearDown(void) {
    // Clean up
}

void test_agent_initialization(void) {
    TestAgent agent;
    
    // Test initial state
    TEST_ASSERT_EQUAL(INVALID_AGENT_ID, agent.id());
    TEST_ASSERT_TRUE(agent.message_queue_.empty());
    
    // Initialize agent
    agent.initialize(5);
    TEST_ASSERT_EQUAL(5, agent.id());
}

void test_agent_message_handling(void) {
    TestAgent agent;
    agent.initialize(1);
    
    TestMessage test_msg = {42, "test"};
    Message<TestMessage> msg(test_msg);
    
    // Push message to agent's queue
    auto result = agent.message_queue_.push(msg, sizeof(Message<TestMessage>));
    TEST_ASSERT_EQUAL(MessageQueue::PushResult::SUCCESS, result);
    TEST_ASSERT_FALSE(agent.message_queue_.empty());
    
    // Process messages
    agent.process_messages();
    
    // Verify message was handled
    TEST_ASSERT_EQUAL(1, agent.get_message_count());
    TEST_ASSERT_EQUAL(42, agent.get_last_message().value);
    TEST_ASSERT_TRUE(agent.message_queue_.empty());
    
    // Verify metrics
    TEST_ASSERT_EQUAL(1, SystemMetrics::get_messages_processed());
}

void test_agent_multiple_message_types(void) {
    TestAgent agent;
    agent.initialize(1);
    
    // Send TestMessage
    TestMessage test_msg = {100, "hello"};
    Message<TestMessage> msg1(test_msg);
    agent.message_queue_.push(msg1, sizeof(Message<TestMessage>));
    
    // Send StatusRequest
    StatusRequest status_req = {123};
    Message<StatusRequest> msg2(status_req);
    agent.message_queue_.push(msg2, sizeof(Message<StatusRequest>));
    
    // Process both messages
    agent.process_messages();
    
    // Verify both were handled
    TEST_ASSERT_EQUAL(2, agent.get_message_count());
    TEST_ASSERT_EQUAL(100, agent.get_last_message().value);
    TEST_ASSERT_TRUE(agent.got_status_request());
}

void test_agent_state_machine_basic(void) {
    StateMachineTestAgent agent;
    agent.initialize(1);
    
    // Verify states were defined
    TEST_ASSERT_NOT_EQUAL(INVALID_STATE_ID, agent.get_idle_state());
    TEST_ASSERT_NOT_EQUAL(INVALID_STATE_ID, agent.get_working_state());
    TEST_ASSERT_NOT_EQUAL(INVALID_STATE_ID, agent.get_error_state());
    
    // Should start in idle state (initial state is 0)
    TEST_ASSERT_EQUAL(0, agent.current_state());
}

void test_agent_state_transitions(void) {
    StateMachineTestAgent agent;
    agent.initialize(1);
    
    StateId initial_state = agent.current_state();
    
    // Transition to working state via message
    TestMessage work_msg = {1, "start_work"};
    Message<TestMessage> msg(work_msg);
    agent.message_queue_.push(msg, sizeof(Message<TestMessage>));
    agent.process_messages();
    
    // Verify state changed
    TEST_ASSERT_EQUAL(agent.get_working_state(), agent.current_state());
    TEST_ASSERT_EQUAL(initial_state, agent.previous_state());
    
    // Verify state callbacks were called
    TEST_ASSERT_EQUAL(1, agent.get_enter_count()); // Enter callback for new state
    TEST_ASSERT_EQUAL(1, agent.get_exit_count());  // Exit callback for old state
}

void test_agent_state_transition_to_error(void) {
    StateMachineTestAgent agent;
    agent.initialize(1);
    
    // Send error message
    TestMessage error_msg = {999, "error"};
    Message<TestMessage> msg(error_msg);
    agent.message_queue_.push(msg, sizeof(Message<TestMessage>));
    agent.process_messages();
    
    // Verify transitioned to error state
    TEST_ASSERT_EQUAL(agent.get_error_state(), agent.current_state());
}

void test_agent_in_state_check(void) {
    StateMachineTestAgent agent;
    agent.initialize(1);
    
    // Initially should be in state 0 (initial state)
    TEST_ASSERT_TRUE(agent.in_state(0));
    TEST_ASSERT_FALSE(agent.in_state(agent.get_working_state()));
    
    // Transition to working state
    TestMessage work_msg = {1, "work"};
    Message<TestMessage> msg(work_msg);
    agent.message_queue_.push(msg, sizeof(Message<TestMessage>));
    agent.process_messages();
    
    // Now should be in working state
    TEST_ASSERT_TRUE(agent.in_state(agent.get_working_state()));
    TEST_ASSERT_FALSE(agent.in_state(0));
}

void test_agent_invalid_message_handling(void) {
    TestAgent agent;
    agent.initialize(1);
    
    // Create a message with unknown type
    struct UnknownMessage { int x; };
    Message<UnknownMessage> unknown_msg({42});
    
    agent.message_queue_.push(unknown_msg, sizeof(Message<UnknownMessage>));
    agent.process_messages();
    
    // Agent should have processed but not handled the unknown message
    TEST_ASSERT_EQUAL(1, SystemMetrics::get_messages_processed());
    TEST_ASSERT_EQUAL(0, agent.get_message_count()); // Agent didn't handle it
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_agent_initialization);
    RUN_TEST(test_agent_message_handling);
    RUN_TEST(test_agent_multiple_message_types);
    RUN_TEST(test_agent_state_machine_basic);
    RUN_TEST(test_agent_state_transitions);
    RUN_TEST(test_agent_state_transition_to_error);
    RUN_TEST(test_agent_in_state_check);
    RUN_TEST(test_agent_invalid_message_handling);
    
    return UNITY_END();
}