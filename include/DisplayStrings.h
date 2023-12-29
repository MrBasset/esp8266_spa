// library of strings held in PROGMEM

#pragma once
#include <pgmspace.h>

#ifndef DISPLAYSTRINGS_h
#define DISPLAYSTRINGS_h

const char VERSION[] PROGMEM = "0.37.4";


const char STRON[] PROGMEM = "ON";
const char STROFF[] PROGMEM = "OFF";

//MQTT Topics
const char MQTT_NODE_MSG[] PROGMEM = "Spa/node/msg";

// Fault codes/messages

const char FAULT_OUT_OF_SYNC[] PROGMEM = "Sensors are out of sync"; //15
const char FAULT_FLOW_LOW[] PROGMEM = "The water flow is low"; //16
const char FAULT_FLOW_FAILED[] PROGMEM = "The water flow has failed"; //17
const char FAULT_SETTINGS_RESET[] PROGMEM = "The settings have been reset"; //18 + 21
const char FAULT_PRIMING_MODE[] PROGMEM = "Priming Mode"; //19
const char FAULT_CLOCK_FAILED[] PROGMEM = "The clock has failed"; //20
const char FAULT_MEMORY_FAILURE[] PROGMEM  = "Program memory failure"; //22
const char FAULT_SENSOR_SYNC[] PROGMEM = "Sensors are out of sync -- Call for service"; //26
const char FAULT_HEATER_DRY[] PROGMEM = "The heater is dry"; //27 + 28
const char FAULT_WATER_TOO_HOT[] PROGMEM = "The water is too hot"; //29
const char FAULT_HEATER_TOO_HOT[] PROGMEM = "The heater is too hot"; //30
const char FAULT_SENSOR_FAILURE[] PROGMEM = "Sensor A Fault"; //31
const char FAULT_SENSORB_FAILURE[] PROGMEM = "Sensor B Fault"; //32
const char FAULT_PUMP_STUCK[] PROGMEM = "A pump may be stuck on"; //34
const char FAULT_HOT_FAULT[] PROGMEM = "Hot fault"; //35
const char FAULT_GFCI_FAILURE[] PROGMEM = "The GFCI test failed"; //36
const char FAULT_STANDY_BY[] PROGMEM = "Standby Mode (Hold Mode)"; //37
const char FAULT_UNKNOWN[] PROGMEM = "Unknown error";

const char* FAULT_TABLE[] PROGMEM = {
    FAULT_OUT_OF_SYNC, //15
    FAULT_FLOW_LOW, //16
    FAULT_FLOW_FAILED, //17
    FAULT_SETTINGS_RESET, //18
    FAULT_PRIMING_MODE, //19
    FAULT_CLOCK_FAILED, //20
    FAULT_SETTINGS_RESET, //21
    FAULT_MEMORY_FAILURE, //22
    NULL, //23
    NULL, //24
    NULL, //25
    FAULT_SENSOR_SYNC, //26
    FAULT_HEATER_DRY, //27
    FAULT_HEATER_DRY, //28
    FAULT_WATER_TOO_HOT, //29
    FAULT_HEATER_TOO_HOT, //30
    FAULT_SENSOR_FAILURE, //31
    FAULT_SENSORB_FAILURE, //32
    NULL, //33
    FAULT_PUMP_STUCK, //34
    FAULT_HOT_FAULT, //35
    FAULT_GFCI_FAILURE, //36
    FAULT_STANDY_BY //37
};

