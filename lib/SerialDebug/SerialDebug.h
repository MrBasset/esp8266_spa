#pragma once
#include <Arduino.h>
#include <WebSerial.h>
#include <ESPAsyncWebServer.h>

class SerialDebug_ {
  private:
    SerialDebug_() = default; // Make constructor private

  public:
    static SerialDebug_ &getInstance(); // Accessor for singleton instance

    SerialDebug_(const SerialDebug_ &) = delete; // no copying
    SerialDebug_ &operator=(const SerialDebug_ &) = delete;

  public:
    void init();
    void initWeb(AsyncWebServer *server);
    void enableWeb();
    void disableWeb();
    void print ();
    void print (const Printable &p, bool nl = true);
    void print (char p);
    void print (const char p[], bool nl = true);
    void print (const String &p, bool nl = true);
    void print (const __FlashStringHelper *p, bool nl = true);
    void print (double p, int i = 2, bool nl = true);
    void print (unsigned long long p, int i = 10, bool nl = true);
    void print (long long p, int i = 10, bool nl = true);
    void print (long p, int i = 10, bool nl = true);
    void print (unsigned long p, int i = 10, bool nl = true);
    void print (unsigned int p, int i = 10, bool nl = true);
    void print (int p, int i = 10, bool nl = true);
    void print (unsigned char p, int i = 10, bool nl = true);
};

extern SerialDebug_ &SerialDebug;