// https://github.com/ccutrer/balboa_worldwide_app/blob/master/doc/protocol.md
// Reference:https://github.com/ccutrer/balboa_worldwide_app/wiki

// Please install the needed dependencies:
// CircularBuffer
// PubSubClient

// TODO:
// Proper states (rather than just ON/OFF but OFF/LOW/HIGH) -> NOT SURE HOW TO SOLVE THIS
// Add define flags to only compile Serial.Print for flag
// Convert Serial class to use __FlashHelper pointers only so that all strings are in Progmem
// Test the speed - with fetching so much from flash will we need to add more yeilds to prevent WDT reboot
// Move SPA configuration into seperate class and use resulting object to run a seperate object once configured


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
#include <ESPAsyncWiFiManager.h>

#include <LCDisplay.h>
#include <Logger.h>

#include <DisplayStrings.h>

#define WEBSOCKET_DISABLED true

#define AUTO_TX true //if your chip needs to pull D1 high/low set this to false

//HomeAssistant autodiscover
#define HASSIO true
#define PRODUCTION false

#define TX485 D1  //find a way to skip this

#define GPIOTX D5
#define GPIORX D6

#define BUTTON D7

CircularBuffer<uint8_t, 35> Q_in;
CircularBuffer<uint8_t, 35> Q_out;

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

ConfigStates have_config = WANT_IT; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
ConfigStates have_preferences = WANT_IT; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
ConfigStates have_faultlog = WANT_IT; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it
ConfigStates have_filtersettings = WANT_IT; //stages: 0-> want it; 1-> requested it; 2-> got it; 3-> further processed it

char faultlog_minutes = 0; //temp logic so we only get the fault log once per 5 minutes
char filtersettings_minutes = 0; //temp logic so we only get the filter settings once per 5 minutes

// MQTT Broker settings
char MqttIp[40];
char MqttPort[6] = "1883";
char MqttUser[40];
char MqttPassword[40];

//flag for saving data
bool shouldSaveConfig = false;

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
  MDNS.update();
  ArduinoOTA.handle();
  LCDisplay.displayCycle();
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.print(F("Should save config"));
  shouldSaveConfig = true;
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
  mqtt.publish(String(FPSTR(MQTT_NODE_MSG)).c_str(), s.c_str());
  _yield();
}

void publishToMqtt(const __FlashStringHelper *topic, const __FlashStringHelper *payload, bool retained) {
  char cPayload[strlen_P((PGM_P)payload)];
  strcpy_P(cPayload, (PGM_P)payload);

  char cTopic[strlen_P((PGM_P)topic)];
  strcpy_P(cTopic, (PGM_P)topic);

  mqtt.publish(cTopic, cPayload, retained);
}

void publishToMqtt(const __FlashStringHelper *topic, const __FlashStringHelper *payload) {
  publishToMqtt(topic, payload, false);
}

void publishToMqtt(const __FlashStringHelper *topic, const char *payload, bool retained) {
  char cTopic[strlen_P((PGM_P)topic)];
  strcpy_P(cTopic, (PGM_P)topic);

  mqtt.publish(cTopic, payload, retained);
}

void publishToMqtt(const __FlashStringHelper *topic, const char *payload)
{
  publishToMqtt(topic, payload, false);
}

void subscribeToMqtt(const __FlashStringHelper *topic)
{
  char cTopic[strlen_P((PGM_P)topic)];
  strcpy_P(cTopic, (PGM_P)topic);

  mqtt.subscribe(cTopic);
}

