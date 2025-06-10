#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <Arduino.h>
#include <time.h>

extern unsigned long lastSyncEpochTime;
String formatEpochTime(long long epoch);

#endif

String getCompileDateTime();