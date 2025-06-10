#ifndef MAIN_PAGE_HTML_H
#define MAIN_PAGE_HTML_H

#include <Ethernet.h>
#include "globals.h"
#include "time_utils.h"

void sendCommonHtmlHeader(EthernetClient &client, const String &title);
void sendCommonHtmlFooter(EthernetClient &client);
String getInputStatusString(int i);
extern const String firmwareVersion;
extern bool inputMonitoringEnabled[4];
extern String inputLabels[4];

// --- Strona główna ---
void sendMainPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "Panel sterowania PowerReset");

  // --- CSS ---
  client.println("<style>");
  client.println(".main-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 30px 28px; margin: 30px 0 24px 0; }");
  client.println(".tile { background: #20232a; border-radius: 14px; box-shadow: 0 3px 16px #1118; padding: 28px 22px 22px 22px; color: #e0e0e0; min-width: 250px; min-height: 180px; display: flex; flex-direction: column; align-items: flex-start; position: relative; opacity: 1; transition: opacity 0.2s; }");
  client.println(".tile-title {font-size: 1.15em;color: #4fc3f7;font-weight: bold;margin-bottom: 2px;cursor: pointer;border-bottom: 1.5px dashed #222000;transition: color 0.18s;}");
  client.println(".tile-title:hover { color: #ffe082; }");
  client.println(".tile-desc {color: #bdbdbd; font-size: 0.96em; margin-bottom: 6px; cursor: pointer;}");
  client.println(".tile-edit {padding: 5px 9px; border-radius: 5px; border: none; background: #262e3c;color: #ffe082; font-size: 1em; margin-bottom: 3px; width: 96%;}");
  client.println(".state-row {display: flex; gap: 12px; align-items: center; margin: 9px 0;}");
  client.println(".input-state { font-weight: bold; font-size: 1.12em; }");
  client.println(".input-low  { color: #ff5252; }");
  client.println(".input-high { color: #b2ff59; }");
  client.println(".monitor-btn {padding:6px 14px;background:#8bc34a;color:#252525;border:none;border-radius:7px;font-weight:bold;cursor:pointer;}");
  client.println(".monitor-btn.on { background: #ffd600; color: #252525;}");
  client.println(".monitor-btn:disabled { opacity: 0.5;}");
  client.println(".relay-row { display: flex; gap: 12px; align-items: center;}");
  client.println(".relay-on { color: #43e03b; font-weight: bold;}");
  client.println(".relay-off { color: #ff5252; font-weight: bold;}");
  client.println(".relay-btn {padding:7px 16px;background:#2196f3;color:#fff;border:none;border-radius:7px;font-weight:bold;cursor:pointer;}");
  client.println(".relay-btn:hover {background:#1976d2;}");
  client.println(".time-count { color: #ffe082; font-size:0.98em; margin-left:10px;}");
  client.println(".save-ok { color: #b2ff59; font-size:0.91em; margin-left:5px;}");
  client.println(".save-err { color: #ff5252; font-size:0.91em; margin-left:5px;}");
  client.println(".main-options {margin: 22px 0 16px 0; display: flex; gap: 30px; flex-wrap:wrap;}");
  client.println(".main-options select, .main-options button {padding:7px 18px; border-radius:7px; border:none; font-size:1em;}");
  client.println(".main-options button {background:#4fc3f7;color:#17242e;font-weight:bold;cursor:pointer;}");
  client.println(".auto-checkbox {margin-left: 16px;}");
  client.println(".sys-info {background:#23262c; border-radius:9px; color:#e0e0e0; padding:13px 16px; margin-bottom:10px; font-size:1em; box-shadow:0 1px 8px #1127; display:flex; flex-wrap:wrap; gap:32px; align-items:center;justify-content:center;}");
  client.println(".sys-info span{margin-right:30px;}");
  client.println(".footer { text-align: center; color: #757575; margin-top: 32px; font-size: 0.97em; border-top: 1px solid #444; padding-top: 16px; letter-spacing: 0.5px;}");
  client.println("</style>");


  client.println("</style>");

  // --- JS do inline-edit (AJAX z fetch) ---
  client.println("<script>");