void decodeFault() {
  SpaFaultLog.totEntry = Q_in[5];
  SpaFaultLog.currEntry = Q_in[6];
  SpaFaultLog.faultCode = Q_in[7];

  char buffer[45]; // buffer for reading the string to (needs to be large enough to take the longest string
  if (SpaFaultLog.faultCode <= 37) {
    strcpy_P(buffer, (char*)pgm_read_dword(&(FAULT_TABLE[SpaFaultLog.faultCode - 15])));
  } else {
    strcpy_P(buffer, (PGM_P)FAULT_UNKNOWN);
  }
  SpaFaultLog.faultMessage = buffer;

  SpaFaultLog.daysAgo = Q_in[8];
  SpaFaultLog.hour = Q_in[9];
  SpaFaultLog.minutes = Q_in[10];

  publishToMqtt(F("Spa/fault/Entries"), String(SpaFaultLog.totEntry).c_str());
  publishToMqtt(F("Spa/fault/Entry"), String(SpaFaultLog.currEntry).c_str());
  publishToMqtt(F("Spa/fault/Code"), String(SpaFaultLog.faultCode).c_str());
  publishToMqtt(F("Spa/fault/Message"), SpaFaultLog.faultMessage.c_str());
  publishToMqtt(F("Spa/fault/DaysAgo"), String(SpaFaultLog.daysAgo).c_str());
  publishToMqtt(F("Spa/fault/Hours"), String(SpaFaultLog.hour).c_str());
  publishToMqtt(F("Spa/fault/Minutes"), String(SpaFaultLog.minutes).c_str());
  have_faultlog = GOT_IT;
}

void decodeFilterSettings() {
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
  publishToMqtt(F("Spa/config/filt1Hour"), String(SpaFilterSettings.filt1Hour).c_str());
  publishToMqtt(F("Spa/config/filt1Minute"), String(SpaFilterSettings.filt1Minute).c_str());
  publishToMqtt(F("Spa/config/filt1DurationHour"), String(SpaFilterSettings.filt1DurationHour).c_str());
  publishToMqtt(F("Spa/config/filt1DurationMinute"), String(SpaFilterSettings.filt1DurationMinute).c_str());
  publishToMqtt(F("Spa/config/filt2Hour"), String(SpaFilterSettings.filt2Hour).c_str());
  publishToMqtt(F("Spa/config/filt2Minute"), String(SpaFilterSettings.filt2Minute).c_str());
  publishToMqtt(F("Spa/config/filt2DurationHour"), String(SpaFilterSettings.filt2DurationHour).c_str());
  publishToMqtt(F("Spa/config/filt2DurationMinute"), String(SpaFilterSettings.filt2DurationMinute).c_str());
  publishToMqtt(F("Spa/config/filt2Enable"), String(SpaFilterSettings.filt2Enable).c_str());

  //Filter 1 time conversion hh:mm
  char payload[36];
  sprintf_P(payload, (PGM_P)F("{\"start\":\"%02d:%02d\",\"duration\":\"%02d:%02d\"}"), 
            SpaFilterSettings.filt1Hour, 
            SpaFilterSettings.filt1Minute, 
            SpaFilterSettings.filt1DurationHour, 
            SpaFilterSettings.filt1DurationMinute);

  publishToMqtt(F("Spa/filter1/state"), payload);

  if ((int)(SpaFilterSettings.filt2Enable) == 1) {
    sprintf_P(payload, (PGM_P)F("{\"start\":\"%02d:%02d\",\"duration\":\"%02d:%02d\"}"), 
              SpaFilterSettings.filt2Hour, 
              SpaFilterSettings.filt2Minute, 
              SpaFilterSettings.filt2DurationHour, 
              SpaFilterSettings.filt2DurationMinute);
    publishToMqtt(F("Spa/filter2_enabled/state"), STRON); 
    publishToMqtt(F("Spa/filter2/state"), payload);
  }
  else publishToMqtt(F("Spa/filter2_enabled/state"), STROFF);

  have_filtersettings = GOT_IT;
}

