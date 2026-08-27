#ifndef DEBUG_SERIAL_H
#define DEBUG_SERIAL_H

#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>

void debugSerialInit(uint32_t baud = 115200);
void debugPrintChar(char c);
void debugPrint(const char* str);
void debugPrintln(const char* str = "");
void debugPrintf(const char* format, ...);

#endif // DEBUG_SERIAL_H
