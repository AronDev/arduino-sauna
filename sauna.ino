#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>

// output
#define HEAT_RELAY_PIN 3

// input
#define MAIN_SWITCH_PIN 2
#define TEMP_INCR_PIN 4
#define TEMP_DECR_PIN 5
#define SENSOR_PIN A0

// memory
#define _TARGET_TEMPERATURE 0
#define _TEMPERATURE_DIFFERENCE 1
#define _TEMPERATURE_STEP 2
#define _BACKLIGHT_TIMER 3 // todo

// min,max,default values
#define MAX_TARGET_TEMP 120
#define MIN_TARGET_TEMP 10

#define MAX_TEMP_DIFF 20
#define MIN_TEMP_DIFF 0

#define MAX_TEMP_STEP 10
#define MIN_TEMP_STEP 1

#define DEFAULT_TARGET_TEMP 20
#define DEFAULT_TEMP_DIFF 5
#define DEFAULT_TEMP_STEP 5

LiquidCrystal_I2C lcd(0x27, 20, 2);

// tasks
/*
1 - Temperature check
2 - LCD display refresh
*/
unsigned long CURRENT_TIME = 0;
long PREV_TIME_T1 = millis();
long PREV_TIME_T2 = millis();
long PREV_TIME_T3 = millis();
const long INTERVAL_T1 = 1000;
const long INTERVAL_T2 = 1000;
const long INTERVAL_T3 = 500;

// Setpoint default values
int TARGET_TEMPERATURE; // temperature setpoint
int CURRENT_TEMPERATURE; // current temperature, refreshed with ntc converted value
int TEMPERATURE_DIFFERENCE; // temperature differential
int TEMPERATURE_STEP; // temperature is increased with this unit when + or - button

// LCD values
int BACKLIGHT_TIMER_INITIAL = 30; // (x * lcd refresh itnerval)
int BACKLIGHT_TIMER = BACKLIGHT_TIMER_INITIAL; 

// Toggle values
bool HEATING_STATUS = false;
bool DEBUG = false;

// Serial command processing
String input; 
char MAIN_COMMAND[32];
char SUB_COMMAND[32];
int COMMAND_VAL;

// NTC
float R1 = 100000; // 100K known resistor
float logR2;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

void checkSerialCommandInput() {
    if (Serial.available()) {
        input = Serial.readStringUntil('\n');
        input.trim();
        sscanf(input.c_str(), "%32s %32s %d", MAIN_COMMAND, SUB_COMMAND, &COMMAND_VAL);
        if (strcmp(MAIN_COMMAND, "set") == 0) {
            if (strcmp(SUB_COMMAND, "diff") == 0) {
                if (setTemperatureDifference(COMMAND_VAL)) {
                    Serial.println(String("Temperature difference has been set to ") + TEMPERATURE_DIFFERENCE + String(" C"));
                } else {
                    Serial.println("Temperature difference must be between " + String(MIN_TEMP_DIFF) + " and " + String(MAX_TEMP_DIFF));
                }
            } else if (strcmp(SUB_COMMAND, "target") == 0) {
                if (setTargetTemperature(COMMAND_VAL)) {
                    Serial.println(String("Target temperature has been set to ") + TARGET_TEMPERATURE + String(" C"));
                } else {
                    Serial.println("Target temperature must be between " + String(MIN_TARGET_TEMP) + " and " + String(MAX_TARGET_TEMP));
                }
            } else if (strcmp(SUB_COMMAND, "step") == 0) {
                if (setTemperatureStep(COMMAND_VAL)) {
                    Serial.println(String("Temperature step has been set to ") + TEMPERATURE_STEP + String(" C"));
                } else {
                    Serial.println("Temperature step must be between " + String(MIN_TEMP_STEP) + " and " + String(MAX_TEMP_STEP));
                }
            } else if (strcmp(SUB_COMMAND, "debug") == 0) {
                if (COMMAND_VAL == 0) {
                    DEBUG = false;
                    Serial.println("Debug mode has been turned off");
                } else if (COMMAND_VAL == 1) {
                    DEBUG = true;
                    Serial.println("Debug mode has been turned on");
                } else {
                    Serial.println("Debug value must be 0 or 1");
                }
            }
            else Serial.println("Type not found");
        } else if (strcmp(MAIN_COMMAND, "get") == 0) {
            if (strcmp(SUB_COMMAND, "diff") == 0) {
                Serial.println(String("Temperature difference is ") + TEMPERATURE_DIFFERENCE + String(" C"));
            } else if (strcmp(SUB_COMMAND, "target") == 0) {
                Serial.println(String("Target temperature is ") + TARGET_TEMPERATURE + String(" C"));
            } else if (strcmp(SUB_COMMAND, "debug") == 0) {
                Serial.println(String("Debug is ") + (DEBUG ? String("ON") : String("OFF")));
            } else if (strcmp(SUB_COMMAND, "step") == 0) {
                Serial.println(String("Temperature step is ") + TEMPERATURE_STEP + String(" C"));
            }
            else Serial.println("Type not found");
        } else if (strcmp(MAIN_COMMAND, "help") == 0) {
            Serial.println("Available commands: set, get, help");
            Serial.println("Type 'help [cmd]' to see command's help");
            if (strcmp(SUB_COMMAND, "set") == 0) {
                Serial.println("Available types for 'set': target, diff, step, debug");
            } else if (strcmp(SUB_COMMAND, "get") == 0) {
                Serial.println("Available types for 'get': target, diff, step, debug");
            }
        } 
        else Serial.println("Command not found");
    }
}

