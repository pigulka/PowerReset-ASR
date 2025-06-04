#ifndef MAIN_PAGE_HTML_H
#define MAIN_PAGE_HTML_H

#include <Ethernet.h>
#include "globals.h"

void sendCommonHtmlHeader(EthernetClient &client, const String &title);
void sendCommonHtmlFooter(EthernetClient &client);

void addLog(const String &message);

void sendWatchdogPage_GET(EthernetClient &client) {
  extern unsigned long deviceUptime;
  extern uint32_t deviceRestarts;
 // extern String firmwareVersion;
  extern int logIndex;
  extern String logHistory[]; // Załóż: MAX_LOGS

  static unsigned long staticLoopCounter = 0; // licznik cykli loop (jeśli chcesz resetować, przestaw na zmienną globalną)

  sendCommonHtmlHeader(client, "Diagnostyka Watchdog & Systemu");

  client.println("<meta http-equiv='refresh' content='5'>"); // auto-refresh co 5 sek
  client.println("<h2>Watchdog & System – Diagnostyka</h2>");

  // ---- SYSTEM INFO ----
  client.println("<b>Wersja firmware:</b> " + firmwareVersion + "<br>");
  client.printf("<b>Liczba restartów urządzenia:</b> %lu<br>", deviceRestarts);
  client.printf("<b>Czas działania od ostatniego restartu:</b> %lu s<br>", deviceUptime);

  // ---- POWÓD RESTARTU ----
  String reason = "Nieznany";
  esp_reset_reason_t r = esp_reset_reason();
  switch(r) {
    case ESP_RST_POWERON:   reason = "Power On"; break;
    case ESP_RST_EXT:       reason = "Zewnętrzny (np. pin EN)"; break;
    case ESP_RST_SW:        reason = "Programowy (ESP.restart())"; break;
    case ESP_RST_PANIC:     reason = "Błąd krytyczny (Panic)"; break;
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:  reason = "Watchdog!"; break;
    case ESP_RST_BROWNOUT:  reason = "Brownout (niskie zasilanie)"; break;
    case ESP_RST_SDIO:      reason = "SDIO"; break;
    default:                reason = "Inny / Nieznany"; break;
  }
  client.printf("<b>Powód ostatniego restartu:</b> %s<br>", reason.c_str());

  // ---- WATCHDOG ----
  client.printf("<b>Watchdog hardware timeout:</b> 30 s<br>");
  client.printf("<b>Status:</b> %s<br>", "AKTYWNY (HW)");

  // ---- HEAP/RAM/STACK ----
  client.printf("<b>Free heap:</b> %u B<br>", ESP.getFreeHeap());
  client.printf("<b>Minimum free heap:</b> %u B<br>", ESP.getMinFreeHeap());
  client.printf("<b>Free PSRAM:</b> %u B<br>", ESP.getFreePsram());

  // Stack usage (może nie być obsługiwane przez wszystkie core/biblioteki, pokazuje main task)
  #ifdef ESP_IDF_VERSION
  TaskHandle_t handle = xTaskGetCurrentTaskHandle();
  client.printf("<b>Free stack (aktualny task):</b> %u B<br>", uxTaskGetStackHighWaterMark(handle) * sizeof(StackType_t));
  #else
  client.print("<b>Free stack: </b>Brak danych w tym środowisku<br>");
  #endif

  // ---- Loop counter ----
  extern volatile uint32_t loopCounter;
  client.printf("<b>Licznik cykli loop():</b> %lu<br>", loopCounter);

  // ---- Ostatnie karmienie Watchdogiem ----
  extern unsigned long ostatnieKarmienie;
  client.printf("<b>Ostatnie karmienie Watchdogiem:</b> %lu ms temu<br>", millis() - ostatnieKarmienie);

  // ---- Przycisk testowy – wymuś watchdog reset ----
  client.println("<form method='POST'><button name='resetWDT' value='1'>Ręcznie wywołaj reset watchdogiem (test)</button></form>");

  // ---- Logi błędów z pliku (ostatnie wpisy) ----
  client.println("<h3>Logi błędów (ostatnie 10):</h3><pre>");
  int printCount = logIndex < 10 ? logIndex : 10;
  for (int i = logIndex - printCount; i < logIndex; ++i) {
    if (i >= 0 && !logHistory[i].isEmpty()) client.println(logHistory[i]);
  }
  client.println("</pre>");

  sendCommonHtmlFooter(client);
}











#endif