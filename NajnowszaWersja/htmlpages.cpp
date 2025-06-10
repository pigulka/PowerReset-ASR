#include "htmlpages.h"
#include "globals.h"
#include "functions.h"
#include <Preferences.h>
extern Preferences preferences;

void sendCommonHtmlHeader(EthernetClient &client, const String &title);
void sendCommonHtmlFooter(EthernetClient &client);
void addLog(const String &msg);

void sendChangeAuthPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Zmiana loginu i hasła");

  client.println("<div class='panel' style='background:#21242c; border-radius:12px; padding:32px; max-width:420px; margin:auto;'>");
  client.println("<form method='POST' action='/changeAuth'>");

  client.println("<div style='margin-bottom:20px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Nowy login:</label>");
  client.println("<input type='text' name='newUser' required style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:20px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Nowe hasło:</label>");
  client.println("<input type='password' id='newPass' name='newPass' required style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:28px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Powtórz nowe hasło:</label>");
  client.println("<input type='password' id='confirmPass' name='confirmPass' required style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  client.println("<label style='display:flex; align-items:center; margin-bottom:25px; gap:10px;'>");
  client.println("<input type='checkbox' id='showPass' onclick=\"togglePasswordVisibility()\">");
  client.println("<span style='color:#bbb;'>Pokaż hasła</span>");
  client.println("</label>");

  client.println("<button type='submit' style='padding:12px 28px; background:#2196f3; color:#fff; border:none; border-radius:6px; font-size:1.05em; cursor:pointer;'>Zapisz zmiany</button>");

  // Skrypt do pokazywania hasła
  client.println("<script>");
  client.println("function togglePasswordVisibility() {");
  client.println("  var newPass = document.getElementById('newPass');");
  client.println("  var confirmPass = document.getElementById('confirmPass');");
  client.println("  if (newPass.type === 'password') {");
  client.println("    newPass.type = 'text';");
  client.println("    confirmPass.type = 'text';");
  client.println("  } else {");
  client.println("    newPass.type = 'password';");
  client.println("    confirmPass.type = 'password';");
  client.println("  }");
  client.println("}");
  client.println("</script>");

  client.println("</form>");
  client.println("</div>");

  sendCommonHtmlFooter(client);
}


// Strona edycji etykiet dla wszystkich przekaźników
void sendChangeAllLinksPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");

  // --- Nowy styl kontenera ---
  client.println("<div class='container' style='max-width:430px; margin:32px auto 24px auto; background:#20232a; padding:22px 18px 18px 18px; border-radius:14px; box-shadow:0 2px 14px #1118; color:#e0e0e0;'>");
  client.println("<h2 style='text-align:center; margin-top:0; margin-bottom:22px; font-size:1.22em; color:#fff;'>Edycja etykiet przekaźników</h2>");
  client.println("<form method='POST' action='/changeAllLinks'>");

  for (int i = 0; i < 4; i++) {
    client.println("<div style='margin-bottom:15px;'>");
    client.println("<label style='display:block; margin-bottom:5px;'>Etykieta przekaźnika " + String(i) + ":</label>");
    client.println("<input type='text' name='link" + String(i) + "' value='" + linkDescriptions[i] + "' style='width:98%; padding:8px;'>");
    client.println("</div>");
  }

  client.println("<button type='submit' style='padding:10px 20px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Zapisz wszystkie etykiety</button>");
  client.println("</form>");
  client.println("</div>");

  sendCommonHtmlFooter(client);
}



// void sendSettingsPage_GET(EthernetClient &client) {
//   Serial.printf("[DEBUG] Wywołano sendSettingsPage_GET, ACTIVE_RELAYS = %d\n", ACTIVE_RELAYS);

//   sendCommonHtmlHeader(client, "");
//   sendMainContainerBegin(client, "Konfiguracja czasów");

//   client.println("<div class='panel' style='background:#21242c; border-radius:12px; padding:32px;'>");
//   client.println("<form method='POST' action='/saveSettings'>");

//   client.println("<div style='margin-bottom:20px;'>");
//   client.println("<label style='display:block; margin-bottom:7px;'>Czas oczekiwania na powrót sygnału (s):</label>");
//   client.println("<input type='number' name='waitTime' value='" + String(WAITING_1MIN_TIME / 1000) + "' min='10' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
//   client.println("</div>");

