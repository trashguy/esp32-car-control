#include "water_temp.h"
#include "../include/shared/config.h"
#include <Arduino.h>
#include <esp_adc_cal.h>
#include <driver/adc.h>
#include <esp_log.h>
#include <math.h>

static const char* TAG = "WATER_TEMP";

// =============================================================================
// GM LS1 Coolant Temperature Sensor Characteristics
// =============================================================================
// The GM LS1 uses a negative temperature coefficient (NTC) thermistor.
// Resistance decreases as temperature increases.
//
// Lookup table derived from GM LS1/LS6 coolant temperature sensor specs.
// These are typical values - actual sensor may vary slightly.
//
// Reference: GM Part Numbers 12146312, 213-928, 15326388
// =============================================================================

// Lookup table: {resistance_ohms, temperature_fahrenheit}
// Sorted by resistance (high to low = cold to hot)
// Using Fahrenheit as primary since this is for American muscle cars!
static const struct {
    uint32_t resistance;  // Ohms
    int16_t tempF;        // Fahrenheit * 10 (for 0.1°F resolution)
} GM_LS1_NTC_TABLE[] = {
    // Cold temperatures
    {100700,  -400},   // -40°F (very cold - same as -40°C!)
    {52300,   -220},   // -22°F (-30°C)
    {27300,    -40},   //  -4°F (-20°C)
    {16000,    140},   //  14°F (-10°C)
    {9500,     320},   //  32°F (0°C, freezing)
    {5900,     500},   //  50°F (10°C)
    {3800,     680},   //  68°F (20°C, room temp)
    {2500,     860},   //  86°F (30°C)
    {1700,    1040},   // 104°F (40°C)
    
    // Normal operating range
    {1180,    1220},   // 122°F (50°C)
    {840,     1400},   // 140°F (60°C)
    {600,     1580},   // 158°F (70°C)
    {440,     1760},   // 176°F (80°C)
    {325,     1940},   // 194°F (90°C) - normal operating temp
    {245,     2120},   // 212°F (100°C) - thermostat fully open
    {185,     2300},   // 230°F (110°C) - getting warm
    {145,     2480},   // 248°F (120°C) - hot!
    
    // Overheat range - time to pull over!
    {112,     2660},   // 266°F (130°C) - overheating!
    {90,      2840},   // 284°F (140°C) - critical!
    {72,      3020},   // 302°F (150°C) - danger! engine damage imminent!
};

#define NTC_TABLE_SIZE (sizeof(GM_LS1_NTC_TABLE) / sizeof(GM_LS1_NTC_TABLE[0]))

// Open circuit / short circuit thresholds
#define OPEN_CIRCUIT_RESISTANCE    200000   // > 200K ohms = open (no sensor)
#define SHORT_CIRCUIT_RESISTANCE   50       // < 50 ohms = shorted

// ADC characteristics
#define ADC_MAX_VALUE           4095        // 12-bit ADC
#define ADC_REFERENCE_VOLTAGE   3300        // 3.3V in millivolts

// =============================================================================
// State
// =============================================================================

static struct {
    bool initialized;
    bool enabled;
    
    // ADC configuration
    adc1_channel_t channel;
    esp_adc_cal_characteristics_t adcChars;
    
    // Averaging
    uint8_t avgSamples;
    uint16_t sampleBuffer[64];
    uint8_t sampleIndex;
    bool bufferFilled;
    
    // Timing
    uint16_t updateRateMs;
    uint32_t lastReadTime;
    
    // Cached values
    uint16_t lastRawAdc;
    float lastVoltage;
    float lastResistance;
    float lastTempF;
    
    // Calibration
    float offsetF;
    
    // Statistics
    uint32_t readCount;
    uint32_t errorCount;
    
    // Sensor status
    bool sensorConnected;
    bool sensorShorted;
} state = {
    .initialized = false,
    .enabled = false,
    .channel = ADC1_CHANNEL_3,  // GPIO4 = ADC1_CH3
    .avgSamples = 16,
    .sampleIndex = 0,
    .bufferFilled = false,
    .updateRateMs = 100,
    .lastReadTime = 0,
    .lastRawAdc = 0,
    .lastVoltage = 0.0f,
    .lastResistance = 0.0f,
    .lastTempF = NAN,
    .offsetF = 0.0f,
    .readCount = 0,
    .errorCount = 0,
    .sensorConnected = false,
    .sensorShorted = false
};

// =============================================================================
// Internal Functions
// =============================================================================

