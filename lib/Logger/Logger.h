#pragma once

#ifndef LOGGER_h
#define LOGGER_h

#include <pgmspace.h>
#include <TelnetStream.h>

//class

enum LogLevel { VERBOSE = 0, DEBUG = 1, INFO = 2, WARNING = 3, ERROR = 4, CRITICAL = 5 };

class Logger_ {
  private:
    Logger_() = default;  //make constructor private
    const char* LogLevelEnumToColor (LogLevel ll);
    void print(const char * format, LogLevel ll, const char* time, va_list va_1);
    LogLevel _loggingLevel = INFO;

    // ANSI Colors
    const char* COLOUR_RESET          = "\x1B[0m";
    const char* COLOUR_VERBOSE        = "\x1B[0;32m"; //green
    const char* COLOUR_DEBUG          = "\x1B[1;32m"; //light green
    const char* COLOUR_INFO           = "\x1B[1;34m"; //light blue
    const char* COLOUR_WARNING        = "\x1B[1;33m"; //light yellow
    const char* COLOUR_ERROR          = "\x1B[1;31m"; //light red
    const char* COLOUR_CRITICAL       = "\x1B[0;31m"; //Red
    const char* COLOUR_RAW            = "\x1B[0;37m"; //white

    const char* MSG_FORMAT           = "%s[%s] - %s - %s\x1B[0m"; //messages follow the format <colour code>[<log level>] - <time> - <message>


  public:
    static Logger_ &getInstance(); //Accessor for singleton

    Logger_(const Logger_ &) = delete; //no copying
    Logger_ &operator=(const Logger_ &) = delete;

  public:

    void init(LogLevel ll);

    void verbose(const __FlashStringHelper *message, ...);
    void debug(const __FlashStringHelper *message, ...);
    void info(const __FlashStringHelper *message, ...);
    void error(const __FlashStringHelper *message, ...);
    void critical(const __FlashStringHelper *message, ...);

};

extern Logger_ &Logger;

#endif
