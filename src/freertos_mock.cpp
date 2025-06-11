// freertos_mock.cpp - Mock FreeRTOS functions for host platform testing
#include <cstdlib>
#include <cstdio>
#include <cstdint>

#ifdef UNIT_TEST

extern "C" {

// Mock FreeRTOS types and constants
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TaskHandle_t;
typedef unsigned int TickType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;

#define portMAX_DELAY 0xFFFFFFFF
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(x) (x)
#define configTICK_RATE_HZ 1000
#define configMINIMAL_STACK_SIZE 128

// Mock FreeRTOS functions
SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    // Return a dummy non-null pointer
    return (SemaphoreHandle_t)0x12345678;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
    (void)xSemaphore;
    (void)xTicksToWait;
    // Always succeed in mock
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
    (void)xSemaphore;
    // Always succeed in mock
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore) {
    (void)xSemaphore;
    // Nothing to do in mock
}

TickType_t xTaskGetTickCount(void) {
    // Simple mock implementation - return increasing ticks
    static TickType_t tick_count = 1000;
    return tick_count += 10;
}

void taskDISABLE_INTERRUPTS(void) {
    // Nothing to do in mock - interrupts don't exist on host
}

void vTaskDelay(TickType_t xTicksToDelay) {
    (void)xTicksToDelay;
    // In real implementation this would yield to other tasks
    // In mock, we do nothing
}

// Additional FreeRTOS task creation mocks (not used in current test but might be needed)
BaseType_t xTaskCreate(void* pxTaskCode, const char* pcName, 
                      uint16_t usStackDepth, void* pvParameters,
                      UBaseType_t uxPriority, TaskHandle_t* pxCreatedTask) {
    (void)pxTaskCode;
    (void)pcName;
    (void)usStackDepth;
    (void)pvParameters;
    (void)uxPriority;
    
    if (pxCreatedTask) {
        *pxCreatedTask = (TaskHandle_t)0x87654321;
    }
    return pdTRUE;
}

void vTaskStartScheduler(void) {
    // In mock, we don't actually start a scheduler
    printf("Mock: vTaskStartScheduler called (no-op in test)\n");
}

// FreeRTOS hook functions (no-op in mock)
void vApplicationMallocFailedHook(void) {
    printf("Mock: Malloc failed hook called\n");
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    printf("Mock: Stack overflow hook called\n");
}

void vApplicationIdleHook(void) {
    // Idle hook - no-op in mock
}

void vApplicationTickHook(void) {
    // Tick hook - no-op in mock
}

} // extern "C"

#endif // UNIT_TEST