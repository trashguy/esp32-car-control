#ifndef MASTER_TASKS_H
#define MASTER_TASKS_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <stdint.h>

// =============================================================================
// Task Configuration
// =============================================================================

// Task priorities (higher = more important)
// CRITICAL: Pump control must be highest priority for safety
#define TASK_PRIORITY_PUMP      10    // Highest - safety critical PWM control
#define TASK_PRIORITY_SPI_COMM  5     // High - slave communication
#define TASK_PRIORITY_UI        3     // Medium - encoder and serial
#define TASK_PRIORITY_NVS       1     // Low - settings persistence

// Stack sizes (in words, not bytes)
#define TASK_STACK_PUMP      4096
#define TASK_STACK_SPI_COMM  4096
#define TASK_STACK_UI        4096
#define TASK_STACK_NVS       2048

// Core assignments (ESP32-S3 has 2 cores)
// Core 0: WiFi/BT stack, lower priority tasks
// Core 1: Time-critical tasks
#define TASK_CORE_PUMP      1     // Pump control on Core 1 for deterministic timing
#define TASK_CORE_SPI_COMM  0     // SPI on Core 0
#define TASK_CORE_UI        1     // UI on Core 1 (encoder needs fast response)
#define TASK_CORE_NVS       0     // NVS on Core 0 (flash operations)

// Queue sizes
#define QUEUE_SIZE_SLAVE_CMD    4     // Commands from slave UI
#define QUEUE_SIZE_RPM_UPDATE   4     // RPM updates for SPI task
#define QUEUE_SIZE_NVS_SAVE     2     // NVS save requests

// =============================================================================
// Timing Configuration
// =============================================================================

#define PUMP_TASK_PERIOD_MS     10    // 100Hz pump control
#define SPI_TASK_PERIOD_MS      100   // 10Hz SPI communication
#define UI_TASK_PERIOD_MS       20    // 50Hz encoder polling
#define NVS_TASK_PERIOD_MS      1000  // 1Hz NVS check

// Safety thresholds
#define SPI_COMM_TIMEOUT_MS     500   // Enter failsafe after this
#define NVS_SAVE_DEBOUNCE_MS    3000  // Wait this long before saving
#define WDT_TIMEOUT_SEC         2     // Watchdog timeout

// Failsafe PWM duty (0-255) - ~78% duty for safe pump speed
#define FAILSAFE_PWM_DUTY       200

// =============================================================================
// System Health States
// =============================================================================

typedef enum {
    HEALTH_OK,           // All systems nominal
    HEALTH_SPI_TIMEOUT,  // SPI communication lost
    HEALTH_CAN_ERROR,    // CAN bus errors
    HEALTH_FAILSAFE      // Running in failsafe mode
} SystemHealth;

// =============================================================================
// Operating Modes
// =============================================================================

typedef enum {
    OP_MODE_SNIFF,       // Log all CAN messages
    OP_MODE_RPM,         // Extract RPM from CAN
    OP_MODE_SIMULATE     // Generate simulated RPM
} OperatingMode;

// =============================================================================
// Inter-Task Message Structures
// =============================================================================

// Message from UI/Slave to update mode/RPM
typedef struct {
    uint8_t mode;        // MODE_AUTO or MODE_MANUAL
    uint16_t rpm;        // Manual RPM value
    bool modeChanged;    // True if mode was changed
    bool rpmChanged;     // True if RPM was changed
} SettingsUpdateMsg;

// Message to request NVS save
typedef struct {
    uint8_t mode;
    uint16_t manualRpm;
} NvsSaveRequest;

// =============================================================================
// Shared State Structure (Protected by Mutex)
// =============================================================================

typedef struct {
    // Current values (master is source of truth)
    uint16_t currentRpm;
    uint8_t displayMode;      // MODE_AUTO or MODE_MANUAL
    uint16_t manualRpm;
    OperatingMode opMode;

    // System health
    SystemHealth health;
    uint32_t lastValidSpiTime;
    uint32_t spiTimeoutCount;

    // PWM state
    uint8_t currentPwmDuty;

    // Simulation state
    bool simGoingUp;
    uint32_t lastSimChange;
} MasterState;

// =============================================================================
// Global Synchronization Objects
// =============================================================================

// Mutex for shared state access
extern SemaphoreHandle_t stateMutex;

// Queues
extern QueueHandle_t queueSlaveCmd;      // Commands received from slave
extern QueueHandle_t queueNvsSave;       // NVS save requests

// Shared state (access with mutex)
extern MasterState masterState;

// =============================================================================
// Task Management Functions
// =============================================================================

// Initialize FreeRTOS objects (queues, mutexes)
bool tasksInit();

// Create and start all tasks
bool tasksStart();

// Get task handles for monitoring
TaskHandle_t getTaskPump();
TaskHandle_t getTaskSpiComm();
TaskHandle_t getTaskUi();
TaskHandle_t getTaskNvs();

// =============================================================================
// State Access Functions (Thread-Safe)
// =============================================================================

// Get current RPM (for pump task)
uint16_t tasksGetCurrentRpm();

// Get display mode
uint8_t tasksGetDisplayMode();

// Get manual RPM
uint16_t tasksGetManualRpm();

// Get system health
SystemHealth tasksGetHealth();

// Update RPM (from simulation or CAN)
void tasksSetCurrentRpm(uint16_t rpm);

// Update mode (from encoder or slave request)
void tasksSetDisplayMode(uint8_t mode);

// Update manual RPM (from encoder or slave request)
void tasksSetManualRpm(uint16_t rpm);

// Request NVS save (debounced)
void tasksRequestNvsSave();

// Enter/exit failsafe mode
void tasksEnterFailsafe(const char* reason);
void tasksExitFailsafe();

// =============================================================================
// Convenience Macros
// =============================================================================

#define STATE_LOCK()    xSemaphoreTake(stateMutex, pdMS_TO_TICKS(10))
#define STATE_UNLOCK()  xSemaphoreGive(stateMutex)

#endif // MASTER_TASKS_H