// Interpolate temperature from resistance using lookup table
// Returns temperature in Fahrenheit
static float resistanceToTempF(float resistance) {
    // Check bounds
    if (resistance >= GM_LS1_NTC_TABLE[0].resistance) {
        // Colder than table minimum
        return GM_LS1_NTC_TABLE[0].tempF / 10.0f;
    }
    if (resistance <= GM_LS1_NTC_TABLE[NTC_TABLE_SIZE - 1].resistance) {
        // Hotter than table maximum
        return GM_LS1_NTC_TABLE[NTC_TABLE_SIZE - 1].tempF / 10.0f;
    }
    
    // Find the two table entries to interpolate between
    for (size_t i = 0; i < NTC_TABLE_SIZE - 1; i++) {
        if (resistance <= GM_LS1_NTC_TABLE[i].resistance && 
            resistance > GM_LS1_NTC_TABLE[i + 1].resistance) {
            
            // Linear interpolation
            float r1 = GM_LS1_NTC_TABLE[i].resistance;
            float r2 = GM_LS1_NTC_TABLE[i + 1].resistance;
            float t1 = GM_LS1_NTC_TABLE[i].tempF / 10.0f;
            float t2 = GM_LS1_NTC_TABLE[i + 1].tempF / 10.0f;
            
            // Interpolate based on resistance ratio
            float ratio = (r1 - resistance) / (r1 - r2);
            return t1 + ratio * (t2 - t1);
        }
    }
    
    // Should not reach here
    return NAN;
}

// Convert Fahrenheit to Celsius
static inline float fahrenheitToCelsius(float fahrenheit) {
    return (fahrenheit - 32.0f) * 5.0f / 9.0f;
}

// Read and process ADC value
static bool readSensor() {
    if (!state.enabled) {
        return false;
    }
    
    // Check if enough time has passed
    uint32_t now = millis();
    if (now - state.lastReadTime < state.updateRateMs) {
        return true;  // Use cached value
    }
    state.lastReadTime = now;
    
    // Read raw ADC value
    int rawAdc = adc1_get_raw(state.channel);
    if (rawAdc < 0) {
        state.errorCount++;
        ESP_LOGE(TAG, "ADC read error");
        return false;
    }
    
    state.lastRawAdc = (uint16_t)rawAdc;
    
    // Add to averaging buffer
    state.sampleBuffer[state.sampleIndex] = state.lastRawAdc;
    state.sampleIndex = (state.sampleIndex + 1) % state.avgSamples;
    if (state.sampleIndex == 0) {
        state.bufferFilled = true;
    }
    
    // Calculate average
    uint32_t sum = 0;
    uint8_t count = state.bufferFilled ? state.avgSamples : state.sampleIndex;
    if (count == 0) count = 1;
    
    for (uint8_t i = 0; i < count; i++) {
        sum += state.sampleBuffer[i];
    }
    uint16_t avgAdc = sum / count;
    
    // Convert to voltage using calibration
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(avgAdc, &state.adcChars);
    state.lastVoltage = voltage_mv / 1000.0f;  // Convert to volts
    
    // Calculate sensor resistance using voltage divider formula
    // V_out = V_in * R_sensor / (R_pullup + R_sensor)
    // Solving for R_sensor:
    // R_sensor = R_pullup * V_out / (V_in - V_out)
    
    float vIn = 3.3f;  // 3.3V supply
    float vOut = state.lastVoltage;
    float rPullup = (float)WATER_TEMP_PULLUP_OHMS;
    
    // Prevent division by zero
    if (vOut >= vIn - 0.01f) {
        // Voltage too high - sensor shorted or open
        state.lastResistance = 0;
        state.sensorShorted = true;
        state.sensorConnected = false;
        state.lastTempF = NAN;
        state.errorCount++;
        return false;
    }
    
    if (vOut <= 0.01f) {
        // Voltage too low - sensor open circuit
        state.lastResistance = INFINITY;
        state.sensorShorted = false;
        state.sensorConnected = false;
        state.lastTempF = NAN;
        state.errorCount++;
        return false;
    }
    
    // Calculate resistance
    state.lastResistance = rPullup * vOut / (vIn - vOut);
    
    // Check for sensor faults
    if (state.lastResistance > OPEN_CIRCUIT_RESISTANCE) {
        state.sensorConnected = false;
        state.sensorShorted = false;
        state.lastTempF = NAN;
        return false;
    }
    
    if (state.lastResistance < SHORT_CIRCUIT_RESISTANCE) {
        state.sensorConnected = true;
        state.sensorShorted = true;
        state.lastTempF = NAN;
        return false;
    }
    
    // Sensor is connected and working
    state.sensorConnected = true;
    state.sensorShorted = false;
    
    // Convert resistance to temperature
    state.lastTempF = resistanceToTempF(state.lastResistance);
    state.readCount++;
    
    return true;
}

