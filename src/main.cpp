#include <Arduino.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

MAX30105 particleSensor;

// BLE характеристики
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// UUID для сервиса и характеристики
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Класс для обработки событий BLE
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

// Данные для пульса
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;

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

    // Инициализация BLE
    BLEDevice::init("PulseSensor");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pService->start();
    
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);
    BLEDevice::startAdvertising();
    Serial.println("BLE started");
}

void loop() {
    irBuffer[bufferIndex] = particleSensor.getIR();
    redBuffer[bufferIndex] = particleSensor.getRed();
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;

    if (checkForBeat(irBuffer[(bufferIndex - 1) % BUFFER_SIZE]) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();

        if (delta > 200 && delta < 2000) {
            beatsPerMinute = 60000.0 / delta;

            if (beatsPerMinute < 255 && beatsPerMinute > 20) {
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
    }

    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 1000) {
        lastPrint = millis();
        
        // Формируем строку для отправки
        String dataString = String(irBuffer[(bufferIndex - 1) % BUFFER_SIZE]) + "," +
                          String(redBuffer[(bufferIndex - 1) % BUFFER_SIZE]) + "," +
                          String((int)beatsPerMinute) + "," +
                          String(beatAvg) + "," +
                          String(irBuffer[(bufferIndex - 1) % BUFFER_SIZE] < 50000 ? 0 : 1);
        
        // Отправляем данные через BLE
        if (deviceConnected) {
            pCharacteristic->setValue(dataString.c_str());
            pCharacteristic->notify();
        }
        
        // Выводим в Serial для отладки
        Serial.println(dataString);
    }

    // Обработка отключения BLE
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    delay(20);
}