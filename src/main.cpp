#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("ESP32 AI Project initialized");
    Serial.printf("CPU Freq: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Flash size: %d bytes\n", ESP.getFlashChipSize());
}

void loop() {
    // Main loop
    delay(1000);
}