// =============================================================================
// Public API
// =============================================================================

bool waterTempInit() {
    if (state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing water temp sensor on GPIO %d (ADC1_CH%d)",
             WATER_TEMP_INPUT_PIN, state.channel);
    
    state.initialized = true;
    state.enabled = false;
    
    return true;
}

void waterTempEnable() {
    if (!state.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return;
    }
    
    if (state.enabled) {
        ESP_LOGW(TAG, "Already enabled");
        return;
    }
    
    ESP_LOGI(TAG, "Enabling water temp sensor");
    
    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(state.channel, ADC_ATTEN_DB_12);  // Full 0-3.3V range
    
    // Characterize ADC for accurate voltage readings
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_12,
        ADC_WIDTH_BIT_12,
        ADC_REFERENCE_VOLTAGE,
        &state.adcChars
    );
    
    // Reset state
    state.sampleIndex = 0;
    state.bufferFilled = false;
    state.lastReadTime = 0;
    state.readCount = 0;
    state.errorCount = 0;
    state.lastTempF = NAN;
    
    state.enabled = true;
    ESP_LOGI(TAG, "Water temp sensor enabled (pull-up: %d ohms, averaging: %d samples)",
             WATER_TEMP_PULLUP_OHMS, state.avgSamples);
}

void waterTempDisable() {
    if (!state.enabled) {
        return;
    }
    
    ESP_LOGI(TAG, "Disabling water temp sensor");
    
    // Reset GPIO (releases ADC control)
    gpio_reset_pin((gpio_num_t)WATER_TEMP_INPUT_PIN);
    gpio_set_direction((gpio_num_t)WATER_TEMP_INPUT_PIN, GPIO_MODE_INPUT);
    
    state.enabled = false;
    ESP_LOGI(TAG, "Water temp sensor disabled");
}

bool waterTempIsEnabled() {
    return state.enabled;
}

float waterTempGetFahrenheit() {
    if (!state.enabled) {
        return NAN;
    }
    
    readSensor();
    
    if (isnan(state.lastTempF)) {
        return NAN;
    }
    
    // Fahrenheit is now our primary unit - just add calibration offset
    return state.lastTempF + state.offsetF;
}

float waterTempGetCelsius() {
    if (!state.enabled) {
        return NAN;
    }
    
    readSensor();
    
    if (isnan(state.lastTempF)) {
        return NAN;
    }
    
    // Convert Fahrenheit to Celsius, then apply offset (converted to C)
    float offsetC = state.offsetF * 5.0f / 9.0f;
    return fahrenheitToCelsius(state.lastTempF) + offsetC;
}

uint16_t waterTempGetRawADC() {
    if (!state.enabled) {
        return 0;
    }
    
    readSensor();
    return state.lastRawAdc;
}

float waterTempGetResistanceOhms() {
    if (!state.enabled) {
        return 0.0f;
    }
    
    readSensor();
    return state.lastResistance;
}

float waterTempGetVoltage() {
    if (!state.enabled) {
        return 0.0f;
    }
    
    readSensor();
    return state.lastVoltage;
}

void waterTempSetAveraging(uint8_t samples) {
    if (samples < 1) samples = 1;
    if (samples > 64) samples = 64;
    
    state.avgSamples = samples;
    state.sampleIndex = 0;
    state.bufferFilled = false;
    
    ESP_LOGI(TAG, "Averaging set to %d samples", samples);
}

void waterTempSetUpdateRateMs(uint16_t ms) {
    if (ms < 10) ms = 10;  // Minimum 10ms
    
    state.updateRateMs = ms;
    ESP_LOGI(TAG, "Update rate set to %d ms", ms);
}

void waterTempSetOffsetF(float offsetF) {
    state.offsetF = offsetF;
    ESP_LOGI(TAG, "Temperature offset set to %.1f°F", offsetF);
}

float waterTempGetOffsetF() {
    return state.offsetF;
}

bool waterTempIsSensorConnected() {
    if (!state.enabled) {
        return false;
    }
    
    readSensor();
    return state.sensorConnected;
}

bool waterTempIsSensorShorted() {
    if (!state.enabled) {
        return false;
    }
    
    readSensor();
    return state.sensorShorted;
}

uint32_t waterTempGetReadCount() {
    return state.readCount;
}

uint32_t waterTempGetErrorCount() {
    return state.errorCount;
}