void decodePreferences() {
  publishToMqtt(F("Spa/debug/preferences/status"), "Got preferences");
  publishToMqtt(F("Spa/debug/preferences/msg"), String(Q_in[3]).c_str());
  
  SpaConfig.temp_scale = Q_in[3]; //Read temperature scale - 0 -> Farenheit, 1-> Celcius

  publishToMqtt(F("Spa/config/temp_scale"), String(SpaConfig.temp_scale).c_str());
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
  publishToMqtt(F("Spa/config/pumps1"), String(SpaConfig.pump1).c_str());
  publishToMqtt(F("Spa/config/pumps2"), String(SpaConfig.pump2).c_str());
  publishToMqtt(F("Spa/config/pumps3"), String(SpaConfig.pump3).c_str());
  publishToMqtt(F("Spa/config/pumps4"), String(SpaConfig.pump4).c_str());
  publishToMqtt(F("Spa/config/pumps5"), String(SpaConfig.pump5).c_str());
  publishToMqtt(F("Spa/config/pumps6"), String(SpaConfig.pump6).c_str());
  publishToMqtt(F("Spa/config/light1"), String(SpaConfig.light1).c_str());
  publishToMqtt(F("Spa/config/light2"), String(SpaConfig.light2).c_str());
  publishToMqtt(F("Spa/config/circ"), String(SpaConfig.circ).c_str());
  publishToMqtt(F("Spa/config/blower"), String(SpaConfig.blower).c_str());
  publishToMqtt(F("Spa/config/mister"), String(SpaConfig.mister).c_str());
  publishToMqtt(F("Spa/config/aux1"), String(SpaConfig.aux1).c_str());
  publishToMqtt(F("Spa/config/aux2"), String(SpaConfig.aux2).c_str());
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

  publishToMqtt(F("Spa/target_temp/state"), String(d, 2).c_str());

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

    publishToMqtt(F("Spa/temperature/state"), String(d, 2).c_str());
    c = d;
  } else {
    d = 0;
  }
  // REMARK Move upper publish to HERE to get 0 for unknown temperature

  // 8:Flag Byte 3 Hour & 9:Flag Byte 4 Minute => Time

  SpaState.hour = Q_in[8];
  SpaState.minutes = Q_in[9];
  char time[5];
  sprintf_P(time, (PGM_P)F("%02d:%02d"), 
            SpaState.hour, 
            SpaState.minutes);
  
  publishToMqtt(F("Spa/time/state"), time);

  // 10:Flag Byte 5 - Heating Mode
  switch (Q_in[10]) {
    case 0:
      publishToMqtt(F("Spa/heatingmode/state"), STRON); //Ready
      publishToMqtt(F("Spa/heat_mode/state"), "heat"); //Ready
      SpaState.restmode = 0;
      break;
    case 3:// Ready-in-Rest
      SpaState.restmode = 0;
      break;
    case 1:
      publishToMqtt(F("Spa/heatingmode/state"), STROFF); //Rest
      publishToMqtt(F("Spa/heat_mode/state"), "off"); //Rest
      SpaState.restmode = 1;
      break;
  }

  // 15:Flags Byte 10 / Heat status, Temp Range
  d = bitRead(Q_in[15], 4);
  if (d == 0) publishToMqtt(F("Spa/heatstate/state"), STROFF);
  else if (d == 1 || d == 2) publishToMqtt(F("Spa/heatstate/state"), STRON);

  d = bitRead(Q_in[15], 2);
  if (d == 0) {
    publishToMqtt(F("Spa/highrange/state"), STROFF); //LOW
    SpaState.highrange = 0;
  } else if (d == 1) {
    publishToMqtt(F("Spa/highrange/state"), STRON); //HIGH
    SpaState.highrange = 1;
  }

  // 16:Flags Byte 11
  if (bitRead(Q_in[16], 1) == 1) {
    publishToMqtt(F("Spa/jet_1/state"), STRON);
    SpaState.jet1 = 1;
  } else {
    publishToMqtt(F("Spa/jet_1/state"), STROFF);
    SpaState.jet1 = 0;
  }

  if (bitRead(Q_in[16], 3) == 1) {
    publishToMqtt(F("Spa/jet_2/state"), STRON);
    SpaState.jet2 = 1;
  } else {
    publishToMqtt(F("Spa/jet_2/state"), STROFF);
    SpaState.jet2 = 0;
  }

  // 18:Flags Byte 13
  if (bitRead(Q_in[18], 1) == 1)
    publishToMqtt(F("Spa/circ/state"), STRON);
  else
    publishToMqtt(F("Spa/circ/state"), STROFF);

  if (bitRead(Q_in[18], 2) == 1) {
    publishToMqtt(F("Spa/blower/state"), STRON);
    SpaState.blower = 1;
  } else {
    publishToMqtt(F("Spa/blower/state"), STROFF);
    SpaState.blower = 0;
  }
  // 19:Flags Byte 14
  if (Q_in[19] == 0x03) {
    publishToMqtt(F("Spa/light/state"), STRON);
    SpaState.light = 1;
  } else {
    publishToMqtt(F("Spa/light/state"), STROFF);
    SpaState.light = 0;
  }

  last_state_crc = Q_in[Q_in[1]];
}

