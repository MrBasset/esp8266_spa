#include "Logger.h"

/*
TODO:
[] All functions working through common print
[] Addition of time printing
[] Convert to TelnetPrint.h for smaller foot print
[] 

*/

const char* _LVL_STR[] = { "VERBOSE", "DEBUG", "INFO", "WARN", "ERROR", "CRITICAL" };

void Logger_::init(LogLevel ll) {
  _loggingLevel = ll;
  TelnetStream.begin();
}

const char* Logger_::LogLevelEnumToColor (LogLevel ll) {
  const char* result = nullptr;

  switch(ll) {
    case VERBOSE:
      result = COLOUR_VERBOSE;
      break;
    case DEBUG:
      result = COLOUR_DEBUG;
      break;
    case INFO:
      result = COLOUR_INFO;
      break;
    case WARNING:
      result = COLOUR_WARNING;
      break;
    case ERROR:
      result = COLOUR_ERROR;
      break;
    case CRITICAL:
      result = COLOUR_CRITICAL;
      break;
    default:
      result = COLOUR_RAW;
      break;
  }

  return result;
}

void Logger_::print(const char * format, LogLevel ll, const char* time, va_list va_1) {
  if (ll < _loggingLevel) return; //don't log anything if the message is below the current logging level

  va_list va_2;
  va_copy(va_2, va_1);
  //get the length of message given we have the length of format + an unknown number of variables by passing to vsnprintf with a nullptr.
  int size = vsnprintf(nullptr, 0, format, va_1) + 1;
  va_end(va_1);

  char message[size];
  vsprintf(message, format, va_2);
  va_end(va_2);

  const char* color = LogLevelEnumToColor(ll);
  size = snprintf(nullptr, 0, MSG_FORMAT, color, _LVL_STR[ll], time, message);

  char logged_message[size];
  snprintf(logged_message, size, MSG_FORMAT, color, _LVL_STR[ll], time, message);

  TelnetStream.printf(message);
}

void Logger_::info(const __FlashStringHelper *fmt, ...) {

  if (INFO < _loggingLevel) return; //don't log anything if the message is below the current logging level

  char format[strlen_P((PGM_P)fmt)];
  strcpy_P(format, (PGM_P)fmt);

  va_list va_1;
  va_start(va_1, fmt);
  
  print(format, INFO, "2023-10-29 10:00", va_1);

  va_end(va_1);
}

void Logger_::debug(const __FlashStringHelper *fmt, ...) {

  if (DEBUG < _loggingLevel) return; //don't log anything if the message is below the current logging level

  char format[strlen_P((PGM_P)fmt)];
  strcpy_P(format, (PGM_P)fmt);

  va_list va_1;
  va_start(va_1, fmt);
  
  print(format, DEBUG, "2023-10-29 10:00", va_1);

  va_end(va_1);
}

void Logger_::verbose(const __FlashStringHelper *fmt, ...) {

  if (VERBOSE < _loggingLevel) return; //don't log anything if the message is below the current logging level

  char format[strlen_P((PGM_P)fmt)];
  strcpy_P(format, (PGM_P)fmt);

  va_list va_1;
  va_start(va_1, fmt);
  
  print(format, VERBOSE, "2023-10-29 10:00", va_1);

  va_end(va_1);
}

void Logger_::error(const __FlashStringHelper *fmt, ...) {

  if (ERROR < _loggingLevel) return; //don't log anything if the message is below the current logging level

  char format[strlen_P((PGM_P)fmt)];
  strcpy_P(format, (PGM_P)fmt);

  va_list va_1;
  va_start(va_1, fmt);
  
  print(format, ERROR, "2023-10-29 10:00", va_1);

  va_end(va_1);
}

void Logger_::critical(const __FlashStringHelper *fmt, ...) {

  if (CRITICAL < _loggingLevel) return; //don't log anything if the message is below the current logging level

  char format[strlen_P((PGM_P)fmt)];
  strcpy_P(format, (PGM_P)fmt);

  va_list va_1;
  va_start(va_1, fmt);
  
  print(format, CRITICAL, "2023-10-29 10:00", va_1);

  va_end(va_1);
}