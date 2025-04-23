#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;

uint32_t irBuffer[100]; // инфракрасные данные
uint32_t redBuffer[100]; // красные данные

int32_t bufferLength = 100;
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

// Переменные для пульса
int lastValidBPM = 0;
unsigned long lastBPMUpdate = 0;
const unsigned long BPM_UPDATE_INTERVAL = 5000; // 5 секунд

// Переменные для SpO2
int lastValidSpO2 = 0;

// Флаг наличия пальца
bool fingerDetected = false;

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    Wire.setClock(400000);
    
    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("Sensor not found! Check wiring:");
        Serial.println("1. VIN -> 3.3V");
        Serial.println("2. GND -> GND");
        Serial.println("3. SDA -> GPIO21");
        Serial.println("4. SCL -> GPIO22");
        while (1) {
            Serial.println("Waiting for reset...");
            delay(1000);
        }
    }
    
    // Настройка датчика
    byte ledBrightness = 50;
    byte sampleAverage = 1;
    byte ledMode = 2;
    byte sampleRate = 100;
    int pulseWidth = 69;
    int adcRange = 4096;
    
    particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
}

void loop() {
    static unsigned long lastSample = 0;
    if (millis() - lastSample >= 20) { // 50 Гц
        lastSample = millis();
        
        // Сдвигаем буфер
        for (byte i = 1; i < bufferLength; i++) {
            redBuffer[i-1] = redBuffer[i];
            irBuffer[i-1] = irBuffer[i];
        }
        
        // Добавляем новый сэмпл
        while (particleSensor.available() == false)
            particleSensor.check();
            
        redBuffer[bufferLength-1] = particleSensor.getRed();
        irBuffer[bufferLength-1] = particleSensor.getIR();
        particleSensor.nextSample();
        
        // Проверяем наличие пальца
        bool currentFinger = irBuffer[bufferLength-1] > 50000;
        
        // Если палец убрали, сбрасываем значения
        if (fingerDetected && !currentFinger) {
            lastValidBPM = 0;
            lastValidSpO2 = 0;
        }
        fingerDetected = currentFinger;
        
        // Рассчитываем SpO2 и пульс только если палец на датчике
        if (fingerDetected) {
            maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
            
            // Обновляем SpO2 если валидно
            if (validSPO2 == 1 && spo2 > 0 && spo2 < 100) {
                lastValidSpO2 = spo2;
            }
            
            // Обновляем пульс каждые 5 секунд если валидно
            if (millis() - lastBPMUpdate >= BPM_UPDATE_INTERVAL) {
                if (validHeartRate == 1 && heartRate > 40 && heartRate < 200) {
                    lastValidBPM = heartRate;
                }
                lastBPMUpdate = millis();
            }
        }
        
        // Выводим результаты
        Serial.print("IR: ");
        Serial.print(irBuffer[bufferLength-1]);
        Serial.print(" RED: ");
        Serial.print(redBuffer[bufferLength-1]);
        Serial.print(" SpO2: ");
        Serial.print(lastValidSpO2);
        Serial.print("% HR: ");
        Serial.print(lastValidBPM);
        Serial.print(" SpO2Valid: ");
        Serial.print(validSPO2);
        Serial.print(" HRValid: ");
        Serial.println(validHeartRate);
    }
}