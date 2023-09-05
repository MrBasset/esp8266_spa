// https://github.com/ccutrer/balboa_worldwide_app/blob/master/doc/protocol.md
// Reference:https://github.com/ccutrer/balboa_worldwide_app/wiki

// Please install the needed dependencies:
// CircularBuffer
// PubSubClient

// TODO:
// HomeAssistant autodiscover - DONE
// Configuration handling -> DONE
// Proper states (rather than just ON/OFF) -> NOT SURE HOW TO SOLVE THIS
// OTA update from Firebase -> TO DO
// ARDUINOOTA -> DOESN'T WORK YET -> SOMETHING WRONG WITH MDNS
// STA Mode to configure wifi -> WIP

// +12V RED
// GND  BLACK
// A    YELLOW
// B    WHITE
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <CircularBuffer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#include <SoftwareSerial.h>

#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

#include <LiquidCrystal_I2C.h>

#include <SerialDebug.h>

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

#define VERSION "0.37.4"
String NEXTVERSION = "http://github.com/EmmanuelLM/esp8266_spa/blob/master/firmware.0.37.5.bin";

#define AUTO_TX true //if your chip needs to pull D1 high/low set this to false

#define STRON String("ON").c_str()
#define STROFF String("OFF").c_str()

//HomeAssistant autodiscover
#define HASSIO true
#define PRODUCTION false

#define TX485 D1  //find a way to skip this
#define RLY1  D5
#define RLY2  D6

#define GPIOTX D5
#define GPIORX D6

CircularBuffer<uint8_t, 35> Q_in;
CircularBuffer<uint8_t, 35> Q_out;

AsyncWebServer server(80);

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

EspSoftwareSerial::UART spaSerial;

uint8_t x, i, j;
uint8_t last_state_crc = 0x00;
uint8_t send = 0x00;
uint8_t settemp = 0x00;
uint8_t id = 0x00;
unsigned long lastrx = 0;

enum ConfigStates {
  WANT_IT,
  REQUESTED_IT,
  GOT_IT,
  PROCESSED_IT
};

enum WifiMode {
  STA,
  AP
};

WifiMode wifiState;

ConfigStates have_config = WANT_IT; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
ConfigStates have_preferences = WANT_IT; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
ConfigStates have_faultlog = WANT_IT; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
ConfigStates have_filtersettings = WANT_IT; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it

char faultlog_minutes = 0; //temp logic so we only get the fault log once per 5 minutes
char filtersettings_minutes = 0; //temp logic so we only get the filter settings once per 5 minutes

// MQTT Broker settings
String MqttIp;
String MqttUser;
String MqttPassword;

struct {
  uint8_t jet1 :2;
  uint8_t jet2 :2;
  uint8_t blower :1;
  uint8_t light :1;
  uint8_t restmode:1;
  uint8_t highrange:1;
  uint8_t padding :2;
  uint8_t hour :5;
  uint8_t minutes :6;
} SpaState;

struct {
  uint8_t pump1 :2; //this could be 1=1 speed; 2=2 speeds
  uint8_t pump2 :2;
  uint8_t pump3 :2;
  uint8_t pump4 :2;
  uint8_t pump5 :2;
  uint8_t pump6 :2;
  uint8_t light1 :1;
  uint8_t light2 :1;
  uint8_t circ :1;
  uint8_t blower :1;
  uint8_t mister :1;
  uint8_t aux1 :1;
  uint8_t aux2 :1;
  uint8_t temp_scale :1; //0 -> Farenheit, 1-> Celcius
} SpaConfig;

struct {
  uint8_t totEntry :5;
  uint8_t currEntry :5;
  uint8_t faultCode :6;
  String faultMessage;
  uint8_t daysAgo :8;
  uint8_t hour :5;
  uint8_t minutes :6;
} SpaFaultLog;

struct {
  uint8_t filt1Hour :5;
  uint8_t filt1Minute :6;
  uint8_t filt1DurationHour :5;
  uint8_t filt1DurationMinute :6;
  uint8_t filt2Enable :1;
  uint8_t filt2Hour :5;
  uint8_t filt2Minute :6;
  uint8_t filt2DurationHour :5;
  uint8_t filt2DurationMinute :6;

} SpaFilterSettings;

void _yield() {
  yield();
  mqtt.loop();
  //httpServer.handleClient();
  MDNS.update();
  ArduinoOTA.handle();
}

void print_msg(CircularBuffer<uint8_t, 35> &data) {
  String s;
  //for (i = 0; i < (Q_in[1] + 2); i++) {
  for (i = 0; i < data.size(); i++) {
    x = Q_in[i];
    if (x < 0x0A) s += "0";
    s += String(x, HEX);
    s += " ";
  }
  mqtt.publish("Spa/node/msg", s.c_str());
  _yield();
}

