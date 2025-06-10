#include "time_utils.h"

unsigned long lastSyncEpochTime = 0; // Jedyna definicja

String formatEpochTime(long long epoch) {
  time_t time = (time_t)epoch;
  struct tm *tm = localtime(&time);
  
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
  
  return String(buffer);
}

String getCompileDateTime() {
  const char *compileDate = __DATE__; // np. "Jun  5 2025"
  const char *compileTime = __TIME__; // np. "16:02:44"
  
  char monthAbbr[4];
  int day, year;
  sscanf(compileDate, "%3s %d %d", monthAbbr, &day, &year);
  
  // Konwersja skrótu miesiąca na numer
  const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                         "Jul","Aug","Sep","Oct","Nov","Dec"};
  int month = 0;
  for (int i = 0; i < 12; i++) {
    if (strcmp(monthAbbr, months[i]) == 0) {
      month = i + 1;
      break;
    }
  }
  
  // Formatowanie daty i czasu
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %s", year, month, day, compileTime);
  
  return String(buffer);
}