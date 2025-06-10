#ifndef MAIN_PAGE_HTML_H
#define MAIN_PAGE_HTML_H

#include <Ethernet.h>
#include "globals.h"
#include <esp_system.h>

#ifndef LOG_HISTORY_SIZE
#define LOG_HISTORY_SIZE 50
#endif

void sendCommonHtmlHeader(EthernetClient &client, const String &title);
void sendCommonHtmlFooter(EthernetClient &client);
void addLog(const String &message);

String formatLogEntryForWeb(const String& logEntry) {
  String color;
  if (logEntry.startsWith("[CRITICAL]")) {
    color = "red; font-weight:bold";
  } else if (logEntry.startsWith("[ERROR]")) {
    color = "red";
  } else if (logEntry.startsWith("[WARN]")) {
    color = "orange";
  } else if (logEntry.startsWith("[INFO]")) {
    color = "blue";
  } else {
    color = "black";
  }
  return "<span style='color:" + color + "'>" + logEntry + "</span>";
}

String getResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  switch(reason) {
    case ESP_RST_POWERON:    return "Power-On Reset";
    case ESP_RST_EXT:        return "External Reset";
    case ESP_RST_SW:         return "Software Reset";
    case ESP_RST_PANIC:      return "System Panic";
    case ESP_RST_INT_WDT:    return "Hardware Watchdog";
    case ESP_RST_TASK_WDT:   return "Task Watchdog";
    case ESP_RST_WDT:        return "Watchdog Timeout";
    case ESP_RST_DEEPSLEEP:  return "Deep Sleep Wake";
    case ESP_RST_BROWNOUT:   return "Brownout Protection";
    case ESP_RST_SDIO:       return "SDIO Reset";
    default:                 return "Unknown (" + String(reason) + ")";
  }
}

