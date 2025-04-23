#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 particleSensor;

const byte RATE_SIZE = 8;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;
float spO2;

const int BUFFER_SIZE = 100;
long irBuffer[BUFFER_SIZE];
long redBuffer[BUFFER_SIZE];
int bufferIndex = 0;

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
    
    particleSensor.setup();
    particleSensor.setPulseAmplitudeRed(0x0A);
    particleSensor.setPulseAmplitudeGreen(0);
    
    for (byte x = 0; x < RATE_SIZE; x++) {
        rates[x] = 0;
    }
}

void loop() {
    irBuffer[bufferIndex] = particleSensor.getIR();
    redBuffer[bufferIndex] = particleSensor.getRed();
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

    // Расчет SpO2
    float redAC = 0;
    float irAC = 0;
    float redDC = 0;
    float irDC = 0;
    
    for(int i = 0; i < BUFFER_SIZE; i++) {
        redDC += redBuffer[i];
        irDC += irBuffer[i];
    }
    redDC /= BUFFER_SIZE;
    irDC /= BUFFER_SIZE;
    
    for(int i = 0; i < BUFFER_SIZE; i++) {
        redAC += pow(redBuffer[i] - redDC, 2);
        irAC += pow(irBuffer[i] - irDC, 2);
    }
    redAC = sqrt(redAC / BUFFER_SIZE);
    irAC = sqrt(irAC / BUFFER_SIZE);
    
    float ratio = (redAC / redDC) / (irAC / irDC);
    spO2 = 110 - 25 * ratio;

    if (irBuffer[(bufferIndex - 1) % BUFFER_SIZE] > 50000) {
        if (checkForBeat(irBuffer[(bufferIndex - 1) % BUFFER_SIZE]) == true) {
            long delta = millis() - lastBeat;
            lastBeat = millis();

            if (delta > 200 && delta < 2000) {
                beatsPerMinute = 60000.0 / delta;
                rates[rateSpot++] = (byte)beatsPerMinute;
                rateSpot %= RATE_SIZE;

                beatAvg = 0;
                int count = 0;
                for (byte x = 0; x < RATE_SIZE; x++) {
                    if (rates[x] > 0) {
                        beatAvg += rates[x];
                        count++;
                    }
                }
                if (count > 0) {
                    beatAvg /= count;
                }
            }
        }
    } else {
        beatsPerMinute = 0;
        beatAvg = 0;
    }

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 1000) {
        lastPrint = millis();
        Serial.print("IR: ");
        Serial.print(irBuffer[(bufferIndex - 1) % BUFFER_SIZE]);
        Serial.print(" RED: ");
        Serial.print(redBuffer[(bufferIndex - 1) % BUFFER_SIZE]);
        Serial.print(" BPM: ");
        Serial.print((int)beatsPerMinute);
        Serial.print(" AVG: ");
        Serial.print(beatAvg);
        Serial.print(" SpO2: ");
        Serial.print((int)spO2);
        Serial.print("% FINGER: ");
        Serial.println(irBuffer[(bufferIndex - 1) % BUFFER_SIZE] < 50000 ? 0 : 1);
    }

    delay(20);
}