client.println("window.isEditing = false;");
client.println("function enableEdit(id, oldValue, api, label) {");
client.println("  window.isEditing = true;");
client.println("  var tile = document.getElementById(id);");
client.println("  tile.innerHTML = \"<input type='text' class='tile-edit' id='\"+id+\"_edit' value='\"+oldValue.replace(/'/g, '&#39;')+\"' onblur=\\\"saveEdit('\"+id+\"',this.value,'\"+api+\"')\\\" onkeydown=\\\"if(event.key=='Enter'){this.blur();}\\\">\";");
client.println("  setTimeout(function(){ document.getElementById(id+'_edit').focus(); }, 80);");
client.println("}");
client.println("function saveEdit(id, val, api) {");
client.println("  fetch(api, {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'value='+encodeURIComponent(val)})");
client.println("    .then(r=>r.ok?r.text():Promise.reject()).then(txt=>{");
client.println("      document.getElementById(id).innerHTML = val+\"<span class='save-ok'>✓</span>\";");
client.println("    })");
client.println("    .catch(function(){ document.getElementById(id).innerHTML = \"<span class='save-err'>Błąd!</span>\"; });");
client.println("  setTimeout(function(){ window.isEditing = false; fetchStatusAndUpdate(); }, 600);");
client.println("}");
client.println("function fetchStatusAndUpdate() {");
client.println("  if(window.isEditing) return;");
client.println("  let numTiles = document.querySelectorAll('.tile').length;");
client.println("  fetch('/api/mainStatus')");
client.println("    .then(function(resp){ return resp.json(); })");
client.println("    .then(function(data){");
client.println("      for (let i = 0; i < numTiles; ++i) {");
client.println("        let stateSpan = document.querySelector('#inState' + i);");
client.println("        if (stateSpan) {");
client.println("          if (data['in'+i] === null) {");
client.println("            stateSpan.innerText = '--';");
client.println("            stateSpan.className = 'input-state';");
client.println("          } else if (data['in'+i] == 1) {");
client.println("            stateSpan.innerText = 'HIGH';");
client.println("            stateSpan.className = 'input-state input-high';");
client.println("          } else {");
client.println("            stateSpan.innerText = 'LOW';");
client.println("            stateSpan.className = 'input-state input-low';");
client.println("          }");
client.println("        }");
client.println("        let relaySpan = document.querySelector('#relayState' + i);");
client.println("        if(relaySpan) relaySpan.innerText = (data['relay'+i] == 'ON' ? 'ON' : 'OFF');");
client.println("        let labelDiv = document.querySelector('#inputLabel' + i);");
client.println("        if(labelDiv && labelDiv.contentEditable !== 'true') labelDiv.innerText = data['label'+i];");
client.println("        let descDiv = document.querySelector('#relayDesc' + i);");
client.println("        if(descDiv && descDiv.contentEditable !== 'true') descDiv.innerText = data['desc'+i];");
client.println("        let offSpan = document.querySelector('#autoOff' + i);");
client.println("        if(offSpan) { let remain = parseInt(data['autoOffRemaining'+i]); offSpan.innerText = remain > 0 ? ('Auto-OFF za: ' + remain + ' s') : ''; }");
client.println("        let stSpan = document.querySelector('#status' + i);");
client.println("        if(stSpan) stSpan.innerHTML = data['status'+i];");
client.println("      }");
client.println("      let placeLabel = document.getElementById('placeStrLabel');");
client.println("      if(placeLabel && placeLabel.contentEditable !== 'true') placeLabel.innerText = data.placeStr;");
client.println("      let curTime = document.getElementById('currentTime');");
client.println("      if(curTime) curTime.innerText = data.currentTime;");
client.println("      let fwVer = document.getElementById('firmwareVer');");
client.println("      if(fwVer) fwVer.innerText = data.firmwareVersion;");
client.println("    })");
client.println("    .catch(function(e){ console.warn('AJAX err', e); });");
client.println("}");
client.println("setInterval(fetchStatusAndUpdate, 2000);");
client.println("window.onload = fetchStatusAndUpdate;");
client.println("</script>");



  // Pasek info o systemie
  IPAddress ip = Ethernet.localIP();
  bool linkOk = (Ethernet.linkStatus() == LinkON);
  client.println("<div class='sys-info'>");
  client.print("<div class='tile-title' id='placeStrLabel' onclick=\"enableEdit('placeStrLabel','" + placeStr + "','/api/place','Nazwa miejsca')\" style='font-size:1.18em;text-align:center;margin-bottom:0px;cursor:pointer;margin-right:30px;'>");
  client.print(placeStr);
  client.println("</div>");
  client.print("<span style='margin-right:30px;'>IP: <b style='color:#4fc3f7'>");
  client.print(ip);
  client.println("</b></span>");
  client.print("<span style='margin-right:30px;'>Połączenie: ");
  client.print(linkOk ? "<span style='color:#b2ff59'>AKTYWNE</span>" : "<span style='color:#ff5252'>OFFLINE</span>");
  client.println("</span>");
  client.println("</div>");

  client.println("<div style='margin: 16px 0; text-align:center; font-size:1.18em;'>");