void decodeFault() {
  SpaFaultLog.totEntry = Q_in[5];
  SpaFaultLog.currEntry = Q_in[6];
  SpaFaultLog.faultCode = Q_in[7];
  switch (SpaFaultLog.faultCode) { // this is a inelegant way to do it, a lookup table would be better
    case 15:
      SpaFaultLog.faultMessage = "Sensors are out of sync";
      break;
    case 16:
      SpaFaultLog.faultMessage = "The water flow is low";
      break;
    case 17:
      SpaFaultLog.faultMessage = "The water flow has failed";
      break;
    case 18:
      SpaFaultLog.faultMessage = "The settings have been reset";
      break;
    case 19:
      SpaFaultLog.faultMessage = "Priming Mode";
      break;
    case 20:
      SpaFaultLog.faultMessage = "The clock has failed";
      break;
    case 21:
      SpaFaultLog.faultMessage = "The settings have been reset";
      break;
    case 22:
      SpaFaultLog.faultMessage = "Program memory failure";
      break;
    case 26:
      SpaFaultLog.faultMessage = "Sensors are out of sync -- Call for service";
      break;
    case 27:
      SpaFaultLog.faultMessage = "The heater is dry";
      break;
    case 28:
      SpaFaultLog.faultMessage = "The heater may be dry";
      break;
    case 29:
      SpaFaultLog.faultMessage = "The water is too hot";
      break;
    case 30:
      SpaFaultLog.faultMessage = "The heater is too hot";
      break;
    case 31:
      SpaFaultLog.faultMessage = "Sensor A Fault";
      break;
    case 32:
      SpaFaultLog.faultMessage = "Sensor B Fault";
      break;
    case 34:
      SpaFaultLog.faultMessage = "A pump may be stuck on";
      break;
    case 35:
      SpaFaultLog.faultMessage = "Hot fault";
      break;
    case 36:
      SpaFaultLog.faultMessage = "The GFCI test failed";
      break;
    case 37:
      SpaFaultLog.faultMessage = "Standby Mode (Hold Mode)";
      break;
    default:
      SpaFaultLog.faultMessage = "Unknown error";
      break;
  }
  SpaFaultLog.daysAgo = Q_in[8];
  SpaFaultLog.hour = Q_in[9];
  SpaFaultLog.minutes = Q_in[10];
  mqtt.publish("Spa/fault/Entries", String(SpaFaultLog.totEntry).c_str());
  mqtt.publish("Spa/fault/Entry", String(SpaFaultLog.currEntry).c_str());
  mqtt.publish("Spa/fault/Code", String(SpaFaultLog.faultCode).c_str());
  mqtt.publish("Spa/fault/Message", SpaFaultLog.faultMessage.c_str());
  mqtt.publish("Spa/fault/DaysAgo", String(SpaFaultLog.daysAgo).c_str());
  mqtt.publish("Spa/fault/Hours", String(SpaFaultLog.hour).c_str());
  mqtt.publish("Spa/fault/Minutes", String(SpaFaultLog.minutes).c_str());
  have_faultlog = GOT_IT;
  //mqtt.publish("Spa/debug/have_faultlog", "have the faultlog, #2");
}

void decodeFilterSettings() {
  String s;
  String d;
  String payld;

  SpaFilterSettings.filt1Hour = Q_in[5];
  SpaFilterSettings.filt1Minute = Q_in[6];
  SpaFilterSettings.filt1DurationHour = Q_in[7];
  SpaFilterSettings.filt1DurationMinute = Q_in[8];
  SpaFilterSettings.filt2Enable = bitRead(Q_in[9],7); // check
  SpaFilterSettings.filt2Hour = Q_in[9] ^ (SpaFilterSettings.filt2Enable << 7); // check
  SpaFilterSettings.filt2Minute = Q_in[10];
  SpaFilterSettings.filt2DurationHour = Q_in[11];
  SpaFilterSettings.filt2DurationMinute = Q_in[12];
  //MQTT stuff
  /*mqtt.publish("Spa/config/filt1Hour", String(SpaFilterSettings.filt1Hour).c_str());
  mqtt.publish("Spa/config/filt1Minute", String(SpaFilterSettings.filt1Minute).c_str());
  mqtt.publish("Spa/config/filt1DurationHour", String(SpaFilterSettings.filt1DurationHour).c_str());
  mqtt.publish("Spa/config/filt1DurationMinute", String(SpaFilterSettings.filt1DurationMinute).c_str());
  mqtt.publish("Spa/config/filt2Hour", String(SpaFilterSettings.filt2Hour).c_str());
  mqtt.publish("Spa/config/filt2Minute", String(SpaFilterSettings.filt2Minute).c_str());
  mqtt.publish("Spa/config/filt2DurationHour", String(SpaFilterSettings.filt2DurationHour).c_str());
  mqtt.publish("Spa/config/filt2DurationMinute", String(SpaFilterSettings.filt2DurationMinute).c_str());
  mqtt.publish("Spa/config/filt2Enable", String(SpaFilterSettings.filt2Enable).c_str());*/

  //Filter 1 time conversion
  if (SpaFilterSettings.filt1Hour < 10) s = "0"; else s = "";
  s = String(SpaFilterSettings.filt1Hour) + ":";
  if (SpaFilterSettings.filt1Minute < 10) s += "0";
  s += String(SpaFilterSettings.filt1Minute);

  if (SpaFilterSettings.filt1DurationHour < 10) d = "0"; else d = "";
  d = String(SpaFilterSettings.filt1DurationHour) + ":";
  if (SpaFilterSettings.filt1DurationMinute < 10) d += "0";
  d += String(SpaFilterSettings.filt1DurationMinute);

  payld = "{\"start\":\""+s+"\",\"duration\":\""+d+"\"}";
  mqtt.publish("Spa/filter1/state", payld.c_str());

  //Filter 2 time conversion
  if (SpaFilterSettings.filt2Hour < 10) s = "0"; else s = "";
  s += String(SpaFilterSettings.filt2Hour) + ":";
  if (SpaFilterSettings.filt2Minute < 10) s += "0";
  s += String(SpaFilterSettings.filt2Minute);

  if (SpaFilterSettings.filt2DurationHour < 10) d = "0"; else d = "";
  d += String(SpaFilterSettings.filt2DurationHour) + ":";
  if (SpaFilterSettings.filt2DurationMinute < 10) d += "0";
  d += String(SpaFilterSettings.filt2DurationMinute);
  if ((int)(SpaFilterSettings.filt2Enable) == 1) mqtt.publish("Spa/filter2_enabled/state", STRON); else mqtt.publish("Spa/filter2_enabled/state", STROFF);


  payld = "{\"start\":\""+s+"\",\"duration\":\""+d+"\"}";
  mqtt.publish("Spa/filter2/state", payld.c_str());

  have_filtersettings = GOT_IT;
}

void decodePreferences() {
  mqtt.publish("Spa/debug/preferences/status", "Got preferences");
  mqtt.publish("Spa/debug/preferences/msg", String(Q_in[3]).c_str());
  
  SpaConfig.temp_scale = Q_in[3]; //Read temperature scale - 0 -> Farenheit, 1-> Celcius

  mqtt.publish("Spa/config/temp_scale", String(SpaConfig.temp_scale).c_str());
  have_preferences = GOT_IT;
}

