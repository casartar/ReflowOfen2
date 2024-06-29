// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License
//
// This example shows how to store your project configuration in a file.
// It uses the SD library but can be easily modified for any other file-system.
//
// The file contains a JSON document with the following content:
// {
//   "hostname": "examples.com",
//   "port": 2731
// }
//
// To run this program, you need an SD card connected to the SPI bus as follows:
// * MOSI <-> pin 11
// * MISO <-> pin 12
// * CLK  <-> pin 13
// * CS   <-> pin 4
//
// https://arduinojson.org/v7/example/config/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <max6675.h>

#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define MAX_ENTIES 10

struct Config {
    uint16_t time[MAX_ENTIES];
    uint16_t temperature[MAX_ENTIES];
    uint8_t number_of_entries;
};

const char* filename = "/config.txt"; // <- SD library uses 8.3 filenames
Config config; // <- global configuration object

int thermoDO = 8;
int thermoCS = 7;
int thermoCLK = 6;

MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

LiquidCrystal_I2C lcd(0x27, 20, 4); // set the LCD address to 0x27 for a 16 chars and 2 line display

int relayOutput = 3;
int buttonInput = 2;

// Loads the configuration from a file
void loadConfiguration(const char* filename, Config& config)
{
    // Open file for reading
    Serial.println(F("Open SD Card"));
    File file = SD.open(filename);

    // Allocate a temporary JsonDocument
    JsonDocument doc;

    // Deserialize the JSON document
    Serial.println(F("Deserialize"));
    DeserializationError error = deserializeJson(doc, file);
    if (error)
        Serial.println(F("Failed to read file, using default configuration"));
    Serial.print("Time Size: ");
    Serial.print(doc["Time"].size());
    Serial.print(" Temperature Size: ");
    Serial.println(doc["Temperature"].size());

    JsonArray temperature = doc["Temperature"];
    JsonArray time = doc["Time"];

    config.number_of_entries = 0;

    for (uint8_t i = 0; i < MAX_ENTIES && i < doc["Time"].size() && i < doc["Temperature"].size(); i++) {
        config.time[i] = time[i];
        config.temperature[i] = temperature[i];
        Serial.print("Time: ");
        Serial.print(config.time[i]);
        Serial.print(" Temperature: ");
        Serial.println(config.temperature[i]);
        config.number_of_entries++;
    }

    file.close();
}

void setup()
{
    pinMode(relayOutput, OUTPUT);
    pinMode(buttonInput, INPUT_PULLUP);

    lcd.init(); // initialize the lcd
    lcd.backlight();
    lcd.print("Test");

    // Initialize serial port
    Serial.begin(9600);
    while (!Serial)
        continue;

    // Initialize SD library

    const int chipSelect = 4;
    while (!SD.begin(chipSelect)) {
        Serial.println(F("Failed to initialize SD library"));
        delay(1000);
    }

    // Should load default config if run for the first time
    Serial.println(F("Loading configuration..."));
    loadConfiguration(filename, config);
}

void loop()
{
    while (digitalRead(buttonInput))
        ;
    lcd.print("Start");
    delay(100);
    while (!digitalRead(buttonInput))
        ;
    delay(100);

    for (uint8_t heatingPhase = 0; heatingPhase < config.number_of_entries; heatingPhase++) {
        Serial.print("Phase: ");
        Serial.println(heatingPhase + 1);

        unsigned long nextSampleTime = millis() + 1000;
        unsigned long finishHeatingPhaseTime = millis() + config.time[heatingPhase] * 1000;
        uint16_t temperatureOffset = 25;
        if (heatingPhase != 0) {
            temperatureOffset = config.temperature[heatingPhase - 1];
        }

        uint16_t timeExpiredSincePhaseStart = 0;
        while (true) {
            if (millis() >= nextSampleTime) {
                nextSampleTime += 1000;
                timeExpiredSincePhaseStart++;
                Serial.print("Time: ");
                Serial.print(timeExpiredSincePhaseStart);
                Serial.print(" Temp: ");
                float temperature = thermocouple.readCelsius();
                Serial.print(temperature);
                Serial.print(" Gain: ");
                float gain = (float)(config.temperature[heatingPhase] - temperatureOffset) / (float)(config.time[heatingPhase]);
                Serial.print(gain);
                Serial.print(" SetPoint: ");
                float setPoint = gain * timeExpiredSincePhaseStart + temperatureOffset;
                Serial.println(setPoint);

                digitalWrite(relayOutput, temperature < setPoint);
            }
            if (millis() >= finishHeatingPhaseTime) {
                break;
            }
            if (!digitalRead(buttonInput)) {
                lcd.print("Aborted");
                delay(100);
                break;
            }
        }
    }
    digitalWrite(relayOutput, LOW);
}