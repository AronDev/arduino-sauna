#include <EEPROM.h>
#include <Eventually.h>

// out
#define HEAT_RELAY_PIN 2

// in
#define TEMP_INCR_PIN 3
#define TEMP_DECR_PIN 4

// address
#define LAST_TEMP_ADDRESS 0

EvtManager mgr;

int ROOM_TEMPERATURE = 20;
int WANTED_TEMPERATURE = 20;
int CURRENT_TEMPERATURE = 25;
const int DIFFERENCE = 2;

void add_listeners() {
    mgr.addListener(new EvtPinListener(TEMP_INCR_PIN, 40, (EvtAction) temp_increase));
    mgr.addListener(new EvtPinListener(TEMP_DECR_PIN, 40, (EvtAction) temp_decrease));
    mgr.addListener(new EvtTimeListener(1000, true, (EvtAction)temperature_check));
}

bool temp_increase() {
    mgr.resetContext(); 
    WANTED_TEMPERATURE ++;
    Serial.println(String("Temperature increased to ") + WANTED_TEMPERATURE);
    add_listeners();
    EEPROM.write(LAST_TEMP_ADDRESS, WANTED_TEMPERATURE);
    return true;
}

bool temp_decrease() {
    mgr.resetContext(); 
    WANTED_TEMPERATURE --;
    Serial.println(String("Temperature decreased to ") + WANTED_TEMPERATURE);
    add_listeners();
    EEPROM.write(LAST_TEMP_ADDRESS, WANTED_TEMPERATURE);
    return true;
}

void temperature_check() {
    if (WANTED_TEMPERATURE - CURRENT_TEMPERATURE >= DIFFERENCE) {
        digitalWrite(HEAT_RELAY_PIN, HIGH);
    } else {
        digitalWrite(HEAT_RELAY_PIN, LOW);
    }   
}

void setup() {
    Serial.begin(9600);

    pinMode(HEAT_RELAY_PIN, OUTPUT);
    pinMode(TEMP_INCR_PIN, INPUT_PULLUP);
    pinMode(TEMP_DECR_PIN, INPUT_PULLUP);
    
    add_listeners();

    WANTED_TEMPERATURE = EEPROM.read(LAST_TEMP_ADDRESS);
    if (WANTED_TEMPERATURE > 120) WANTED_TEMPERATURE = 40;

    Serial.println("Loaded");
}

USE_EVENTUALLY_LOOP(mgr)