void checkButtons() {
    if (digitalRead(TEMP_INCR_PIN) == LOW && digitalRead(TEMP_DECR_PIN) != LOW) {
        if (CURRENT_TIME - PREV_TIME_T3 > INTERVAL_T3) {
            BACKLIGHT_TIMER = BACKLIGHT_TIMER_INITIAL;
            setTargetTemperature(TARGET_TEMPERATURE + 1);
            refreshDisplay();
            PREV_TIME_T3 = CURRENT_TIME;
        }
    } else if (digitalRead(TEMP_DECR_PIN) == LOW && digitalRead(TEMP_INCR_PIN) != LOW) {
        if (CURRENT_TIME - PREV_TIME_T3 > INTERVAL_T3) {
            BACKLIGHT_TIMER = BACKLIGHT_TIMER_INITIAL;
            setTargetTemperature(TARGET_TEMPERATURE - 1);
            refreshDisplay();
            PREV_TIME_T3 = CURRENT_TIME;
        }
    }
}

void checkTemperature() {
    logR2 = log(R1 * (1023.0 / (float) analogRead(SENSOR_PIN) - 1.0));
    CURRENT_TEMPERATURE = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2)) - 273.15;
    if (DEBUG) Serial.println(String("-> Current: ") + CURRENT_TEMPERATURE + String(" C | Target: ") + TARGET_TEMPERATURE + String(" C | Diff: ") + TEMPERATURE_DIFFERENCE + String(" C"));
    if (HEATING_STATUS) {
        if ((TARGET_TEMPERATURE - CURRENT_TEMPERATURE < TEMPERATURE_DIFFERENCE) && TARGET_TEMPERATURE - CURRENT_TEMPERATURE > 0) {
            toggleHeating(true);
        } else if (TARGET_TEMPERATURE - CURRENT_TEMPERATURE <= 0) {
            toggleHeating(false);
        }
    } else {
        if (TARGET_TEMPERATURE - CURRENT_TEMPERATURE >= TEMPERATURE_DIFFERENCE) {
            toggleHeating(true);
        } 
    }
    refreshDisplay();
}