void decodeSettings() {

  SpaConfig.pump1 = Q_in[5] & 0x03;
  SpaConfig.pump2 = (Q_in[5] & 0x0C) >> 2;
  SpaConfig.pump3 = (Q_in[5] & 0x30) >> 4;
  SpaConfig.pump4 = (Q_in[5] & 0xC0) >> 6;
  SpaConfig.pump5 = (Q_in[6] & 0x03);
  SpaConfig.pump6 = (Q_in[6] & 0xC0) >> 6;
  SpaConfig.light1 = (Q_in[7] & 0x03);
  SpaConfig.light2 = (Q_in[7] >> 2) & 0x03;
  SpaConfig.circ = ((Q_in[8] & 0x80) != 0);
  SpaConfig.blower = ((Q_in[8] & 0x03) != 0);
  SpaConfig.mister = ((Q_in[9] & 0x30) != 0);
  SpaConfig.aux1 = ((Q_in[9] & 0x01) != 0);
  SpaConfig.aux2 = ((Q_in[9] & 0x02) != 0);
  mqtt.publish("Spa/config/pumps1", String(SpaConfig.pump1).c_str());
  mqtt.publish("Spa/config/pumps2", String(SpaConfig.pump2).c_str());
  mqtt.publish("Spa/config/pumps3", String(SpaConfig.pump3).c_str());
  mqtt.publish("Spa/config/pumps4", String(SpaConfig.pump4).c_str());
  mqtt.publish("Spa/config/pumps5", String(SpaConfig.pump5).c_str());
  mqtt.publish("Spa/config/pumps6", String(SpaConfig.pump6).c_str());
  mqtt.publish("Spa/config/light1", String(SpaConfig.light1).c_str());
  mqtt.publish("Spa/config/light2", String(SpaConfig.light2).c_str());
  mqtt.publish("Spa/config/circ", String(SpaConfig.circ).c_str());
  mqtt.publish("Spa/config/blower", String(SpaConfig.blower).c_str());
  mqtt.publish("Spa/config/mister", String(SpaConfig.mister).c_str());
  mqtt.publish("Spa/config/aux1", String(SpaConfig.aux1).c_str());
  mqtt.publish("Spa/config/aux2", String(SpaConfig.aux2).c_str());
  have_config = GOT_IT;
}

