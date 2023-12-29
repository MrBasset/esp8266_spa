#pragma once
#include <CircularBuffer.h>
#include <LiquidCrystal_I2C.h>

class LCDisplay_ {
  private:
    LCDisplay_() = default;  //make constructor private

  public:
    static LCDisplay_ &getInstance(); //Accessor for singleton

    LCDisplay_(const LCDisplay_ &) = delete; //no copying
    LCDisplay_ &operator=(const LCDisplay_ &) = delete;

  public:
    void init();
    void addCycle(String s);
    void displayCycle();
    void displayImmediately(const char *c1, const char *c2 = NULL);
    void display(const __FlashStringHelper *line1, const __FlashStringHelper *line2 = NULL);
    void backlightTrigger();
};

extern LCDisplay_ &LCDisplay;