///////////////////////////////////////////////////////////////////////////////

void hardreset() {
  ESP.wdtDisable();
  while (1) {};
}

void mqttpubsub() {
  // ONLY DO THE FOLLOWING IF have_config == true otherwise it will not work

  // ... Hassio autodiscover
  if (HASSIO) {

      //clear topics:
      publishToMqtt(F("homeassistant/binary_sensor/Spa"), "");
      publishToMqtt(F("homeassistant/sensor/Spa"), "");
      publishToMqtt(F("homeassistant/switch/Spa"), "");
      publishToMqtt(F("/Spa"), "");

      char payload[500];
      sprintf_P(payload, (PGM_P)FPSTR(PAYLOAD_SPA_STATE), (PGM_P)FPSTR(VERSION));

      publishToMqtt(F("homeassistant/binary_sensor/Spa/state/config"), payload, true);
      //climate temperature
      if (SpaConfig.temp_scale == 0) {
        publishToMqtt(F("Spa/debug/temparture/publish"), "True");
        publishToMqtt(F("homeassistant/climate/Spa/temperature/config"), FPSTR(PAYLOAD_SPA_TEMP_F), true);
      } else if (SpaConfig.temp_scale == 1) {
        publishToMqtt(F("Spa/debug/temparture/publish"), "True");
        publishToMqtt(F("homeassistant/climate/Spa/temperature/config"), FPSTR(PAYLOAD_SPA_TEMP_C), true);
      }
      //heat mode
      publishToMqtt(F("homeassistant/switch/Spa/heatingmode/config"), FPSTR(PAYLOAD_SPA_HEAT_MODE), true);
      //heating state
      publishToMqtt(F("homeassistant/binary_sensor/Spa/heatstate/config"), FPSTR(PAYLOAD_SPA_HEAT_STATE), true);
      //high range
      publishToMqtt(F("homeassistant/switch/Spa/highrange/config"), FPSTR(PAYLOAD_SPA_HIGH_RANGE), true);

      //OPTIONAL ELEMENTS
      if (SpaConfig.circ){
        //circulation pump
        publishToMqtt(F("homeassistant/binary_sensor/Spa/circ/config"), FPSTR(PAYLOAD_SPA_CIRC_PUMP), true);
      }
      if (SpaConfig.light1) {
        //light 1
        publishToMqtt(F("homeassistant/switch/Spa/light/config"), FPSTR(PAYLOAD_SPA_LED_LIGHT), true);
      }
      if (SpaConfig.pump1 != 0) {
        //jets 1
        publishToMqtt(F("homeassistant/switch/Spa/jet_1/config"), FPSTR(PAYLOAD_SPA_PUMP_1), true);
      }
      if (SpaConfig.pump2 != 0) {
        //jets 2
        publishToMqtt(F("homeassistant/switch/Spa/jet_2/config"), FPSTR(PAYLOAD_SPA_PUMP_2), true);
      }
      if (SpaConfig.blower)
      {
        //blower
        publishToMqtt(F("homeassistant/switch/Spa/blower/config"), FPSTR(PAYLOAD_SPA_BLOWER), true);
      }

      publishToMqtt(F("homeassistant/sensor/Spa/filter1_start/config"), FPSTR(PAYLOAD_SPA_FILTER_1_CONFIG), true);
      publishToMqtt(F("homeassistant/sensor/Spa/filter2_start/config"), FPSTR(PAYLOAD_SPA_FILTER_2_CONFIG), true);
      publishToMqtt(F("homeassistant/sensor/Spa/filter1_duration/config"), FPSTR(PAYLOAD_SPA_FILTER_1_DURATION), true);
      publishToMqtt(F("homeassistant/sensor/Spa/filter2_duration/config"), FPSTR(PAYLOAD_SPA_FILTER_1_DURATION), true);
      publishToMqtt(F("homeassistant/binary_sensor/Spa/filter2_enabled/config"), FPSTR(PAYLOAD_SPA_FILTER_2_ENABLED), true);
  }

  publishToMqtt(F("Spa/node/state"), F("ON"));
  publishToMqtt(F("Spa/node/debug"), F("RECONNECT"));
  publishToMqtt(F("Spa/node/version"), VERSION);
  publishToMqtt(F("Spa/node/flashsize"), String(ESP.getFlashChipRealSize()).c_str());
  publishToMqtt(F("Spa/node/chipid"), String(ESP.getChipId()).c_str());
	publishToMqtt(F("Spa/node/speed"), String(ESP.getCpuFreqMHz()).c_str());

  // ... and resubscribe
  subscribeToMqtt(F("Spa/command"));
  subscribeToMqtt(F("Spa/target_temp/set"));
  subscribeToMqtt(F("Spa/heatingmode/set"));
  subscribeToMqtt(F("Spa/heat_mode/set"));
  subscribeToMqtt(F("Spa/highrange/set"));

  //OPTIONAL ELEMENTS
  if (SpaConfig.pump1 != 0) {
    subscribeToMqtt(F("Spa/jet_1/set"));
  }
  if (SpaConfig.pump2 != 0) {
    subscribeToMqtt(F("Spa/jet_2/set"));
  }
  if (SpaConfig.blower) {
    subscribeToMqtt(F("Spa/blower/set"));
  }
  if (SpaConfig.light1) {
    subscribeToMqtt(F("Spa/light/set"));
  }

  subscribeToMqtt(F("Spa/relay_1/set"));
  subscribeToMqtt(F("Spa/relay_2/set"));

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

    char id[10];
    sprintf_P(id, (PGM_P)F("Spa%07d"), millis());

    //make sure the MqttPassword pointer isn't null or empty
    if ((MqttPassword != NULL) && (MqttPassword[0] == '\0')) {
      //connection =
      mqtt.connect(id);
    }
    else {
      //connection =
      mqtt.connect(id, MqttUser, MqttPassword);
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
void callback(char* topic, byte * p_payload, unsigned int p_length) {

  const char *payload = reinterpret_cast<const char*>(p_payload);

  publishToMqtt(F("Spa/node/debug"), topic);
  _yield();



  // handle message topic
  if (strcmp_P(topic, F("Spa/command")) == 0) {
    if (strcmp_P(payload, F("reset")) == 0) hardreset();
  } else if (strcmp_P(topic, F("Spa/heatingmode/set")) == 0) {
    if (strcmp_P(payload, F("ON")) == 0 && SpaState.restmode == 1) send = 0x51; // ON = Ready; OFF = Rest
    else if (strcmp_P(payload, F("OFF")) == 0 && SpaState.restmode == 0) send = 0x51;
  } else if (strcmp_P(topic, F("Spa/heat_mode/set")) == 0) {
    if (strcmp_P(payload, F("heat")) == 0 && SpaState.restmode == 1) send = 0x51; // ON = Ready; OFF = Rest
    else if (strcmp_P(payload, F("off")) == 0 && SpaState.restmode == 0) send = 0x51;
  } else if (strcmp_P(topic, F("Spa/light/set")) == 0) {
    if (strcmp_P(payload, F("ON")) == 0 && SpaState.light == 0) send = 0x11;
    else if (strcmp_P(payload, F("OFF")) == 0 && SpaState.light == 1) send = 0x11;
  } else if (strcmp_P(topic, F("Spa/jet_1/set")) == 0) {
    if (strcmp_P(payload, F("ON")) == 0 && SpaState.jet1 == 0) send = 0x04;
    else if (strcmp_P(payload, F("OFF")) == 0 && SpaState.jet1 == 1) send = 0x04;
  } else if (strcmp_P(topic, F("Spa/jet_2/set")) == 0) {
    if (strcmp_P(payload, F("ON")) == 0 && SpaState.jet2 == 0) send = 0x05;
    else if (strcmp_P(payload, F("OFF")) == 0 && SpaState.jet2 == 1) send = 0x05;
  } else if (strcmp_P(topic, F("Spa/blower/set")) == 0) {
    if (strcmp_P(payload, F("ON")) == 0 && SpaState.blower == 0) send = 0x0C;
    else if (strcmp_P(payload, F("OFF")) == 0 && SpaState.blower == 1) send = 0x0C;
  } else if (strcmp_P(topic, F("Spa/highrange/set")) == 0) {
    if (strcmp_P(payload, F("ON")) == 0 && SpaState.highrange == 0) send = 0x50; //ON = High, OFF = Low
    else if (strcmp_P(payload, F("OFF")) == 0 && SpaState.highrange == 1) send = 0x50;
  } else if (strcmp_P(topic, F("Spa/target_temp/set")) == 0) {
    // Get new set temperature
    double d = atof(payload);
    if (d > 0) d *= 2; // Convert to internal representation
    settemp = d;
    send = 0xff;
  }
}

/// UPDATE FILESYSTE
void update_started() {
  publishToMqtt(F("Spa/node/debug"), F("Updated started"));
}

void update_finished() {
  publishToMqtt(F("Spa/node/debug"), F("Updated finished"));
}

void update_progress(int cur, int total) {
  publishToMqtt(F("Spa/node/debug"), F("Update in progress"));
}

void update_error(int err) {
  publishToMqtt(F("Spa/node/debug"), F("Updated error"));
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

void configModeCallback(AsyncWiFiManager *myWiFiManager) {
  Serial.println(F("Entered config mode"));
  Serial.println(WiFi.softAPIP());

  LCDisplay.displayImmediately("IP address:", WiFi.softAPIP().toString().c_str());

  Serial.println(myWiFiManager->getConfigPortalSSID());
}

///////////////////////////////////////////////////////////////////////////////

const char *config_path = "/config.json";

void setup() {

  Serial.begin(9600);
  LCDisplay.init();

  if (LittleFS.begin() && LittleFS.exists(config_path)) {
    //WiFi Config exists, grab the config and connect as normal
    DynamicJsonDocument jsonSettings(1024);

    LCDisplay.displayImmediately("Fetching settings");
    Serial.println("Fetching Settings");

    String WiFiSsid;
    String WiFiPassword;

    File file = LittleFS.open(config_path, "r");
    if (!file) {

      Serial.println("Fatal:: Could not open file for reading");        
      ESP.reset();
      delay(5000);

    } else {
      deserializeJson(jsonSettings, file);

      strcpy(MqttIp, jsonSettings["BROKER_IP"].as<String>().c_str());
      strcpy(MqttPort, jsonSettings["BROKER_PORT"].as<String>().c_str());
      strcpy(MqttUser, jsonSettings["BROKER_USER"].as<String>().c_str());
      strcpy(MqttPassword, jsonSettings["BROKER_PASS"].as<String>().c_str());

      Serial.println("Successfully read the configuration");
    }
    file.close();
  }

  AsyncWebServer server(80);
  DNSServer dns;

  AsyncWiFiManagerParameter custom_mqtt_ip("mqtt_ip", "mqtt ip", MqttIp, 40);
  AsyncWiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt port", MqttPort, 5);
  AsyncWiFiManagerParameter custom_mqtt_user("mqtt_user", "mqtt user", MqttUser, 40);
  AsyncWiFiManagerParameter custom_mqtt_password("mqtt_password", "mqtt password", MqttPassword, 40);

  AsyncWiFiManager wifiManager(&server,&dns);

  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.setAPCallback(configModeCallback);

  wifiManager.addParameter(&custom_mqtt_ip);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);

  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("esp-spa")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //read updated parameters
  strcpy(MqttIp, custom_mqtt_ip.getValue());
  strcpy(MqttPort, custom_mqtt_port.getValue());
  strcpy(MqttUser, custom_mqtt_user.getValue());
  strcpy(MqttPassword, custom_mqtt_password.getValue());

  if (shouldSaveConfig) {
    
    //WiFi Config exists, grab the config and connect as normal
    DynamicJsonDocument jsonSettings(1024);
    jsonSettings["BROKER_IP"] = MqttIp;
    jsonSettings["BROKER_PORT"] = MqttPort;
    jsonSettings["BROKER_USER"] = MqttUser;
    jsonSettings["BROKER_PASS"] = MqttPassword;

    File f = LittleFS.open(config_path, "w");
    if (!f) {
      Serial.println(F("Fatal:: failed to create file"));
    }

    if (serializeJson(jsonSettings, f) == 0) {
      Serial.println(F("Fatal:: failed to write file"));
    }

    Serial.println(F("Config file written, restarting"));
  }

  //LittleFS.end();

  // Begin RS485 in listening mode -> no longer required with new RS485 chip
#if !AUTO_TX
  pinMode(TX485, OUTPUT);
  digitalWrite(TX485, LOW);
#endif

  LCDisplay.displayImmediately("Setting up Serial Busses");
  Serial.print("Setting up software serial");

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

  LCDisplay.displayImmediately("IP address:", WiFi.localIP().toString().c_str());
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());

  LCDisplay.addCycle("IP address:" + WiFi.localIP().toString());

  Serial.println(F("Arudino OTA Starting"));

  const char HOST_NAME[8] = "esp-spa";

  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(HOST_NAME);
  ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println(F("Start updating ") + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });

  ArduinoOTA.begin();

  Serial.println("MQTT Starting");

  mqtt.setServer(MqttIp, 1883);
  mqtt.setCallback(callback);
  mqtt.setKeepAlive(10);
  mqtt.setSocketTimeout(20);

  //MDNS.begin("spa");
  MDNS.addService("telnet", "tcp", 23);  // Telnet server of RemoteDebug, register as telnet

  Logger.init(DEBUG);
  Logger.debug(F("Logging initialised"));

  /*the below is for debug purposes*/
  mqtt.connect("Spa1", MqttUser, MqttPassword);
  publishToMqtt(F("Spa/debug/wifi_ssid"), WiFi.SSID().c_str());
  publishToMqtt(F("Spa/debug/broker"), MqttIp);
  publishToMqtt(F("Spa/debug/broker_login"), MqttUser);

  char buffer[70];
  sprintf(buffer, "Connnected to MQTT on IP: %s", MqttIp);
  LCDisplay.addCycle(buffer);
  
  Serial.println("MQTT Started");

  pinMode(BUTTON, INPUT);

  LCDisplay.backlightTrigger();
}

