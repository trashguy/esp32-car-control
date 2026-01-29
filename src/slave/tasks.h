#ifndef TASKS_H
#define TASKS_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdint.h>

// =============================================================================
// Task Configuration
// =============================================================================

// Task priorities (higher = more important)
#define TASK_PRIORITY_SPI_COMM    5   // Highest - must respond to master quickly
#define TASK_PRIORITY_DISPLAY     3   // Medium - UI responsiveness
#define TASK_PRIORITY_SERIAL      1   // Low - debug only

// Stack sizes (in words, not bytes - multiply by 4 for bytes)
#define TASK_STACK_SPI_COMM    4096
#define TASK_STACK_DISPLAY     8192   // Display needs more for TFT operations
#define TASK_STACK_SERIAL      2048

// Core assignments (ESP32-S3 has 2 cores: 0 and 1)
// Core 0: WiFi/BT stack runs here by default
// Core 1: Application tasks
#define TASK_CORE_SPI_COMM     1      // SPI on core 1 for deterministic timing
#define TASK_CORE_DISPLAY      1      // Display on core 1
#define TASK_CORE_SERIAL       0      // Serial can share core 0 with WiFi

// Queue sizes
#define QUEUE_SIZE_RPM_DATA    4      // RPM updates from SPI to display
#define QUEUE_SIZE_UI_CMD      4      // UI commands from display to SPI

// =============================================================================
// Inter-Task Communication Structures
// =============================================================================

// Message from SPI task to Display task
typedef struct {
    uint16_t rpm;
    uint8_t mode;
    bool connected;
    bool forceRefresh;  // True on reconnection
} SpiToDisplayMsg;

// Message from Display task to SPI task
typedef struct {
    uint8_t requestedMode;
    uint16_t requestedRpm;
} DisplayToSpiMsg;

// =============================================================================
// Global Synchronization Objects
// =============================================================================

// Queues for inter-task communication
extern QueueHandle_t queueSpiToDisplay;   // SPI -> Display: RPM/mode updates
extern QueueHandle_t queueDisplayToSpi;   // Display -> SPI: user commands

// Mutex for TFT access (TFT_eSPI is not thread-safe)
extern SemaphoreHandle_t mutexTft;

// Mutex for I2C/Wire access (touch controller)
extern SemaphoreHandle_t mutexI2C;

// =============================================================================
// Task Management Functions
// =============================================================================

// Initialize all FreeRTOS objects (queues, mutexes)
// Call before creating tasks
bool tasksInit();

// Create and start all tasks
// Call after hardware initialization (SD, display, SPI)
bool tasksStart();

// Get task handles (for debugging/monitoring)
TaskHandle_t getTaskSpiComm();
TaskHandle_t getTaskDisplay();
TaskHandle_t getTaskSerial();

// =============================================================================
// Thread-Safe TFT Access Macros
// =============================================================================

// Use these macros to safely access TFT from display task
// Timeout of 100ms should be plenty for any TFT operation to complete
#define TFT_LOCK()    xSemaphoreTake(mutexTft, pdMS_TO_TICKS(100))
#define TFT_UNLOCK()  xSemaphoreGive(mutexTft)

// Use these macros for I2C/touch access
#define I2C_LOCK()    xSemaphoreTake(mutexI2C, pdMS_TO_TICKS(50))
#define I2C_UNLOCK()  xSemaphoreGive(mutexI2C)

#endif // TASKS_H