//HASSIO Payloads
const char PAYLOAD_SPA_STATE[] PROGMEM = "{\"name\":\"Hot tub status\",\"uniq_id\":\"ESP82Spa_1\",\"stat_t\":\"Spa/node/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"],\"name\":\"Esp Spa\",\"sw\":\"%s\"}}";
const char PAYLOAD_SPA_TEMP_F[] PROGMEM = "{\"name\":\"Hot tub thermostat\",\"uniq_id\":\"ESP82Spa_0\",\"temp_cmd_t\":\"Spa/target_temp/set\",\"mode_cmd_t\":\"Spa/heat_mode/set\",\"mode_stat_t\":\"Spa/heat_mode/state\",\"temp_unit\": \"F\",\"curr_temp_t\":\"Spa/temperature/state\",\"temp_stat_t\":\"Spa/target_temp/state\",\"min_temp\":\"80\",\"max_temp\":\"105\",\"modes\":[\"off\", \"heat\"], \"temp_step\":\"1\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_TEMP_C[] PROGMEM = "{\"name\":\"Hot tub thermostat\",\"uniq_id\":\"ESP82Spa_0\",\"temp_cmd_t\":\"Spa/target_temp/set\",\"mode_cmd_t\":\"Spa/heat_mode/set\",\"mode_stat_t\":\"Spa/heat_mode/state\",\"temp_unit\": \"C\",\"curr_temp_t\":\"Spa/temperature/state\",\"temp_stat_t\":\"Spa/target_temp/state\",\"min_temp\":\"27\",\"max_temp\":\"40\",\"modes\":[\"off\", \"heat\"], \"temp_step\":\"0.5\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_HEAT_MODE[] PROGMEM = "{\"name\":\"Hot tub heating mode\",\"uniq_id\":\"ESP82Spa_3\",\"cmd_t\":\"Spa/heatingmode/set\",\"stat_t\":\"Spa/heatingmode/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_HEAT_STATE[] PROGMEM = "{\"name\":\"Hot tub heating state\",\"uniq_id\":\"ESP82Spa_6\",\"stat_t\":\"Spa/heatstate/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_HIGH_RANGE[] PROGMEM = "{\"name\":\"Hot tub high range\",\"uniq_id\":\"ESP82Spa_4\",\"cmd_t\":\"Spa/highrange/set\",\"stat_t\":\"Spa/highrange/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_CIRC_PUMP[] PROGMEM = "{\"name\":\"Hot tub circulation pump\",\"uniq_id\":\"ESP82Spa_5\",\"device_class\":\"power\",\"stat_t\":\"Spa/circ/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_LED_LIGHT[] PROGMEM = "{\"name\":\"Hot tub light\",\"uniq_id\":\"ESP82Spa_7\",\"cmd_t\":\"Spa/light/set\",\"stat_t\":\"Spa/light/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_PUMP_1[] PROGMEM = "{\"name\":\"Hot tub jet1\",\"uniq_id\":\"ESP82Spa_8\",\"cmd_t\":\"Spa/jet_1/set\",\"stat_t\":\"Spa/jet_1/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_PUMP_2[] PROGMEM = "{\"name\":\"Hot tub jet2\",\"uniq_id\":\"ESP82Spa_9\",\"cmd_t\":\"Spa/jet_2/set\",\"stat_t\":\"Spa/jet_2/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_BLOWER[] PROGMEM = "{\"name\":\"Hot tub blower\",\"uniq_id\":\"ESP82Spa_10\",\"cmd_t\":\"Spa/blower/set\",\"stat_t\":\"Spa/blower/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";

const char PAYLOAD_SPA_FILTER_1_CONFIG[] PROGMEM = "{\"name\":\"Filter 1 start\",\"val_tpl\": \"{{value_json.start}}\",\"uniq_id\":\"ESP82Spa_11\",\"stat_t\":\"Spa/filter1/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_FILTER_2_CONFIG[] PROGMEM = "{\"name\":\"Filter 2 start\",\"val_tpl\": \"{{value_json.start}}\",\"uniq_id\":\"ESP82Spa_12\",\"stat_t\":\"Spa/filter2/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_FILTER_1_DURATION[] PROGMEM = "{\"name\":\"Filter 1 duration\",\"val_tpl\": \"{{value_json.duration}}\",\"uniq_id\":\"ESP82Spa_13\",\"stat_t\":\"Spa/filter1/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_FILTER_2_DURATION[] PROGMEM = "{\"name\":\"Filter 2 duration\",\"val_tpl\": \"{{value_json.duration}}\",\"uniq_id\":\"ESP82Spa_14\",\"stat_t\":\"Spa/filter2/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";
const char PAYLOAD_SPA_FILTER_2_ENABLED[] PROGMEM = "{\"name\":\"Filter 2 enabled\",\"uniq_id\":\"ESP82Spa_15\",\"stat_t\":\"Spa/filter2_enabled/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}";

#endif