void loop() {

  if (WiFi.status() != WL_CONNECTED) ESP.restart();
  if (!mqtt.connected()) reconnect();
  if (have_config == GOT_IT && have_preferences == GOT_IT) mqttpubsub(); //do mqtt stuff after we're connected and if we have got the config elements

  _yield();

  int state = digitalRead(BUTTON);
  //Check if the button is pushed, if it is the state will be High
  if(state == HIGH) {
    Serial.print(F("Button pushed illuminating LCD"));
    LCDisplay.backlightTrigger();
  }

  //Every x minutes, read the fault log and filter settings using SpaState,minutes, and check for updates
  if ((int)(SpaState.minutes % 5) == 0)
  {

    //Serial.print("Status Reading Fault/Filter");

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
  }

  // Read from Spa RS485
  if (spaSerial.available()) {
    x = spaSerial.read();
    Q_in.push(x);

    //Serial.print(" " + String(x, HEX));

    // Drop until SOF is seen
    if (Q_in.first() != 0x7E) Q_in.clear();
    lastrx = millis();
  }

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
        publishToMqtt(F("Spa/node/id"), String(id).c_str());
        LCDisplay.addCycle("Got Spa ID: " + id);
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
            //publishToMqtt(F("Spa/config/status"), "Getting config");
            have_config = REQUESTED_IT;
          } else if (have_preferences == WANT_IT) {
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x22);
            Q_out.push(0x08);
            Q_out.push(0x00);
            Q_out.push(0x00);
            //publishToMqtt(F("Spa/config/status"), "Getting config");
            have_preferences = REQUESTED_IT;
          } else if (have_faultlog == WANT_IT) { // Get the fault log
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x22);
            Q_out.push(0x20);
            Q_out.push(0xFF);
            Q_out.push(0x00);
            have_faultlog = REQUESTED_IT;
            //publishToMqtt(F("Spa/debug/have_faultlog"), "requesting fault log, #1");
          } else if ((have_filtersettings == WANT_IT) && (have_faultlog == GOT_IT)) { // Get the filter cycles log once we have the faultlog
            Q_out.push(id);
            Q_out.push(0xBF);
            Q_out.push(0x22);
            Q_out.push(0x01);
            Q_out.push(0x00);
            Q_out.push(0x00);
            //publishToMqtt(F("Spa/debug/have_faultlog"), "requesting filter settings, #1");
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
        //publishToMqtt(F("Spa/debug/have_faultlog"), "decoding filter settings");
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
      Serial.print(F("Hard reset"));
      hardreset();
    }
  }
}