void refreshDisplay() {
    lcd.clear();
    if (--BACKLIGHT_TIMER <= 0 && !DEBUG) {
      lcd.noBacklight();
    } else {
      lcd.backlight();
    }

    lcd.setCursor(2, 0);
    lcd.print(CURRENT_TEMPERATURE + String(" C    ") + TARGET_TEMPERATURE + String(" C"));
    
    lcd.setCursor(2, 1);
    if (HEATING_STATUS) lcd.print("[H]");
    if (DEBUG) lcd.print("[D]");
}

void toggleHeating(bool toggle) {
    if (toggle) {
        HEATING_STATUS = true;
        digitalWrite(HEAT_RELAY_PIN, HIGH);
    } else {
        HEATING_STATUS = false;
        digitalWrite(HEAT_RELAY_PIN, LOW);
    }
}

bool setTargetTemperature(int temp) {
    if (temp >= MIN_TARGET_TEMP && temp <= MAX_TARGET_TEMP) {
        TARGET_TEMPERATURE = temp;
        EEPROM.write(_TARGET_TEMPERATURE, TARGET_TEMPERATURE);
        return true;
    } else return false;
}

bool setTemperatureDifference(int diff) {
    if (diff >= MIN_TEMP_DIFF && diff <= MAX_TEMP_DIFF) {
        TEMPERATURE_DIFFERENCE = diff;
        EEPROM.write(_TEMPERATURE_DIFFERENCE, TEMPERATURE_DIFFERENCE);
        return true;
    } else return false;
}

bool setTemperatureStep(int step) {
    if (step >= MIN_TEMP_STEP && step <= MAX_TEMP_STEP) {
        TEMPERATURE_STEP = step;
        EEPROM.write(_TEMPERATURE_STEP, TEMPERATURE_STEP);
        return true;
    } else return false;
}

void setup() {
    Serial.begin(9600);

    lcd.init();
    lcd.clear();
    lcd.backlight();
    lcd.setCursor(0, 0);

    pinMode(HEAT_RELAY_PIN, OUTPUT);
    pinMode(MAIN_SWITCH_PIN, INPUT_PULLUP);
    pinMode(TEMP_INCR_PIN, INPUT_PULLUP);
    pinMode(TEMP_DECR_PIN, INPUT_PULLUP);

    // Load last saved setpoint
    TARGET_TEMPERATURE = EEPROM.read(_TARGET_TEMPERATURE);
    if (TARGET_TEMPERATURE == 255) {
        setTargetTemperature(DEFAULT_TARGET_TEMP);
    }

    TEMPERATURE_DIFFERENCE = EEPROM.read(_TEMPERATURE_DIFFERENCE);
    if (TEMPERATURE_DIFFERENCE == 255) {
        setTemperatureDifference(DEFAULT_TEMP_DIFF);
    }

    TEMPERATURE_STEP = EEPROM.read(_TEMPERATURE_STEP);
    if (TEMPERATURE_STEP == 255) {
        setTemperatureStep(DEFAULT_TEMP_STEP);
    }

    if (TARGET_TEMPERATURE > 120) setTargetTemperature(40);

    Serial.println("");
    Serial.println("Sauna controller v1.0 loaded");
    Serial.println("Author: Aron Kovacs (zipike02@gmail.com)");
    Serial.println("For help type 'help' in the console");
    Serial.println("");
    Serial.println("Read values from EEPROM:");
    Serial.println(" -> TARGET_TEMPERATURE = " + String(TARGET_TEMPERATURE));
    Serial.println(" -> TEMPERATURE_DIFFERENCE = " + String(TEMPERATURE_DIFFERENCE));
    Serial.println(" -> TEMPERATURE_STEP = " + String(TEMPERATURE_STEP));
}

void loop() {
    CURRENT_TIME = millis();

    if (CURRENT_TIME - PREV_TIME_T1 > INTERVAL_T1) {
        checkTemperature();
        PREV_TIME_T1 = CURRENT_TIME;
    }

    if (CURRENT_TIME - PREV_TIME_T2 > INTERVAL_T2) {
        refreshDisplay();
        PREV_TIME_T2 = CURRENT_TIME;
    }

    checkButtons();
    checkSerialCommandInput();
}
