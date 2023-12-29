#include "LCDisplay.h"

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 20 chars and 4 line display
CircularBuffer<String, 15> Q_cycle;

unsigned long cycle_time = 5000;
unsigned long last_cycle = millis();
unsigned int idx = 0;

unsigned long last_illuminated = millis();

bool isOff = false;

void LCDisplay_::init() {
  lcd.init();
  lcd.clear();
  lcd.backlight();
}

void LCDisplay_::addCycle(String s) {
  Q_cycle.push(s.c_str());
}

void LCDisplay_::displayCycle() {
  if (isOff) return;

  if ((millis() - last_cycle) >= cycle_time) {
    if (idx >= Q_cycle.size()) {
      //SerialDebug.print(F("Rolling display queue"));
      idx = 0;
    }

    this->displayImmediately(Q_cycle[idx].c_str());
    idx++;
    last_cycle = millis();
  }

  if(millis() - last_illuminated > (2 * cycle_time * Q_cycle.size())) {
    //SerialDebug.print(F("Switching off backlight"));
    lcd.noBacklight();
    lcd.clear();
    isOff = true;
  }
}

void LCDisplay_::backlightTrigger() {
  lcd.backlight();
  last_illuminated = millis();
  isOff = false;
}

void LCDisplay_::displayImmediately(const char *c1, const char *c2) {
  lcd.clear();
  lcd.setCursor(0,1); // character 0 line 2
  lcd.print(c1);
  if ((c2 != NULL) && (c2[0] != '\0'))
  {
    lcd.setCursor(0,2); // character 0 line 3
    lcd.print(c2);
  }
}

LCDisplay_ &LCDisplay_::getInstance() {
  static LCDisplay_ instance;
  return instance;
}

LCDisplay_ &LCDisplay = LCDisplay.getInstance();