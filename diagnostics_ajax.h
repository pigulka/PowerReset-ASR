#ifndef DIAGNOSTICS_AJAX_H
#define DIAGNOSTICS_AJAX_H

#include <Ethernet.h>
#include "globals.h"  // jeśli korzystasz z globalnych zmiennych

void sendDiagnosticsData(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Cache-Control: no-cache");
  client.println("Connection: close");
  client.println();

  client.println("<table id='diagTable' style='width:100%; margin-bottom:18px; border-collapse:collapse;'>");
  client.println("<tr style='background:#282c34;'><th>Wejście</th><th>Monitorowane</th><th>Stan wejścia</th><th>Przekaźnik</th><th>Stan automatu</th><th>Próby</th><th>Pozostały czas</th></tr>");
  
  for (int i = 0; i < 4; i++) {
    client.print("<tr>");
    client.print("<td>"); client.print(inputLabels[i]); client.print("</td>");
    // Monitorowane
    if (inputMonitoringEnabled[i])
      client.print("<td style='color:#8bc34a;'>TAK</td>");
    else
      client.print("<td style='color:#bdbdbd;'>NIE</td>");
    // Stan wejścia
    if (!inputMonitoringEnabled[i]) {
      client.print("<td style='color:#bdbdbd;'>--</td>");
    } else if (digitalRead(inputPins[i]) == HIGH) {
      client.print("<td style='color:#4fc3f7;'>HIGH</td>");
    } else {
      client.print("<td style='color:#ff5252;'>LOW</td>");
    }
    // Przekaźnik
    if (relayStates[i] == "ON")
      client.print("<td style='color:#76ff03;'>ON</td>");
    else
      client.print("<td style='color:#bdbdbd;'>OFF</td>");
    // Stan maszyny stanów
    String stanStr = "";
    String kolorStan = "#bdbdbd";
    switch (inputState[i]) {
      case IDLE: stanStr = "IDLE"; kolorStan="#bdbdbd"; break;
      case WAITING_1MIN: stanStr = "WAITING_1MIN"; kolorStan="#ffc107"; break;
      case DO_RESET: stanStr = "DO_RESET"; kolorStan="#ff5252"; break;
      case WAIT_START: stanStr = "WAIT_START"; kolorStan="#4fc3f7"; break;
      case MAX_ATTEMPTS: stanStr = "MAX_ATTEMPTS"; kolorStan="#e53935"; break;
    }
    client.print("<td style='color:" + kolorStan + "; font-weight:bold;'>" + stanStr + "</td>");
    // Próby resetu
    client.print("<td>"); client.print(resetAttempts[i]); client.print("</td>");
    // Pozostały czas
    String czasStr = "-";
    if (inputState[i] == WAITING_1MIN) {
      unsigned long elapsed = millis() - stateStartTime[i];
      if (WAITING_1MIN_TIME > elapsed)
        czasStr = String((WAITING_1MIN_TIME - elapsed) / 1000) + " s";
      else
        czasStr = "0 s";
    } else if (inputState[i] == DO_RESET) {
      unsigned long elapsed = millis() - stateStartTime[i];
      if (RESET_DURATION > elapsed)
        czasStr = String((RESET_DURATION - elapsed) / 1000) + " s";
      else
        czasStr = "0 s";
    } else if (inputState[i] == WAIT_START) {
      unsigned long elapsed = millis() - stateStartTime[i];
      if (DEVICE_STARTTIME > elapsed)
        czasStr = String((DEVICE_STARTTIME - elapsed) / 1000) + " s";
      else
        czasStr = "0 s";
    }
    client.print("<td style='color:#4fc3f7;'>"); client.print(czasStr); client.print("</td>");
    client.println("</tr>");
  }
  client.println("</table>");

  // Informacja o statusie auto-resetu (tryb automatyczny)
client.println("<div style='margin: 15px 0; text-align: center;'>");
if (autoResetEnabled) {
    client.println("<span style='color: #26a69a; font-size: 1.25em; font-weight: bold;'>Tryb automatyczny: WŁĄCZONY</span>");
} else {
    client.println("<span style='color: #ff5252; font-size: 1.25em; font-weight: bold;'>Tryb automatyczny: WYŁĄCZONY</span>");
}
client.println("</div>");

  // Ostatnie logi
  client.println("<h3 style='margin-top:24px; color:#4fc3f7;'>Ostatnie logi</h3>");
  client.println("<ul id='diagLogs' style='list-style:none; padding-left:0; max-height:220px; overflow:auto;'>");
  int startIdx = logIndex - 1;
  if (startIdx < 0) startIdx = MAX_LOGS - 1;
  for (int j = 0, idx = startIdx; j < 10; j++) {
    if (logHistory[idx].length() > 0)
      client.println("<li style='margin-bottom:4px;'>" + logHistory[idx] + "</li>");
    idx--;
    if (idx < 0) idx = MAX_LOGS - 1;
  }
  client.println("</ul>");
}

#endif