//   client.println("<div style='margin-bottom:20px;'>");
//   client.println("<label style='display:block; margin-bottom:7px;'>Czas trwania resetu (s):</label>");
//   client.println("<input type='number' name='resetDur' value='" + String(RESET_DURATION / 1000) + "' min='10' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
//   client.println("</div>");

//   client.println("<div style='margin-bottom:20px;'>");
//   client.println("<label style='display:block; margin-bottom:7px;'>Czas potrzebny na uruchomienie urządzenia (s):</label>");
//   client.println("<input type='number' name='devStart' value='" + String(DEVICE_STARTTIME / 1000) + "' min='10' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
//   client.println("</div>");

//   // OPCJA LICZBA PRZEKAŹNIKÓW
//   client.println("<div style='margin-bottom:32px;'>");
//   client.println("<label style='display:block; margin-bottom:7px;'>Liczba aktywnych przekaźników:</label>");
//   client.println("<select name='relayCount' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444; font-size:1em;'>");

//   for (int i = 1; i <= 4; i++) {
//     client.print("<option value='");
//     client.print(i);
//     client.print("'");
//     if (ACTIVE_RELAYS == i) client.print(" selected");
//     client.print(">");
//     client.print(i);
//     client.println("</option>");
//   }

//   client.println("</select>");
//   client.println("</div>");

//   client.println("<div style='margin-bottom:32px;'>");
//   client.println("<label style='display:block; margin-bottom:7px;'>Czas auto-off przekaźników (s):</label>");
//   client.println("<input type='number' name='autoOff' value='" + String(autoOffTime / 1000) + "' min='30' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
//   client.println("</div>");

//   client.println("<button type='submit' style='padding:12px 28px; background:#2196f3; color:#fff; border:none; border-radius:6px; font-size:1.05em; cursor:pointer;'>Zapisz ustawienia</button>");
//   client.println("</form>");
//   client.println("<div style='margin-bottom:32px;'>");
//   client.println("<label><input type='checkbox' name='logFilesEnabled' value='1' " + String(enableLogFiles ? "checked" : "") + "> Zapisuj pliki logów i historii logowań</label>");
//   client.println("</div>");

//   // NOWY formularz tylko do resetu
//   client.println("<form method='POST' action='/resetSettings' style='margin-top:14px;'>");
//   client.println("<button type='submit' style='padding:10px 24px; background:#d32f2f; color:#fff; border:none; border-radius:6px; font-size:1.05em; cursor:pointer;'>Przywróć domyślne</button>");
//   client.println("</form>");

//   client.println("</form>");
//   client.println("</div>");

