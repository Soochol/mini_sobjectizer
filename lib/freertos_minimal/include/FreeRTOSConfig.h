/*
 * FreeRTOS Configuration for Mini-SObjectizer v2.0
 * STM32F103RC Configuration (Cortex-M3)
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Ensure stdint is only used by the compiler, and not the assembler. */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
 #include <stdint.h>
 #ifdef __cplusplus
 extern "C" {
 #endif
 extern uint32_t SystemCoreClock;
 #ifdef __cplusplus
 }
 #endif
#endif

/* FreeRTOS v11+ compatibility - Only define one */
#define configUSE_16_BIT_TICKS            0

/* ============================================================================
 * Basic FreeRTOS Configuration for STM32F103RC (Cortex-M3)
 * ============================================================================ */

#define configUSE_PREEMPTION              1
#define configUSE_IDLE_HOOK               0
#define configUSE_TICK_HOOK               0
#define configCPU_CLOCK_HZ                (SystemCoreClock)
#define configTICK_RATE_HZ                ((TickType_t)1000)
#define configMAX_PRIORITIES              (7)
#define configMINIMAL_STACK_SIZE          ((uint16_t)128)
#define configTOTAL_HEAP_SIZE             ((size_t)(16 * 1024))  // 16KB for STM32F103RC
#define configMAX_TASK_NAME_LEN           (16)
#define configUSE_TRACE_FACILITY          1
#define configIDLE_SHOULD_YIELD           1

/* ============================================================================
 * Memory Management - Use heap_4 for ARM Cortex-M3
 * ============================================================================ */

#define configSUPPORT_STATIC_ALLOCATION   1    // Enable static allocation
#define configSUPPORT_DYNAMIC_ALLOCATION  1    // Also enable dynamic for flexibility
#define configUSE_MALLOC_FAILED_HOOK      1    // Enable malloc failed hook

/* ============================================================================
 * Synchronization Primitives
 * ============================================================================ */

#define configUSE_MUTEXES                 1
#define configUSE_RECURSIVE_MUTEXES       1
#define configUSE_COUNTING_SEMAPHORES     1
#define configQUEUE_REGISTRY_SIZE         8

/* ============================================================================
 * Software Timers
 * ============================================================================ */

#define configUSE_TIMERS                  1
#define configTIMER_TASK_PRIORITY         (2)
#define configTIMER_QUEUE_LENGTH          10
#define configTIMER_TASK_STACK_DEPTH      (configMINIMAL_STACK_SIZE * 2)

/* ============================================================================
 * Co-routines (typically not used in modern applications)
 * ============================================================================ */

#define configUSE_CO_ROUTINES             0
#define configMAX_CO_ROUTINE_PRIORITIES   (2)

/* ============================================================================
 * Safety and Debug Features for STM32F103RC
 * ============================================================================ */

#define configCHECK_FOR_STACK_OVERFLOW    2    // Enable stack overflow checking
#define configUSE_APPLICATION_TASK_TAG    0
#define configGENERATE_RUN_TIME_STATS     0    // Disable for reduced overhead

/* ============================================================================
 * API Function Inclusions
 * ============================================================================ */

#define INCLUDE_vTaskPrioritySet          1
#define INCLUDE_uxTaskPriorityGet         1
#define INCLUDE_vTaskDelete               1
#define INCLUDE_vTaskCleanUpResources     0
#define INCLUDE_vTaskSuspend              1
#define INCLUDE_vTaskDelayUntil           1
#define INCLUDE_vTaskDelay                1
#define INCLUDE_xTaskGetSchedulerState    1
#define INCLUDE_xTimerPendFunctionCall    1
#define INCLUDE_xTaskGetCurrentTaskHandle 1
#define INCLUDE_uxTaskGetStackHighWaterMark 1
#define INCLUDE_xTaskGetIdleTaskHandle    1

/* ============================================================================
 * Cortex-M3 specific definitions for STM32F103RC
 * ============================================================================ */

#ifdef __NVIC_PRIO_BITS
 #define configPRIO_BITS         __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS        4        /* Cortex-M3: 4 bits = 16 priority levels */
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority" function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY   0xf

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Interrupt priorities used by the kernel port layer itself.  These are generic
to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY   ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

/* ============================================================================
 * Mini-SObjectizer Specific Configuration
 * ============================================================================ */

/* Assert definition for Mini-SObjectizer integration */
#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

/* Cortex-M3 specific optimizations for STM32F103RC */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0  // Use generic selection for CM3

/* Hook function declarations with proper C linkage */
#ifdef __cplusplus
extern "C" {
#endif

#if configUSE_MALLOC_FAILED_HOOK == 1
void vApplicationMallocFailedHook( void );
#endif

#if configCHECK_FOR_STACK_OVERFLOW > 0
/* Forward declaration - will be properly defined after task.h is included */
struct tskTaskControlBlock;
typedef struct tskTaskControlBlock* TaskHandle_t;
void vApplicationStackOverflowHook( TaskHandle_t xTask, char *pcTaskName );
#endif

#if configUSE_IDLE_HOOK == 1
void vApplicationIdleHook( void );
#endif

#if configUSE_TICK_HOOK == 1
void vApplicationTickHook( void );
#endif

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * STM32F103RC HAL Integration
 * ============================================================================ */

/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS standard names. */
#define vPortSVCHandler    SVC_Handler
#define xPortPendSVHandler PendSV_Handler

/* IMPORTANT: FreeRTOS is using the SysTick as internal time base, thus make sure the system and peripherals are
              using a different time base (TIM based for example). */
#define xPortSysTickHandler SysTick_Handler

/* Use the system definition, if there is one */
#ifdef __NVIC_PRIO_BITS
 #define configPRIO_BITS       __NVIC_PRIO_BITS
#else
 #define configPRIO_BITS       4
#endif

#endif /* FREERTOS_CONFIG_H */