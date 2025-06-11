#include <unity.h>
#include "mini_sobjectizer/mini_sobjectizer.h"
#include <cstring>

using namespace mini_so;

// Mock FreeRTOS functions for testing
extern "C" {
    SemaphoreHandle_t xSemaphoreCreateMutex(void) {
        return (SemaphoreHandle_t)1; // Return non-null pointer for testing
    }
    
    BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
        (void)xSemaphore;
        (void)xTicksToWait;
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
    
    void taskDISABLE_INTERRUPTS(void) {
        // Mock implementation
    }
}

// Test message structure
struct TestMessage {
    uint32_t value;
    uint16_t id;
};

void setUp(void) {
    // Reset error state before each test
    ErrorHandler::reset_error_state();
    SystemMetrics::reset_metrics();
}

void tearDown(void) {
    // Clean up after each test
}

void test_message_queue_creation(void) {
    MessageQueue queue;
    
    TEST_ASSERT_TRUE(queue.empty());
    TEST_ASSERT_FALSE(queue.full());
    TEST_ASSERT_EQUAL(0, queue.size());
}

void test_message_queue_push_pop(void) {
    MessageQueue queue;
    TestMessage test_msg = {42, 123};
    Message<TestMessage> msg(test_msg);
    
    // Test successful push
    auto result = queue.push(msg, sizeof(Message<TestMessage>));
    TEST_ASSERT_EQUAL(MessageQueue::PushResult::SUCCESS, result);
    TEST_ASSERT_FALSE(queue.empty());
    TEST_ASSERT_EQUAL(1, queue.size());
    
    // Test successful pop
    uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    MessageSize size;
    bool pop_result = queue.pop(buffer, size);
    
    TEST_ASSERT_TRUE(pop_result);
    TEST_ASSERT_EQUAL(sizeof(Message<TestMessage>), size);
    TEST_ASSERT_TRUE(queue.empty());
    TEST_ASSERT_EQUAL(0, queue.size());
    
    // Verify message content
    const Message<TestMessage>* retrieved_msg = 
        reinterpret_cast<const Message<TestMessage>*>(buffer);
    TEST_ASSERT_EQUAL(42, retrieved_msg->data.value);
    TEST_ASSERT_EQUAL(123, retrieved_msg->data.id);
}

void test_message_queue_oversized_message(void) {
    MessageQueue queue;
    TestMessage test_msg = {42, 123};
    Message<TestMessage> msg(test_msg);
    
    // Test with oversized message
    auto result = queue.push(msg, MINI_SO_MAX_MESSAGE_SIZE + 1);
    TEST_ASSERT_EQUAL(MessageQueue::PushResult::MESSAGE_TOO_LARGE, result);
    TEST_ASSERT_TRUE(queue.empty());
    
    // Verify error was reported
    TEST_ASSERT_EQUAL(ErrorCode::MESSAGE_TOO_LARGE, ErrorHandler::get_last_error());
    TEST_ASSERT_EQUAL(1, ErrorHandler::get_error_count());
}

void test_message_queue_undersized_message(void) {
    MessageQueue queue;
    TestMessage test_msg = {42, 123};
    Message<TestMessage> msg(test_msg);
    
    // Test with undersized message (smaller than MessageBase)
    auto result = queue.push(msg, sizeof(MessageBase) - 1);
    TEST_ASSERT_EQUAL(MessageQueue::PushResult::INVALID_MESSAGE, result);
    TEST_ASSERT_TRUE(queue.empty());
    
    // Verify error was reported
    TEST_ASSERT_EQUAL(ErrorCode::INVALID_MESSAGE, ErrorHandler::get_last_error());
}

void test_message_queue_full_condition(void) {
    MessageQueue queue;
    TestMessage test_msg = {42, 123};
    Message<TestMessage> msg(test_msg);
    
    // Fill the queue to capacity
    for (size_t i = 0; i < MINI_SO_MAX_QUEUE_SIZE; ++i) {
        auto result = queue.push(msg, sizeof(Message<TestMessage>));
        TEST_ASSERT_EQUAL(MessageQueue::PushResult::SUCCESS, result);
    }
    
    TEST_ASSERT_TRUE(queue.full());
    TEST_ASSERT_EQUAL(MINI_SO_MAX_QUEUE_SIZE, queue.size());
    
    // Try to push one more message - should fail
    auto result = queue.push(msg, sizeof(Message<TestMessage>));
    TEST_ASSERT_EQUAL(MessageQueue::PushResult::QUEUE_FULL, result);
    
    // Verify error was reported
    TEST_ASSERT_EQUAL(ErrorCode::QUEUE_OVERFLOW, ErrorHandler::get_last_error());
}

void test_message_queue_clear(void) {
    MessageQueue queue;
    TestMessage test_msg = {42, 123};
    Message<TestMessage> msg(test_msg);
    
    // Add some messages
    for (int i = 0; i < 5; ++i) {
        queue.push(msg, sizeof(Message<TestMessage>));
    }
    
    TEST_ASSERT_EQUAL(5, queue.size());
    TEST_ASSERT_FALSE(queue.empty());
    
    // Clear the queue
    queue.clear();
    
    TEST_ASSERT_EQUAL(0, queue.size());
    TEST_ASSERT_TRUE(queue.empty());
    TEST_ASSERT_FALSE(queue.full());
}

void test_message_type_id_generation(void) {
    // Test that different message types get different IDs
    MessageId id1 = MESSAGE_TYPE_ID(TestMessage);
    MessageId id2 = MESSAGE_TYPE_ID(uint32_t);
    MessageId id3 = MESSAGE_TYPE_ID(TestMessage); // Should be same as id1
    
    TEST_ASSERT_NOT_EQUAL(id1, id2);
    TEST_ASSERT_EQUAL(id1, id3);
}

void test_metrics_tracking(void) {
    MessageQueue queue;
    TestMessage test_msg = {42, 123};
    Message<TestMessage> msg(test_msg);
    
    // Reset metrics
    SystemMetrics::reset_metrics();
    TEST_ASSERT_EQUAL(0, SystemMetrics::get_max_queue_depth());
    
    // Push messages and verify metrics
    queue.push(msg, sizeof(Message<TestMessage>));
    TEST_ASSERT_EQUAL(1, SystemMetrics::get_max_queue_depth());
    
    queue.push(msg, sizeof(Message<TestMessage>));
    TEST_ASSERT_EQUAL(2, SystemMetrics::get_max_queue_depth());
    
    // Pop one message - max depth should remain
    uint8_t buffer[MINI_SO_MAX_MESSAGE_SIZE];
    MessageSize size;
    queue.pop(buffer, size);
    TEST_ASSERT_EQUAL(2, SystemMetrics::get_max_queue_depth()); // Max should not decrease
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_message_queue_creation);
    RUN_TEST(test_message_queue_push_pop);
    RUN_TEST(test_message_queue_oversized_message);
    RUN_TEST(test_message_queue_undersized_message);
    RUN_TEST(test_message_queue_full_condition);
    RUN_TEST(test_message_queue_clear);
    RUN_TEST(test_message_type_id_generation);
    RUN_TEST(test_metrics_tracking);
    
    return UNITY_END();
}