void decodeState() {
  String s;
  double d = 0.0;
  double c = 0.0;

  // DEBUG for finding meaning:
  //print_msg(Q_in);

  // 25:Flag Byte 20 - Set Temperature
  if (SpaConfig.temp_scale == 0) {
    d = Q_in[25];
  } else if (SpaConfig.temp_scale == 1){
    d = Q_in[25] / 2;
    if (Q_in[25] % 2 == 1) d += 0.5;
  }

  mqtt.publish("Spa/target_temp/state", String(d, 2).c_str());

  // 7:Flag Byte 2 - Actual temperature
  if (Q_in[7] != 0xFF) {
    if (SpaConfig.temp_scale == 0) {
      d = Q_in[7];
    } else if (SpaConfig.temp_scale == 1){
      d = Q_in[7] / 2;
      if (Q_in[7] % 2 == 1) d += 0.5;
    }

    if (c > 0) {
      if ((d > c * 1.2) || (d < c * 0.8)) d = c; //remove spurious readings greater or less than 20% away from previous read
    }

    mqtt.publish("Spa/temperature/state", String(d, 2).c_str());
    c = d;
  } else {
    d = 0;
  }
  // REMARK Move upper publish to HERE to get 0 for unknown temperature

  // 8:Flag Byte 3 Hour & 9:Flag Byte 4 Minute => Time
  if (Q_in[8] < 10) s = "0"; else s = "";
  SpaState.hour = Q_in[8];
  s += String(Q_in[8]) + ":";
  if (Q_in[9] < 10) s += "0";
  s += String(Q_in[9]);
  SpaState.minutes = Q_in[9];
  mqtt.publish("Spa/time/state", s.c_str());

  // 10:Flag Byte 5 - Heating Mode
  switch (Q_in[10]) {
    case 0:mqtt.publish("Spa/heatingmode/state", STRON); //Ready
      mqtt.publish("Spa/heat_mode/state", "heat"); //Ready
      SpaState.restmode = 0;
      break;
    case 3:// Ready-in-Rest
      SpaState.restmode = 0;
      break;
    case 1:mqtt.publish("Spa/heatingmode/state", STROFF); //Rest
      mqtt.publish("Spa/heat_mode/state", "off"); //Rest
      SpaState.restmode = 1;
      break;
  }

  // 15:Flags Byte 10 / Heat status, Temp Range
  d = bitRead(Q_in[15], 4);
  if (d == 0) mqtt.publish("Spa/heatstate/state", STROFF);
  else if (d == 1 || d == 2) mqtt.publish("Spa/heatstate/state", STRON);

  d = bitRead(Q_in[15], 2);
  if (d == 0) {
    mqtt.publish("Spa/highrange/state", STROFF); //LOW
    SpaState.highrange = 0;
  } else if (d == 1) {
    mqtt.publish("Spa/highrange/state", STRON); //HIGH
    SpaState.highrange = 1;
  }

  // 16:Flags Byte 11
  if (bitRead(Q_in[16], 1) == 1) {
    mqtt.publish("Spa/jet_1/state", STRON);
    SpaState.jet1 = 1;
  } else {
    mqtt.publish("Spa/jet_1/state", STROFF);
    SpaState.jet1 = 0;
  }

  if (bitRead(Q_in[16], 3) == 1) {
    mqtt.publish("Spa/jet_2/state", STRON);
    SpaState.jet2 = 1;
  } else {
    mqtt.publish("Spa/jet_2/state", STROFF);
    SpaState.jet2 = 0;
  }

  // 18:Flags Byte 13
  if (bitRead(Q_in[18], 1) == 1)
    mqtt.publish("Spa/circ/state", STRON);
  else
    mqtt.publish("Spa/circ/state", STROFF);

  if (bitRead(Q_in[18], 2) == 1) {
    mqtt.publish("Spa/blower/state", STRON);
    SpaState.blower = 1;
  } else {
    mqtt.publish("Spa/blower/state", STROFF);
    SpaState.blower = 0;
  }
  // 19:Flags Byte 14
  if (Q_in[19] == 0x03) {
    mqtt.publish("Spa/light/state", STRON);
    SpaState.light = 1;
  } else {
    mqtt.publish("Spa/light/state", STROFF);
    SpaState.light = 0;
  }

  last_state_crc = Q_in[Q_in[1]];

  // Publish own relay states
  s = "OFF";
  if (digitalRead(RLY1) == LOW) s = "ON";
  mqtt.publish("Spa/relay_1/state", s.c_str());

  s = "OFF";
  if (digitalRead(RLY2) == LOW) s = "ON";
  mqtt.publish("Spa/relay_2/state", s.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void hardreset() {
  ESP.wdtDisable();
  while (1) {};
}

void mqttpubsub() {
  // ONLY DO THE FOLLOWING IF have_config == true otherwise it will not work
  String Payload;

  // ... Hassio autodiscover
  if (HASSIO) {

      //clear topics:
      mqtt.publish("homeassistant/binary_sensor/Spa", "");
      mqtt.publish("homeassistant/sensor/Spa", "");
      mqtt.publish("homeassistant/switch/Spa", "");
      mqtt.publish("/Spa", "");

      //temperature -> can we try and remove the Payload below, it's messy
      Payload = "{\"name\":\"Hot tub status\",\"uniq_id\":\"ESP82Spa_1\",\"stat_t\":\"Spa/node/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"],\"name\":\"Esp Spa\",\"sw\":\""+String(VERSION)+"\"}}";
      mqtt.publish("homeassistant/binary_sensor/Spa/state/config", Payload.c_str(), true);
      //climate temperature
      if (SpaConfig.temp_scale == 0) {
        mqtt.publish("Spa/debug/temparture/publish", "True");
        mqtt.publish("homeassistant/climate/Spa/temperature/config", "{\"name\":\"Hot tub thermostat\",\"uniq_id\":\"ESP82Spa_0\",\"temp_cmd_t\":\"Spa/target_temp/set\",\"mode_cmd_t\":\"Spa/heat_mode/set\",\"mode_stat_t\":\"Spa/heat_mode/state\",\"temp_unit\": \"F\",\"curr_temp_t\":\"Spa/temperature/state\",\"temp_stat_t\":\"Spa/target_temp/state\",\"min_temp\":\"80\",\"max_temp\":\"105\",\"modes\":[\"off\", \"heat\"], \"temp_step\":\"1\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      } else if (SpaConfig.temp_scale == 1) {
        mqtt.publish("Spa/debug/temparture/publish", "True");
        mqtt.publish("homeassistant/climate/Spa/temperature/config", "{\"name\":\"Hot tub thermostat\",\"uniq_id\":\"ESP82Spa_0\",\"temp_cmd_t\":\"Spa/target_temp/set\",\"mode_cmd_t\":\"Spa/heat_mode/set\",\"mode_stat_t\":\"Spa/heat_mode/state\",\"temp_unit\": \"C\",\"curr_temp_t\":\"Spa/temperature/state\",\"temp_stat_t\":\"Spa/target_temp/state\",\"min_temp\":\"27\",\"max_temp\":\"40\",\"modes\":[\"off\", \"heat\"], \"temp_step\":\"0.5\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      }
      //heat mode
      mqtt.publish("homeassistant/switch/Spa/heatingmode/config", "{\"name\":\"Hot tub heating mode\",\"uniq_id\":\"ESP82Spa_3\",\"cmd_t\":\"Spa/heatingmode/set\",\"stat_t\":\"Spa/heatingmode/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      //heating state
      mqtt.publish("homeassistant/binary_sensor/Spa/heatstate/config", "{\"name\":\"Hot tub heating state\",\"uniq_id\":\"ESP82Spa_6\",\"stat_t\":\"Spa/heatstate/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      //high range
      mqtt.publish("homeassistant/switch/Spa/highrange/config", "{\"name\":\"Hot tub high range\",\"uniq_id\":\"ESP82Spa_4\",\"cmd_t\":\"Spa/highrange/set\",\"stat_t\":\"Spa/highrange/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);

      //OPTIONAL ELEMENTS
      if (SpaConfig.circ){
        //circulation pump
        mqtt.publish("homeassistant/binary_sensor/Spa/circ/config", "{\"name\":\"Hot tub circulation pump\",\"uniq_id\":\"ESP82Spa_5\",\"device_class\":\"power\",\"stat_t\":\"Spa/circ/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      }
      if (SpaConfig.light1) {
        //light 1
        mqtt.publish("homeassistant/switch/Spa/light/config", "{\"name\":\"Hot tub light\",\"uniq_id\":\"ESP82Spa_7\",\"cmd_t\":\"Spa/light/set\",\"stat_t\":\"Spa/light/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      }
      if (SpaConfig.pump1 != 0) {
        //jets 1
        mqtt.publish("homeassistant/switch/Spa/jet_1/config", "{\"name\":\"Hot tub jet1\",\"uniq_id\":\"ESP82Spa_8\",\"cmd_t\":\"Spa/jet_1/set\",\"stat_t\":\"Spa/jet_1/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      }
      if (SpaConfig.pump2 != 0) {
        //jets 2
        mqtt.publish("homeassistant/switch/Spa/jet_2/config", "{\"name\":\"Hot tub jet2\",\"uniq_id\":\"ESP82Spa_9\",\"cmd_t\":\"Spa/jet_2/set\",\"stat_t\":\"Spa/jet_2/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      }
      if (SpaConfig.blower)
      {
        //blower
        mqtt.publish("homeassistant/switch/Spa/blower/config", "{\"name\":\"Hot tub blower\",\"uniq_id\":\"ESP82Spa_10\",\"cmd_t\":\"Spa/blower/set\",\"stat_t\":\"Spa/blower/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      }

      mqtt.publish("homeassistant/sensor/Spa/filter1_start/config", "{\"name\":\"Filter 1 start\",\"val_tpl\": \"{{value_json.start}}\",\"uniq_id\":\"ESP82Spa_11\",\"stat_t\":\"Spa/filter1/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      mqtt.publish("homeassistant/sensor/Spa/filter2_start/config", "{\"name\":\"Filter 2 start\",\"val_tpl\": \"{{value_json.start}}\",\"uniq_id\":\"ESP82Spa_12\",\"stat_t\":\"Spa/filter2/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      mqtt.publish("homeassistant/sensor/Spa/filter1_duration/config", "{\"name\":\"Filter 1 duration\",\"val_tpl\": \"{{value_json.duration}}\",\"uniq_id\":\"ESP82Spa_13\",\"stat_t\":\"Spa/filter1/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      mqtt.publish("homeassistant/sensor/Spa/filter2_duration/config", "{\"name\":\"Filter 2 duration\",\"val_tpl\": \"{{value_json.duration}}\",\"uniq_id\":\"ESP82Spa_14\",\"stat_t\":\"Spa/filter2/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
      mqtt.publish("homeassistant/binary_sensor/Spa/filter2_enabled/config", "{\"name\":\"Filter 2 enabled\",\"uniq_id\":\"ESP82Spa_15\",\"stat_t\":\"Spa/filter2_enabled/state\",\"platform\":\"mqtt\",\"dev\":{\"ids\":[\"ESP82Spa\"]}}", true);
  }

  mqtt.publish("Spa/node/state", "ON");
  mqtt.publish("Spa/node/debug", "RECONNECT");
  //mqtt.publish("Spa/node/debug", String(millis()).c_str());
  //mqtt.publish("Spa/node/debug", String(oldstate).c_str());
  mqtt.publish("Spa/node/version", VERSION);
  mqtt.publish("Spa/node/flashsize", String(ESP.getFlashChipRealSize()).c_str());
  mqtt.publish("Spa/node/chipid", String(ESP.getChipId()).c_str());
	mqtt.publish("Spa/node/speed", String(ESP.getCpuFreqMHz()).c_str());

  // ... and resubscribe
  mqtt.subscribe("Spa/command");
  mqtt.subscribe("Spa/target_temp/set");
  mqtt.subscribe("Spa/heatingmode/set");
  mqtt.subscribe("Spa/heat_mode/set");
  mqtt.subscribe("Spa/highrange/set");

  //OPTIONAL ELEMENTS
  if (SpaConfig.pump1 != 0) {
    mqtt.subscribe("Spa/jet_1/set");
  }
  if (SpaConfig.pump2 != 0) {
    mqtt.subscribe("Spa/jet_2/set");
  }
  if (SpaConfig.blower) {
    mqtt.subscribe("Spa/blower/set");
  }
  if (SpaConfig.light1) {
    mqtt.subscribe("Spa/light/set");
  }

  mqtt.subscribe("Spa/relay_1/set");
  mqtt.subscribe("Spa/relay_2/set");

  //not sure what this is
  last_state_crc = 0x00;

  //done with config
  have_config = PROCESSED_IT;
  have_preferences = PROCESSED_IT;
}

void reconnect() {
  //int oldstate = mqtt.state();
  //boolean connection = false;
  // Loop until we're reconnected
  if (!mqtt.connected()) {
    // Attempt to connect
    if (MqttPassword == "") {
      //connection =
      mqtt.connect(String(String("Spa") + String(millis())).c_str());
    }
    else {
      //connection =
      mqtt.connect(String(String("Spa") + String(millis())).c_str(), MqttUser.c_str(), MqttPassword.c_str());
    }
    //time to connect
    delay(1000);

    //have_config = 2;
    if (have_config == PROCESSED_IT && have_preferences == PROCESSED_IT) {
      //have_config = 2; // we have disconnected, let's republish our configuration
      mqttpubsub();
    }

  }
  mqtt.setBufferSize(512); //increase pubsubclient buffer size
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte * p_payload, unsigned int p_length) {
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }
  String topic = String(p_topic);

  mqtt.publish("Spa/node/debug", topic.c_str());
  _yield();

  // handle message topic
  if (topic.startsWith("Spa/relay_")) {
    bool newstate = 0;

    if (payload.equals("ON")) newstate = LOW;
    else if (payload.equals("OFF")) newstate = HIGH;

    if (topic.charAt(10) == '1') {
      pinMode(RLY1, INPUT);
      delay(25);
      pinMode(RLY1, OUTPUT);
      digitalWrite(RLY1, newstate);
    }
    else if (topic.charAt(10) == '2') {
      pinMode(RLY2, INPUT);
      delay(25);
      pinMode(RLY2, OUTPUT);
      digitalWrite(RLY2, newstate);
    }
  } else if (topic.equals("Spa/command")) {
    if (payload.equals("reset")) hardreset();
  } else if (topic.equals("Spa/heatingmode/set")) {
    if (payload.equals("ON") && SpaState.restmode == 1) send = 0x51; // ON = Ready; OFF = Rest
    else if (payload.equals("OFF") && SpaState.restmode == 0) send = 0x51;
  } else if (topic.equals("Spa/heat_mode/set")) {
    if (payload.equals("heat") && SpaState.restmode == 1) send = 0x51; // ON = Ready; OFF = Rest
    else if (payload.equals("off") && SpaState.restmode == 0) send = 0x51;
  } else if (topic.equals("Spa/light/set")) {
    if (payload.equals("ON") && SpaState.light == 0) send = 0x11;
    else if (payload.equals("OFF") && SpaState.light == 1) send = 0x11;
  } else if (topic.equals("Spa/jet_1/set")) {
    if (payload.equals("ON") && SpaState.jet1 == 0) send = 0x04;
    else if (payload.equals("OFF") && SpaState.jet1 == 1) send = 0x04;
  } else if (topic.equals("Spa/jet_2/set")) {
    if (payload.equals("ON") && SpaState.jet2 == 0) send = 0x05;
    else if (payload.equals("OFF") && SpaState.jet2 == 1) send = 0x05;
  } else if (topic.equals("Spa/blower/set")) {
    if (payload.equals("ON") && SpaState.blower == 0) send = 0x0C;
    else if (payload.equals("OFF") && SpaState.blower == 1) send = 0x0C;
  } else if (topic.equals("Spa/highrange/set")) {
    if (payload.equals("ON") && SpaState.highrange == 0) send = 0x50; //ON = High, OFF = Low
    else if (payload.equals("OFF") && SpaState.highrange == 1) send = 0x50;
  } else if (topic.equals("Spa/target_temp/set")) {
    // Get new set temperature
    double d = payload.toDouble();
    if (d > 0) d *= 2; // Convert to internal representation
    settemp = d;
    send = 0xff;
  }
}

/// UPDATE FILESYSTE
void update_started() {
  mqtt.publish("Spa/node/debug", "Updated started");
}

void update_finished() {
  mqtt.publish("Spa/node/debug", "Updated finished");
}

void update_progress(int cur, int total) {
  mqtt.publish("Spa/node/debug", "Update in progress");
}

void update_error(int err) {
  mqtt.publish("Spa/node/debug", "Updated error");
}

void lcd_init() {
  lcd.init();
  lcd.clear();         
  lcd.backlight();
}

void lcd_print(const char line1[], const char line2[]) {
  lcd.clear();
  lcd.setCursor(0,1); // character 0 line 2
  lcd.print(line1);
  lcd.setCursor(0,2); // character 0 line 3
  lcd.print(line2);
}

void lcd_print_msg(CircularBuffer<uint8_t, 35> &data) {
  String s;
  //for (i = 0; i < (Q_in[1] + 2); i++) {
  for (i = 0; i < data.size(); i++) {
    x = Q_in[i];
    if (x < 0x0A) s += "0";
    s += String(x, HEX);
    s += " ";
  }

  lcd_print("Message", s.c_str());
  _yield();
}

uint8_t crc8(CircularBuffer<uint8_t, 35> &data) {
  unsigned long crc;
  int i, bit;
  uint8_t length = data.size();

  crc = 0x02;
  for ( i = 0 ; i < length ; i++ ) {
    crc ^= data[i];
    for ( bit = 0 ; bit < 8 ; bit++ ) {
      if ( (crc & 0x80) != 0 ) {
        crc <<= 1;
        crc ^= 0x7;
      }
      else {
        crc <<= 1;
      }
    }
  }

  return crc ^ 0x02;
}

void rs485_send() {
  // The following is not required for the new RS485 chip
#if !AUTO_TX
  digitalWrite(TX485, HIGH);
  delay(1);
#endif


  // Add telegram length
  Q_out.unshift(Q_out.size() + 2);

  // Add CRC
  Q_out.push(crc8(Q_out));

  // Wrap telegram in SOF/EOF
  Q_out.unshift(0x7E);
  Q_out.push(0x7E);

  for (i = 0; i < Q_out.size(); i++)
    spaSerial.write(Q_out[i]);

  //print_msg(Q_out);

  spaSerial.flush();

#if !AUTO_TX
    digitalWrite(TX485, LOW);
#endif

  // DEBUG: print_msg(Q_out);
  Q_out.clear();
}

void ID_request() {
  Q_out.push(0xFE);
  Q_out.push(0xBF);
  Q_out.push(0x01);
  Q_out.push(0x02);
  Q_out.push(0xF1);
  Q_out.push(0x73);

  rs485_send();
}

void ID_ack() {
  Q_out.push(id);
  Q_out.push(0xBF);
  Q_out.push(0x03);

  rs485_send();
}

///////////////////////////////////////////////////////////////////////////////

void setup() {
  SerialDebug.init();

  lcd_init();

  if (LittleFS.begin()) {
    if (LittleFS.exists("/ip.json")) {
      //WiFi Config exists, grab the config and connect as normal
      DynamicJsonDocument jsonSettings(1024);

      String error_msg = "";

      lcd_print("Fetching settings", "");
      SerialDebug.print("Fetching Settings");

      String WiFiSsid;
      String WiFiPassword;

      File file = LittleFS.open("/ip.txt", "r");
      if (!file) {
        SerialDebug.print("could not open file for reading");        
        ESP.restart();
      } else {
        deserializeJson(jsonSettings, file);

        WiFiSsid = jsonSettings["WIFI_SSID"].as<String>();
        WiFiPassword = jsonSettings["WIFI_PASSWORD"].as<String>();
        MqttIp = jsonSettings["BROKER"].as<String>();
        MqttUser = jsonSettings["BROKER_LOGIN"].as<String>();
        MqttPassword = jsonSettings["BROKER_PASS"].as<String>();
        SerialDebug.print("Successfully read the configuration");
      }
      file.close();

      //LittleFS.end();

      // Begin RS485 in listening mode -> no longer required with new RS485 chip
#if !AUTO_TX
      pinMode(TX485, OUTPUT);
      digitalWrite(TX485, LOW);
#endif

      SerialDebug.initWeb(&server);

        // Route for root index.html
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send(LittleFS, "/index.html", "text/html"); });

      lcd_print("Setting up Serial Busses", "");
      SerialDebug.print("Setting up software serial");

      //pinMode(RLY1, OUTPUT);
      //digitalWrite(RLY1, HIGH);
      //pinMode(RLY2, OUTPUT);
      //digitalWrite(RLY2, HIGH);

      // Spa communication, 115.200 baud 8N1
      spaSerial.begin(115200,EspSoftwareSerial::SWSERIAL_8N1, GPIORX, GPIOTX, false); // Start software serial
      spaSerial.enableIntTx(false); // This is needed with high baudrates

      // give Spa time to wake up after POST
      for (uint8_t i = 0; i < 5; i++) {
        delay(1000);
        yield();
      }

      Q_in.clear();
      Q_out.clear();

      lcd_print("Connecting to WiFi", "");
      SerialDebug.print(F("Connecting to WiFi"));

      WiFi.mode(WIFI_STA);
      WiFi.setOutputPower(20.5); // this sets wifi to highest power
      WiFi.begin(WiFiSsid.c_str(), WiFiPassword.c_str());
      unsigned long timeout = millis() + 10000;

      while (WiFi.status() != WL_CONNECTED && millis() < timeout) {
        yield();
      }

      lcd_print("Connected to WiFi", "");
      SerialDebug.print(F("WiFi Connected"));

      // Reset because of no connection
      if (WiFi.status() != WL_CONNECTED) {

        lcd_print("Failed to connect to WiFi", "");

        // SAVE WIFI SETTINGS TO FILESYSTEM
        ESP.restart();
      }

      lcd_print("IP address:", WiFi.localIP().toString().c_str());
      SerialDebug.print(F("IP address: "), false);
      SerialDebug.print(WiFi.localIP());

      SerialDebug.print(F("Arudino OTA Starting"));

      ArduinoOTA.setPort(8266);
      ArduinoOTA.setHostname("esp-spa");
      ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

      ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
          type = "sketch";
        } else {  // U_FS
          type = "filesystem";
        }

        // NOTE: if updating FS this would be the place to unmount FS using FS.end()
        SerialDebug.print(F("Start updating ") + type);
      });
      ArduinoOTA.onEnd([]() {
        SerialDebug.print("\nEnd");
      });
      ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        WebSerial.printf("Progress: %u%%\r", (progress / (total / 100)));
      });
      ArduinoOTA.onError([](ota_error_t error) {
        WebSerial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
          SerialDebug.print("Auth Failed");
        } else if (error == OTA_BEGIN_ERROR) {
          SerialDebug.print("Begin Failed");
        } else if (error == OTA_CONNECT_ERROR) {
          SerialDebug.print("Connect Failed");
        } else if (error == OTA_RECEIVE_ERROR) {
          SerialDebug.print("Receive Failed");
        } else if (error == OTA_END_ERROR) {
          SerialDebug.print("End Failed");
        }
      });

      ArduinoOTA.begin();

      SerialDebug.print("MQTT Starting");

      mqtt.setServer(MqttIp.c_str(), 1883);
      mqtt.setCallback(callback);
      mqtt.setKeepAlive(10);
      mqtt.setSocketTimeout(20);

      //MDNS.begin("spa");
      MDNS.addService("http", "tcp", 80);

      /*the below is for debug purposes*/
      mqtt.connect("Spa1", MqttUser.c_str(), MqttPassword.c_str());
      mqtt.publish("Spa/debug/wifi_ssid", WiFiSsid.c_str());
      mqtt.publish("Spa/debug/broker", MqttIp.c_str());
      mqtt.publish("Spa/debug/broker_login", MqttUser.c_str());
      mqtt.publish("Spa/debug/error", error_msg.c_str());
      
      SerialDebug.print("MQTT Started");

      //lcd.noBacklight();
      wifiState = STA;
    }
    else
    {
      //WiFi Config doesn't exist, start up in AP mode
      SerialDebug.print(F("WiFi config doesn't exist. Setting AP mode"));

      WiFi.softAP("ESP-SPA", NULL);

      IPAddress IP = WiFi.softAPIP();
      SerialDebug.print("AP IP address: ", false);
      SerialDebug.print(IP);

      lcd_print("IP address:", IP.toString().c_str());

      // Web Server Root URL
      SerialDebug.print(F("Set up wifi manager index page"));
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/wifimanager.html", "text/html");
      });
      server.on("/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/bootstrap.min.js.gz", "application/javascipt");
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
      });
      server.on("/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request){
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/bootstrap.min.css.gz", "text/css");
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
      });
            
      SerialDebug.print(F("Handle wifi manager submit"));
      server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {

        SerialDebug.print(F("Got POST with settings"));
        DynamicJsonDocument jsonSettings(1024);

        int params = request->params();
        for(int i=0;i<params;i++){
          AsyncWebParameter* p = request->getParam(i);
          if(p->isPost()) {
            // HTTP POST ssid value
            if (p->name() == "ssid") {
              String ssid = p->value().c_str();

              SerialDebug.print("WiFi SSID is " + ssid);

              jsonSettings["WIFI_SSID"] = ssid.c_str();
            }
            // HTTP POST pass value
            if (p->name() == "wifipassword") {
              String pass = p->value().c_str();

              SerialDebug.print(F("WiFi Password is ") + pass);

              jsonSettings["WIFI_SSID"] = pass.c_str();
            }
            // HTTP POST MQTT ip value
            if (p->name() == "brokerip") {
              String ip = p->value().c_str();

              SerialDebug.print(F("MQTT IP ") + ip);

              // Write file to save value
              jsonSettings["BROKER"] =  ip.c_str();
            }
            // HTTP POST MQTT user value
            if (p->name() == "brokeruser") {
              String user = p->value().c_str();

              SerialDebug.print(F("MQTT User ") + user);

              // Write file to save value
              jsonSettings["BROKER_LOGIN"] =  user.c_str();
            }
            // HTTP POST MQTT password value
            if (p->name() == "brokerpassword") {
              String pass = p->value().c_str();

              SerialDebug.print(F("MQTT Password ") + pass);

              // Write file to save value
              jsonSettings["BROKER_PASS"] =  pass.c_str();
            }

            File f = LittleFS.open("/ip.json", "w");
            if (!f) {
              SerialDebug.print("failed to create file");
            }

            if (serializeJson(jsonSettings, f) == 0) {
              SerialDebug.print(F("failed to write file"));
            }

            SerialDebug.print(F("Config file written, restarting"));

            f.close();
          }
        }

        request->send(200, "text/plain", "Done. ESP will restart.");
        delay(3000);
        ESP.restart();
      });

      server.serveStatic("/", LittleFS, "/").setDefaultFile("wifimanager.html");;

      SerialDebug.print(F("Start the server"));
      server.begin();
      SerialDebug.print(F("Set up MDNS"));
      MDNS.begin("eps-spa");
      MDNS.addService("http", "tcp", 80);
      
      wifiState = AP;
    }
  
  } else {
    SerialDebug.print(F("Could not mount fs, restarting"));
    ESP.restart();
  }
}

void loop() {
  if (wifiState == AP) return;

  if (WiFi.status() != WL_CONNECTED) ESP.restart();
  if (!mqtt.connected()) reconnect();
  if (have_config == GOT_IT && have_preferences == GOT_IT) mqttpubsub(); //do mqtt stuff after we're connected and if we have got the config elements
  //httpServer.handleClient(); needed?
  _yield();

  /*
  ESPhttpUpdate.onStart(update_started);
  ESPhttpUpdate.onEnd(update_finished);
  ESPhttpUpdate.onProgress(update_progress);
  ESPhttpUpdate.onError(update_error);
  */

  //SerialDebug.print("SpaState Minutes: " + SpaState.minutes);

  //Every x minutes, read the fault log and filter settings using SpaState,minutes, and check for updates
  if ((int)(SpaState.minutes % 5) == 0)
  {

    //SerialDebug.print("Status Reading Fault/Filter");

    //logic to only get the error message once -> this is dirty
    //have_faultlog = 0;
    if (have_faultlog == 2) { // we got the fault log before and treated it
      if (faultlog_minutes == SpaState.minutes) { // we got the fault log this interval so do nothing
      }
      else {
        faultlog_minutes = SpaState.minutes;
        have_faultlog = WANT_IT;
      }
    }
    if (have_filtersettings == 2) { // we got the filter cycles before and treated it
      if (filtersettings_minutes == SpaState.minutes) { // we got the filter cycles this interval so do nothing
      }
      else {
        filtersettings_minutes = SpaState.minutes;
        have_filtersettings = WANT_IT;
      }
    }

    //perform binary updates
    /*
    WiFiClient wifiClient2;
    t_httpUpdate_return ret = ESPhttpUpdate.update(wifiClient2, NEXTVERSION);
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        mqtt.publish("Spa/node/debug", "Update failed");
        break;

      case HTTP_UPDATE_NO_UPDATES:
        mqtt.publish("Spa/node/debug", "No update");
        break;

      case HTTP_UPDATE_OK:
        mqtt.publish("Spa/node/debug", "Update ok");
        break;
    }*/

  }

  //SerialDebug.print("Spa Serial is available: " + spaSerial.available());

  // DEBUG:mqtt.publish("Spa/rcv", String(x).c_str()); _yield();
  // Read from Spa RS485
  if (spaSerial.available()) {
    x = spaSerial.read();
    Q_in.push(x);

    //mqtt.publish("Spa/debug/rcv", String(x).c_str()); //_yield();

    //Serial.print(" " + String(x, HEX));

    // Drop until SOF is seen
    if (Q_in.first() != 0x7E) Q_in.clear();
    lastrx = millis();
  }
  // else 
  // {
  //   mqtt.publish("Spa/debug/serial", String(Serial.available()).c_str());
  // }

  // Double SOF-marker, drop last one
  if (Q_in[1] == 0x7E && Q_in.size() > 1) Q_in.pop();

  // Complete package
  //if (x == 0x7E && Q_in[0] == 0x7E && Q_in[1] != 0x7E) {
  if (x == 0x7E && Q_in.size() > 2) {
    //print_msg();

    //serial_print_msg(Q_in);

    // Unregistered or yet in progress
    if (id == 0) {
      if (Q_in[2] == 0xFE) print_msg(Q_in);

      // FE BF 02:got new client ID
      if (Q_in[2] == 0xFE && Q_in[4] == 0x02) {
        id = Q_in[5];
        if (id > 0x2F) id = 0x2F;

        ID_ack();
        mqtt.publish("Spa/node/id", String(id).c_str());
      }

      // FE BF 00:Any new clients?
      if (Q_in[2] == 0xFE && Q_in[4] == 0x00) {
        ID_request();
      }
    } else if (Q_in[2] == id && Q_in[4] == 0x06) { // we have an ID, do clever stuff
        // id BF 06:Ready to Send
        if (send == 0xff) {
          // 0xff marks dirty temperature for now
          Q_out.push(id);
          Q_out.push(0xBF);
          Q_out.push(0x20);
          Q_out.push(settemp);
        } else if (send == 0x00) {
          if (have_config == WANT_IT) { // Get configuration of the hot tub
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x22);
            Q_out.push(0x00);
            Q_out.push(0x00);
            Q_out.push(0x01);
            //mqtt.publish("Spa/config/status", "Getting config");
            have_config = REQUESTED_IT;
          } else if (have_preferences == WANT_IT) {
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x22);
            Q_out.push(0x08);
            Q_out.push(0x00);
            Q_out.push(0x00);
            //mqtt.publish("Spa/config/status", "Getting config");
            have_preferences = REQUESTED_IT;
          } else if (have_faultlog == WANT_IT) { // Get the fault log
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x22);
            Q_out.push(0x20);
            Q_out.push(0xFF);
            Q_out.push(0x00);
            have_faultlog = REQUESTED_IT;
            //mqtt.publish("Spa/debug/have_faultlog", "requesting fault log, #1");
          } else if ((have_filtersettings == WANT_IT) && (have_faultlog == GOT_IT)) { // Get the filter cycles log once we have the faultlog
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x22);
            Q_out.push(0x01);
            Q_out.push(0x00);
            Q_out.push(0x00);
            //mqtt.publish("Spa/debug/have_faultlog", "requesting filter settings, #1");
            have_filtersettings = REQUESTED_IT;
          } else {
            // A Nothing to Send message is sent by a client immediately after a Clear to Send message if the client has no messages to send.
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x07);
          }
        } else {
          // Send toggle commands
          Q_out.push(id);
          Q_out.push(0xBF);
          Q_out.push(0x11);
          Q_out.push(send);
          Q_out.push(0x00);
        }

        rs485_send();
        send = 0x00;
    } else if (Q_in[2] == id && Q_in[4] == 0x2E) {
      if (last_state_crc != Q_in[Q_in[1]]) {
        decodeSettings();
      }
    } else if (Q_in[2] == id && Q_in[4] == 0x26) {
      if (last_state_crc != Q_in[Q_in[1]]) {
        decodePreferences();
      }
    } else if (Q_in[2] == id && Q_in[4] == 0x28) {
      if (last_state_crc != Q_in[Q_in[1]]) {
        decodeFault();
      }
    } else if (Q_in[2] == 0xFF && Q_in[4] == 0x13) { // FF AF 13:Status Update - Packet index offset 5
      if (last_state_crc != Q_in[Q_in[1]]) {
        decodeState();
      }
    } else if (Q_in[2] == id && Q_in[4] == 0x23) { // FF AF 23:Filter Cycle Message - Packet index offset 5
      if (last_state_crc != Q_in[Q_in[1]]) {
        //mqtt.publish("Spa/debug/have_faultlog", "decoding filter settings");
        decodeFilterSettings();
      }
    } else {
      // DEBUG for finding meaning
      //if (Q_in[2] & 0xFE || Q_in[2] == id)
      //print_msg(Q_in);
    }

    // Clean up queue
    _yield();
    Q_in.clear();
  }

  // Long time no receive
  if (millis() - lastrx > 10000) {
    if (PRODUCTION) {
      SerialDebug.print(F("Hard reset"));
      hardreset();
    }
  }
}
