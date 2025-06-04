#ifndef MAIN_PAGE_HTML_H
#define MAIN_PAGE_HTML_H

#include <Ethernet.h>
#include "globals.h"

void sendCommonHtmlHeader(EthernetClient &client, const String &title);
void sendCommonHtmlFooter(EthernetClient &client);

void addLog(const String &message);

extern const String firmwareVersion;
extern bool inputMonitoringEnabled[4];
extern String inputLabels[4];

// Główna strona
void sendMainPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  client.println("<meta http-equiv='refresh' content='5'>");

  // Kontener główny (mniejszy, bez scrolla)
  client.println("<div class='container' style='max-width:800px; margin:0px auto 8px auto; background:#20232a; padding:10px 10px 8px 10px; border-radius:18px; box-shadow:0 2px 18px #111c; color:#e0e0e0;'>");

  // Pasek info w prawym górnym rogu
  client.println("<div style='position:relative; height:38px; margin-bottom:8px;'>");
  client.println("<div style='position:absolute; top:0; right:0; text-align:right; color:#bdbdbd; font-size:1.03em;'>");
  client.println("<span style='color:#4fc3f7; font-weight:bold;'>PowerReset 2 • v" + String(firmwareVersion) + "</span><br>");
  client.println("<span style='font-size:0.98em;'>© 2025 TAURON Dystrybucja S.A.</span><br>");
  client.println("<span style='font-size:0.93em;'>Strona odświeża się co 5 sek.</span>");
  client.println("</div>");
  client.println("</div>");

  // Nagłówek strony - centrowany, lekki margines dolny
  client.println("<h2 style='text-align:center; margin:0 0 12px 0; font-size:1.18em; color:#fff;'>Strona główna</h2>");

  // Górny panel z miejscem i połączeniem
  client.println("<div style='display:flex; justify-content:space-between; align-items:flex-start; margin-bottom:4px; flex-wrap:wrap;'>");
  client.println("<div style='min-width:180px; margin-bottom:2px;'>");
  client.println("<h3 style='color:#4fc3f7; margin:0 0 4px 0; font-size:1em;'>" + placeStr + "</h3>");
  bool linkOk = (Ethernet.linkStatus() == LinkON);
  IPAddress currentIP = Ethernet.localIP();
  if (linkOk) {
    client.println("<p style='margin:0 0 3px 0;'>Stan połączenia: <span style='color:#76ff03; font-weight:bold;'>ACTIVE</span></p>");
  } else {
    client.println("<p style='margin:0 0 3px 0;'>Stan połączenia: <span style='color:#ff5252; font-weight:bold;'>OFFLINE</span></p>");
  }
  client.print("<p style='margin:0;'>Adres IP: <span style='color:#4fc3f7; font-weight:bold;'>");
  client.print(currentIP);
  client.println("</span></p>");
  client.println("</div>");
  client.println("</div>");

  // Status automatu/przekaźników (węższy, niższy)
  client.println("<div class='status-container' style='max-width:730px; margin:0 auto 10px auto;'>");
  bool anyMaxReached = false;
  for (int i = 0; i < 4; i++) if (maxResetReached[i]) anyMaxReached = true;

  if (anyMaxReached) {
    client.println("<button class='status-message status-error' disabled style='width:100%; margin-bottom:4px; font-size:0.98em;'>Osiągnięto maksymalną liczbę prób uruchomienia urządzenia (przynajmniej jeden przekaźnik)</button>");
  } else {
    for (int i = 0; i < 4; i++) {
      String label = inputLabels[i];
      String relayState = relayStates[i];
      ResetState state = inputState[i];
      String monitorInfo = inputMonitoringEnabled[i]
        ? "<br><span style='color:#8bc34a; font-size:0.92em;'>&#9679; monitorowane</span>"
        : "<br><span style='color:#bdbdbd; font-size:0.92em;'>&#9679; nieaktywne</span>";

      if (!inputMonitoringEnabled[i]) {
        client.println("<button class='status-message status-normal' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
          label + ": <span style='color:#bdbdbd;'>nieaktywne wejście</span>" +
          monitorInfo + "</button>");
        continue;
      }
      if (state == IDLE && relayState == "ON") {
        unsigned long elapsed = millis() - relayOnTime[i];
        unsigned long remaining = (autoOffTime > elapsed) ? ((autoOffTime - elapsed) / 1000UL) : 0;
        client.println("<button class='status-message status-warning' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
          label + ": Tryb ręczny. Auto-off za: " + String(remaining) + " s" + monitorInfo + "</button>");
      }
      else if (state == WAITING_1MIN) {
        unsigned long elapsed = millis() - stateStartTime[i];
        unsigned long remaining = (WAITING_1MIN_TIME > elapsed) ? ((WAITING_1MIN_TIME - elapsed) / 1000UL) : 0;
        if (remaining == 0) {
          client.println("<button class='status-message status-warning' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
            label + ": Input LOW. Trwa analiza/reset..." + monitorInfo + "</button>");
        } else {
          client.println("<button class='status-message status-warning' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
            label + ": Input LOW. Czekamy " + String(remaining) + " s, czy sygnał wróci do normy." + monitorInfo + "</button>");
        }
      }
      else if (state == DO_RESET) {
        unsigned long elapsed = millis() - stateStartTime[i];
        unsigned long remaining = (RESET_DURATION > elapsed) ? ((RESET_DURATION - elapsed) / 1000UL) : 0;
        if (remaining > 0) {
          client.println("<button class='status-message status-error' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
            label + ": Reset automatyczny w toku. Przekaźnik włączony. Pozostało: " + String(remaining) + " s" + monitorInfo + "</button>");
        } else {
          client.println("<button class='status-message status-warning' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
            label + ": Urządzenie restartuje się. Proszę czekać..." + monitorInfo + "</button>");
        }
      }
      else if (state == WAIT_START) {
        unsigned long elapsed = millis() - stateStartTime[i];
        unsigned long remaining = (DEVICE_STARTTIME > elapsed) ? ((DEVICE_STARTTIME - elapsed) / 1000UL) : 0;
        if (remaining > 0) {
          client.println("<button class='status-message status-warning' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
            label + ": Urządzenie restartuje się. Proszę czekać ok. " + String(remaining) + " s." + monitorInfo + "</button>");
        } else {
          client.println("<button class='status-message status-normal' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
            label + ": Praca normalna" + monitorInfo + "</button>");
        }
      }
      else if (state == MAX_ATTEMPTS) {
        client.println("<button class='status-message status-error' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
          label + ": Osiągnięto max prób resetu!" + monitorInfo + "</button>");
      }
      else {
        client.println("<button class='status-message status-normal' disabled style='width:100%; margin-bottom:4px; font-size:0.96em;'>" +
          label + ": Praca normalna" + monitorInfo + "</button>");
      }
    }
  }
  client.println("</div>");

  // Przyciski
  client.println("<div class='btn-group' style='margin:14px 0 10px 0; text-align:center;'>");
  if (anyMaxReached) {
    client.println("<button disabled style='background:#bdbdbd; color:#9e9e9e; min-width: 110px;'>Auto-reset: WYŁ. (MAX)</button>");
    client.println("<button style='min-width: 110px;' onclick=\"location.href='/resetAttempts'\">Reset prób</button>");
  } else {
    if (autoResetEnabled) {
      client.println("<button style='background:linear-gradient(120deg,#b7efc5 0%,#d6ffe6 100%); color:#17603a; font-weight:bold; min-width:110px; border:1.5px solid #60e39a; box-shadow:0 2px 8px #9cdebc55;' onclick=\"location.href='/toggleAuto'\">Auto-reset: WŁ.</button>");
    } else {
      client.println("<button style='background:linear-gradient(120deg,#ff8585 0%,#ffd1b8 100%); color:#7a1d1d; font-weight:bold; min-width:110px; border:1.5px solid #f57373;' onclick=\"location.href='/toggleAuto'\">Auto-reset: WYŁ.</button>");
    }
  }
  client.println("<button style='min-width: 110px;' onclick=\"location.href='/updateFirmware'\">Aktualizuj</button>");
  client.println("<button style='min-width: 110px;' onclick=\"location.href='/resetESP'\">Reset ESP</button>");
  client.println("</div>");

  // Tabela przekaźników (jeszcze niższa)
  client.println("<table style='width:100%; margin-bottom:4px; font-size:0.98em;'><tbody>");
  for (int i = 0; i < 4; i++) {
    client.println("<tr>");
    client.print("<td style='text-align:left; font-weight:bold; width:90px;'>Relay " + String(i) + ": ");
    if (relayStates[i] == "ON") {
      client.print("<span class='relay-on'>ON</span>");
    } else {
      client.print("<span class='relay-off'>OFF</span>");
    }
    client.print("</td>");
    client.print("<td style='text-align:left;'>");
    if (linkDescriptions[i] != "Brak opisu") {
      client.print(linkDescriptions[i]);
    }
    client.print("</td>");
    client.print("<td class='timeCell' style='text-align:right; width:80px;'>");
    if (relayStates[i] == "ON") {
      unsigned long elapsed = millis() - relayOnTime[i];
      if (elapsed < autoOffTime) {
        unsigned long remainingSec = (autoOffTime - elapsed) / 1000;
        client.print(String(remainingSec) + " s");
      }
    }
    client.print("</td>");
    client.print("<td style='text-align:right; width:80px;'>");
    client.print("<button style='margin:2px 0; padding:5px 11px; border-radius:7px;' onclick=\"location.href='/toggle?relay=");
    client.print(i);
    client.print("'\">Przełącz</button></td>");
    client.println("</tr>");
  }
  client.println("</tbody></table>");

  // Checkbox Auto-reset
  client.println("<div style='text-align: center; margin: 10px 0;'>");
  if (anyMaxReached) {
    client.println("<label style='color: #9e9e9e;'><input type='checkbox' disabled> Auto-reset: WYŁ. (MAX)</label>");
  } else {
    client.println("<label style='color: #5b5b5b;'><input type='checkbox' " + String(autoResetEnabled ? "checked" : "") + " onclick=\"location.href='/toggleAuto'\"> Auto-reset</label>");
  }
  client.println("</div>");

  // Informacja o statusie zapisu logów
