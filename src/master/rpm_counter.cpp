#include "rpm_counter.h"
#include "../include/shared/config.h"
#include <Arduino.h>
#include <driver/pcnt.h>
#include <esp_log.h>

static const char* TAG = "RPM_COUNTER";

// =============================================================================
// Configuration
// =============================================================================

#define PCNT_HIGH_LIMIT     32767       // Max count before overflow
#define PCNT_LOW_LIMIT      0           // Min count (we only count up)
#define DEFAULT_FILTER_NS   1000        // 1us glitch filter
#define DEFAULT_STALL_MS    500         // Consider engine stalled after 500ms

// =============================================================================
// State
// =============================================================================

static struct {
    bool initialized;
    bool enabled;

    // PCNT configuration
    pcnt_unit_t unit;
    gpio_num_t pin;

    // Counting state
    volatile int32_t overflowCount;     // Accumulated from overflows
    int16_t lastCount;                  // Last raw count read
    uint32_t lastReadTime;              // micros() at last getRPM() call
    uint32_t lastPulseTime;             // micros() when last pulse detected
    uint32_t totalPulses;               // Total since enable
    uint32_t overflowEvents;            // Number of overflow events

    // Configuration
    uint16_t filterValue;               // APB clock cycles for filter
    uint32_t stallTimeoutMs;
} state = {
    .initialized = false,
    .enabled = false,
    .unit = PCNT_UNIT_0,
    .pin = (gpio_num_t)RPM_INPUT_PIN,
    .overflowCount = 0,
    .lastCount = 0,
    .lastReadTime = 0,
    .lastPulseTime = 0,
    .totalPulses = 0,
    .overflowEvents = 0,
    .filterValue = 80,                  // ~1us at 80MHz APB clock
    .stallTimeoutMs = DEFAULT_STALL_MS
};

// =============================================================================
// Overflow ISR
// =============================================================================

static void IRAM_ATTR pcntOverflowHandler(void* arg) {
    uint32_t status = 0;
    pcnt_get_event_status(state.unit, &status);

    if (status & PCNT_EVT_H_LIM) {
        state.overflowCount += PCNT_HIGH_LIMIT;
        state.overflowEvents++;
    }
}

// =============================================================================
// Public API
// =============================================================================

bool rpmCounterInit() {
    if (state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }

    ESP_LOGI(TAG, "Initializing RPM counter on GPIO %d, PCNT unit %d",
             state.pin, state.unit);

    state.initialized = true;
    state.enabled = false;

    return true;
}

void rpmCounterEnable() {
    if (!state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return;
    }

    if (state.enabled) {
        ESP_LOGW(TAG, "Already enabled");
        return;
    }

    ESP_LOGI(TAG, "Enabling RPM counter");

    // Configure PCNT
    pcnt_config_t config = {
        .pulse_gpio_num = state.pin,
        .ctrl_gpio_num = PCNT_PIN_NOT_USED,
        .lctrl_mode = PCNT_MODE_KEEP,
        .hctrl_mode = PCNT_MODE_KEEP,
        .pos_mode = PCNT_COUNT_INC,     // Count on rising edge
        .neg_mode = PCNT_COUNT_DIS,     // Ignore falling edge
        .counter_h_lim = PCNT_HIGH_LIMIT,
        .counter_l_lim = PCNT_LOW_LIMIT,
        .unit = state.unit,
        .channel = PCNT_CHANNEL_0,
    };

    esp_err_t err = pcnt_unit_config(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure PCNT: %s", esp_err_to_name(err));
        return;
    }

    // Configure glitch filter
    pcnt_set_filter_value(state.unit, state.filterValue);
    pcnt_filter_enable(state.unit);

    // Set up overflow interrupt
    pcnt_event_enable(state.unit, PCNT_EVT_H_LIM);

    // Install ISR service if not already done
    static bool isrServiceInstalled = false;
    if (!isrServiceInstalled) {
        err = pcnt_isr_service_install(0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
            return;
        }
        isrServiceInstalled = true;
    }

    pcnt_isr_handler_add(state.unit, pcntOverflowHandler, nullptr);

    // Reset state
    state.overflowCount = 0;
    state.lastCount = 0;
    state.lastReadTime = micros();
    state.lastPulseTime = 0;
    state.totalPulses = 0;
    state.overflowEvents = 0;

    // Initialize and start counter
    pcnt_counter_pause(state.unit);
    pcnt_counter_clear(state.unit);
    pcnt_counter_resume(state.unit);

    state.enabled = true;
    ESP_LOGI(TAG, "RPM counter enabled");
}