void sendWatchdogPage_GET(EthernetClient &client) {
  extern unsigned long deviceUptime;
  extern uint32_t deviceRestarts;
  extern int logIndex;
  extern String logHistory[];
  extern unsigned long lastWatchdogFeed;
  extern bool hwWatchdogActive;
  extern uint32_t wdtResets;
  extern volatile uint32_t loopCounter;
  extern const uint32_t WDT_TIMEOUT;

  // Monitorowanie stanu watchdoga
  static unsigned long lastWdtCheck = 0;
  if(millis() - lastWdtCheck > 10000) {
    lastWdtCheck = millis();
    unsigned long timeSinceFeed = millis() - lastWatchdogFeed;
    
    if(timeSinceFeed > WDT_TIMEOUT * 0.9) {
      addLog("[WARN] Watchdog nie był karmiony przez " + String(timeSinceFeed/1000) + "s!");
    }
    else if(timeSinceFeed > WDT_TIMEOUT * 0.7) {
      addLog("[WARN] Długi czas od ostatniego karmienia: " + String(timeSinceFeed/1000) + "s"); 
    }
  }

  sendCommonHtmlHeader(client, "STRONA W BUDOWIE Diagnostyka Watchdog & Systemu");

  // Przyciski sterujące
  client.println("<div style='text-align:right; margin-bottom:10px;'>");
  client.println("<form method='GET' style='display:inline; margin-right:10px;'>");
  client.println("<button type='submit' style='padding:5px 10px; background-color:#4CAF50; color:white; border:none; border-radius:4px; cursor:pointer;'>Odśwież</button>");
  client.println("</form>");
  client.println("<form method='POST' style='display:inline;'>");
  client.println("<button name='testAlarm' value='1' style='padding:5px 10px; background-color:#f44336; color:white; border:none; border-radius:4px; cursor:pointer;'>Testowy alarm</button>");
  client.println("</form>");
  client.println("</div>");

  client.println("<meta http-equiv='refresh' content='30'>");
  client.println("<h2>Watchdog & System – Diagnostyka</h2>");

  // ---- PRZYCZYNA RESTARTU ----
  String resetReason = getResetReason();
  client.println("<div style='background:#f8f8f8; padding:10px; margin-bottom:15px; border-radius:5px;'>");
  client.print("<b>Przyczyna ostatniego restartu:</b> <span style='color:");
  
  // Kolorowanie w zależności od typu błędu
  esp_reset_reason_t reason = esp_reset_reason();
  if(reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || 
     reason == ESP_RST_TASK_WDT || reason == ESP_RST_BROWNOUT) {
    client.print("red; font-weight:bold");
  } else {
    client.print("blue");
  }
  client.print("'>" + resetReason + "</span>");
  client.println("</div>");

  // ---- SYSTEM INFO ----
  client.println("<b>Wersja firmware:</b> " + firmwareVersion + "<br>");
  client.printf("<b>Liczba restartów urządzenia:</b> %lu<br>", deviceRestarts);
  
  unsigned long uptimeSeconds = deviceUptime / 1000;
  unsigned long uptimeMinutes = uptimeSeconds / 60;
  uptimeSeconds %= 60;
  unsigned long uptimeHours = uptimeMinutes / 60;
  uptimeMinutes %= 60;
  client.printf("<b>Czas działania od ostatniego restartu:</b> %lu h %lu min %lu s<br>", 
               uptimeHours, uptimeMinutes, uptimeSeconds);

  // ---- WATCHDOG ----
  client.printf("<b>Watchdog hardware timeout:</b> %.1f s<br>", WDT_TIMEOUT / 1000.0);
  client.printf("<b>Status:</b> %s<br>", hwWatchdogActive ? "AKTYWNY (HARDWARE)" : "NIEAKTYWNY");
  
  unsigned long timeSinceLastFeed = millis() - lastWatchdogFeed;
  bool wdtOK = (timeSinceLastFeed < (WDT_TIMEOUT / 2));
  client.printf("<b>Status pracy:</b> <span style='color:%s; font-weight:bold'>%s</span><br>", 
               wdtOK ? "green" : "red", 
               wdtOK ? "OK" : "BAD");

  client.printf("<b>Liczba karmień Watchdoga:</b> %lu<br>", wdtResets % 10);
  
  unsigned long secondsSinceLastFeed = timeSinceLastFeed / 1000;
  unsigned long minutesSinceLastFeed = secondsSinceLastFeed / 60;
  secondsSinceLastFeed %= 60;
  client.printf("<b>Ostatnie karmienie Watchdogiem:</b> %lu min %lu s temu<br>", 
               minutesSinceLastFeed, secondsSinceLastFeed);

  // ---- ZASOBY SYSTEMOWE ----
  uint32_t freeHeap = ESP.getFreeHeap();
  if(freeHeap > 1000) {
    client.printf("<b>Free heap:</b> %.2f kB<br>", freeHeap / 1024.0);
  } else {
    client.printf("<b>Free heap:</b> %u B<br>", freeHeap);
  }

  uint32_t minFreeHeap = ESP.getMinFreeHeap();
  if(minFreeHeap > 1000) {
    client.printf("<b>Minimum free heap:</b> %.2f kB<br>", minFreeHeap / 1024.0);
  } else {
    client.printf("<b>Minimum free heap:</b> %u B<br>", minFreeHeap);
  }

  uint32_t freePsram = ESP.getFreePsram();
  if(freePsram > 1000) {
    client.printf("<b>Free PSRAM:</b> %.2f kB<br>", freePsram / 1024.0);
  } else if(freePsram > 0) {
    client.printf("<b>Free PSRAM:</b> %u B<br>", freePsram);
  } else {
    client.print("<b>Free PSRAM:</b> 0 B (brak PSRAM lub nieaktywne)<br>");
  }

  #ifdef ESP_IDF_VERSION
  TaskHandle_t handle = xTaskGetCurrentTaskHandle();
  uint32_t freeStack = uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t);
  if(freeStack > 1000) {
    client.printf("<b>Free stack (aktualny task):</b> %.2f kB<br>", freeStack / 1024.0);
  } else {
    client.printf("<b>Free stack (aktualny task):</b> %u B<br>", freeStack);
  }
  #else
  client.print("<b>Free stack: </b>Brak danych w tym środowisku<br>");
  #endif

  // ---- LICZNIK PETLI ----
  client.printf("<b>Licznik cykli loop():</b> %lu<br>", loopCounter % 10);

  // ---- LOGI ZDARZEŃ ----
  client.println("<h3>Logi zdarzeń:</h3>");
  client.println("<div style='border:1px solid #ccc; padding:10px; max-height:300px; overflow-y:auto; background:#f8f8f8;'>");
  
  int printCount = min(logIndex, LOG_HISTORY_SIZE);
  for (int i = logIndex - printCount; i < logIndex; ++i) {
    int idx = i % LOG_HISTORY_SIZE;
    if (!logHistory[idx].isEmpty()) {
      client.println(formatLogEntryForWeb(logHistory[idx]) + "<br>");
    }
  }
  
  if(logIndex == 0) {
    client.println("Brak wpisów w logu");
  }
  
  client.println("</div>");

  sendCommonHtmlFooter(client);
}
#endif