if (!enableLogFiles) {
  client.println("<div style='margin-bottom:22px; padding:12px; background:#662222; color:#ffaaaa; border-radius:7px; border:1px solid #bb4444; font-size:1.1em;'>");
  client.println("<b>Uwaga:</b> Zapisywanie plików logów i historii logowań jest <b>wyłączone</b>.<br>Nie będą tworzone nowe logi ani backupy TXT.");
  client.println("</div>");
}

  // Sekcja czasu i synchronizacji
  client.println("<div class='time-container' style='min-height:38px; padding:5px; background:#252525; border-radius:8px; margin-bottom: 3px;'>");
  String currentDateTime = getCurrentDateTime();
  if (currentDateTime != "brak synchronizacji") {
    client.println("<p style='margin: 5px 0; color: #4fc3f7;'>" + currentDateTime + "</p>");
  } else {
    client.println("<p style='margin: 5px 0; color: #bdbdbd;'>brak synchronizacji czasu</p>");
  }
  client.println("<form method='POST' action='/syncTime' style='margin:0;'>");
  client.println("<button type='submit' class='btn-small'>Synchronizuj teraz</button>");
  client.println("</form>");
  client.println("</div>");

  client.println("</div>"); // KONIEC głównego kontenera

  sendCommonHtmlFooter(client);
}



#endif