if (relayMode == 0) {
    client.println("<span style='color:#4fc3f7;font-weight:bold;'>Tryb pracy przekaźników: <b>4 x 1</b> (każde wejście steruje osobnym przekaźnikiem)</span>");
} else {
    client.println("<span style='color:#ffb300;font-weight:bold;'>Tryb pracy przekaźników: <b>2 x 2</b> (wejścia sterują przekaźnikami parami)</span>");
}
client.println("</div>");


  // --- KAFELKI WEJŚĆ / PRZEKAŹNIKÓW ---
  client.println("<div class='main-grid'>");
  if (relayMode == 0) {
    // Tryb 4x1: 4 niezależne kafle
    for (int i = 0; i < 4; ++i) {
      client.println("<div class='tile'>");

      // Nazwa wejścia (edytowalna)
      client.print("<div class='tile-title' id='inputLabel" + String(i) + "' onclick=\"enableEdit('inputLabel" + String(i) + "','" + inputLabels[i] + "','/api/label?input=" + String(i) + "','Etykieta wejścia')\">");
      client.print(inputLabels[i]);
      client.println("</div>");

      // Opis przekaźnika (edytowalny)
      client.print("<div class='tile-desc' id='relayDesc" + String(i) + "' onclick=\"enableEdit('relayDesc" + String(i) + "','" + linkDescriptions[i] + "','/api/desc?relay=" + String(i) + "','Opis przekaźnika')\">");
      client.print(linkDescriptions[i]);
      client.println("</div>");

      // Stan wejścia
      client.print("<div class='state-row'>Stan wejścia: <span id='inState" + String(i) + "' class='input-state ");
      if (!inputMonitoringEnabled[i]) {
        client.print("'>--");
      } else {
        int val = digitalRead(inputPins[i]);
        client.print(val ? "input-high'>" : "input-low'>");
        client.print(val ? "HIGH" : "LOW");
      }
      client.println("</span></div>");
      client.print("<button class='monitor-btn");
      if (inputMonitoringEnabled[i]) client.print(" on");
      client.print("' onclick=\"location.href='/toggleInput?input=" + String(i) + "'\">");
      client.print(inputMonitoringEnabled[i] ? "Wyłącz monitorowanie" : "Włącz monitorowanie");
      client.println("</button>");
      client.print("<div class='relay-row'>Przekaźnik: <span id='relayState" + String(i) + "' class='");
      client.print(relayStates[i] == "ON" ? "relay-on'>ON" : "relay-off'>OFF");
      client.print("</span>");
      client.print("<button class='relay-btn' style='margin-left:18px;' onclick=\"location.href='/toggle?relay=" + String(i) + "'\">Przełącz</button>");
      client.println("</div>");
      client.print("<span class='time-count' id='autoOff" + String(i) + "'>");
      if (relayStates[i] == "ON") {
        unsigned long elapsed = millis() - relayOnTime[i];
        if (elapsed < autoOffTime) {
          unsigned long remainingSec = (autoOffTime - elapsed) / 1000;
          client.print("Auto-OFF za: " + String(remainingSec) + " s");
        }
      }
      client.println("</span>");
      client.print("<div style='margin-top:10px; font-size:1em;' id='status" + String(i) + "'>");
      client.print(getInputStatusString(i));
      client.println("</div>");
      client.println("</div>");
    }
  } else {
    // Tryb 2x2: 2 kafle, obsługa par wejść/przekaźników
    for (int j = 0; j < 2; ++j) {
      int idx1 = j * 2;
      int idx2 = idx1 + 1;
      client.println("<div class='tile'>");
      client.print("<div class='tile-title'>Wejścia ");
      client.print(String(idx1 + 1) + "/" + String(idx2 + 1) + " → Przekaźniki " + String(idx1 + 1) + "/" + String(idx2 + 1));
      client.println("</div>");
      client.print("<div class='tile-desc'>");
      client.print(inputLabels[idx1] + " + " + inputLabels[idx2] + " sterują " + linkDescriptions[idx1] + " + " + linkDescriptions[idx2]);
      client.println("</div>");
      client.print("<div class='state-row'>Stany wejść: <span id='inState" + String(idx1) + "' class='input-state ");
      client.print(inputState[idx1] ? "input-high'>" : "input-low'>");
      client.print(inputState[idx1] ? "HIGH" : "LOW");
      client.print("</span> / <span id='inState" + String(idx2) + "' class='input-state ");
      client.print(inputState[idx2] ? "input-high'>" : "input-low'>");
      client.print(inputState[idx2] ? "HIGH" : "LOW");
      client.println("</span></div>");
      // --- Przycisk monitorowania (pary) ---
      client.print("<button class='monitor-btn");
      if (inputMonitoringEnabled[idx1] || inputMonitoringEnabled[idx2]) client.print(" on");
      client.print("' onclick=\"location.href='/toggleInput?input=" + String(idx1) + "&pair=1'\">");
      client.print((inputMonitoringEnabled[idx1] || inputMonitoringEnabled[idx2]) ? "Wyłącz monitorowanie" : "Włącz monitorowanie");
      client.println("</button>");
      client.print("<div class='relay-row'>Przekaźniki: <span id='relayState" + String(idx1) + "' class='");
      client.print(relayStates[idx1] == "ON" ? "relay-on'>ON" : "relay-off'>OFF");
      client.print("</span> / <span id='relayState" + String(idx2) + "' class='");
      client.print(relayStates[idx2] == "ON" ? "relay-on'>ON" : "relay-off'>OFF");
      client.print("</span>");
      // Przycisk przełącza oba przekaźniki naraz
      client.print("<button class='relay-btn' style='margin-left:18px;' onclick=\"location.href='/toggle?relay=" + String(idx1) + "&pair=1'\">Przełącz oba</button>");
      client.println("</div>");
      // --- Licznik czasu (pary) ---
      client.print("<span class='time-count' id='autoOff" + String(idx1) + "'>");
      if (relayStates[idx1] == "ON") {
        unsigned long elapsed = millis() - relayOnTime[idx1];
        if (elapsed < autoOffTime) {
          unsigned long remainingSec = (autoOffTime - elapsed) / 1000;
          client.print("Auto-OFF za: " + String(remainingSec) + " s");
        }
      }
      client.println("</span>");
      client.print("<span class='time-count' id='autoOff" + String(idx2) + "'>");
      if (relayStates[idx2] == "ON") {
        unsigned long elapsed = millis() - relayOnTime[idx2];
        if (elapsed < autoOffTime) {
          unsigned long remainingSec = (autoOffTime - elapsed) / 1000;
          client.print("Auto-OFF za: " + String(remainingSec) + " s");
        }
      }
      client.println("</span>");
      // --- Status / info automatu (pary) ---
      client.print("<div style='margin-top:10px; font-size:1em;' id='status" + String(idx1) + "'>");
      client.print(getInputStatusString(idx1));
      client.println("</div>");
      client.print("<div style='margin-top:10px; font-size:1em;' id='status" + String(idx2) + "'>");
      client.print(getInputStatusString(idx2));
      client.println("</div>");
      client.println("</div>");
    }
  }
  client.println("</div>");  // main-grid

  // --- Opcje pod kaflami ---
  client.println("<div class='main-options'>");
  client.println("<form method='POST' action='/setRelayMode' style='display:inline;'>");
  client.println("<label for='relayMode' style='color:#4fc3f7;font-weight:bold;'>Tryb pracy przekaźników: </label>");
  client.println("<select name='relayMode' id='relayMode'>");
  client.print("<option value='4x1'");
  if (relayMode == 0) client.print(" selected");
  client.println(">4 x 1 (E1→R1, ...)</option>");
  client.print("<option value='2x2'");
  if (relayMode == 1) client.print(" selected");
  client.println(">2 x 2 (E1/E2→R1+R2, ...)</option>");
  client.println("</select>");
  client.println("<button type='submit'>Zmień tryb</button>");
  client.println("</form>");
  client.print("<label class='auto-checkbox'><input type='checkbox' ");
  if (autoResetEnabled) client.print("checked ");
  client.print("onclick=\"location.href='/toggleAuto'\"> Auto-reset</label>");
  client.println("</div>");

  // --- Czas, synchronizacja, wersja, data kompilacji ---
  client.println("<div style='margin:18px 0 0 0; background:#262d39; padding:10px 18px; border-radius:8px; color:#fff;'>");
  String currentDateTime = getCurrentDateTime();
  client.print("<span id='currentTime' style='color:#4fc3f7;'>Aktualny czas: ");
  client.print(currentDateTime);
  client.println("</span>");
  if (lastSyncEpochTime > 0) {
    String lastSyncStr = formatEpochTime(lastSyncEpochTime);
    client.println("<br><span style='color:#b2dfdb;'>Ostatnia synchronizacja NTP: " + lastSyncStr + "</span>");
  }
  client.println("<form method='POST' action='/syncTime' style='display:inline; margin-left:16px;'>"
                 "<button type='submit' class='btn-small' style='font-weight:bold; margin-left:16px;'>Synchronizuj teraz</button></form>");
  client.println("<span style='margin-left:30px; color:#bdbdbd; font-size:0.92em;'>Wersja: v<span id='firmwareVer'>" + String(firmwareVersion) + "</span></span>");
  client.println("</div>");

  // --- Stopka ---
  client.println("<div class='footer'>PowerReset &copy; 2025 TAURON Dystrybucja S.A.</div>");

  client.println("<script>");
