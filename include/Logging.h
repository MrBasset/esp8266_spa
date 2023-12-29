#pragma once
#include <pgmspace.h>
#include <RemoteDebug.h>

#ifndef LOGGING_h
#define LOGGING_h

#define info(const __FlashStringHelper *s) \
{                                       \
  char buffer[strlen_P((PGM_P)s)];      \
  strcpy_P(buffer, (PGM_P)s);           \
  debugI(buffer);                       \
}

#endif