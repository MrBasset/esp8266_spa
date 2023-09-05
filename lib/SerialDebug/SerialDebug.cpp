#include "SerialDebug.h"

bool webEnabled = false;

void SerialDebug_::init() {
  Serial.begin(9600);
  Serial.println("Starting");
}

void SerialDebug_::initWeb(AsyncWebServer *server) {
  WebSerial.begin(server);
  server->begin();
  this->enableWeb();
}

void SerialDebug_::enableWeb() {
    webEnabled = true;
}

void SerialDebug_::disableWeb() {
    webEnabled = false;
}

void SerialDebug_::print () {
  Serial.println();
  if (webEnabled) WebSerial.println();
}

void SerialDebug_::print (const Printable &p, bool nl) {
  if (nl) {
    Serial.println(p);
    if (webEnabled) WebSerial.println(p);
  }
  else
  {
    Serial.print(p);
    if (webEnabled) WebSerial.print(p);
  }
}

void SerialDebug_::print (char p) {
  Serial.println(p);
  if (webEnabled) WebSerial.println(p);
}

void SerialDebug_::print (const char p[], bool nl) {
  if (nl) {
    Serial.println(p);
    if (webEnabled) WebSerial.println(p);
  }
  else
  {
    Serial.print(p);
    if (webEnabled) WebSerial.print(p);
  }
}

void SerialDebug_::print (const String &p, bool nl) {
  if (nl) {
    Serial.println(p);
    if (webEnabled) WebSerial.println(p);
  }
  else
  {
    Serial.print(p);
    if (webEnabled) WebSerial.print(p);
  }
}

void SerialDebug_::print (const __FlashStringHelper *p, bool nl) {
  if (nl) {
    Serial.println(p);
    if (webEnabled) WebSerial.println(p);
  }
  else
  {
    Serial.print(p);
    if (webEnabled) WebSerial.print(p);
  }
}

void SerialDebug_::print (double p, int i, bool nl) {
  if (nl) {
    Serial.println(p, i);
    if (webEnabled) WebSerial.println(p, i);
  }
  else
  {
    Serial.print(p, i);
    if (webEnabled) WebSerial.print(p, i);
  }
}

void SerialDebug_::print (unsigned long long p, int i, bool nl) {
  if (nl) {
    Serial.println(p, i);
    if (webEnabled) WebSerial.println(p, i);
  }
  else
  {
    Serial.print(p, i);
    if (webEnabled) WebSerial.print(p, i);
  }
}

void SerialDebug_::print (long long p, int i, bool nl) {
  if (nl) {
    Serial.println(p, i);
    if (webEnabled) WebSerial.println(p, i);
  }
  else
  {
    Serial.print(p, i);
    if (webEnabled) WebSerial.print(p, i);
  }
}

void SerialDebug_::print (long p, int i, bool nl) {
  if (nl) {
    Serial.println(p, i);
    if (webEnabled) WebSerial.println(p, i);
  }
  else
  {
    Serial.print(p, i);
    if (webEnabled) WebSerial.print(p, i);
  }
}

void SerialDebug_::print (unsigned long p, int i, bool nl) {
  if (nl) {
    Serial.println(p, i);
    if (webEnabled) WebSerial.println(p, i);
  }
  else
  {
    Serial.print(p, i);
    if (webEnabled) WebSerial.print(p, i);
  }
}

void SerialDebug_::print (unsigned int p, int i, bool nl) {
  if (nl) {
    Serial.println(p, i);
    if (webEnabled) WebSerial.println(p, i);
  }
  else
  {
    Serial.print(p, i);
    if (webEnabled) WebSerial.print(p, i);
  }
}

void SerialDebug_::print (int p, int i, bool nl) {
  if (nl) {
    Serial.println(p, i);
    if (webEnabled) WebSerial.println(p, i);
  }
  else
  {
    Serial.print(p, i);
    if (webEnabled) WebSerial.print(p, i);
  }
}

void SerialDebug_::print (unsigned char p, int i, bool nl) {
  if (nl) {
    Serial.println(p, i);
    if (webEnabled) WebSerial.println(p, i);
  }
  else
  {
    Serial.print(p, i);
    if (webEnabled) WebSerial.print(p, i);
  }
}

SerialDebug_ &SerialDebug_::getInstance() {
  static SerialDebug_ instance;
  return instance;
}

SerialDebug_ &SerialDebug = SerialDebug.getInstance();