//   sendCommonHtmlFooter(client);
// }
void sendSettingsPage_GET(EthernetClient &client) {
  Serial.printf("[DEBUG] Wywołano sendSettingsPage_GET, ACTIVE_RELAYS = %d\n", ACTIVE_RELAYS);

  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Konfiguracja czasów");

  client.println("<div class='panel' style='background:#21242c; border-radius:12px; padding:32px;'>");
  client.println("<form method='POST' action='/saveSettings'>");

  // Czas oczekiwania na powrót sygnału
  client.println("<div style='margin-bottom:20px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Czas oczekiwania na powrót sygnału (s):</label>");
  client.println("<input type='number' name='waitTime' value='" + String(WAITING_1MIN_TIME / 1000) + "' min='10' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  // Czas trwania resetu
  client.println("<div style='margin-bottom:20px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Czas trwania resetu (s):</label>");
  client.println("<input type='number' name='resetDur' value='" + String(RESET_DURATION / 1000) + "' min='10' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  // Czas potrzebny na uruchomienie urządzenia
  client.println("<div style='margin-bottom:20px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Czas potrzebny na uruchomienie urządzenia (s):</label>");
  client.println("<input type='number' name='devStart' value='" + String(DEVICE_STARTTIME / 1000) + "' min='10' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  // Liczba aktywnych przekaźników
  client.println("<div style='margin-bottom:32px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Liczba aktywnych przekaźników:</label>");
  client.println("<select name='relayCount' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444; font-size:1em;'>");
  for (int i = 1; i <= 4; i++) {
    client.print("<option value='");
    client.print(i);
    client.print("'");
    if (ACTIVE_RELAYS == i) client.print(" selected");
    client.print(">");
    client.print(i);
    client.println("</option>");
  }
  client.println("</select>");
  client.println("</div>");

  // Czas auto-off przekaźników
  client.println("<div style='margin-bottom:28px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Czas auto-off przekaźników (s):</label>");
  client.println("<input type='number' name='autoOff' value='" + String(autoOffTime / 1000) + "' min='30' style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  // Checkbox: Zapisuj pliki logów
  client.println("<div style='margin-bottom:28px;'>");
  client.print("<label><input type='checkbox' name='logFilesEnabled' value='1' ");
  if (enableLogFiles) client.print("checked ");
  client.println("> Zapisuj pliki logów i historii logowań</label>");
  client.println("</div>");

  // Przycisk zapisz
  client.println("<button type='submit' style='padding:12px 28px; background:#2196f3; color:#fff; border:none; border-radius:6px; font-size:1.05em; cursor:pointer;'>Zapisz ustawienia</button>");
  client.println("</form>");

  // Osobny formularz do resetu ustawień
  client.println("<form method='POST' action='/resetSettings' style='margin-top:18px;'>");
  client.println("<button type='submit' style='padding:10px 24px; background:#d32f2f; color:#fff; border:none; border-radius:6px; font-size:1.05em; cursor:pointer;'>Przywróć domyślne</button>");
  client.println("</form>");

  client.println("</div>");

  sendCommonHtmlFooter(client);
}

//resetowanie do ustawień domyślonych czasów
void handleResetSettings_POST(EthernetClient &client) {
  // Ustaw domyślne wartości
  RESET_DURATION = 60000;     // 20 sek
  DEVICE_STARTTIME = 900000;   // 900 sek
  autoOffTime = 30000;        // 30 sek
  WAITING_1MIN_TIME = 120000;  // 120 sek
  ACTIVE_RELAYS = 4;          // Domyślnie 4
  enableLogFiles = true;      // Domyślnie włączone

  // Zapisz domyślne do Preferences
  preferences.begin("timings", false);
  preferences.putULong("resetDur", RESET_DURATION);
  preferences.putULong("devStart", DEVICE_STARTTIME);
  preferences.putULong("waitTime", WAITING_1MIN_TIME);
  preferences.putUInt("relayCount", ACTIVE_RELAYS);
  preferences.end();

  preferences.begin("autooff", false);
  preferences.putUInt("autoOffSec", autoOffTime / 1000);
  preferences.end();

  preferences.begin("settings", false);
  preferences.putBool("logFilesEnabled", enableLogFiles);
  preferences.end();

  // Log
  addLog("[INFO] Przywrócono domyślne ustawienia czasów!");

  // Przekierowanie do ustawień
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /settings");
  client.println("Connection: close");
  client.println();
}


void sendUpdateFirmwarePage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Aktualizacja oprogramowania");

  client.println("<div class='panel' style='background:#23262a; border-radius:12px; padding:38px; max-width:470px; margin:auto;'>");
  client.println("<h3 style='margin-top:0; color:#90caf9;'>Aktualizacja oprogramowania</h3>");
  client.println("<p style='color:#bdbdbd; margin-bottom:30px;'>Aktualna wersja: <b>" + String(firmwareVersion) + "</b></p>");

  // Pole hasła
  client.println("<div style='margin-bottom:20px;'>");
  client.println("<label style='display:block; margin-bottom:8px;'>Aktualne hasło:</label>");
  client.println("<input type='password' id='updatePassword' placeholder='Hasło administratora' style='width:100%; padding:12px; border-radius:6px; background:#21242c; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  // Pole wyboru pliku
  client.println("<div style='margin-bottom:30px;'>");
  client.println("<label style='display:block; margin-bottom:8px;'>Plik firmware (.bin):</label>");
  client.println("<input type='file' id='firmwareFile' accept='.bin' style='width:100%; padding:8px; border-radius:6px; background:#21242c; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  // Przycisk
  client.println("<button onclick='uploadFirmware()' style='padding:13px 32px; background:#2196f3; color:#fff; border:none; border-radius:6px; font-size:1.08em; font-weight:bold; cursor:pointer;'>Rozpocznij aktualizację</button>");

  // Wynik operacji
  client.println("<div id='result' style='margin-top:28px; padding:12px; border-radius:4px; min-height:28px;'></div>");

  // Skrypt JS do obsługi aktualizacji
  client.println("<script>");
  client.println("function uploadFirmware() {");
  client.println("  const fileInput = document.getElementById('firmwareFile');");
  client.println("  const passwordInput = document.getElementById('updatePassword');");
  client.println("  const resultDiv = document.getElementById('result');");
  client.println("  resultDiv.innerHTML = '';");
  client.println("  if (fileInput.files.length === 0) {");
  client.println("    resultDiv.innerHTML = '<p style=\"color:#ff5252;\">Wybierz plik firmware!</p>'; return;");
  client.println("  }");
  client.println("  if (passwordInput.value.trim() === '') {");
  client.println("    resultDiv.innerHTML = '<p style=\"color:#ff5252;\">Wprowadź hasło!</p>'; return;");
  client.println("  }");
  client.println("  const file = fileInput.files[0];");
  client.println("  const password = passwordInput.value;");
  client.println("  const xhr = new XMLHttpRequest();");
  client.println("  xhr.open('POST', '/updateFirmware', true);");
  client.println("  xhr.setRequestHeader('Authorization', 'Basic ' + btoa('admin:' + password));");
  client.println("  xhr.setRequestHeader('Content-Type', 'application/octet-stream');");
  client.println("  xhr.upload.onprogress = function(event) {");
  client.println("    if (event.lengthComputable) {");
  client.println("      const percentComplete = Math.round((event.loaded / event.total) * 100);");
  client.println("      resultDiv.innerHTML = '<p style=\"color:#90caf9;\">Przesyłanie: ' + percentComplete + '%</p>'; } };");
  client.println("  xhr.onload = function() {");
  client.println("    if (xhr.status === 200) {");
  client.println("      resultDiv.innerHTML = '<p style=\"color:#76ff03;\">Aktualizacja zakończona sukcesem!</p>'; }");
  client.println("    else { resultDiv.innerHTML = '<p style=\"color:#ff5252;\">Błąd: ' + xhr.statusText + '</p>'; } };");
  client.println("  xhr.onerror = function() {");
  client.println("    resultDiv.innerHTML = '<p style=\"color:#ff5252;\">Błąd połączenia</p>'; };");
  client.println("  xhr.send(file);");
  client.println("}");
  client.println("</script>");

  client.println("</div>");

  sendCommonHtmlFooter(client);
}

// Strona edycji pojedynczego wejścia
void sendChangeInputPage_GET(EthernetClient &client, int inputIndex) {
  sendCommonHtmlHeader(client, "" + String(inputIndex + 1));

  client.println("<div class='container'>");
  client.println("<h2>Edycja wejścia " + String(inputIndex + 1) + "</h2>");
  client.println("<form method='POST' action='/changeInput?input=" + String(inputIndex) + "'>");

  client.println("<label>Etykieta:</label>");
  client.println("<input type='text' name='newInput' value='" + inputLabels[inputIndex] + "' required>");

  client.println("<button type='submit'>Zapisz</button>");
  client.println("</form>");
  client.println("</div>");

  sendCommonHtmlFooter(client);
}
void sendChangeNetPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Zmiana ustawień sieci");

  client.println("<form method='POST' action='/changeNet'>");

  client.println("<div style='margin-bottom:18px;'>");
  client.println("<label style='display:block; margin-bottom:6px;'>Adres IP:</label>");
  client.println("<input type='text' name='ip' value='" + ipStr + "' required style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:18px;'>");
  client.println("<label style='display:block; margin-bottom:6px;'>Brama domyślna:</label>");
  client.println("<input type='text' name='gateway' value='" + gatewayStr + "' required style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:18px;'>");
  client.println("<label style='display:block; margin-bottom:6px;'>Maska podsieci:</label>");
  client.println("<input type='text' name='subnet' value='" + subnetStr + "' required style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:24px;'>");
  client.println("<label style='display:block; margin-bottom:6px;'>Serwer DNS:</label>");
  client.println("<input type='text' name='dns' value='" + dnsStr + "' required style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  client.println("<button type='submit' style='padding:12px 28px; background:#2196f3; color:#fff; border:none; border-radius:6px; font-size:1.05em; cursor:pointer; margin-bottom:12px;'>Zapisz ustawienia</button>");
  client.println("</form>");

  client.println("<p style='color:#bdbdbd; font-size:0.96em; margin-top:20px;'>Uwaga: Po zmianie ustawień sieci urządzenie zostanie ponownie zainicjalizowane.</p>");
  client.println("</div>");  // zamknięcie main container

  sendCommonHtmlFooter(client);
}



void sendChangePlacePage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Zmiana nazwy miejsca");

  client.println("<form method='POST' action='/changePlace'>");

  client.println("<div style='margin-bottom:24px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Nazwa miejsca (wyświetlana na TFT i stronie):</label>");
  client.println("<input type='text' name='newPlace' value='" + placeStr + "' required "
                                                                           "style='width:100%; padding:10px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  client.println("<button type='submit' style='padding:12px 28px; background:#2196f3; color:#fff; border:none; border-radius:6px; font-size:1.05em; cursor:pointer;'>Zapisz zmiany</button>");
  client.println("</form>");
  client.println("</div>");  // zamknięcie main container

  sendCommonHtmlFooter(client);
}



void sendChangeNetPage(EthernetClient &client) {
  sendChangeNetPage_GET(client);
}

void sendAboutPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");

  client.println("<div class='container' style='max-width:1200px; margin:auto; background:#20232a; padding:36px 26px 28px 26px; border-radius:18px; box-shadow:0 2px 18px #111c; color:#e0e0e0;'>");
  client.println("<h2 style='color:#4fc3f7;'>PowerReset – Informacje o systemie</h2>");
  client.println("<div style='margin:18px 0 26px 0;'>");
  client.println("<button class='tabBtn' onclick=\"showTab('funkcje')\" id='defaultTab'>Funkcjonalności</button>");
  client.println("<button class='tabBtn' onclick=\"showTab('bezpieczenstwo')\">Bezpieczeństwo</button>");
  client.println("<button class='tabBtn' onclick=\"showTab('aktualizacja')\">Aktualizacja firmware</button>");
  client.println("<button class='tabBtn' onclick=\"showTab('techniczne')\">Dane techniczne</button>");
  client.println("<button class='tabBtn' onclick=\"showTab('zastosowania')\">Zastosowania</button>");
  client.println("<button class='tabBtn' onclick=\"showTab('historia')\">Historia wersji</button>");
  client.println("<button class='tabBtn' onclick=\"showTab('autorzy')\">Autor</button>");
  client.println("<button class='tabBtn' onclick=\"showTab('kontakt')\">Kontakt</button>");
  client.println("<button class='tabBtn' onclick=\"showTab('licencja')\">Licencja</button>");
  client.println("</div>");

  // Funkcjonalności
  client.println("<div id='funkcje' class='tabSection'>");
  client.println("<h3>Główne funkcjonalności systemu PowerReset</h3>");
  client.println("<ul>");
  client.println("<li><b>Automatyczne monitorowanie i reset urządzeń</b> – każdy kanał działa niezależnie na maszynie stanów (IDLE, WAITING_1MIN, DO_RESET, WAIT_START, MAX_ATTEMPTS).</li>");
  client.println("<li><b>Dynamiczna liczba przekaźników</b> – liczba aktywnych kanałów przekaźnikowych ustawiana z poziomu interfejsu WWW (1-4).</li>");
  client.println("<li><b>Wyłączanie/zapisywanie logów txt</b> – opcja zapisu lub blokady historii logowań i plików logów przez checkbox w ustawieniach (oszczędność pamięci SPIFFS).</li>");
  client.println("<li><b>Limit rozmiaru logów</b> – automatyczne przycinanie log.txt, login_history.txt i errors.log do 50kB (starsze wpisy kasowane automatycznie).</li>");
  client.println("<li><b>Panel ustawień czasów i auto-off</b> – czasy resetu, startu urządzenia, auto-off, indywidualna konfiguracja przez panel WWW.</li>");
  client.println("<li><b>Przywracanie ustawień domyślnych</b> – jedno kliknięcie resetuje wszystkie czasy, liczbę przekaźników i inne ustawienia do fabrycznych wartości.</li>");
  client.println("<li><b>Zaawansowane logowanie zdarzeń</b> – panel logów, historia, backupy, potwierdzanie i czyszczenie, AJAX, filtrowanie, auto-refresh.</li>");
  client.println("<li><b>Monitorowanie wejść (E1)</b> – indywidualna konfiguracja (nazwa, tryb, monitorowanie, pull-up), liczniki, szybki podgląd i kolorowa wizualizacja na TFT.</li>");
  client.println("<li><b>Zaawansowane statystyki</b> – cykle przekaźników, liczba aktywacji, historia logowań, liczniki wejść i wyjść, panel zerowania.</li>");
  client.println("<li><b>Bezpieczeństwo</b> – wymuszanie zmiany hasła, weryfikacja siły, logowanie prób, historia logowań (z datą, IP i statusem OK/FAIL).</li>");
  client.println("<li><b>Automatyczna synchronizacja czasu</b> – obsługa NTP i lokalnych serwerów czasu, zapis do RTC, synchronizacja z przyciskiem 'Synchronizuj'.</li>");
  client.println("<li><b>Aktualizacja firmware OTA</b> – przez WWW i narzędzie curl, pełne logowanie procesu, zachowanie konfiguracji po update.</li>");
  client.println("<li><b>Dynamiczny wyświetlacz TFT</b> – wizualizacja stanów, aktywności, auto ON/OFF, kolorowe wskaźniki monitorowanych wejść.</li>");
  client.println("</ul>");
  client.println("</div>");

  // Bezpieczeństwo
  client.println("<div id='bezpieczenstwo' class='tabSection' style='display:none;'>");
  client.println("<h3>Bezpieczeństwo</h3>");
  client.println("<ul>");
  client.println("<li><b>Autoryzacja BasicAuth</b> – wszystkie funkcje konfiguracyjne dostępne tylko dla użytkownika zalogowanego (login/hasło). Zmiana danych logowania dostępna przez panel WWW.</li>");
  client.println("<li><b>Weryfikacja siły hasła</b> – system nie pozwala ustawić zbyt prostego lub powtarzalnego hasła (wymusza minimum długość i zróżnicowanie znaków).</li>");
  client.println("<li><b>Przechowywanie konfiguracji</b> – wszystkie ustawienia (sieć, czasy, etykiety) są zapisywane trwale w Preferences (w formie tekstowej, możliwa rozbudowa o szyfrowanie).</li>");
  client.println("<li><b>Bezpieczeństwo połączenia</b> – domyślnie HTTP, zalecane uruchamianie w sieci wydzielonej. Możliwość wdrożenia HTTPS (samodzielnie generowane certyfikaty).</li>");
  client.println("<li><b>Logowanie wszystkich zmian</b> – każda istotna zmiana konfiguracji, logowanie, aktualizacja firmware czy restart są zapisywane w logach i wyświetlane użytkownikowi.</li>");
  client.println("<li><b>Historia prób logowania</b> – każda próba logowania (OK/FAIL, data, IP) zapisywana do pliku login_history.txt (jeśli logi są aktywne).</li>");
  client.println("</ul>");
  client.println("</div>");

  // Aktualizacja firmware
  client.println("<div id='aktualizacja' class='tabSection' style='display:none;'>");
  client.println("<h3>Obsługa aktualizacji firmware (OTA)</h3>");
  client.println("<p>System PowerReset umożliwia łatwe i szybkie aktualizacje oprogramowania układowego bez potrzeby fizycznego dostępu do urządzenia. Wszystkie dane konfiguracyjne i logi są zachowywane po aktualizacji.</p>");
  client.println("<ul>");
  client.println("<li><b>Przez przeglądarkę</b> – wybierz plik .bin i zatwierdź aktualizację na stronie 'Aktualizacja firmware'.</li>");
  client.println("<li><b>Przez narzędzie curl</b> – dla zaawansowanych użytkowników oraz do zautomatyzowanych wdrożeń:<br>");
  client.println("<code style='font-size:0.97em;'>curl -X POST -H \"Authorization: Basic ...\" -H \"Content-Type: application/octet-stream\" --data-binary @firmware.bin http://adres/updateFirmware</code></li>");
  client.println("</ul>");
  client.println("<p>Po udanej aktualizacji następuje automatyczny restart urządzenia. Każda operacja jest logowana w systemie.</p>");
  client.println("</div>");

  // Dane techniczne
  client.println("<div id='techniczne' class='tabSection' style='display:none;'>");
  client.println("<h3>Wybrane dane techniczne</h3>");
  client.println("<ul>");
  client.println("<li><b>Płyta główna:</b> ESP32 (dwurdzeniowy mikrokontroler, 240 MHz, WiFi+BT+Ethernet)</li>");
  client.println("<li><b>Moduł Ethernet:</b> W5500 (stabilne, przewodowe połączenie sieciowe)</li>");
  client.println("<li><b>Wyświetlacz:</b> TFT_eSPI (kolorowy, obsługa Sprite, płynna grafika, 128x160 lub 240x320 px)</li>");
  client.println("<li><b>RTC:</b> DS3231 (dokładny zegar czasu rzeczywistego z podtrzymaniem bateryjnym)</li>");
  client.println("<li><b>Pamięć plików:</b> SPIFFS (na logi i kopie zapasowe, automatyczne limitowanie rozmiaru 50kB)</li>");
  client.println("<li><b>Liczba przekaźników:</b> 1-4 (konfigurowalna przez interfejs WWW)</li>");
  client.println("<li><b>Wejścia:</b> 4x transoptory (optoizolacja, monitorowanie sygnałów zewnętrznych, różne tryby pracy, liczniki cykli i czasu aktywności)</li>");
  client.println("<li><b>Wyjścia:</b> 4x przekaźniki (sterowanie z poziomu programu i interfejsu WWW, liczniki cykli, czasu pracy, historii)</li>");
  client.println("<li><b>Aktualizacja OTA:</b> przez Ethernet (bezprzewodowo przez WWW lub curl)</li>");
  client.println("<li><b>Panel WWW:</b> sidebar, tryb ciemny, AJAX, filtry, responsywność, dynamiczne zakładki</li>");
  client.println("<li><b>Logi/pliki historii:</b> limit 50kB na plik, automatyczne czyszczenie, opcja włączenia/wyłączenia zapisu do SPIFFS</li>");
  client.println("</ul>");
  client.println("</div>");

  // Zastosowania
  client.println("<div id='zastosowania' class='tabSection' style='display:none;'>");
  client.println("<h3>Zastosowania PowerReset</h3>");
  client.println("<ul>");
  client.println("<li><b>Zdalny nadzór nad serwerami, urządzeniami sieciowymi, modemami i routerami</b> – automatyczny reset przy awarii sieci lub braku odpowiedzi.</li>");
  client.println("<li><b>Przemysłowy monitoring systemów automatyki</b> – szybkie wykrywanie i reagowanie na awarie wejść/wyjść.</li>");
  client.println("<li><b>Wspomaganie pracy administratorów IT</b> – podgląd, logowanie, restart i analiza awarii bez fizycznego dostępu do urządzenia.</li>");
  client.println("<li><b>Serwisy i działy utrzymania ruchu</b> – możliwość rekonfiguracji systemu, zdalnych testów, analizy historii i zachowania urządzeń.</li>");
  client.println("</ul>");
  client.println("</div>");

  // Historia wersji
  client.println("<div id='historia' class='tabSection' style='display:none;'>");
  client.println("<h3>Historia wersji</h3>");
  client.println("<ul>");
  client.println("<li><b>v1.51 (2025-05-26):</b> Dynamiczna liczba przekaźników (ustawiana w panelu), flaga zapisu logów, checkboxy i informacja o statusie logów, limit rozmiaru plików txt do 50kB, automatyczne przycinanie, poprawki UI, przywracanie domyślnych ustawień, usprawnienia w panelu TFT (wizualizacja monitorowanych wejść).</li>");
  client.println("<li><b>v1.46 (2025-05-25):</b> Nowy panel statystyk (liczniki wejść/wyjść, historia logowań, zerowanie liczników), poprawki AJAX i zgodności stylów, ujednolicony sidebar na wszystkich stronach, rozbudowany system logowania, automatyczne backupy logów.</li>");
  client.println("<li><b>v1.45 (2025-05-24):</b> Dodano możliwość zerowania statystyk, lepsze liczniki, pełna obsługa historii wejść i przekaźników, poprawki bezpieczeństwa i stylu.</li>");
  client.println("<li><b>v1.43 (2025-05-10):</b> Rozbudowa panelu o zakładki, poprawki w systemie logowania, ujednolicenie wyglądu stron, naprawa drobnych błędów.</li>");
  client.println("<li><b>v1.42 (2025-04-04):</b> Pełna obsługa diagnostyki online, automatyczne kopie zapasowe logów, panel ustawień czasów.</li>");
  client.println("<li><b>v1.41 (2025-03-15):</b> Dodano możliwość zmiany etykiet wejść, tryb auto-off przekaźników, lepsza obsługa watchdog.</li>");
  client.println("<li><b>v1.40 (2025-02-28):</b> Zaimplementowano OTA przez curl, panel aktualizacji firmware, synchronizacja z serwerem czasu TCP.</li>");
  client.println("<li>...</li>");
  client.println("</ul>");
  client.println("</div>");

  // Autor
  client.println("<div id='autorzy' class='tabSection' style='display:none;'>");
  client.println("<h3>Autor</h3>");
  client.println("<ul>");
  client.println("<li><b>Piotr Kapa</b> – starszy teletechnik, Wydział Łączności TAURON Dystrybucja S.A. Oddział Bielsko-Biała</li>");
  client.println("</ul>");
  client.println("</div>");

  // Kontakt
  client.println("<div id='kontakt' class='tabSection' style='display:none;'>");
  client.println("<h3>Kontakt</h3>");
  client.println("<ul>");
  client.println("<li><b>Email służbowy:</b> piotr.kapa@tauron-bielsko.pl</li>");
  client.println("<li><b>Email prywatny:</b> amletka1@gmail.com</li>");
  client.println("</ul>");
  client.println("</div>");

  // Licencja
  client.println("<div id='licencja' class='tabSection' style='display:none;'>");
  client.println("<h3>Licencja i prawa autorskie</h3>");
  client.println("<ul>");
  client.println("<li>Licencja udzielona wyłącznie dla TAURON Dystrybucja S.A.</li>");
  client.println("<li>Wszystkie prawa zastrzeżone.</li>");
  client.println("<li>Wzór użytkowy – kopiowanie, modyfikowanie i dystrybucja zabronione bez zgody autora i firmy.</li>");
  client.println("</ul>");
  client.println("</div>");

  // Styl przycisków i sekcji
  client.println("<style>");
  client.println(".tabBtn { background: #181c23; color: #4fc3f7; border: none; border-bottom: 2.5px solid transparent; font-size: 1em; padding: 10px 22px; cursor: pointer; border-radius: 11px 11px 0 0; margin-right: 4px; margin-bottom: 0; transition: background 0.25s, color 0.25s; }");
  client.println(".tabBtn.active, .tabBtn:focus { background: #23293b; color: #fff; border-bottom: 2.5px solid #4fc3f7; outline: none; }");
  client.println(".tabSection { margin-top:18px; }");
  client.println("</style>");

  // Skrypt obsługi zakładek
  client.println("<script>");
  client.println("function showTab(tabId) {");
  client.println("  var tabs = document.getElementsByClassName('tabSection');");
  client.println("  for (var i = 0; i < tabs.length; i++) tabs[i].style.display = 'none';");
  client.println("  document.getElementById(tabId).style.display = 'block';");
  client.println("  var btns = document.getElementsByClassName('tabBtn');");
  client.println("  for (var i = 0; i < btns.length; i++) btns[i].classList.remove('active');");
  client.println("  event.target.classList.add('active');");
  client.println("}");
  client.println("window.onload = function() { document.getElementById('defaultTab').click(); };");
  client.println("</script>");

  client.println("</div>");
  sendCommonHtmlFooter(client);
}


void sendMainContainerBegin(EthernetClient &client, const String &title) {
  client.println("<div class='container' style='max-width:950px; margin:36px auto 24px auto; background:#20232a; padding:36px 28px 28px 28px; border-radius:18px; box-shadow:0 2px 18px #111c; color:#e0e0e0;'>");
  // Pasek info w prawym górnym rogu
  client.println("<div style='position:relative; height:44px;'>");
  client.println("<div style='position:absolute; top:0; right:0; text-align:right; color:#bdbdbd; font-size:1.03em;'>");
  client.println("<span style='color:#4fc3f7; font-weight:bold;'>PowerReset v2 • " + String(firmwareVersion) + "</span><br>");
  client.println("<span style='font-size:0.98em;'>© 2025 TAURON Dystrybucja S.A.</span><br>");
  client.println("<span style='font-size:0.93em;'>Strona odświeża się co 5 sek.</span>");
  client.println("</div>");
  client.println("</div>");
  // Nagłówek sekcji
  client.println("<h2 style='color:#4fc3f7; margin-bottom:18px;'>" + title + "</h2>");
}