void rpmCounterDisable() {
    if (!state.enabled) {
        return;
    }

    ESP_LOGI(TAG, "Disabling RPM counter");

    // Stop counter
    pcnt_counter_pause(state.unit);

    // Remove ISR handler
    pcnt_isr_handler_remove(state.unit);

    // Disable events
    pcnt_event_disable(state.unit, PCNT_EVT_H_LIM);

    // Reset GPIO to input (releases PCNT control)
    gpio_reset_pin(state.pin);
    gpio_set_direction(state.pin, GPIO_MODE_INPUT);

    state.enabled = false;
    ESP_LOGI(TAG, "RPM counter disabled");
}

bool rpmCounterIsEnabled() {
    return state.enabled;
}

float rpmCounterGetRPM() {
    if (!state.enabled) {
        return 0.0f;
    }

    // Read current counter value
    int16_t currentCount;
    pcnt_get_counter_value(state.unit, &currentCount);
    unsigned long now = micros();

    // Calculate total count including overflows
    int32_t totalNow = state.overflowCount + currentCount;
    int32_t totalLast = state.lastCount;

    // Handle first read
    if (state.lastReadTime == 0) {
        state.lastCount = totalNow;
        state.lastReadTime = now;
        return 0.0f;
    }

    // Calculate pulses in this period
    int32_t pulses = totalNow - totalLast;
    if (pulses < 0) {
        // Counter wrapped unexpectedly, reset
        pulses = currentCount;
    }

    // Update last pulse time if we got pulses
    if (pulses > 0) {
        state.lastPulseTime = now;
        state.totalPulses += pulses;
    }

    // Calculate time elapsed
    unsigned long elapsed = now - state.lastReadTime;

    // Store for next call
    state.lastCount = totalNow;
    state.lastReadTime = now;

    // Check for stall condition
    if (state.lastPulseTime > 0) {
        uint32_t timeSincePulse = (now - state.lastPulseTime) / 1000; // to ms
        if (timeSincePulse > state.stallTimeoutMs) {
            return 0.0f;  // Engine stalled
        }
    }

    // Avoid division by zero
    if (elapsed == 0) {
        return 0.0f;
    }

    // RPM = (pulses / seconds) * 60
    // = pulses * 60,000,000 / elapsed_us
    return (pulses * 60000000.0f) / elapsed;
}

uint32_t rpmCounterGetPulseCount() {
    if (!state.enabled) {
        return 0;
    }

    int16_t currentCount;
    pcnt_get_counter_value(state.unit, &currentCount);
    return state.overflowCount + currentCount;
}

uint32_t rpmCounterGetTimeSinceLastPulse() {
    if (!state.enabled || state.lastPulseTime == 0) {
        return UINT32_MAX;
    }

    return (micros() - state.lastPulseTime) / 1000;  // Return in ms
}

void rpmCounterSetFilterNs(uint16_t nanoseconds) {
    // APB clock is typically 80MHz = 12.5ns per cycle
    // filterValue = nanoseconds / 12.5
    state.filterValue = (nanoseconds * 80) / 1000;

    if (state.enabled) {
        pcnt_set_filter_value(state.unit, state.filterValue);
    }

    ESP_LOGI(TAG, "Filter set to %u ns (%u APB cycles)", nanoseconds, state.filterValue);
}

void rpmCounterSetStallTimeoutMs(uint32_t ms) {
    state.stallTimeoutMs = ms;
    ESP_LOGI(TAG, "Stall timeout set to %u ms", ms);
}

uint32_t rpmCounterGetTotalPulses() {
    return state.totalPulses;
}

uint32_t rpmCounterGetOverflowCount() {
    return state.overflowEvents;
}