client.println("window.isEditing = false;");
client.println("function enableEdit(id, oldValue, api, label) {");
client.println("  window.isEditing = true;");
client.println("  var tile = document.getElementById(id);");
client.println("  tile.innerHTML = \"<input type='text' class='tile-edit' id='\"+id+\"_edit' value='\"+oldValue.replace(/'/g, '&#39;')+\"' onblur='saveEdit(\\\"\"+id+\"\\\",this.value,\\\"\"+api+\"\\\")' onkeydown='if(event.key==\\\"Enter\\\"){this.blur();}\">\";");
client.println("  setTimeout(()=>{document.getElementById(id+'_edit').focus();},80);");
client.println("}");
client.println("function saveEdit(id, val, api) {");
client.println("  fetch(api, {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'value='+encodeURIComponent(val)})");
client.println("    .then(r=>r.ok?r.text():Promise.reject()).then(txt=>{");
client.println("      document.getElementById(id).innerHTML = val+\"<span class='save-ok'>✓</span>\";");
client.println("    })");
client.println("    .catch(()=>{ document.getElementById(id).innerHTML = \"<span class='save-err'>Błąd!</span>\"; });");
client.println("  setTimeout(()=>{ window.isEditing = false; fetchStatusAndUpdate(); }, 600);");
client.println("}");



