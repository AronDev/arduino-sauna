#include <EEPROM.h>
#include <Eventually.h>

// out
#define HEAT_RELAY_PIN 2

// in
#define TEMP_INCR_PIN 3
#define TEMP_DECR_PIN 4
#define SENSOR_PIN 0

// address
#define LAST_TEMP_ADDRESS 0

EvtManager mgr;

int ROOM_TEMPERATURE = 20;
int WANTED_TEMPERATURE = 20;
int CURRENT_TEMPERATURE = WANTED_TEMPERATURE;
const int DIFFERENCE = 2;

float R1 = 10000;
float logR2;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

void add_listeners() {
    mgr.addListener(new EvtPinListener(TEMP_INCR_PIN, 40, (EvtAction) temp_increase));
    mgr.addListener(new EvtPinListener(TEMP_DECR_PIN, 40, (EvtAction) temp_decrease));
    mgr.addListener(new EvtTimeListener(1000, true, (EvtAction)temperature_check));
}

bool temp_increase() {
    mgr.resetContext(); 
    WANTED_TEMPERATURE ++;
    add_listeners();
    EEPROM.write(LAST_TEMP_ADDRESS, WANTED_TEMPERATURE);
    return true;
}

bool temp_decrease() {
    mgr.resetContext(); 
    WANTED_TEMPERATURE --;
    add_listeners();
    EEPROM.write(LAST_TEMP_ADDRESS, WANTED_TEMPERATURE);
    return true;
}

void temperature_check() {
    logR2 = log(R1 * (1023.0 / (float) analogRead(SENSOR_PIN) - 1.0));
    CURRENT_TEMPERATURE = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2)) - 273.15;
    //Serial.println(String("=> Current: ") + CURRENT_TEMPERATURE + String(", Target: ") + WANTED_TEMPERATURE);
    if (WANTED_TEMPERATURE - CURRENT_TEMPERATURE >= DIFFERENCE) {
        digitalWrite(HEAT_RELAY_PIN, HIGH);
    } else {
        digitalWrite(HEAT_RELAY_PIN, LOW);
    }   
}

void setup() {
    //Serial.begin(9600);
    pinMode(HEAT_RELAY_PIN, OUTPUT);
    pinMode(TEMP_INCR_PIN, INPUT_PULLUP);
    pinMode(TEMP_DECR_PIN, INPUT_PULLUP);
    
    add_listeners();

    WANTED_TEMPERATURE = EEPROM.read(LAST_TEMP_ADDRESS);
    if (WANTED_TEMPERATURE > 120) WANTED_TEMPERATURE = 40;

    //Serial.println("Loaded");
}

USE_EVENTUALLY_LOOP(mgr)