// AJAX update z dynamiczną ilością kafelków i poprawnym wyświetlaniem stanu wejść
client.println("function fetchStatusAndUpdate() {");
client.println("  if(window.isEditing) return;");
client.println("  let numTiles = document.querySelectorAll('.tile').length;");
client.println("  fetch('/api/mainStatus')");
client.println("    .then(resp => resp.json())");
client.println("    .then(data => {");
client.println("      for (let i = 0; i < numTiles; ++i) {");
client.println("        let stateSpan = document.querySelector('#inState' + i);");
client.println("        if (stateSpan) {");
client.println("          if (data['in'+i] === null) {");
client.println("            stateSpan.innerText = '--';");
client.println("            stateSpan.className = 'input-state';");
client.println("          } else if (data['in'+i] == 1) {");
client.println("            stateSpan.innerText = 'HIGH';");
client.println("            stateSpan.className = 'input-state input-high';");
client.println("          } else {");
client.println("            stateSpan.innerText = 'LOW';");
client.println("            stateSpan.className = 'input-state input-low';");
client.println("          }");
client.println("        }");
client.println("        let relaySpan = document.querySelector('#relayState' + i);");
client.println("        if(relaySpan) relaySpan.innerText = (data['relay'+i] == 'ON' ? 'ON' : 'OFF');");
client.println("        let labelDiv = document.querySelector('#inputLabel' + i);");
client.println("        if(labelDiv && labelDiv.contentEditable !== 'true') labelDiv.innerText = data['label'+i];");
client.println("        let descDiv = document.querySelector('#relayDesc' + i);");
client.println("        if(descDiv && descDiv.contentEditable !== 'true') descDiv.innerText = data['desc'+i];");
client.println("        let offSpan = document.querySelector('#autoOff' + i);");
client.println("        if(offSpan) { let remain = parseInt(data['autoOffRemaining'+i]); offSpan.innerText = remain > 0 ? ('Auto-OFF za: ' + remain + ' s') : ''; }");
client.println("        let stSpan = document.querySelector('#status' + i);");
client.println("        if(stSpan) stSpan.innerHTML = data['status'+i];");  // <-- tylko RAZ!
client.println("      }");
client.println("      let placeLabel = document.getElementById('placeStrLabel');");
client.println("      if(placeLabel && placeLabel.contentEditable !== 'true') placeLabel.innerText = data.placeStr;");
client.println("      let curTime = document.getElementById('currentTime');");
client.println("      if(curTime) curTime.innerText = data.currentTime;");
client.println("      let fwVer = document.getElementById('firmwareVer');");
client.println("      if(fwVer) fwVer.innerText = data.firmwareVersion;");
client.println("    })");
client.println("    .catch(e => { console.warn('AJAX err', e); });");
client.println("}");
client.println("setInterval(fetchStatusAndUpdate, 2000);");
client.println("window.onload = fetchStatusAndUpdate;");
client.println("</script>");


sendCommonHtmlFooter(client);
}

#endif

// --- Funkcja pomocnicza do statusów (musisz mieć ją w kodzie!) ---
String getInputStatusString(int i) {
  // Możesz tutaj dodać swój switch dla statusu
  InputViewState view = getInputViewState(i);
  switch (view) {
    case VIEW_IDLE: return "<span style='color:#b2ff59;'>Praca normalna</span>";
    case VIEW_WAITING_1MIN: return "<span style='color:#ffe082;'>Input LOW – czekamy na zmianę stanu na wejściu</span>";
    case VIEW_DO_RESET: return "<span style='color:#ffa000;'>Reset w toku...</span>";
    case VIEW_WAIT_START: return "<span style='color:#64b5f6;'>Restart, trwa rozruch urządzenia</span>";
    case VIEW_MAX_ATTEMPTS:
    return "<div style='margin-top:8px;'>"
       "<span style='color:#ff5252;'>Osiągnięto ustaloną maksymalną liczbę prób!</span>"
       "<br><button class='relay-btn' style='margin-top:6px;background:#ff5252;' "
       "onclick=\"location.href='/resetAttempts'\">Reset prób</button>"
       "</div>";
    
    default: return "<span>Nieznany stan</span>";
  }
}
