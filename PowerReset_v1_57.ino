#include "diagnostics_ajax.h"
#include <time.h>
#include "globals.h"
#include "functions.h"
#include "sendMainPageHTML.h"
#include "htmlpages.h"
#include "about.h"
//#include <FS.h>
//#include <vector>
//#include <algorithm>
//#include <regex>
//#include "esp_system.h"

#include <SPIFFS.h>
#include <HTTPUpdate.h>

//WATCHDOG
#include <esp_task_wdt.h>  //dołączam bibliotekę watchdog
unsigned long ostatnieKarmienie = 0;
const long interwalKarmienia = 6000;  // 6 sekund w milisekundach


#include <TFT_eSPI.h>
#include <Preferences.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>  //nowe
#include "NTPConfig.h"
#include <NTPClient.h>  //nowe
#include <base64.h>
#include <RTClib.h>
extern NTPConfig ntpConfig;
EthernetUDP udp;
// Liczba aktywnych przekaźników (1-4)
#define LOGIN_HISTORY_MAX_SIZE 50000  // 50 kB
#define SERIAL_BUFFER_SIZE 2000
// Limit rozmiaru pliku log.txt w bajtach (np. 128 KB)
#define MAX_LOG_FILE_SIZE (128 * 1024)
#define MAX_LOG_BACKUPS 10

void printLogsByType(EthernetClient &client, File &file, const String &type, const String &color);
void handleClearLogs_POST(EthernetClient &client);
void logLoginAttemptToFile(const String &ip, bool success);


// Definicje pamięci dla różnych platform
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
#define MEMORY_SIZE 2048  // Arduino Uno/Nano
#elif defined(__AVR_ATmega2560__)
#define MEMORY_SIZE 8192  // Arduino Mega
#elif defined(ESP8266)
#define MEMORY_SIZE 81920  // ESP8266
#elif defined(ESP32)
#define MEMORY_SIZE 327680  // ESP32
#else
#define MEMORY_SIZE 2048  // Domyślna wartość
#endif


// Zmienne systemowe
unsigned long startTime = millis();
const String SYSTEM_VERSION = "2.1";
bool isWatchdogInitialized = false;


//----------------------------------------------------------
// KONFIGURACJA W5500
//----------------------------------------------------------
#define W5500_CS 5
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

int timezoneOffset = 3600;

EthernetUDP udpClient;  // Do komunikacji UDP (jeśli używasz NTP)
bool timeSynced = false;

IPAddress ip, gateway, subnet, dns;
EthernetServer server(80);

bool unreadLogs = false;                                   // Flaga nowych nieprzeczytanych logów
bool errorLogsPresent = false;                             // Flaga obecności błędów w logach
bool autoResetActive[4] = { false, false, false, false };  // jeśli per wejście
bool reinitEthernet = false;
bool isPostChangeInput = false;
bool ntpSyncSuccess = false;
RTC_DS3231 rtc;
bool rtcFound = false;

// Wybieramy, które wejścia będą decydować o auto-resecie.
const int E1Pin = 16;
const int E2Pin = 17;
const int E3Pin = 26;
const int E4Pin = 25;



// WYŚWIETLACZ TFT + SPRITE
//----------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);  // Sprite do rysowania bez migotania

//----------------------------------------------------------
// PREFERENCES / LOGINY / PLACE
//----------------------------------------------------------
Preferences preferences;
String httpUser;
String httpPass;
String httpAuthBase64;

struct LogLine {
  String line;
  bool isError;
  bool isWarn;
  bool isSuccess;
  bool isInfo;
};

// Flaga określająca, czy wyświetlić pełną historię, czy tylko ostatnie 20 logów.
// Jeśli w URL pojawi się parametr ?history=1 lub ustawimy tę flagę w inny sposób, wyświetlimy pełną historię.
bool showAllLogs = false;  // Domyślnie tylko ostatnie 20 zdarzeń

//----------------------------------------------------------
// Funkcja: removePolish
// Usuwa polskie znaki z podanego ciągu znaków
//----------------------------------------------------------
String removePolish(const String &in) {
  String out = in;
  out.replace("ą", "a");
  out.replace("Ą", "A");
  out.replace("ć", "c");
  out.replace("Ć", "C");
  out.replace("ę", "e");
  out.replace("Ę", "E");
  out.replace("ł", "l");
  out.replace("Ł", "L");
  out.replace("ń", "n");
  out.replace("Ń", "N");
  out.replace("ó", "o");
  out.replace("Ó", "O");
  out.replace("ś", "s");
  out.replace("Ś", "S");
  out.replace("ź", "z");
  out.replace("Ź", "Z");
  out.replace("ż", "z");
  out.replace("Ż", "Z");
  out.replace("+", " ");
  out.replace("-", " ");
  return out;
}

int extractBackupNum(const String &filename) {
  // Plik: /log_backup_001_xxx.txt
  int firstUnderscore = filename.indexOf('_');
  int secondUnderscore = filename.indexOf('_', firstUnderscore + 1);
  if (firstUnderscore >= 0 && secondUnderscore > firstUnderscore) {
    String numStr = filename.substring(firstUnderscore + 1, secondUnderscore);
    return numStr.toInt();
  }
  return 0;
}

bool setTimeFromString(String timeStr) {
  struct tm tm;
  int year, month, day, hour, min, sec;

  if (sscanf(timeStr.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &min, &sec) == 6) {
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;

    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, nullptr);

    Serial.println("Czas został ustawiony:");
    Serial.println(ctime(&t));
    return true;
  } else {
    Serial.println("Nie udało się sparsować stringa czasu.");
    return false;
  }
}

void syncTimeManual() {
  String result = getTimeFromServer();
  if (result.startsWith("ERROR")) {
    Serial.println("Błąd synchronizacji czasu: " + result);
  } else {
    if (setTimeFromString(result)) {
      Serial.println("Synchronizacja zakończona sukcesem");
      ntpSyncSuccess = true;  // Ustaw flagę synchronizacji
    } else {
      Serial.println("Błąd przy ustawianiu czasu");
      ntpSyncSuccess = false;  // Ustaw flagę synchronizacji
    }
  }
}




void setCurrentDateTime(const String &timeStr) {
  // Oczekiwany format: "YYYY-MM-DD HH:MM:SS"
  int spaceIndex = timeStr.indexOf(' ');
  if (spaceIndex <= 0) {
    Serial.println("Błąd: niepoprawny format daty/czasu. Oczekiwany format: YYYY-MM-DD HH:MM:SS");
    return;
  }

  currentDate = timeStr.substring(0, spaceIndex);
  currentTime = timeStr.substring(spaceIndex + 1);

  Serial.print("Ustawiono datę: ");
  Serial.println(currentDate);
  Serial.print("Ustawiono czas: ");
  Serial.println(currentTime);

  struct tm tm;
  int parsed = sscanf(timeStr.c_str(), "%d-%d-%d %d:%d:%d",
                      &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                      &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

  if (parsed == 6) {
    tm.tm_year -= 1900;  // Przekształcenie roku
    tm.tm_mon -= 1;      // Miesiące od 0 do 11

    time_t t = mktime(&tm);
    if (t == -1) {
      Serial.println("Błąd: mktime zwróciło -1");
      return;
    }

    struct timeval now;
    now.tv_sec = t;
    now.tv_usec = 0;

    if (settimeofday(&now, nullptr) == 0) {
      Serial.println("Zegar systemowy ESP32 ustawiony.");
    } else {
      Serial.println("Błąd: settimeofday() nie powiodło się.");
    }
  } else {
    Serial.println("Błąd: nie udało się sparsować czasu do struktury tm");
  }
}

void (*resetFunc)(void) = 0;




void sendLogPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Historia zdarzeń");

  unreadLogs = false;  // Oznacz jako przeczytane

  // Panel filtrów i wyszukiwarka
  client.println("<div style='display:flex;align-items:center;gap:10px;margin-bottom:10px;'>");
  client.println("<input type='text' id='logSearch' placeholder='Szukaj...' style='flex:1; padding:6px;'>");
  client.println("<label><input type='checkbox' id='infoCb' checked> INFO</label>");
  client.println("<label><input type='checkbox' id='warnCb' checked> WARN</label>");
  client.println("<label><input type='checkbox' id='errCb' checked> ERROR</label>");
  client.println("<label style='margin-left:20px;'><input type='checkbox' id='autoRefreshCb' checked> Auto-odświeżanie</label>");
  client.println("</div>");

  // --- Informacja o statusie zapisu logów ---
  // client.println("<div style='margin-bottom:28px; padding:12px; background:"
  //                + String(enableLogFiles ? "#213922" : "#662222")
  //                + "; color:"
  //                + String(enableLogFiles ? "#aaffaa" : "#ffaaaa")
  //                + "; border-radius:7px; border:1px solid "
  //                + String(enableLogFiles ? "#44bb44" : "#bb4444")
  //                + "; font-size:1.08em;'>");
  // client.println("<label style='display:flex; align-items:center; opacity:0.65;'>");
  // client.print("<input type='checkbox' disabled style='width:1.3em;height:1.3em;margin-right:10px;' ");
  // if (enableLogFiles) client.print(" checked");
  // client.print("> ");
  // client.print("Zapis plików logów i historii logowań</label>");
  // client.print("<div style='margin-top:4px;font-size:0.96em;'>Status: <b>"
  //              + String(enableLogFiles ? "Aktywny" : "Wyłączony")
  //              + "</b></div>");
  // if (!enableLogFiles) {
  //   client.println("<div style='margin-top:2px;color:#ffd1d1;font-size:0.97em;'>"
  //                  "Uwaga: logi oraz backupy nie są tworzone – funkcja wyłączona w ustawieniach!</div>");
  // }
  // client.println("</div>");
  client.println("<div style='margin-bottom:28px; padding:12px; background:"
                 + String(enableLogFiles ? "#213922" : "#662222")
                 + "; color:"
                 + String(enableLogFiles ? "#aaffaa" : "#ffaaaa")
                 + "; border-radius:7px; border:1px solid "
                 + String(enableLogFiles ? "#44bb44" : "#bb4444")
                 + "; font-size:1.08em;'>");
  client.println("<label style='display:flex; align-items:center; opacity:0.65;'>");
  client.print("<input type='checkbox' disabled style='width:1.3em;height:1.3em;margin-right:10px;' ");
  if (enableLogFiles) client.print(" checked");
  client.print("> ");
  client.print("Zapis plików logów i historii logowań</label>");
  client.print("<div style='margin-top:4px;font-size:0.96em;'>Status: <b>"
               + String(enableLogFiles ? "Aktywny" : "Wyłączony")
               + "</b></div>");
  if (!enableLogFiles) {
    client.println("<div style='margin-top:2px;color:#ffd1d1;font-size:0.97em;'>"
                   "Uwaga: logi oraz backupy nie są tworzone – funkcja wyłączona.<br>"
                   "Możesz ją <b>włączyć w <a href='/settings' style='color:#33aaff;text-decoration:underline;'>Ustawieniach</a></b>.</div>");
  }
  client.println("</div>");


  // Kontener na logi
  client.println("<div id='logsBox' style='max-height:400px; overflow-y:auto; border:1px solid #333; padding:10px; font-family:monospace; background:#23262a; color:#e0e0e0;'></div>");

  // Przyciski akcji
  client.println("<div style='margin-top:20px; display:flex; gap:10px;'>");
  client.println("<form method='POST' action='/confirmLogs'><button type='submit' style='background:#4CAF50; color:white;'>Potwierdź przeczytanie</button></form>");
  client.println("<form method='POST' action='/clearLogs' onsubmit='return confirm(\"Czy na pewno chcesz wyczyścić wszystkie logi?\");'><button type='submit' style='background:#FF5252; color:white;'>Wyczyść logi</button></form>");
  client.println("</div>");

  // Skrypt do filtrowania i auto-refreshu
  client.println("<script>");
  client.println("let autoRefresh=true;");
  client.println("document.getElementById('autoRefreshCb').onchange=function(){autoRefresh=this.checked;};");
  client.println("function highlight(line){");
  client.println("  if(line.includes('[ERROR]')||line.includes('[BŁĄD]'))return'<span style=\"color:#ff5252;font-weight:bold;\">'+line+'</span>';");
  client.println("  if(line.includes('[WARN]'))return'<span style=\"color:#ffc107;font-weight:bold;\">'+line+'</span>';");
  client.println("  if(line.includes('[INFO]'))return'<span style=\"color:#90caf9;\">'+line+'</span>';");
  client.println("  return line;");
  client.println("}");
  client.println("function loadLogs(){");
  client.println("  var xhr=new XMLHttpRequest();xhr.open('GET','/api/logs',true);");
  client.println("  xhr.onload=function(){");
  client.println("    if(xhr.status==200){");
  client.println("      let lines=xhr.responseText.split('\\n');");
  client.println("      let filter=document.getElementById('logSearch').value.toLowerCase();");
  client.println("      let infoOk=document.getElementById('infoCb').checked;");
  client.println("      let warnOk=document.getElementById('warnCb').checked;");
  client.println("      let errOk=document.getElementById('errCb').checked;");
  client.println("      let html='';");
  // client.println("      lines.forEach(function(line){");
  client.println("      lines.reverse().forEach(function(line){");
  client.println("        let l=line.toLowerCase();");
  client.println("        if(l){");
  client.println("          if((l.includes('[info]') && !infoOk) || (l.includes('[warn]') && !warnOk) || ((l.includes('[error]')||l.includes('[błąd]')) && !errOk))return;");
  client.println("          if(filter && l.indexOf(filter)==-1)return;");
  client.println("          html+=highlight(line)+'<br>';");
  client.println("        }");
  client.println("      });");
  client.println("      document.getElementById('logsBox').innerHTML=html;");
  client.println("    }");
  client.println("  };xhr.send();");
  client.println("}");
  client.println("setInterval(function(){if(autoRefresh)loadLogs();},4000);");
  client.println("document.getElementById('logSearch').oninput=loadLogs;");
  client.println("document.getElementById('infoCb').onchange=loadLogs;");
  client.println("document.getElementById('warnCb').onchange=loadLogs;");
  client.println("document.getElementById('errCb').onchange=loadLogs;");
  client.println("window.onload=loadLogs;");
  client.println("</script>");

  client.println("</div>");  // zamknięcie .container z sendMainContainerBegin
  sendCommonHtmlFooter(client);
}




// Funkcja pomocnicza (musi być zadeklarowana w tym samym pliku)
void printLogsByType(EthernetClient &client, File &file, const String &type, const String &color) {
  file.seek(0);
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.indexOf(type) != -1) {
      client.print("<div class='event-item' style='border-color:");
      client.print(color);
      client.print("; color:");
      client.print(color);
      client.print(";'>");
      client.print(line);
      client.println("</div>");
    }
  }
}



void sendInputsPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Stan wejść");

  // Informacja o auto-refresh
  client.println("<p style='color:#bdbdbd; margin-bottom:18px;'>Strona odświeża się automatycznie co 5 sekund.</p>");

  // Tabela wejść
  client.println("<table style='width:100%; border-spacing:0 7px;'>");
  client.println("<tr><th>Wejście</th><th>Etykieta</th><th>Stan</th><th>Akcje</th></tr>");
  for (int i = 0; i < 4; i++) {
    int state = digitalRead(inputPins[i]);
    client.println("<tr style='background:#222c; border-radius:6px;'>");

    // Kolumna: Wejście
    client.print("<td style='font-weight:bold;'>Wejście " + String(i + 1) + "</td>");

    // Kolumna: Etykieta
    client.print("<td>" + inputLabels[i] + "</td>");

    // Kolumna: Stan
    client.print("<td><span style='color:" + String(state == HIGH ? "#76ff03" : "#ff5252") + "; font-weight:bold;'>");
    client.print(state == HIGH ? "HIGH" : "LOW");
    client.println("</span></td>");

    // Kolumna: Akcje
    client.print("<td style='text-align:right;'>");
    client.print("<button onclick=\"location.href='/changeInput?input=" + String(i) + "'\" "
                                                                                      "style='padding:6px 18px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer; margin-right:8px;'>"
                                                                                      "Edytuj</button>");

    if (inputMonitoringEnabled[i]) {
      client.print("<button onclick=\"location.href='/toggleInput?input=" + String(i) + "'\" "
                                                                                        "style='padding:6px 18px; background:#ff5252; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Wyłącz monitorowanie</button>");
    } else {
      client.print("<button onclick=\"location.href='/toggleInput?input=" + String(i) + "'\" "
                                                                                        "style='padding:6px 18px; background:#8bc34a; color:#252525; border:none; border-radius:4px; cursor:pointer;'>Włącz monitorowanie</button>");
    }
    client.println("</td>");

    client.println("</tr>");
  }
  client.println("</table>");

  client.println("</div>");  // zamknięcie .container z sendMainContainerBegin
  sendCommonHtmlFooter(client);
}




void sendToggleRelayConfirmPage(EthernetClient &client, int relayIndex) {
  sendCommonHtmlHeader(client, "Potwierdzenie przełączenia przekaźnika");
  sendMainContainerBegin(client, "Potwierdzenie przełączenia przekaźnika");

  client.print("<p>Czy na pewno chcesz przełączyć przekaźnik nr: ");
  client.print(relayIndex);
  client.println("?</p>");

  client.println("<div style='margin-top:25px;'>");
  client.print("<button onclick=\"location.href='/toggle_confirm?relay=");
  client.print(relayIndex);
  client.println("'\" style='padding:10px 20px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer; margin-right:10px;'>Tak, przełącz</button>");

  client.println("<button onclick=\"location.href='/'\" style='padding:10px 20px; background:#616161; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Anuluj</button>");
  client.println("</div>");

  client.println("</div>");  // zamknięcie .container z sendMainContainerBegin
  sendCommonHtmlFooter(client);
}


void sendClearLogsConfirmPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "Potwierdzenie czyszczenia logów");

  client.println("<div class='container' style='max-width:400px; margin:30px auto 0 auto; text-align:center;'>");
  client.println("<h2 style='color:#d32f2f;'>Potwierdzenie czyszczenia logów</h2>");
  client.println("<p style='font-size:1.07em;'>Czy na pewno chcesz <b>wyczyścić wszystkie logi</b>?</p>");

  client.println("<div style='margin-top:30px; display:flex; justify-content:center; gap:16px;'>");

  // Przycisk TAK – wysyła POST do /clearLogs
  client.println("<form method='POST' action='/clearLogs' style='display:inline;'>");
  client.println("<button type='submit' style='padding:11px 26px; background:#d32f2f; color:#fff; border:none; border-radius:6px; cursor:pointer; font-size:1em; font-weight:bold;'>Tak, wyczyść</button>");
  client.println("</form>");

  // Przycisk ANULUJ – powrót do logów
  client.println("<button onclick=\"location.href='/logs'\" style='padding:11px 26px; background:#616161; color:#fff; border:none; border-radius:6px; cursor:pointer; font-size:1em; font-weight:bold;'>Anuluj</button>");

  client.println("</div>");
  client.println("</div>");

  sendCommonHtmlFooter(client);
}




void sendTimeSettingsPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "Ustawienia czasu");
  sendMainContainerBegin(client, "Ustawienia czasu");

  // Formularz z metodą POST -> '/saveTimeSettings'
  client.println("<form method='POST' action='/saveTimeSettings'>");

  // Pole: strefa czasowa
  client.println("<div style='margin-bottom:15px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Strefa czasowa (timeZone):</label>");
  client.print("<input type='text' name='timezone' value='");
  client.print(timeZone);  // wstawiamy obecną wartość
  client.println("' required style='width:100%; padding:8px;'>");
  client.println("</div>");

  // Pole: serwer NTP 1
  client.println("<div style='margin-bottom:15px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Serwer NTP 1:</label>");
  client.print("<input type='text' name='ntp1' value='");
  client.print(ntpServer1);
  client.println("' style='width:100%; padding:8px;'>");
  client.println("</div>");

  // Pole: serwer NTP 2
  client.println("<div style='margin-bottom:15px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Serwer NTP 2:</label>");
  client.print("<input type='text' name='ntp2' value='");
  client.print(ntpServer4);
  client.println("' style='width:100%; padding:8px;'>");
  client.println("</div>");

  // Pole: serwer NTP 3
  client.println("<div style='margin-bottom:20px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Serwer NTP 3:</label>");
  client.print("<input type='text' name='ntp3' value='");
  client.print(ntpServer3);
  client.println("' style='width:100%; padding:8px;'>");
  client.println("</div>");

  // Przycisk "Zapisz"
  client.println("<button type='submit' style='padding:10px 20px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Zapisz ustawienia</button>");
  client.println("</form>");  // koniec formularza

  // Link powrotny
  client.println("<p style='margin-top:18px;'><a href='/' style='color:#4fc3f7;'>Powrót</a></p>");

  client.println("</div>");  // zamknięcie .container z sendMainContainerBegin
  sendCommonHtmlFooter(client);
}

// void handleSaveTimeSettings_POST(EthernetClient &client, const String &body) {
//   // Odczyt parametrów z body HTTP
//   String newTimeZone = getParamValue(body, "timezone");
//   String newNtp1 = getParamValue(body, "ntp1");
//   String newNtp2 = getParamValue(body, "ntp2");
//   String newNtp3 = getParamValue(body, "ntp3");

//   // Zapisz wartości do zmiennych globalnych (o ile pola nie są puste)
//   if (newTimeZone.length() > 0) {
//     timeZone = newTimeZone;
//   }
//   if (newNtp1.length() > 0) {
//     ntpServer1 = newNtp1;
//   }
//   if (newNtp2.length() > 0) {
//     ntpServer4 = newNtp2;
//   }
//   if (newNtp3.length() > 0) {
//     ntpServer3 = newNtp3;
//   }


//   char logBuffer[128];
//   snprintf(
//     logBuffer,
//     sizeof(logBuffer),
//     "Zmieniono ustawienia: timeZone=%d, ntp1=%s, ntp2=%s, ntp3=%s",
//     timeZone,
//     ntpServer1.c_str(),  // jeśli to obiekty String
//     ntpServer4.c_str(),
//     ntpServer3.c_str());
//   addLog(logBuffer);

//   // Odpowiedź HTTP - potwierdzenie
//   sendHttpResponseHeader(client, 200, "text/html");
//   client.println("<html><body style='background:#222; color:#ccc; font-family:Arial;'>");
//   client.println("<h2>Ustawienia czasu zapisane.</h2>");
//   client.println("<p>Strefa: " + timeZone + "</p>");
//   client.println("<p>NTP1: " + ntpServer1 + "</p>");
//   client.println("<p>NTP2: " + ntpServer4 + "</p>");
//   client.println("<p>NTP3: " + ntpServer3 + "</p>");
//   client.println("<p><a href='/'>Powrót do strony głównej</a></p>");
//   client.println("</body></html>");
// }

// void handleSaveTimeSettings_POST(EthernetClient &client, const String &body) {
//   // Odczyt parametrów
//   String newTimeZone = getParamValue(body, "timezone");
//   String newNtp1 = getParamValue(body, "ntp1");
//   String newNtp2 = getParamValue(body, "ntp2");
//   String newNtp3 = getParamValue(body, "ntp3");

//   bool changed = false;

//   // Zapisz do Preferences i zmiennych globalnych
//   preferences.begin("ntp", false);

//   if (newTimeZone.length() > 0 && timeZone != newTimeZone) {
//     timeZone = newTimeZone;
//     preferences.putString("timezone", timeZone);
//     changed = true;
//   }

//   if (newNtp1.length() > 0 && isValidNtpServer(newNtp1) && ntpServer1 != newNtp1) {
//     ntpServer1 = newNtp1;
//     preferences.putString("ntp1", ntpServer1);
//     changed = true;
//   }

//   if (newNtp2.length() > 0 && isValidNtpServer(newNtp2) && ntpServer4 != newNtp2) {
//     ntpServer4 = newNtp2;
//     preferences.putString("ntp2", ntpServer4);
//     changed = true;
//   }

//   if (newNtp3.length() > 0 && isValidNtpServer(newNtp3) && ntpServer3 != newNtp3) {
//     ntpServer3 = newNtp3;
//     preferences.putString("ntp3", ntpServer3);
//     changed = true;
//   }

//   preferences.end();

//   // Logowanie tylko jeśli były zmiany
//   if (changed) {
//     addLog("Zmieniono ustawienia czasu: " + timeZone + ", " +
//            ntpServer1 + ", " + ntpServer4 + ", " + ntpServer3);

//     // Wymuś synchronizację jeśli zmieniono serwery
//     if (newNtp1.length() > 0 || newNtp2.length() > 0 || newNtp3.length() > 0) {
//       syncTimeOnce();
//     }
//   }

//   // Odpowiedź HTTP
//   sendHttpResponseHeader(client, 200, "text/html");
//   client.println("<html><body style='background:#222; color:#ccc; font-family:Arial;'>");
//   client.println("<h2>Ustawienia czasu " + String(changed ? "zaktualizowane" : "nie zmienione") + "</h2>");
//   client.println("<p>Strefa: " + timeZone + "</p>");
//   client.println("<p>NTP1: " + ntpServer1 + "</p>");
//   client.println("<p>NTP2: " + ntpServer4 + "</p>");
//   client.println("<p>NTP3: " + ntpServer3 + "</p>");
//   client.println("<p><a href='/'>Powrót do strony głównej</a></p>");
//   client.println("</body></html>");
// }
// void handleSaveTimeSettings_POST(EthernetClient &client, const String &body) {
//   // Rate limiting
//   static unsigned long lastUpdate = 0;
//   if (millis() - lastUpdate < 5000) {
//     sendHttpResponseHeader(client, 429, "text/html");
//     client.println("<h2>Zbyt wiele prób</h2><p>Poczekaj 5 sekund przed kolejną zmianą</p>");
//     return;
//   }
//   lastUpdate = millis();

//   // Odczyt i przycinanie parametrów
//   String newTimeZone = getParamValue(body, "timezone").substring(0, 50);
//   String newNtp1 = getParamValue(body, "ntp1").substring(0, 64);
//   String newNtp2 = getParamValue(body, "ntp2").substring(0, 64);
//   String newNtp3 = getParamValue(body, "ntp3").substring(0, 64);

//   bool changed = false;
//   String errorMsg;

//   // Otwarcie Preferences
//   if (!preferences.begin("ntp", false)) {
//     errorMsg = "Błąd dostępu do pamięci!";
//     addLog(errorMsg);
//     sendErrorResponse(client, errorMsg);
//     return;
//   }

//   // Zapisz strefę czasową
//   if (newTimeZone.length() > 0 && timeZone != newTimeZone) {
//     timeZone = newTimeZone;
//     if (!preferences.putString("timezone", timeZone)) {
//       errorMsg = "Błąd zapisu strefy czasowej!";
//     }
//     changed = true;
//   }

//   // Funkcja pomocnicza dla serwerów NTP
//   auto processNtpServer = [&](const String &newServer, String &currentServer, const char* prefKey) {
//     if (newServer.length() > 0 && currentServer != newServer) {
//       if (!isValidNtpServer(newServer)) {
//         errorMsg = "Nieprawidłowy adres NTP: " + newServer;
//         return false;
//       }
//       currentServer = newServer;
//       if (!preferences.putString(prefKey, currentServer)) {
//         errorMsg = String("Błąd zapisu ") + prefKey;
//         return false;
//       }
//       return true;
//     }
//     return false;
//   };

//   // Przetwarzanie serwerów NTP (1,4,3)
//   if (processNtpServer(newNtp1, ntpServer1, "ntp1")) changed = true;
//   if (processNtpServer(newNtp2, ntpServer4, "ntp4")) changed = true; // Uwaga: ntp2 w formularzu mapowane na ntpServer4
//   if (processNtpServer(newNtp3, ntpServer3, "ntp3")) changed = true;

//   preferences.end();

//   // Obsługa błędów
//   if (!errorMsg.isEmpty()) {
//     addLog("Błąd: " + errorMsg);
//     sendErrorResponse(client, errorMsg);
//     return;
//   }

//   // Logowanie i synchronizacja
//   if (changed) {
//     addLog("Zapisano ustawienia: " + timeZone + ", " + ntpServer1 + ", " + ntpServer4 + ", " + ntpServer3);

//     if (newNtp1.length() > 0 || newNtp2.length() > 0 || newNtp3.length() > 0) {
//       if (!syncTimeOnce()) {
//         addLog("Uwaga: Synchronizacja czasu nie powiodła się");
//       }
//     }
//   }

//   // Odpowiedź sukces
//   sendHttpResponseHeader(client, 200, "text/html");
//   client.println("<div style='max-width:600px; margin:0 auto; padding:20px; background:#2a2a2a; color:#eee;'>");
//   client.println("<h2 style='color:#4fc3f7'>Ustawienia czasu</h2>");
//   client.println("<div style='background:#333; padding:15px; border-radius:8px; margin-bottom:20px;'>");
//   client.println("<p><strong>Status:</strong> " + String(changed ? "Zaktualizowano" : "Brak zmian") + "</p>");
//   client.println("<p><strong>Strefa:</strong> " + escapeHtml(timeZone) + "</p>");
//   client.println("<p><strong>NTP1:</strong> " + escapeHtml(ntpServer1) + "</p>");
//   client.println("<p><strong>NTP4:</strong> " + escapeHtml(ntpServer4) + "</p>");  // Zmienione z NTP2 na NTP4
//   client.println("<p><strong>NTP3:</strong> " + escapeHtml(ntpServer3) + "</p>");
//   client.println("</div>");
//   client.println("<a href='/' style='display:inline-block; padding:10px 15px; background:#2196f3; color:#fff; text-decoration:none; border-radius:4px;'>Powrót</a>");
//   client.println("</div>");
// }



void handleSaveTimeSettings_POST(EthernetClient &client, const String &body) {
  // Rate limiting - max 1 żądanie na 5 sekund
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 5000) {
    sendHttpResponseHeader(client, 429, "text/html");
    client.println("<h2>Zbyt wiele prób</h2><p>Poczekaj chwilę przed kolejną zmianą</p>");
    return;
  }
  lastUpdate = millis();

  // Odczyt i przycinanie parametrów
  String newTimeZone = getParamValue(body, "timezone").substring(0, 50);
  String newNtp1 = getParamValue(body, "ntp1").substring(0, 64);
  String newNtp2 = getParamValue(body, "ntp2").substring(0, 64);
  String newNtp3 = getParamValue(body, "ntp3").substring(0, 64);

  bool changed = false;
  String errorMsg;

  // Otwarcie Preferences
  if (!preferences.begin("ntp", false)) {
    errorMsg = "Błąd dostępu do pamięci!";
    addLog(errorMsg);
    sendHttpErrorResponse(client, errorMsg);  // Zmieniona nazwa funkcji
    return;
  }

  // Zapisz strefę czasową
  if (newTimeZone.length() > 0 && timeZone != newTimeZone) {
    timeZone = newTimeZone;
    if (!preferences.putString("timezone", timeZone)) {
      errorMsg = "Błąd zapisu strefy czasowej!";
    }
    changed = true;
  }

  // Funkcja pomocnicza dla serwerów NTP
  auto processNtpServer = [&](const String &newServer, String &currentServer, const char *prefKey) {
    if (newServer.length() > 0 && currentServer != newServer) {
      if (!isValidNtpServer(newServer)) {
        errorMsg = "Nieprawidłowy adres NTP: " + newServer;
        return false;
      }
      currentServer = newServer;
      if (!preferences.putString(prefKey, currentServer)) {
        errorMsg = String("Błąd zapisu ") + prefKey;
        return false;
      }
      return true;
    }
    return false;
  };

  // Przetwarzanie serwerów NTP (1,4,3)
  if (processNtpServer(newNtp1, ntpServer1, "ntp1")) changed = true;
  if (processNtpServer(newNtp2, ntpServer4, "ntp4")) changed = true;
  if (processNtpServer(newNtp3, ntpServer3, "ntp3")) changed = true;

  preferences.end();

  // Obsługa błędów
  if (!errorMsg.isEmpty()) {
    addLog("Błąd: " + errorMsg);
    sendHttpErrorResponse(client, errorMsg);  // Zmieniona nazwa funkcji
    return;
  }

  // Logowanie i synchronizacja
  if (changed) {
    addLog("Zapisano ustawienia: " + timeZone + ", " + ntpServer1 + ", " + ntpServer4 + ", " + ntpServer3);

    if (newNtp1.length() > 0 || newNtp2.length() > 0 || newNtp3.length() > 0) {
      syncTimeOnce();
    }
  }

  // Odpowiedź sukces
  sendSuccessResponse(client, changed, timeZone, ntpServer1, ntpServer4, ntpServer3);
}

// Nowe funkcje pomocnicze do odpowiedzi HTTP:

void sendHttpErrorResponse(EthernetClient &client, const String &msg) {
  sendHttpResponseHeader(client, 500, "text/html");
  client.println("<div style='max-width:600px; margin:20px auto; padding:20px; background:#422; color:#ffaaaa;'>");
  client.println("<h2>Błąd!</h2>");
  client.println("<p>" + escapeHtml(msg) + "</p>");
  client.println("<a href='/settings' style='color:#4fc3f7'>Spróbuj ponownie</a>");
  client.println("</div>");
}

void sendSuccessResponse(EthernetClient &client, bool changed, const String &timeZone,
                         const String &ntp1, const String &ntp4, const String &ntp3) {
  sendHttpResponseHeader(client, 200, "text/html");
  client.println("<div style='max-width:600px; margin:0 auto; padding:20px; background:#2a2a2a; color:#eee;'>");
  client.println("<h2 style='color:#4fc3f7'>Ustawienia czasu</h2>");
  client.println("<div style='background:#333; padding:15px; border-radius:8px; margin-bottom:20px;'>");
  client.println("<p><strong>Status:</strong> " + String(changed ? "Zaktualizowano" : "Brak zmian") + "</p>");
  client.println("<p><strong>Strefa:</strong> " + escapeHtml(timeZone) + "</p>");
  client.println("<p><strong>NTP1:</strong> " + escapeHtml(ntp1) + "</p>");
  client.println("<p><strong>NTP4:</strong> " + escapeHtml(ntp4) + "</p>");
  client.println("<p><strong>NTP3:</strong> " + escapeHtml(ntp3) + "</p>");
  client.println("</div>");
  client.println("<a href='/' style='display:inline-block; padding:10px 15px; background:#2196f3; color:#fff; text-decoration:none; border-radius:4px;'>Powrót</a>");
  client.println("</div>");
}

// Pozostałe funkcje pomocnicze:

bool isValidNtpServer(const String &server) {
  // Sprawdza czy to IP (np. 192.168.1.1) lub domena (pool.ntp.org)
  return server.length() >= 3 && (server.indexOf('.') != -1 || server == "pool.ntp.org");
}

String escapeHtml(const String &input) {
  String output = input;
  output.replace("&", "&amp;");
  output.replace("<", "&lt;");
  output.replace(">", "&gt;");
  return output;
}



void syncTime() {
  EthernetClient client;
  if (client.connect(IPAddress(192, 168, 0, 100), 123)) {
    String timeStr = client.readString();
    client.stop();

    Serial.print("Otrzymany czas: ");
    Serial.println(timeStr);

    // Tutaj parsuj czas i ustaw w systemie
  } else {
    Serial.println("Nie udało się połączyć z serwerem czasu!");
  }
}

void handleSyncTime_POST(EthernetClient &client) {
  String timeStr = getTimeFromServer();  // tu pobierasz czas przez NTP

  if (timeStr != "ERROR_CONN" && timeStr != "ERROR_TIMEOUT") {
    setCurrentDateTime(timeStr);  // funkcja do ustawiania czasu
    ntpSyncSuccess = true;
    addLog("[SUCCESS] Ręczna synchronizacja czasu - sukces: " + timeStr);
  } else {
    ntpSyncSuccess = false;
    addLog("[INFO] Ręczna synchronizacja czasu - nieudana (serwer nie odpowiada).");
  }

  // Przekierowanie z powrotem na stronę główną
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();
}
// Funkcja obsługująca żądanie synchronizacji czasu (endpoint /syncTime)
void handleSyncTime(EthernetClient &client) {
  syncTime();
  client.println("HTTP/1.1 302 Found");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();
}

//----------------------------------------------------------
// Funkcja: makeAuthBase64
// Tworzy zakodowany ciąg Base64 z loginu i hasła
//----------------------------------------------------------
String makeAuthBase64(const String &user, String pass) {
  pass.replace("%23", "#");  // Przywraca poprawny znak #
  return base64::encode(user + ":" + pass);
}

//----------------------------------------------------------
// Funkcja: parseIpString
// Konwertuje ciąg reprezentujący IP do obiektu IPAddress
//----------------------------------------------------------
bool parseIpString(const String &str, IPAddress &ip) {
  int parts[4] = { 0, 0, 0, 0 };
  int part = 0;
  int lastPos = 0;
  for (int i = 0; i < str.length() && part < 4; i++) {
    if (str[i] == '.') {
      parts[part++] = str.substring(lastPos, i).toInt();
      lastPos = i + 1;
    }
  }
  parts[part] = str.substring(lastPos).toInt();
  ip = IPAddress(parts[0], parts[1], parts[2], parts[3]);
  return true;
}

//----------------------------------------------------------
// Funkcja: readHttpHeaders
// Odczytuje nagłówki HTTP do momentu napotkania "\r\n\r\n"
//----------------------------------------------------------
String readHttpHeaders(EthernetClient &client) {
  String headers;
  while (true) {
    if (!client.available()) {
      delay(1);
      continue;
    }
    char c = client.read();
    headers += c;
    if (headers.endsWith("\r\n\r\n")) break;
  }
  return headers;
}


//----------------------------------------------------------
// Funkcja: readHttpBody
// Odczytuje treść żądania HTTP
//----------------------------------------------------------
String readHttpBody(EthernetClient &client, int contentLen) {
  String body;
  body.reserve(contentLen);
  while (contentLen > 0) {
    while (!client.available()) delay(1);
    char c = client.read();
    body += c;
    contentLen--;
  }
  return body;
}

//----------------------------------------------------------
// Funkcja: checkAuth
// Sprawdza poprawność Basic Auth przesłanego w nagłówkach HTTP
//----------------------------------------------------------
//bool wasLoggedIn = false; // globals.cpp
//extern bool wasLoggedIn;  // globals.h

bool checkAuth(const String &headers, EthernetClient &client) {
  int idx = headers.indexOf("Authorization: Basic ");
  if (idx == -1) {
    // Nie podano danych, próba błędna
    loginAttempts++;  // każda nieudana próba (brak lub złe dane)
    failedLoginAttempts++;
    logLoginAttemptToFile(client.remoteIP().toString(), false);
    return false;
  }
  int lineEnd = headers.indexOf("\r\n", idx);
  if (lineEnd == -1) lineEnd = headers.length();
  String authLine = headers.substring(idx, lineEnd);
  String expected = "Authorization: Basic " + httpAuthBase64;
  bool success = (authLine == expected);
  logLoginAttemptToFile(client.remoteIP().toString(), success);

  if (success) {
    // Tylko PIERWSZE udane logowanie w tej sesji (od startu)
    if (!wasLoggedIn) {
      wasLoggedIn = true;
      lastLoginTime = getCurrentTimeString();
      lastLoginIP = client.remoteIP().toString();
      // (opcjonalnie możesz mieć loginSuccesses++, jeśli chcesz liczyć ile razy ktoś zalogował się po starcie)
    }
    return true;
  } else {
    loginAttempts++;  // każda nieudana próba
    failedLoginAttempts++;
    return false;
  }
}




void sendAuthRequired(EthernetClient &client) {
  client.println("HTTP/1.1 401 Unauthorized");
  client.println("WWW-Authenticate: Basic realm=\"ESP32Auth\", charset=\"UTF-8\"");
  client.println("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
  client.println("Pragma: no-cache");  // Wyłącza cache dla starszych przeglądarek
  client.println("Expires: 0");        // Wyłącza cache
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println("Access-Control-Allow-Credentials: false");  // Wyłącza zapamiętywanie po stronie klienta
  client.println();
  client.println("<html><body><h2>401 Unauthorized</h2></body></html>");
}

void handleLogout(EthernetClient &client) {
  client.println("HTTP/1.1 401 Unauthorized");
  client.println("WWW-Authenticate: Basic realm=\"Logout\", charset=\"UTF-8\"");
  client.println("Cache-Control: no-store, no-cache, must-revalidate, max-age=0");
  client.println("Pragma: no-cache");
  client.println("Expires: 0");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html><head><meta charset='UTF-8'><title>Wylogowano</title></head><body>");
  client.println("<h2>Wylogowano!</h2>");
  client.println("<p>Jeśli przeglądarka nadal pamięta dane logowania, zamknij kartę lub przeglądarkę i otwórz ją ponownie.</p>");
  client.println("<p>Kliknij <a href='/'>tutaj</a>, aby przejść do strony głównej.</p>");
  client.println("</body></html>");
}

void handle404(EthernetClient &client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<html><body>");
  client.println("<h2>Błąd 404 - Strona nie istnieje</h2>");
  client.println("<p>Przepraszamy, ale żądana strona nie została odnaleziona.</p>");
  client.println("</body></html>");
}

void send200(EthernetClient &client, const String &contentType, const String &message) {
  client.println("HTTP/1.1 200 OK");
  client.println("Connection: close");
  client.print("Content-Type: ");
  client.println(contentType);
  client.print("Content-Length: ");
  client.println(message.length());
  client.println();
  client.print(message);
}


//----------------------------------------------------------
// Funkcja: toggleRelay
// Przełącza stan przekaźnika o danym indeksie (0-3)z info do LOG
//----------------------------------------------------------
void toggleRelay(int index) {
  bool curr = (digitalRead(relayPins[index]) == HIGH);
  bool next = !curr;
  digitalWrite(relayPins[index], next ? HIGH : LOW);
  relayStates[index] = next ? "ON" : "OFF";
  if (next) {
    relayOnTime[index] = millis();
    addLog("[INFO] Relay " + String(index) + " turned ON");
  } else {
    addLog("[INFO] Relay " + String(index) + " turned OFF");
  }
  Serial.printf("[INFO] Relay %d -> %s\n", index, relayStates[index].c_str());
}

//----------------------------------------------------------
// Funkcja: displayWelcomeScreen
// Wyświetla ekran powitalny na TFT przez 5 sekund
//----------------------------------------------------------
void displayWelcomeScreen() {
  spr.createSprite(tft.width(), tft.height());
  spr.fillSprite(TFT_BLACK);
  spr.setTextSize(1);
  spr.setTextColor(TFT_ORANGE, TFT_BLACK);
  String title = "PowerReset v2.52";
  int16_t tw = spr.textWidth(title);
  int16_t x = (spr.width() - tw) / 2;
  spr.setCursor(x, 20);
  spr.println(title);
  spr.setTextSize(1);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  String compileInfo = String("") + __DATE__ + " " + __TIME__;
  tw = spr.textWidth(compileInfo);
  x = (spr.width() - tw) / 2;
  spr.setCursor(x, 50);
  spr.println(compileInfo);
  String author = "Autor: Pigulk@ 2025";
  tw = spr.textWidth(author);
  x = (spr.width() - tw) / 2;
  spr.setCursor(x, 70);
  spr.println(author);
  spr.pushSprite(0, 0);
  delay(5000);
  spr.deleteSprite();
}

int parseContentLength(const String &headers) {
  int index = headers.indexOf("Content-Length: ");
  if (index == -1) return 0;
  index += 16;  // długość "Content-Length: "
  int end = headers.indexOf("\r\n", index);
  return headers.substring(index, end).toInt();
}





void displayOnTFT() {
  spr.createSprite(tft.width(), tft.height());
  spr.fillSprite(TFT_BLACK);

  // Adres IP i stan linku
  IPAddress localIP = Ethernet.localIP();
  spr.setTextSize(1);
  spr.setCursor(0, 0);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.printf("IP: %d.%d.%d.%d ", localIP[0], localIP[1], localIP[2], localIP[3]);
  bool linkActive = (Ethernet.linkStatus() == LinkON);
  spr.setTextColor(linkActive ? TFT_RED : TFT_GREEN, TFT_BLACK);
  spr.print(linkActive ? "(ON)" : "(OFF)");
  spr.println();

  // AUTO ON/OFF poniżej IP
  spr.setTextSize(2);
  String autoText = autoResetEnabled ? "AUTO ON" : "AUTO OFF";
  spr.setTextColor(autoResetEnabled ? TFT_SKYBLUE : TFT_ORANGE, TFT_BLACK);
  int16_t tw = spr.textWidth(autoText);
  int16_t x = (spr.width() - tw) / 2;
  spr.setCursor(x, 12);
  spr.print(autoText);
  spr.setTextSize(1);

  // Sekcja przekaźniki + wejścia W JEDNYM WIERSZU
  int baseY = 33;       // Gdzie zaczyna się tabela poniżej AUTO ON
  int relayColX = 0;    // lewa kolumna
  int inputColX = 80;   // prawa kolumna (dostosuj do swojego ekranu TFT!)
  int lineHeight = 20;  // odstęp pionowy

  for (int i = 0; i < ACTIVE_RELAYS; i++) {
    // Przekaźnik
    spr.setCursor(relayColX, baseY + i * lineHeight);
    spr.setTextColor((relayStates[i] == "ON") ? TFT_RED : TFT_GREEN, TFT_BLACK);
    char buf[30];
    sprintf(buf, "Relay %d: %s", i + 1, relayStates[i].c_str());
    spr.print(buf);

    // Wejście w tej samej linii!
    spr.setCursor(inputColX, baseY + i * lineHeight);
    int inState = digitalRead(inputPins[i]);
    spr.setTextColor(inState == HIGH ? TFT_RED : TFT_GREEN, TFT_BLACK);
    char inBuf[20];
    sprintf(inBuf, "IN%d: %s", i + 1, inState == HIGH ? "HIGH" : "LOW");
    spr.print(inBuf);

    // MONITOROWANIE – kropka tylko przy monitorowanym wejściu!
    if (inputMonitoringEnabled[i]) {
      int circleX = inputColX + spr.textWidth(inBuf) + 12;
      int circleY = baseY + i * lineHeight + 3;
      // Kolor zależny od stanu przekaźnika
      uint16_t dotColor = (relayStates[i] == "ON") ? TFT_RED : TFT_GREEN;
      spr.fillCircle(circleX, circleY, 3, dotColor);
    }

    // Opis przekaźnika pod spodem (jeśli jest)
    if (linkDescriptions[i] != "Brak opisu") {
      String linkTFT = removePolish(linkDescriptions[i]);
      spr.setCursor(relayColX, baseY + i * lineHeight + 8);
      spr.setTextColor(0x8410, TFT_BLACK);
      spr.println(linkTFT);
    }
  }


  // Nazwa miejsca na dole
  spr.setTextSize(1);
  spr.setTextColor(TFT_CYAN, TFT_BLACK);
  String placeTFT = removePolish(placeStr);
  int16_t yPos = spr.height() - 10;
  if (yPos < 0) yPos = 0;
  int16_t textW = spr.textWidth(placeTFT);
  int16_t xPos = (spr.width() - textW) / 2;
  spr.setCursor(xPos, yPos);
  spr.println(placeTFT);

  spr.pushSprite(0, 0);
  spr.deleteSprite();
}



bool isAnyInputLow() {
  bool e1 = false, e2 = false, e3 = false, e4 = false;

  if (inputMonitoringEnabled[0]) {
    e1 = (digitalRead(E1Pin) == LOW);
  }
  if (inputMonitoringEnabled[1]) {
    e2 = (digitalRead(E2Pin) == LOW);
  }
  if (inputMonitoringEnabled[2]) {
    e3 = (digitalRead(E3Pin) == LOW);
  }
  if (inputMonitoringEnabled[3]) {
    e4 = (digitalRead(E4Pin) == LOW);
  }

  return (e1 || e2 || e3 || e4);
}

// Załącza/wyłącza WSZYSTKIE przekaźniki (u Ciebie 4)
void setAllRelays(bool on) {
  // Masz 4 piny: 27,12,13,14
  // Dostępne też w relayPins[], ale można zrobić tak:
  digitalWrite(27, on ? HIGH : LOW);
  digitalWrite(12, on ? HIGH : LOW);
  digitalWrite(13, on ? HIGH : LOW);
  digitalWrite(14, on ? HIGH : LOW);

  // Zaktualizuj stany w tablicy (jeśli chcesz je spójne z autoResetActive):
  for (int i = 0; i < 4; i++) {
    relayStates[i] = on ? "ON" : "OFF";
    if (on) relayOnTime[i] = millis();
  }
}


void handleAutoResetLogic() {
  for (int i = 0; i < 4; i++) {
    if (!inputMonitoringEnabled[i]) continue;  // pomiń, jeśli nie monitorujemy tego wejścia

    switch (inputState[i]) {
      case IDLE:
        if (digitalRead(inputPins[i]) == LOW) {
          inputState[i] = WAITING_1MIN;
          stateStartTime[i] = millis();
          addLog("[WARN] AutoReset: [input " + String(i + 1) + "] Przejście z IDLE do WAITING_1MIN (wejście LOW)");
        }
        break;

      case WAITING_1MIN:
        {
          // ZAWSZE NAJPIERW sprawdź, czy wejście wróciło do HIGH – natychmiast wracaj do IDLE
          if (digitalRead(inputPins[i]) == HIGH) {
            inputState[i] = IDLE;
            resetAttempts[i] = 0;
            addLog("[INFO] AutoReset: [input " + String(i + 1) + "] Wejście wróciło HIGH, powrót do IDLE");
            break;
          }

          unsigned long elapsed = millis() - stateStartTime[i];
          if (elapsed >= WAITING_1MIN_TIME) {
            // Sprawdź jeszcze raz – czy nadal LOW po upływie czasu
            if (digitalRead(inputPins[i]) == LOW) {
              resetAttempts[i]++;
              if (resetAttempts[i] >= 3) {
                inputState[i] = MAX_ATTEMPTS;
                maxResetReached[i] = true;
                addLog("[WARN] AutoReset: [input " + String(i + 1) + "] Maksymalna liczba prób resetu osiągnięta");
              } else {
                inputState[i] = DO_RESET;
                stateStartTime[i] = millis();
                addLog("[INFO] AutoReset: [input " + String(i + 1) + "] Przejście do DO_RESET");
              }
            } else {
              // Dodatkowa ochrona: nawet po przekroczeniu czasu, jeśli wróciło na HIGH – wracamy do IDLE
              inputState[i] = IDLE;
              resetAttempts[i] = 0;
              addLog("[INFO] AutoReset: [input " + String(i + 1) + "] Wejście wróciło HIGH (po odczekaniu), powrót do IDLE");
            }
          }
          break;
        }

      case DO_RESET:
        if (millis() - stateStartTime[i] < RESET_DURATION) {
          setRelay(i, true);  // Włącz tylko ten przekaźnik
        } else {
          setRelay(i, false);
          inputState[i] = WAIT_START;
          stateStartTime[i] = millis();
          addLog("[INFO] AutoReset: [input " + String(i + 1) + "] Reset wykonany (" + String(resetAttempts[i]) + ") -> przejście do WAIT_START");
        }
        break;

      case WAIT_START:
        if (millis() - stateStartTime[i] >= DEVICE_STARTTIME) {
          inputState[i] = IDLE;
          maxResetReached[i] = false;
          addLog("[INFO] AutoReset: [input " + String(i + 1) + "] Urządzenie uruchomione, powrót do IDLE");
        }
        break;

      case MAX_ATTEMPTS:
        // Tu można dodać obsługę ręcznego kasowania blokady przez użytkownika
        break;
    }
  }
  //  esp_task_wdt_reset(); // Jeśli używasz watchdog
}



String getTimeFromServer() {
  Serial.println("\n=== PRÓBA POŁĄCZENIA (NTP przez UDP) ===");
  Serial.print("Adres serwera czasu: ");
  Serial.println(ntpServerAddress);
  Serial.print("Port serwera czasu: ");
  Serial.println(ntpServerPort);

  byte packetBuffer[48];
  memset(packetBuffer, 0, 48);

  // Ustaw LI=0, VN=4, Mode=3 (klient) – wartość 0x23
  packetBuffer[0] = 0x23;  // binarnie: 0010 0011

  // Debug: Wyświetl wysyłany pakiet
  Serial.println("Wysyłany pakiet NTP:");
  for (int i = 0; i < 48; i++) {
    if (packetBuffer[i] < 16) Serial.print("0");
    Serial.print(packetBuffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  // Wysłanie pakietu przez UDP na port ntpServerPort (np. 123)
  udp.beginPacket(ntpServerAddress.c_str(), ntpServerPort);
  udp.write(packetBuffer, 48);
  udp.endPacket();

  Serial.println("Oczekiwanie na odpowiedź...");
  unsigned long start = millis();
  // Oczekiwanie na odpowiedź przez maksymalnie 2000 ms
  while (udp.parsePacket() < 48 && (millis() - start < 2000)) {
    delay(10);
  }

  if (udp.parsePacket() >= 48) {
    udp.read(packetBuffer, 48);

    // Debug: Wyświetl odebrany pakiet
    Serial.println("Odebrano pakiet NTP:");
    for (int i = 0; i < 48; i++) {
      if (packetBuffer[i] < 16) Serial.print("0");
      Serial.print(packetBuffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    // Odczyt czasu – bajty 40-43 zawierają sekundy od 1900-01-01
    unsigned long secsSince1900 = ((unsigned long)packetBuffer[40] << 24) | ((unsigned long)packetBuffer[41] << 16) | ((unsigned long)packetBuffer[42] << 8) | ((unsigned long)packetBuffer[43]);
    const unsigned long seventyYears = 2208988800UL;
    unsigned long epoch = secsSince1900 - seventyYears;

    Serial.print("Otrzymano czas (Unix epoch): ");
    Serial.println(epoch);
    return String(epoch);
  } else {
    Serial.println("Brak odpowiedzi (timeout)");
    return "ERROR_TIMEOUT";
  }
}





void sendChangeLinkPage_GET(EthernetClient &client, int relayIndex) {
  sendCommonHtmlHeader(client, "Edytuj etykietę linku");
  sendMainContainerBegin(client, "Edytuj etykietę linku");

  client.println("<form method='POST' action='/changeLink?relay=" + String(relayIndex) + "'>");
  client.println("<div style='margin-bottom:18px;'>");
  client.println("<label for='newLink' style='display:block; margin-bottom:7px;'>Etykieta dla przekaźnika " + String(relayIndex) + ":</label>");
  client.print("<input type='text' id='newLink' name='newLink' value='");
  client.print(linkDescriptions[relayIndex]);
  client.println("' style='width:100%; padding:10px; background:#333; color:#e0e0e0; border:none; border-radius:4px;'>");
  client.println("</div>");
  client.println("<button type='submit' style='padding:10px 20px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Zapisz</button>");
  client.println("</form>");
  client.println("<p style='margin-top:14px;'><a href='/' style='color:#4fc3f7;'>Powrót do strony głównej</a></p>");

  client.println("</div>");  // zamknięcie .container z sendMainContainerBegin
  sendCommonHtmlFooter(client);
}


void handleChangeLinkPage_POST(EthernetClient &client, int relayIndex, const String &body) {
  int idx = body.indexOf("newLink=");
  if (idx < 0) {
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Connection: close");
    client.println();
    client.println("<h2>Brak parametru newLink</h2>");
    return;
  }

  // Wyciągamy wartość parametru
  String newLink = body.substring(idx + 8);
  int amp = newLink.indexOf('&');
  if (amp >= 0) newLink = newLink.substring(0, amp);

  // Jeśli chcesz dekodować np. 'Zasilanie%C5%82' -> 'Zasilanieł', możesz tu dodać:
  newLink = urlDecode(newLink);

  // NIE wołamy removePolish(newLink)! Zostawiamy oryginalny tekst (z ogonkami).
  linkDescriptions[relayIndex] = newLink;

  // Zapis do Preferences
  preferences.begin("linkdesc", false);
  preferences.putString(("link" + String(relayIndex)).c_str(), newLink);
  preferences.end();

  addLog("[INFO] Zmieniono etykietę przekaźnika " + String(relayIndex) + " na: " + newLink);

  // Odpowiedź
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html><head><meta charset='UTF-8'><title>Potwierdzenie</title></head><body>");
  client.println("<h2>Etykieta została zaktualizowana!</h2>");
  client.println("<p><a href='/' style='color:#0af;'>Powrót do strony głównej</a></p>");
  client.println("</body></html>");
}

#include <Update.h>


bool skipToFirmwareStart(EthernetClient &client, const String &boundary) {
  // Oczekiwany boundary – według RFC musi mieć dwa dodatkowe minusy
  String expectedBoundaryLine = "--" + boundary;
  Serial.println("🔍 Szukam boundary: '" + expectedBoundaryLine + "'");

  unsigned long timeout = millis() + 15000;  // 15 sekund timeout
  String headerData = "";

  // Czytamy dane, aż znajdziemy podwójny znak końca linii
  while (millis() < timeout) {
    while (client.available()) {
      char c = client.read();
      headerData += c;
      // Jeśli nagłówki multipart zakończyły się sekwencją "\r\n\r\n"
      if (headerData.indexOf("\r\n\r\n") != -1) {
        // Sprawdzamy, czy dane nagłówkowe zaczynają się od oczekiwanego boundary
        if (headerData.startsWith(expectedBoundaryLine)) {
          Serial.println("✅ Znaleziono początek danych firmware.");
          Serial.println("Odebrane nagłówki:\n" + headerData);
          return true;
        } else {
          Serial.println("❌ Oczekiwano boundary, otrzymano:\n" + headerData);
          return false;
        }
      }
    }
  }

  Serial.println("❌ Nie znaleziono boundary - timeout.");
  return false;
}


bool skipHttpHeaders(EthernetClient &client, unsigned long timeoutMs = 10000) {
  unsigned long startTime = millis();
  int emptyLines = 0;
  String line;

  while (millis() - startTime < timeoutMs) {
    if (client.available()) {
      char c = client.read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (line.length() == 0) {
          emptyLines++;
          if (emptyLines >= 1) return true;
        }
        line = "";
      } else {
        line += c;
      }
    } else {
      delay(1);
    }
  }

  return false;  // timeout
}

#include <Update.h>
#include <FS.h>
#include <SPIFFS.h>



void handleUpdateFirmware_POST(EthernetClient &client, const String &headers, int contentLength) {
  const size_t MIN_FW_SIZE = 480 * 1024;       // Minimalny rozmiar .bin ESP32
  const size_t MAX_FW_SIZE = 2 * 1024 * 1024;  // 2MB

  Serial.println("\n=== Rozpoczęcie aktualizacji OTA ===");
  Serial.printf("Content-Length: %d\n", contentLength);

  if (contentLength < MIN_FW_SIZE) {
    sendHttpError(client, "400 Bad Request", "Plik jest za mały. To raczej nie jest firmware.");
    return;
  }
  if (contentLength > MAX_FW_SIZE) {
    sendHttpError(client, "413 Payload Too Large", "Plik jest za duży.");
    return;
  }

  if (!Update.begin(contentLength)) {
    Serial.printf("❌ Błąd inicjalizacji OTA: %s\n", Update.errorString());
    sendHttpError(client, "500 Internal Server Error", "Błąd inicjalizacji aktualizacji.");
    return;
  }

  Serial.println("🔄 Rozpoczynam pobieranie danych firmware...");
  uint8_t buff[1024];
  size_t totalWritten = 0;
  unsigned long lastData = millis();
  bool signatureChecked = false;

  while (totalWritten < contentLength) {
    if (!client.connected()) {
      Serial.println("❌ Klient rozłączył się.");
      Update.abort();
      logUpdateError("Klient rozłączył się.");
      sendHttpError(client, "500 Internal Server Error", "Klient rozłączył się.");
      return;
    }
    if (client.available()) {
      lastData = millis();
      size_t toRead = min(sizeof(buff), contentLength - totalWritten);
      size_t bytesRead = client.readBytes(buff, toRead);

      if (bytesRead > 0) {
        // Sprawdź podpis firmware tylko raz, na początku
        if (!signatureChecked && bytesRead > 0) {
          signatureChecked = true;
          if (buff[0] != 0xE9 && buff[0] != 0xEA) {
            Update.abort();
            logUpdateError("Nieprawidłowa sygnatura pliku firmware.");
            sendHttpError(client, "400 Bad Request", "Plik nie wygląda na firmware ESP32.");
            return;
          }
        }
        if (Update.write(buff, bytesRead) != bytesRead) {
          Serial.printf("❌ Błąd zapisu: %s\n", Update.errorString());
          Update.abort();
          logUpdateError("Błąd zapisu firmware.");
          sendHttpError(client, "500 Internal Server Error", "Błąd zapisu firmware.");
          return;
        }
        totalWritten += bytesRead;
        Serial.printf("📦 Zapisano: %d / %d (%.1f%%)\n", totalWritten, contentLength, 100.0 * totalWritten / contentLength);
      }
    } else {
      if (millis() - lastData > 15000) {
        Serial.println("\n❌ Timeout - brak danych przez 15s");
        Update.abort();
        logUpdateError("Timeout podczas uploadu.");
        sendHttpError(client, "408 Request Timeout", "Brak danych.");
        return;
      }
      delay(1);
    }
  }

  if (Update.end(true)) {
    Serial.println("\n✅ Aktualizacja zakończona sukcesem.");
    logUpdateSuccess(totalWritten);
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html; charset=UTF-8");
    client.println("Connection: close");
    client.println();
    client.println("<!DOCTYPE html>");
    client.println("<html><head><meta charset='UTF-8'><title>Aktualizacja udana</title></head><body>");
    client.println("<h2>Aktualizacja udana! Urządzenie zostanie zrestartowane.</h2>");
    client.println("<script>setTimeout(function(){ window.location.href = '/'; }, 5000);</script>");
    client.println("<p><a href='/' style='color:#0af;'>Powrót do strony głównej</a></p>");
    client.println("</body></html>");
    client.flush();
    delay(5000);
    ESP.restart();
  } else {
    Serial.printf("❌ Aktualizacja nieudana: %s\n", Update.errorString());
    logUpdateError("Update.end(false)");
    sendHttpError(client, "500 Internal Server Error", "Aktualizacja nieudana.");
    Update.abort();
  }
}



// Funkcje pomocnicze
String getBoundary(const String &headers) {
  int start = headers.indexOf("boundary=");
  if (start == -1) return "";
  start += 9;  // Pomijamy "boundary="

  int end = headers.indexOf("\r\n", start);
  if (end == -1) end = headers.length();

  String boundary = headers.substring(start, end);

  // Usuwamy cudzysłowy jeśli istnieją
  if (boundary.startsWith("\"") && boundary.endsWith("\"")) {
    boundary = boundary.substring(1, boundary.length() - 1);
  }

  return boundary;
}

bool readUntil(EthernetClient &client, const char *terminator) {
  size_t idx = 0;
  size_t len = strlen(terminator);
  unsigned long timeout = millis() + 10000;

  while (millis() < timeout) {
    if (client.available()) {
      char c = client.read();
      if (c == terminator[idx]) {
        idx++;
        if (idx == len) return true;
      } else {
        idx = 0;
      }
    }
  }
  return false;
}

void logUpdateSuccess(size_t size) {
  File logFile = SPIFFS.open("/update.log", "a");
  if (logFile) {
    logFile.printf("Udana aktualizacja, rozmiar: %d bajtów\n", size);
    logFile.close();
  }
}

// Funkcja pomocnicza do pomijania nagłówków
bool skipHeaders(EthernetClient &client) {
  unsigned long timeout = millis() + 30000;
  while (millis() < timeout) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      line.trim();
      if (line.isEmpty()) return true;  // Znaleziono koniec nagłówków
    }
    delay(1);
  }
  return false;
}

// Funkcja wysyłająca odpowiedź sukcesu
void sendSuccessResponse(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Connection: close");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println();
  client.println("<html><body><h2>✅ Aktualizacja udana! Restartowanie...</h2></body></html>");
  client.flush();
}

String readLine(EthernetClient &client) {
  String line;
  char c;
  while (client.connected()) {
    if (client.available()) {
      c = client.read();
      if (c == '\n') break;
      if (c != '\r') line += c;
    }
  }
  return line;
}

// Przykładowa funkcja pomocnicza wczytująca i pomijająca linie
void skipToBinary(EthernetClient &client) {
  while (true) {
    String line = readLine(client);
    if (line.length() == 0) break;
  }
}

//Obsługa zapisu ustawień
// Funkcja obsługująca POST do /saveSettings – pobiera dane, zapisuje i ustawia flagę reinitEthernet
// Pomocnicza funkcja do wyodrębniania wartości parametru z treści żądania
String getParamValue(const String &body, const String &param) {
  int start = body.indexOf(param + "=");
  if (start < 0) return "";
  start += param.length() + 1;  // Po '='
  int end = body.indexOf('&', start);
  if (end < 0) end = body.length();  // Jeśli nie ma '&', używamy końca ciągu
  return body.substring(start, end);
}



void handleSaveSettings_POST(EthernetClient &client, const String &body) {
  auto getParam = [&](const String &name) -> String {
    int start = body.indexOf(name + "=");
    if (start < 0) return "";
    int end = body.indexOf('&', start);
    if (end < 0) end = body.length();
    return body.substring(start + name.length() + 1, end);
  };

  // --- Ilość przekaźników ---
  String relayCountStr = getParam("relayCount");
  int relayCount = relayCountStr.toInt();
  if (relayCount < 1 || relayCount > 4) relayCount = 4;  // zabezpieczenie

  // --- Pozostałe parametry ---
  String resetStr = getParam("resetDur");
  String startStr = getParam("devStart");
  String autoOffStr = getParam("autoOff");
  String waitTimeStr = getParam("waitTime");

  unsigned long resetSec = resetStr.toInt();
  unsigned long startSec = startStr.toInt();
  unsigned long autoOffSec = autoOffStr.toInt();
  unsigned long waitTimeSec = waitTimeStr.toInt();

  // Minimalne wartości (w sekundach)
  if (resetSec < 10 || startSec < 10 || autoOffSec < 10 || waitTimeSec < 10) {
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Content-Type: text/html; charset=UTF-8");
    client.println("Connection: close");
    client.println();
    client.println("<html><body style='background:#222; color:#ccc;'>");
    client.println("<h2>Błąd - Podane wartości są za niskie!</h2>");
    client.println("<p>Minimalnie: 10 s dla resetu, uruchomienia, auto-off oraz czasu oczekiwania.</p>");
    client.println("<p><a href='/settings' style='color:#0af;'>Powrót do ustawień</a></p></body></html>");
    return;
  }

  String logFilesEnabledStr = getParam("logFilesEnabled");  // z Twojej funkcji getParam
  enableLogFiles = (logFilesEnabledStr == "1");
  preferences.begin("settings", false);
  preferences.putBool("logFilesEnabled", enableLogFiles);
  preferences.end();

  // Konwersja na milisekundy
  RESET_DURATION = resetSec * 1000UL;
  DEVICE_STARTTIME = startSec * 1000UL;
  autoOffTime = autoOffSec * 1000UL;
  WAITING_1MIN_TIME = waitTimeSec * 1000UL;

  // --- Zapis do Preferences, również ilości przekaźników ---
  preferences.begin("timings", false);
  preferences.putULong("resetDur", RESET_DURATION);
  preferences.putULong("devStart", DEVICE_STARTTIME);
  preferences.putULong("waitTime", WAITING_1MIN_TIME);
  preferences.putUInt("relayCount", relayCount);
  preferences.end();

  preferences.begin("autooff", false);
  preferences.putUInt("autoOffSec", autoOffSec);
  preferences.end();

  // --- Ustaw zmienną globalną ---
  ACTIVE_RELAYS = relayCount;

  addLog("[INFO] Zmieniono ustawienia: liczba przekaźników = " + String(relayCount) + ", resetDur=" + resetStr + "s, devStart=" + startStr + "s, autoOff=" + autoOffStr + "s, waitTime=" + waitTimeStr + "s");

  // Przekierowanie do strony głównej
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();
  client.flush();

  Serial.printf("NOWE: RELAYS=%d, RESET_DURATION=%lums, DEVICE_STARTTIME=%lums, autoOffTime=%lums, WAITING_1MIN_TIME=%lums\n",
                ACTIVE_RELAYS, RESET_DURATION, DEVICE_STARTTIME, autoOffTime, WAITING_1MIN_TIME);
}




void handleHttpRequest(EthernetClient &client) {
  String requestLine = client.readStringUntil('\r');
  client.read();  // consume '\n'
  String headers = readHttpHeaders(client);

  if (!checkAuth(headers, client)) {
    sendAuthRequired(client);
    return;
  }

  // ======================
  // Handle GET requests
  // ======================
  if (requestLine.startsWith("GET")) {
    // API for AJAX stats
    if (requestLine.indexOf("GET /api/stats") == 0) {
      sendStatsApiJson(client);
      return;
    }

    // API for logs
    if (requestLine.indexOf("GET /api/logs") == 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain; charset=UTF-8");
      client.println("Connection: close");
      client.println();
      File file = SPIFFS.open("/log.txt", FILE_READ);
      if (file) {
        while (file.available()) client.write(file.read());
        file.close();
      }
      return;
    }

    // Log backups page
    if (requestLine.indexOf("GET /logBackups") >= 0) {
      sendLogBackupsPage_GET(client);
      return;
    }

    // Download backup
    if (requestLine.indexOf("GET /downloadLogBackup") >= 0) {
      String fileParam = getParamValue(requestLine, "file");
      handleDownloadLogBackup_GET(client, fileParam);
      return;
    }

    // View single backup
    if (requestLine.indexOf("GET /viewLogBackup") >= 0) {
      String fileParam = getParamValue(requestLine, "file");
      sendLogBackupViewPage(client, fileParam);
      return;
    }

    // Download main log
    if (requestLine.indexOf("GET /downloadLog") >= 0) {
      handleDownloadLogFile(client, "/log.txt");
      return;
    }

    // Classic log panel
    if (requestLine.indexOf("GET /logs") >= 0) {
      sendLogPage_GET(client);
      return;
    }

    if (requestLine.indexOf("GET /timeSettings") >= 0) {
      sendTimeSettingsPage_GET(client);
      return;
    }

    if (requestLine.indexOf("GET /diagnostics_data") == 0) {
      sendDiagnosticsData(client);
      return;
    }

    if (requestLine.indexOf("GET /diagnostics") == 0) {
      sendPanelDiagnostyczny(client);
      return;
    }

    if (requestLine.indexOf("GET /diagnostyka") == 0 || 
        requestLine.indexOf("GET /serialData") == 0 || 
        requestLine.indexOf("GET /ping") == 0) {
      handleDiagnosticRequest(client, requestLine);
      return;
    }

    if (requestLine.indexOf("GET /about") == 0) {
      sendAboutPage(client);
      return;
    }

    if (requestLine.indexOf("GET /stats") == 0) {
      sendStatsPage(client);
      return;
    }

    // Watchdog page
    if (requestLine.indexOf("GET /watchdog") == 0) {
      sendWatchdogPage_GET(client);
      return;
    }

    // Classic GET
    handleGetRequest(client, requestLine, headers);
    return;
  }

  // ======================
  // Handle POST requests
  // ======================
  if (requestLine.startsWith("POST")) {
    // First read the body content for all POST requests
    int contentLength = parseContentLength(headers);
    String body = readHttpBody(client, contentLength);

    // Reset stats AJAX
    if (requestLine.indexOf("POST /resetStats") >= 0) {
      handleResetStats_POST(client);
      return;
    }

    if (requestLine.indexOf("POST /resetSettings") >= 0) {
      handleResetSettings_POST(client);
      return;
    }

    // Reset restart counter AJAX
    if (requestLine.indexOf("POST /resetRestarts") >= 0) {
      deviceRestarts = 0;
      preferences.begin("stats", false);
      preferences.putUInt("restarts", 0);
      preferences.end();
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/plain");
      client.println("Connection: close");
      client.println();
      client.println("OK");
      return;
    }

    // Firmware update
    if (requestLine.indexOf("POST /updateFirmware") >= 0) {
      handleUpdateFirmware_POST(client, headers, contentLength);
      return;
    }

    // Save time settings
    if (requestLine.indexOf("POST /saveTimeSettings") >= 0) {
      handleSaveTimeSettings_POST(client, body);
      return;
    }

    // Logs
    if (requestLine.indexOf("POST /confirmLogs") >= 0) {
      handleConfirmLogs_POST(client);
      return;
    }

    if (requestLine.indexOf("POST /clearLogs") >= 0) {
      handleClearLogs_POST(client);
      return;
    }

    if (requestLine.indexOf("POST /deleteLogBackup") >= 0) {
      handleDeleteLogBackup_POST(client, body);
      return;
    }

    if (requestLine.indexOf("POST /renameLogBackup") >= 0) {
      handleRenameLogBackup_POST(client, body);
      return;
    }

    // Watchdog test endpoint
    if (requestLine.indexOf("POST /watchdog") == 0 && body.indexOf("resetWDT=1") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println("Watchdog test initiated. Device should reset in 30 seconds!");
      client.flush();
      delay(500);
      while (1) { 
        delay(1000); // Infinite loop to trigger watchdog
      }
      return;
    }

    // Classic POST
    handlePostRequest(client, requestLine, body, headers);
    return;
  }

  // Unsupported request
  sendNotFound(client);
}



void handleConfirmLogs_POST(EthernetClient &client) {
  unreadLogs = false;        // Oznacz logi jako przeczytane
  errorLogsPresent = false;  // Resetuj flagę błędów

  addLog("[INFO] Użytkownik potwierdził przeczytanie logów.");
  // Przekieruj na stronę główną zamiast do /logs
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");  // Przekierowanie do strony głównej
  client.println();
}


void handleClearLogs_POST(EthernetClient &client) {
  String backupName = createLogsBackup();
  cleanupOldLogBackups();  // ← DODAJ TO ZARAZ PO createLogsBackup()
  // Wyczyść log.txt
  if (!enableLogFiles) return;  // Nie zapisuj jeśli flaga wyłączona!
  File file = SPIFFS.open("/log.txt", FILE_WRITE);
  if (file) {
    file.print("");
    file.close();
  }

  // Reset RAM-owych logów
  for (int i = 0; i < MAX_LOGS; i++) logHistory[i] = "";
  logIndex = 0;
  unreadLogs = false;
  errorLogsPresent = false;

  // Informacja do logu i na Serial
  if (backupName != "") {
    String infoMsg = "[INFO] Logi wyczyszczone, kopia: " + backupName;
    addLog(infoMsg);
    Serial.println(infoMsg);
  } else {
    String warnMsg = "[WARN] Logi wyczyszczone, ale kopia NIE powstała!";
    addLog(warnMsg);
    Serial.println(warnMsg);
  }

  // Wyświetl listę plików w SPIFFS na Serial (kontrola debugowania)
  listFilesInSPIFFS();

  // Przekieruj na /logs
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /logs");
  client.println();
}


void skipToBinaryData(EthernetClient &client) {
  String line;
  while (true) {
    line = client.readStringUntil('\n');
    if (line.length() == 1 && line[0] == '\r') break;  // Pusta linia oznacza koniec nagłówków
  }
}




void handlePostRequest(EthernetClient &client, const String &requestLine, const String &body, const String &headers) {
  Serial.println("Odebrano żądanie POST:");
  Serial.println("Request Line: " + requestLine);
  Serial.println("Headers: " + headers);
  Serial.println("Body: " + body);

  if (requestLine.indexOf("POST /saveSettings") >= 0) {
    handleSaveSettings_POST(client, body);
    return;
  } else if (requestLine.indexOf("POST /changeAuth") >= 0) {
    handleChangeAuthPage_POST(client, body);
    return;
  } else if (requestLine.indexOf("POST /changeNet") >= 0) {
    handleChangeNetPage_POST(client, body);
    return;
  } else if (requestLine.indexOf("POST /changePlace") >= 0) {
    handleChangePlacePage_POST(client, body);
    return;
  } else if (requestLine.indexOf("POST /changeAutoOffTime") >= 0) {
    handleChangeAutoOffTimePage_POST(client, body);
    return;
  } else if (requestLine.indexOf("POST /changeLink") >= 0) {
    handleChangeLink_POST(client, requestLine, body);
    return;
  } else if (requestLine.indexOf("POST /changeAllLinks") >= 0) {
    handleChangeAllLinksPage_POST(client, body);
    return;
  } else if (requestLine.indexOf("POST /updateFirmware") >= 0) {
    int contentLength = parseContentLength(headers);
    handleUpdateFirmware_POST(client, headers, contentLength);
    return;
  } else if (requestLine.indexOf("POST /settings") >= 0) {
    handleUstawienia_POST(client, body);
    return;
  } else if (requestLine.indexOf("POST /changeInput") >= 0) {
    handleChangeInputPage_POST(client, requestLine, body);
    return;
  } else if (requestLine.indexOf("POST /clearLogs") >= 0) {
    handleClearLogs_POST(client);
    return;
  } else if (requestLine.indexOf("POST /syncTime") >= 0) {
    handleSyncTime_POST(client);
    return;
  } else if (requestLine.indexOf("POST /saveTimeServerSettings") >= 0) {
    handleSaveTimeServerSettings_POST(client, body);
    return;
  } else if (requestLine.indexOf("POST /setRefresh") != -1) {
    int pos = body.indexOf("refreshTime=");
    if (pos != -1) {
      pos += String("refreshTime=").length();
      String value = body.substring(pos);
      unsigned int newRefresh = value.toInt();
      if (newRefresh > 0) {
        refreshInterval = newRefresh;
      }
    }
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /diagnostics");
    client.println("Connection: close");
    client.println();
    delay(1);
    return;
  }

  // Jeśli nie obsłużono powyżej:
  sendNotFound(client);
  return;
}






void sendHttpError(EthernetClient &client, const String &status, const String &message) {
  client.println("HTTP/1.1 " + status);
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html><head><title>Błąd</title></head><body>");
  client.println("<h2>Błąd: " + status + "</h2>");
  client.println("<p>" + message + "</p>");
  client.println("<p><a href='/'>Powrót do strony głównej</a></p>");
  client.println("</body></html>");
}






void handleChangeAuthPage_POST(EthernetClient &client, const String &body) {
  auto getParam = [&](const String &name) -> String {
    int start = body.indexOf(name + "=");
    if (start < 0) return "";
    int end = body.indexOf('&', start);
    if (end < 0) end = body.length();
    String val = body.substring(start + name.length() + 1, end);
    val.replace("+", " ");
    val.replace("%40", "@");  // dla emaili
    return val;
  };

  String userPart = getParam("newUser");
  String passPart = getParam("newPass");
  String confirmPart = getParam("confirmPass");

  if (userPart == "" || passPart == "" || confirmPart == "") {
    sendSimplePanelMessage(client, "Błąd", "Brak parametrów w formularzu.");
    return;
  }

  if (passPart != confirmPart) {
    sendSimplePanelMessage(client, "Błąd", "Hasła nie są zgodne!");
    return;
  }

  if (!isPasswordStrong(passPart)) {
    sendSimplePanelMessage(client, "Błąd", "Hasło musi mieć co najmniej 4 znaki!");
    return;
  }

  // Możesz dodać bardziej złożoną walidację loginu!
  if (userPart.length() < 3 || userPart.length() > 24) {
    sendSimplePanelMessage(client, "Błąd", "Login musi mieć 3-24 znaki.");
    return;
  }

  // Zapisz nowe dane
  httpUser = userPart;
  httpPass = passPart;
  httpAuthBase64 = makeAuthBase64(httpUser, httpPass);

  preferences.begin("auth", false);
  preferences.putString("login", httpUser);
  preferences.putString("password", httpPass);
  preferences.end();

  // Przekierowanie (zamiast surowej strony)
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /settings");
  client.println("Connection: close");
  client.println();
}



void handleChangeNetPage_POST(EthernetClient &client, const String &body) {
  auto getParam = [&](const String &name) {
    int start = body.indexOf(name + "=");
    if (start < 0) return String("");
    int end = body.indexOf("&", start);
    if (end < 0) end = body.length();
    return body.substring(start + name.length() + 1, end);
  };

  String newIp = getParam("ip");
  String newGateway = getParam("gateway");
  String newSubnet = getParam("subnet");
  String newDns = getParam("dns");

  if (newIp == "" || newGateway == "" || newSubnet == "" || newDns == "") {
    sendSimplePanelMessage(client, "Błąd", "Brak wszystkich parametrów (ip, gateway, subnet, dns).");
    return;
  }

  IPAddress t_ip, t_gw, t_sn, t_dn;
  if (!parseIpString(newIp, t_ip) || !parseIpString(newGateway, t_gw) || !parseIpString(newSubnet, t_sn) || !parseIpString(newDns, t_dn)) {
    sendSimplePanelMessage(client, "Błąd", "Nieprawidłowy format adresu IP!");
    return;
  }

  preferences.begin("net", false);
  preferences.putString("ip_str", newIp);
  preferences.putString("gw_str", newGateway);
  preferences.putString("subnet_str", newSubnet);
  preferences.putString("dns_str", newDns);
  preferences.end();

  ipStr = newIp;
  gatewayStr = newGateway;
  subnetStr = newSubnet;
  dnsStr = newDns;

  ip = t_ip;
  gateway = t_gw;
  subnet = t_sn;
  dns = t_dn;

  // Ethernet.begin(mac, ip, dns, gateway, subnet);

  // Odpowiedź do klienta:
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();

  delay(300);  // Czas na wysłanie odpowiedzi
  ESP.restart();
}

// Funkcja panelowego komunikatu — **wrzuć ją raz do helpersów!**
void sendSimplePanelMessage(EthernetClient &client, const String &title, const String &msg) {
  sendCommonHtmlHeader(client, title);
  sendMainContainerBegin(client, title);
  client.println("<div style='padding:32px; text-align:center; font-size:1.15em; color:#e74c3c;'>");
  client.println(msg);
  client.println("<p style='margin-top:18px;'><a href='/changeNet' style='color:#4fc3f7;'>Powrót do ustawień sieci</a></p>");
  client.println("</div>");
  client.println("</div>");
  sendCommonHtmlFooter(client);
}



void handleChangePlacePage_POST(EthernetClient &client, const String &body) {
  int idx = body.indexOf("newPlace=");
  if (idx < 0) {
    sendSimplePanelMessage(client, "Błąd", "Brak parametru <b>newPlace</b> w formularzu.");
    return;
  }

  String val = body.substring(idx + 9);
  int amp = val.indexOf('&');
  if (amp >= 0) val = val.substring(0, amp);

  // Najpierw dekodujemy z %xx
  val = urlDecode(val);

  // Zapis do Preferences
  preferences.begin("info", false);
  preferences.putString("place", val);
  preferences.end();

  placeStr = val;
  addLog("[INFO] Miejsce zmienione na: " + val);

  // Przekierowanie do strony głównej (żadnego HTML niżej!)
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();

  Serial.println("Zapisano placeStr = " + placeStr);
}







void sendCombinedSettingsPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  // Zmniejszamy szerokość kontenera centralnego!
  client.println("<div class='container' style='max-width:500px; margin:24px auto 16px auto; background:#20232a; padding:14px 14px 10px 14px; border-radius:14px; box-shadow:0 2px 14px #111c; color:#e0e0e0;'>");

  // Sekcja zmiany lokalizacji
  client.println("<div class='panel' style='margin-bottom:20px; background:#21242c; border-radius:10px; padding:16px;'>");
  client.println("<h3 style='margin-top:0; color:#4fc3f7; font-size:1.12em;'>Zmiana nazwy miejsca</h3>");
  client.println("<form method='POST' action='/changePlace'>");
  client.println("<div style='margin-bottom:13px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Nazwa miejsca (wyświetlana na TFT i stronie):</label>");
  client.print("<input type='text' name='newPlace' value='");
  client.print(placeStr);
  client.println("' required style='width:100%; padding:7px; border-radius:5px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");
  client.println("<button type='submit' style='padding:9px 18px; background:#2196f3; color:#fff; border:none; border-radius:5px; font-size:1em; cursor:pointer;'>Zapisz nazwę</button>");
  client.println("</form>");
  client.println("</div>");

  // Sekcja etykiet przekaźników
  client.println("<div class='panel' style='background:#21242c; border-radius:10px; padding:16px;'>");
  client.println("<h3 style='margin-top:0; color:#4fc3f7; font-size:1.12em;'>Edycja etykiet przekaźników</h3>");
  client.println("<form method='POST' action='/changeAllLinks'>");

  for (int i = 0; i < 4; i++) {
    client.println("<div style='margin-bottom:10px;'>");
    client.print("<label style='display:block; margin-bottom:5px;'>Przekaźnik ");
    client.print(i);
    client.println(":</label>");
    client.print("<input type='text' name='link");
    client.print(i);
    client.print("' value='");
    client.print(linkDescriptions[i]);
    client.println("' style='width:100%; padding:7px; border-radius:5px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
    client.println("</div>");
  }

  client.println("<button type='submit' style='padding:9px 18px; background:#2196f3; color:#fff; border:none; border-radius:5px; font-size:1em; cursor:pointer;'>Zapisz etykiety</button>");
  client.println("</form>");
  client.println("</div>");  // .panel

  client.println("</div>");  // .container

  sendCommonHtmlFooter(client);
}




void sendChangeAutoOffTimePage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Zmiana czasu auto-off");

  client.println("<form method='POST' action='/changeAutoOffTime' style='max-width:450px; margin:auto;'>");
  client.println("<div style='margin-bottom:18px;'>");
  client.println("<label style='display:block; margin-bottom:7px;'>Podaj nowy czas auto-off (minimum 30 sekund):</label>");
  client.print("<span style='display:block; margin-bottom:12px;'>Aktualny czas: <b>" + String(autoOffTime / 1000) + " s</b></span>");
  client.println("<input type='number' name='newAutoOff' value='30' min='30' style='width:100%; padding:10px; background:#222; color:#e0e0e0; border:none; border-radius:5px;'>");
  client.println("</div>");
  client.println("<button type='submit' style='padding:10px 24px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Zapisz</button>");
  client.println("</form>");

  // Możesz dodać przycisk powrotu:
  client.println("<div style='margin-top:24px; text-align:center;'>");
  client.println("<a href='/' style='color:#4fc3f7;'>Powrót do strony głównej</a>");
  client.println("</div>");

  client.println("</div>");  // zamknięcie .container z sendMainContainerBegin
  sendCommonHtmlFooter(client);
}






void handleChangeAutoOffTimePage_POST(EthernetClient &client, const String &body) {
  int idx = body.indexOf("newAutoOff=");
  if (idx < 0) {
    sendSimplePanelMessage(client, "Błąd", "Brak parametru <b>newAutoOff</b> w formularzu.");
    return;
  }

  String val = body.substring(idx + 11);
  int amp = val.indexOf('&');
  if (amp >= 0) val = val.substring(0, amp);

  int sec = val.toInt();
  if (sec < 30) sec = 30;  // minimalnie 30 s

  // Zapis w Preferences - upewnij się, że zapisujesz do przestrzeni "autooff"
  preferences.begin("autooff", false);
  preferences.putUInt("autoOffSec", sec);
  preferences.end();

  autoOffTime = (unsigned long)sec * 1000UL;

  // Po sukcesie: przekierowanie do strony głównej (lub do zakładki z czasami)
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();

  Serial.printf("Zmieniono autoOffTime -> %d s\n", sec);
}






void handleChangeAllLinksPage_POST(EthernetClient &client, const String &body) {
  for (int i = 0; i < 4; i++) {
    String paramName = "link" + String(i) + "=";
    int start = body.indexOf(paramName);
    String newLink = "";

    if (start >= 0) {
      newLink = body.substring(start + paramName.length());
      int amp = newLink.indexOf('&');
      if (amp >= 0) {
        newLink = newLink.substring(0, amp);
      }
    }

    // Dekoduj ciągi URL-encoded
    newLink = urlDecode(newLink);

    // Jeśli pole jest puste, ustaw "Brak opisu"
    if (newLink.length() == 0) {
      linkDescriptions[i] = "Brak opisu";
    } else {
      linkDescriptions[i] = newLink;
    }

    // Zapisz do Preferences
    preferences.begin("linkdesc", false);
    preferences.putString(("link" + String(i)).c_str(), linkDescriptions[i]);
    preferences.end();
  }

  addLog("[INFO] Zmieniono etykiety wszystkich przekaźników.");

  // Przekierowanie do strony etykiet (możesz zmienić na "/settings" lub inną)
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /settings");
  client.println("Connection: close");
  client.println();
}


//----------------------------------------------------------
// Funkcja: urlDecode
// Dekoduje ciąg znaków w stylu URL-encode (np. %C4%85 -> ą).
//----------------------------------------------------------
// String urlDecode(const String &src) {
//   String decoded;
//   decoded.reserve(src.length());

//   for (size_t i = 0; i < src.length(); i++) {
//     char c = src[i];
//     if (c == '%') {
//       // Oczekujemy dwóch cyfr szesnastkowych
//       if (i + 2 < src.length()) {
//         String hex = src.substring(i + 1, i + 3);
//         char decodedChar = (char)strtol(hex.c_str(), NULL, 16);
//         decoded += decodedChar;
//         i += 2;  // pomijamy dwie kolejne cyfry
//       }
//     } else if (c == '+') {
//       // w URL encodingu '+' oznacza spację
//       decoded += ' ';
//     } else {
//       decoded += c;
//     }
//   }
//   return decoded;
// }
String urlDecode(const String &input) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = input.length();
  unsigned int i = 0;
  while (i < len) {
    char c = input.charAt(i);
    if (c == '+') {
      decoded += ' ';
    } else if (c == '%' && i + 2 < len) {
      temp[2] = input.charAt(i + 1);
      temp[3] = input.charAt(i + 2);
      decoded += char(strtol(temp, NULL, 16));
      i += 2;
    } else {
      decoded += c;
    }
    i++;
  }
  return decoded;
}


//======================================== wersje przeciążeniowe ===========

// Wersja dla tekstów z pamięci FLASH
// void addLog(const __FlashStringHelper *flashMsg) {
//   // Bezpośredni odczyt z FLASH bez kopiowania do RAM
//   Serial.print(F("[LOG] "));
//   Serial.println(reinterpret_cast<const char *>(flashMsg));
// }

void addLog(const __FlashStringHelper *flashMsg) {
  String msg(flashMsg);
  addLog(msg);
  Serial.print(F("[LOG] "));
  Serial.println(msg);
}

// Wersja dla zwykłych C-stringów
void addLog(const char *msg) {
  Serial.print(F("[LOG] "));
  Serial.println(msg);
}


// void addLog(const String &eventMessage) {
//   String dateStr = ntpConfig.getFormattedDate();
//   String timeStr = ntpConfig.getFormattedTime();

//   // do testów dateStr = "2025-06-01"; timeStr = "17:54:11"; // <--- ręcznie na sztywno

//   String timestamp;
//   if (dateStr == "1970-01-01" || dateStr == "" || timeStr == "" || timeStr == "00:00:00") {
//     timestamp = "[NOSYNC]";
//   } else {
//     timestamp = dateStr + " " + timeStr;
//   }

//   String logEntry = timestamp + " - " + eventMessage;
//   logHistory[logIndex] = logEntry;
//   logIndex = (logIndex + 1) % MAX_LOGS;
//   Serial.println("Log: " + logEntry);

//   if (!enableLogFiles) return;
//   File f = SPIFFS.open("/log.txt", FILE_APPEND);
//   if (f) {
//     f.println(logEntry);
//     f.close();
//   }
// }
void addLog(const String &eventMessage) {
  // np. jeśli masz obiekt ntpConfig z metodami getFormattedDate() i getFormattedTime():
  String dateStr = ntpConfig.getFormattedDate();  // np. "2025-04-20"
  String timeStr = ntpConfig.getFormattedTime();  // np. "12:34:56"

  String timestamp;
  // Sprawdź, czy to nie "1970-01-01" / "00:00:00" itp.
  if (dateStr == "1970-01-01" && timeStr == "00:00:00") {
    timestamp = "brak synchronizacji";
  } else {
    timestamp = dateStr + " " + timeStr;
  }

  // Dalej tworzysz wpis do logu
  String logEntry = timestamp + " - " + eventMessage;
  logHistory[logIndex] = logEntry;
  logIndex = (logIndex + 1) % MAX_LOGS;

  Serial.println("Log: " + logEntry);

  // Zapis do pliku
  File file = SPIFFS.open("/log.txt", FILE_APPEND);
  if (file) {
    file.println(logEntry);
    file.close();
  } else {
    Serial.println("Błąd otwarcia log.txt");
  }

  // Oznacz jako nieprzeczytane i sprawdź czy to błąd
  unreadLogs = true;
  if (eventMessage.indexOf("ERROR") != -1 || eventMessage.indexOf("BŁĄD") != -1 || timestamp == "brak synchronizacji") {
    errorLogsPresent = true;
  }
}



void checkLogs() {
  bool hasLogs = false;
  for (int i = 0; i < MAX_LOGS; i++) {
    if (logHistory[i].length() > 0) {
      hasLogs = true;
      if (logHistory[i].indexOf("ERROR") != -1 || logHistory[i].indexOf("BŁĄD") != -1) {
        errorLogsPresent = true;
      }
    }
  }

  if (!hasLogs) {
    errorLogsPresent = false;
  }
}

String getCurrentTimeStr() {  //stara wersja
  if (rtcFound) {
    DateTime now = rtc.now();
    char buf[20];
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());
    return String(buf);
  } else {
    // Fallback: czas od uruchomienia w sekundach
    unsigned long seconds = millis() / 1000;
    return String(seconds) + " s";
  }
}


void loadSettings() {
  // Ustawienia z sekcji "timings"
  preferences.begin("timings", true);
  WAITING_1MIN_TIME = preferences.getULong("wait1min", 60 * 1000UL);
  RESET_DURATION = preferences.getULong("resetDur", 60 * 1000UL);
  DEVICE_STARTTIME = preferences.getULong("devStart", 15 * 60 * 1000UL);
  // Odczytujemy także wartość WAITING_1MIN_TIME
  WAITING_1MIN_TIME = preferences.getULong("waitTime", 60000UL);

  // Dodajemy odczyt adresu i portu serwera NTP
  ntpServerAddress = preferences.getString("ntpAddr", ntpServerAddress);
  ntpServerPort = preferences.getInt("ntpPort", ntpServerPort);

  preferences.end();

  // Ustawienia auto-off
  preferences.begin("autooff", true);
  unsigned int savedSec = preferences.getUInt("autoOffSec", 30);  // domyślnie 30 sekund
  preferences.end();
  autoOffTime = (unsigned long)savedSec * 1000UL;
}







void handleChangeInputPage_POST(EthernetClient &client, int inputIndex, const String &body) {
  Serial.print("handleChangeInputPage_POST called for input index: ");
  Serial.println(inputIndex);

  int idx = body.indexOf("newInput=");
  if (idx < 0) {
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Connection: close");
    client.println();
    client.println("<h2>Błąd - brak parametru newInput</h2>");
    return;
  }

  // Pobierz wartość parametru newInput
  String newInput = body.substring(idx + 9);
  int amp = newInput.indexOf('&');
  if (amp >= 0) {
    newInput = newInput.substring(0, amp);
  }

  // Dekodowanie URL-encoded
  newInput = urlDecode(newInput);

  // Ograniczenie długości etykiety do 32 znaków
  if (newInput.length() > 32) {
    newInput = newInput.substring(0, 32);
  }

  // Aktualizacja globalnej tablicy etykiet
  inputLabels[inputIndex] = newInput;

  // Zapis do Preferences (używamy przestrzeni "inputLabels" i unikalnego klucza, np. "inputLabel0")
  preferences.begin("inputLabels", false);
  preferences.putString(("inputLabel" + String(inputIndex)).c_str(), newInput);
  preferences.end();

  Serial.print("Updated input label for input ");
  Serial.print(inputIndex);
  Serial.print(" to: ");
  Serial.println(newInput);

  addLog("[INFO] Zmieniono etykietę wejścia " + String(inputIndex) + " na: " + newInput);

  // Przekierowanie do strony ze stanem wejść
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /inputs");
  client.println("Connection: close");
  client.println();
}




void sendUstawieniaPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Ustawienia czasów");

  client.println("<form method='POST' action='/settings' style='max-width:450px; margin:auto;'>");

  client.println("<div style='margin-bottom:18px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Oczekiwanie na reset (s):</label>");
  client.print("<input type='number' name='wait1min' value='");
  client.print(WAITING_1MIN_TIME / 1000);
  client.println("' min='10' style='width:100%; padding:10px; background:#222; color:#e0e0e0; border:none; border-radius:5px;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:18px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Czas resetu (s):</label>");
  client.print("<input type='number' name='resetDur' value='");
  client.print(RESET_DURATION / 1000);
  client.println("' min='10' style='width:100%; padding:10px; background:#222; color:#e0e0e0; border:none; border-radius:5px;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:18px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Czas startu (s):</label>");
  client.print("<input type='number' name='devStart' value='");
  client.print(DEVICE_STARTTIME / 1000);
  client.println("' min='10' style='width:100%; padding:10px; background:#222; color:#e0e0e0; border:none; border-radius:5px;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:22px;'>");
  client.println("<label style='display:block; margin-bottom:5px;'>Czas auto-off (s):</label>");
  client.print("<input type='number' name='autoOff' value='");
  client.print(autoOffTime / 1000);
  client.println("' min='30' style='width:100%; padding:10px; background:#222; color:#e0e0e0; border:none; border-radius:5px;'>");
  client.println("</div>");

  client.println("<button type='submit' style='padding:10px 24px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Zapisz ustawienia</button>");
  client.println("</form>");

  // Możesz dodać link powrotu pod spodem, jeśli lubisz:
  client.println("<div style='margin-top:24px; text-align:center;'>");
  client.println("<a href='/' style='color:#4fc3f7;'>Powrót do strony głównej</a>");
  client.println("</div>");

  client.println("</div>");  // zamknięcie kontenera
  sendCommonHtmlFooter(client);
}




void handleUstawienia_POST(EthernetClient &client, const String &body) {
  auto getParam = [&](const String &name) -> String {
    int start = body.indexOf(name + "=");
    if (start < 0) return "";
    int end = body.indexOf('&', start);
    if (end < 0) end = body.length();
    return body.substring(start + name.length() + 1, end);
  };

  String waitStr = getParam("wait1min");
  String resetStr = getParam("resetDur");
  String startStr = getParamValue(body, "devStart");
  String autoOffStr = getParam("autoOff");

  // Walidacja – upewnij się, że wartości są dodatnie i autoOff minimum 30 s
  unsigned long newWait = waitStr.toInt() * 1000UL;
  unsigned long newReset = resetStr.toInt() * 1000UL;
  unsigned long newStart = startStr.toInt() * 1000UL;
  unsigned long newAutoOff = autoOffStr.toInt() * 1000UL;
  if (newWait < 10000 || newReset < 10000 || newStart < 10000 || newAutoOff < 30000) {
    client.println("HTTP/1.1 400 Bad Request");
    client.println("Content-Type: text/html; charset=UTF-8");
    client.println("Connection: close");
    client.println();
    client.println("<html><body style='background:#222; color:#ccc;'><h2>Błąd - Podane wartości są za niskie!</h2>");
    client.println("<p>Minimalnie: 10 s dla resetu, uruchomienia, auto-off oraz czasu oczekiwania.</p>");
    client.println("<p><a href='/settings' style='color:#0af;'>Powrót do ustawień</a></p></body></html>");
    return;
  }

  // Zapis do Preferences
  preferences.begin("timings", false);
  preferences.putULong("wait1min", newWait);
  preferences.putULong("resetDur", newReset);
  preferences.putULong("devStart", newStart);
  preferences.end();

  preferences.begin("autooff", false);
  preferences.putUInt("autoOffSec", newAutoOff / 1000);
  preferences.end();

  // Aktualizacja zmiennych globalnych
  WAITING_1MIN_TIME = newWait;
  RESET_DURATION = newReset;
  DEVICE_STARTTIME = newStart;
  autoOffTime = newAutoOff;

  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /settings");
  client.println("Connection: close");
  client.println();
  return;
}


// siła hasła
// bool isPasswordStrong(const String &password) {
//   if (password.length() < 8) return false;  // Minimalna długość 8 znaków
//   bool hasDigit = false;
//   bool hasSpecial = false;
//   for (size_t i = 0; i < password.length(); i++) {
//     char c = password[i];
//     if (isdigit(c)) {
//       hasDigit = true;
//     }
//     if (!isalnum(c)) {
//       hasSpecial = true;
//     }
//   }
//   return (hasDigit && hasSpecial);
// }

bool isPasswordStrong(const String &password) {
  return (password.length() >= 4);
}



void sendHtmlHeader(EthernetClient &client, const String &title) {

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html><head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<title>" + title + "</title>");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; background: #222; color: #ccc; margin: 0; padding: 20px; text-align: center; }");
  client.println(".container { max-width: 600px; margin: 40px auto; background: #333; padding: 20px; border-radius: 8px; text-align: left; }");
  client.println("label { display: block; margin-bottom: 5px; }");
  client.println("input[type='number'] { width: 100%; padding: 10px; margin-bottom: 5px; border: 1px solid #444; border-radius: 4px; background: #444; color: #eee; }");
  client.println("span.desc { display: block; font-size: 0.8em; color: #aaa; margin-bottom: 15px; }");
  client.println("input[type='submit'] { padding: 10px 20px; background: #007bff; border: none; border-radius: 4px; color: #fff; cursor: pointer; }");
  client.println("input[type='submit']:hover { background: #0056b3; }");
  client.println("a { color: #0af; text-decoration: none; }");
  client.println("a:hover { text-decoration: underline; }");
  client.println("</style>");
  client.println("</head><body>");
  client.println("<div class='container'>");
  client.println("<h1>" + title + "</h1>");
}


void sendHtmlFooter(EthernetClient &client) {
  // client.println("<p><a href='/' style='color:#0af;'>Powrót do strony głównej</a></p>");
  client.println("</div>");  // koniec .container
  client.println("</body></html>");
}




void reconnectEthernet() {
  static unsigned long ostatniaProba = 0;
  static uint8_t liczbaProb = 0;
  const uint8_t maksProb = 5;
  const unsigned long interwal = 10000;  // 10 sekund między próbami

  // Sprawdź status połączenia
  EthernetLinkStatus status = Ethernet.linkStatus();
  IPAddress aktualneIP = Ethernet.localIP();

  // Jeśli wszystko OK - wyzeruj licznik prób
  if (status == LinkON && aktualneIP != INADDR_NONE) {
    if (liczbaProb > 0) {
      liczbaProb = 0;
      addLog(F("[INFO] Połączenie Ethernet przywrócone"));
    }
    return;
  }

  // Jeśli problem i czas na kolejną próbę
  if (millis() - ostatniaProba > interwal) {
    ostatniaProba = millis();
    liczbaProb++;

    // Logowanie stanu
    String komunikat = "Problem z Ethernet: ";
    if (status == LinkOFF) komunikat += "brak linku";
    else if (aktualneIP == INADDR_NONE) komunikat += "brak adresu IP";
    komunikat += ", próba " + String(liczbaProb) + "/" + String(maksProb);
    addLog(komunikat);

    // Próba ponownego połączenia
    Ethernet.begin(mac, ip, dns, gateway, subnet);

    // Dodatkowe opóźnienie dla stabilizacji
    delay(2000);

    // Jeśli przekroczono maksymalną liczbę prób
    if (liczbaProb >= maksProb) {
      addLog(F("[WARN] Krytyczny błąd Ethernet - restart systemu"));
      delay(1000);
      ESP.restart();
    }
  }
}


void logError(const String &errorMessage) {
  String timestamp = getCurrentTimeStr();  // Pobierz aktualny czas
  String logEntry = timestamp + " - ERROR: " + errorMessage;

  // Zapis do pliku na SPIFFS
  File file = SPIFFS.open("/errors.log", FILE_APPEND);
  if (!file) {
    Serial.println("Nie udało się otworzyć pliku do zapisu!");
    return;
  }

  file.println(logEntry);  // Zapisz błąd do pliku
  file.close();

  Serial.println(logEntry);  // Wyświetl błąd w konsoli
}

void readStoredErrors() {
  Serial.println("Odczyt zapisanych błędów z pliku:");

  File file = SPIFFS.open("/errors.log", FILE_READ);
  if (!file) {
    Serial.println("Nie udało się otworzyć pliku do odczytu!");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    Serial.println(line);  // Wyświetl błąd
  }

  file.close();
}

void criticalError(const String &errorMessage) {
  logError(errorMessage);
  delay(5000);    // Czekaj 5 sekund
  ESP.restart();  // Restart ESP32
}

void handleChangeLinkPage_GET(EthernetClient &client, const String &requestLine) {
  int relayIndex = extractParamValue(requestLine, "relay");
  if (relayIndex >= 0 && relayIndex < 4) {
    sendChangeLinkPage_GET(client, relayIndex);
  } else {
    sendNotFound(client);
  }
}

void handleToggleRelayConfirmPage_GET(EthernetClient &client, const String &requestLine) {
  int relayIndex = extractRelayIndex(requestLine);
  if (relayIndex >= 0 && relayIndex < 4) {
    sendToggleRelayConfirmPage(client, relayIndex);  // <- tu poprawnie
  } else {
    sendNotFound(client);
  }
}

void handleToggleRelayConfirm_GET(EthernetClient &client, const String &requestLine) {
  int relayIndex = extractRelayIndex(requestLine);
  if (relayIndex >= 0 && relayIndex < 4) {
    toggleRelay(relayIndex);
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /");
    client.println("Connection: close");
    client.println();
  } else {
    sendNotFound(client);
  }
}

int extractRelayIndex(const String &requestLine) {
  int startIndex = requestLine.indexOf("relay=");
  if (startIndex == -1) return -1;
  int endIndex = requestLine.indexOf(' ', startIndex);
  String numberStr = requestLine.substring(startIndex + 6, endIndex);
  return numberStr.toInt();
}

int extractInputIndex(const String &requestLine) {
  int startIndex = requestLine.indexOf("input=");
  if (startIndex == -1) return -1;
  int endIndex = requestLine.indexOf(' ', startIndex);
  String numberStr = requestLine.substring(startIndex + 6, endIndex);
  return numberStr.toInt();
}

void handleChangeInputPage_GET(EthernetClient &client, const String &requestLine) {
  int inputIndex = extractInputIndex(requestLine);
  if (inputIndex >= 0 && inputIndex < 4) {
    sendChangeInputPage_GET(client, inputIndex);
  } else {
    sendNotFound(client);
  }
}

void handleChangeInputPage_POST(EthernetClient &client, const String &requestLine, const String &body) {
  int inputIndex = extractInputIndex(requestLine);
  if (inputIndex >= 0 && inputIndex < 4) {
    handleChangeInputPage_POST(client, inputIndex, body);
  } else {
    sendNotFound(client);
  }
}

void handleChangeLink_POST(EthernetClient &client, const String &requestLine, const String &body) {
  int relayIndex = extractRelayIndex(requestLine);
  if (relayIndex >= 0 && relayIndex < 4) {
    handleChangeLinkPage_POST(client, relayIndex, body);
  } else {
    sendNotFound(client);
  }
}

void handleToggleAutoReset_GET(EthernetClient &client) {
  autoResetEnabled = !autoResetEnabled;
  if (!autoResetEnabled) currentState = IDLE;
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");
  client.println("Connection: close");
  client.println();
}

void sendNotFound(EthernetClient &client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  client.println("<html><body><h2>404 Not Found</h2></body></html>");
}

int extractParamValue(const String &requestLine, const String &param) {
  int startIndex = requestLine.indexOf(param + "=");
  if (startIndex == -1) return -1;
  int endIndex = requestLine.indexOf(' ', startIndex);
  String numberStr = requestLine.substring(startIndex + param.length() + 1, endIndex);
  return numberStr.toInt();
}



void sendHttpResponseHeader(EthernetClient &client, int statusCode, const char *contentType = "text/html") {
  client.print("HTTP/1.1 ");
  client.print(statusCode);
  if (statusCode == 200) client.println(" OK");
  else if (statusCode == 404) client.println(" Not Found");
  else if (statusCode == 401) client.println(" Unauthorized");
  else client.println(" Unknown Status");
  client.print("Content-Type: ");
  client.println(contentType);
  client.println("Connection: close");
  client.println();  // koniec nagłówków
}




// Funkcja obsługująca potwierdzenie resetu ESP
void handleResetESPConfirm_GET(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html>");
  client.println("<html><head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<title>Resetowanie ESP</title>");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; background: #222; color: #ccc; margin: 0; padding: 20px; }");
  client.println(".container { max-width: 600px; margin: 40px auto; background: #333; padding: 20px; border-radius: 8px; }");
  client.println("h2 { color: #fff; }");
  client.println("</style>");
  client.println("</head><body>");
  client.println("<div class='container'>");
  client.println("<h2>Resetowanie ESP</h2>");
  client.println("<p>Urządzenie zostanie zresetowane za 3 sekundy...</p>");
  client.println("</div>");
  client.println("</body></html>");

  delay(3000);    // Opóźnienie przed resetem
  ESP.restart();  // Resetowanie urządzenia
}




void handleResetAttempts_GET(EthernetClient &client) {
  // Zerujemy liczniki prób i ustawiamy stan na IDLE dla wszystkich wejść
  for (int i = 0; i < NUM_INPUTS; i++) {
    resetAttempts[i] = 0;
    inputState[i] = IDLE;
    maxResetReached[i] = false;
  }

  addLog("[INFO] Wyzerowano liczniki prób resetu (resetAttempts[0-3]=0).");

  // Wysyłamy stronę z informacją i meta-refresh do strony głównej
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html><head>");
  client.println("<meta http-equiv='refresh' content='3;url=/' />");
  client.println("<title>Licznik prób wyzerowany</title>");
  client.println("</head><body>");
  client.println("<h1>Liczniki prób resetu zostały wyzerowane!</h1>");
  client.println("<p>Za 3 sekundy nastąpi przekierowanie do strony głównej.</p>");
  client.println("<p><a href='/'>Kliknij tutaj, jeśli przekierowanie nie nastąpi.</a></p>");
  client.println("</body></html>");
  client.flush();
}



void handleToggleInput_GET(EthernetClient &client, const String &requestLine) {
  int inputIndex = extractInputIndex(requestLine);
  if (inputIndex >= 0 && inputIndex < 4) {
    // Przełącz stan monitorowania wejścia
    inputMonitoringEnabled[inputIndex] = !inputMonitoringEnabled[inputIndex];

    // Zapis do Preferences
    preferences.begin("inputMon", false);
    preferences.putBool(("inputMon" + String(inputIndex)).c_str(), inputMonitoringEnabled[inputIndex]);
    preferences.end();

    addLog(String("[INFO] Monitorowanie wejścia ") + (inputIndex + 1) + (inputMonitoringEnabled[inputIndex] ? " włączone" : " wyłączone"));


    // Przekierowanie do strony ze stanem wejść
    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /inputs");
    client.println("Connection: close");
    client.println();
  } else {
    sendNotFound(client);
  }
}


void sendResetESPConfirmPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Potwierdzenie resetu ESP");

  client.println("<div style='max-width:460px; margin:48px auto; background:#23262a; border-radius:14px; padding:40px 36px 32px 36px; text-align:center; box-shadow:0 2px 16px #111c;'>");
  client.println("<h2 style='color:#fff; margin-bottom:22px;'>Czy na pewno chcesz zresetować urządzenie?</h2>");
  client.println("<div style='display:flex; flex-wrap:wrap; gap:24px; justify-content:center;'>");

  // Przycisk potwierdzający reset
  client.println("<button onclick=\"location.href='/reset_confirm'\" style='padding:12px 30px; background:linear-gradient(120deg,#ff8585 0%,#ffd1b8 100%); color:#7a1d1d; font-weight:bold; border:1.5px solid #f57373; border-radius:7px; cursor:pointer; box-shadow:0 2px 8px #9cdebc55;'>Potwierdź reset</button>");

  // Przycisk anulujący
  client.println("<button onclick=\"location.href='/'\" style='padding:12px 30px; background:#23262a; color:#e0e0e0; border:1.5px solid #555; border-radius:7px; cursor:pointer;'>Anuluj</button>");

  client.println("</div>");
  client.println("</div>");
  client.println("</div>");  // zamyka główny kontener panelu

  sendCommonHtmlFooter(client);
}



// void syncTimeOnce() {
//   // Ustawienie serwerów i strefy czasowej
//   configTime(timezoneOffset, 0,
//              ntpServer1.c_str(),
//              ntpServer4.c_str(),
//              ntpServer3.c_str());

//   struct tm timeinfo;
//   int retry = 0;
//   const int maxRetries = 3;

//   // Próby synchronizacji (maksymalnie 5)
//   // Za każdym razem rośnie liczba kropek (.)
//   while (!getLocalTime(&timeinfo) && retry < maxRetries) {
//     Serial.print("Synchronizacja czasu");
//     for (int i = 0; i <= retry; i++) {
//       Serial.print(".");
//     }
//     if (!getLocalTime(&timeinfo)) {
//   Serial.printf("Błąd synchronizacji: serwer NTP nie odpowiada! (po %d próbach)\n", maxRetries);
//   return;
// }
//     Serial.println();  // nowa linia

//     delay(1000);  // Opóźnienie 1 sekunda
//     retry++;
//   }

//   // Jeśli nadal brak synchronizacji, wyświetl komunikat o błędzie
//   if (!getLocalTime(&timeinfo)) {
//     Serial.println("Błąd synchronizacji: serwer NTP nie odpowiada!");
//     return;
//   }

//   // Jeśli się udało, potwierdź
//   Serial.println("Czas zsynchronizowany:");
//   Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
//   ntpSyncSuccess = true;  // Ustawiamy flagę na true, bo synchronizacja się powiodła
// }

void syncTimeOnce() {
  // Ustawienie serwerów i strefy czasowej
  configTime(timezoneOffset, 0,
             ntpServer1.c_str(),
             ntpServer4.c_str(),
             ntpServer3.c_str());

  struct tm timeinfo;
  int retry = 0;
  const int maxRetries = 3;

  // Próby synchronizacji (maksymalnie 3)
  while (!getLocalTime(&timeinfo) && retry < maxRetries) {
    Serial.print("Synchronizacja czasu");
    for (int i = 0; i <= retry; i++) {
      Serial.print(".");
    }
    Serial.println();
    delay(1000);
    retry++;
  }

  // Jeśli nadal brak synchronizacji, wyświetl komunikat o błędzie
  if (!getLocalTime(&timeinfo)) {
    Serial.printf("Błąd synchronizacji: serwer NTP nie odpowiada! (po %d próbach)\n", maxRetries);
    ntpSyncSuccess = false;
    return;
  }

  // Jeśli się udało, potwierdź i ZAPISZ CZAS!
  Serial.println("Czas zsynchronizowany:");
  Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
  ntpSyncSuccess = true;

  // Zapisz aktualny czas (epoch) do zmiennej globalnej i Preferences
  time_t now = time(nullptr);
  lastEpochTime = now;  // (unsigned long lastEpochTime) - globalna!

  preferences.begin("ntp", false);
  preferences.putULong("lastEpochTime", lastEpochTime);
  preferences.end();

  Serial.print("Zapisano lastEpochTime do Preferences: ");
  Serial.println(lastEpochTime);
}



void handleTimeSync(EthernetClient &client) {
  syncTimeOnce();  // ta sama funkcja co w setup()

  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /");  // wracamy na stronę główną
  client.println("Connection: close");
  client.println();
}




void handleGetRequest(EthernetClient &client, const String &requestLine, const String &headers) {
  if (requestLine.indexOf("GET / ") >= 0) sendMainPage(client);
  else if (requestLine.indexOf("GET /log") >= 0) sendLogPage_GET(client);
  else if (requestLine.indexOf("GET /inputs") >= 0) sendInputsPage_GET(client);
  else if (requestLine.indexOf("GET /settings") >= 0) sendSettingsPage_GET(client);
  else if (requestLine.indexOf("GET /changeAuth") >= 0) sendChangeAuthPage_GET(client);
  else if (requestLine.indexOf("GET /changeNet") >= 0) sendChangeNetPage_GET(client);
  else if (requestLine.indexOf("GET /changePlace") >= 0) sendChangePlacePage_GET(client);
  else if (requestLine.indexOf("GET /changeAutoOffTime") >= 0) sendChangeAutoOffTimePage_GET(client);
  else if (requestLine.indexOf("GET /changeAllLinks") >= 0) sendCombinedSettingsPage_GET(client);
  else if (requestLine.indexOf("GET /resetESP") >= 0) sendResetESPConfirmPage_GET(client);
  else if (requestLine.indexOf("GET /reset_confirm") >= 0) handleResetESPConfirm_GET(client);
  else if (requestLine.indexOf("GET /resetAttempts") >= 0) handleResetAttempts_GET(client);
  else if (requestLine.indexOf("GET /toggleAuto") >= 0) handleToggleAutoReset_GET(client);
  else if (requestLine.indexOf("GET /toggle?relay=") >= 0) handleToggleRelayConfirmPage_GET(client, requestLine);
  else if (requestLine.indexOf("GET /toggle_confirm?relay=") >= 0) handleToggleRelayConfirm_GET(client, requestLine);
  else if (requestLine.indexOf("GET /updateFirmware") >= 0) sendUpdateFirmwarePage_GET(client);
  else if (requestLine.indexOf("GET /changeLink") >= 0) handleChangeLinkPage_GET(client, requestLine);
  else if (requestLine.indexOf("GET /changeInput") >= 0) handleChangeInputPage_GET(client, requestLine);
  else if (requestLine.indexOf("GET /toggleInput") >= 0) handleToggleInput_GET(client, requestLine);
  else if (requestLine.indexOf("GET /logout") >= 0) handleLogout(client);
  else if (requestLine.indexOf("GET /timeServerSettings") >= 0) sendTimeServerSettingsPage_GET(client);
  else if (requestLine.indexOf("GET /komenda?") != -1) handleKomenda_GET(client, requestLine);
  else if (requestLine.indexOf("GET /ustawntp") != -1) handleUstawNtp_GET(client, requestLine);
  else if (requestLine.indexOf("GET /diagnostic") != -1) sendDiagnosticPage(client);
  else sendNotFound(client);
}

void handleKomenda_GET(EthernetClient &client, const String &requestLine) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  // 1) Wyciągamy parametr cmd (np. "restart" lub "test")
  String cmd = getUrlParameter(requestLine, "cmd");

  // 2) Sprawdzamy wartość cmd i reagujemy
  if (cmd == "restart") {
    // Tu kod, który rzeczywiście robi restart
    // Np. ESP.restart() na ESP32/ESP8266, lub watchdog reset
    // Ale najpierw wyślij odpowiedź, bo po restarcie i tak
    // klient nie dostanie już nic więcej
    sendHttpResponseHeader(client, 200, "text/html");
    client.println("<html><body>");
    client.println("<h2>Urządzenie się restartuje...</h2>");
    client.println("</body></html>");

    delay(100);
    // Teraz faktycznie zainicjuj restart
    ESP.restart();  // lub inna metoda, zależna od platformy
  } else if (cmd == "test") {
    // Wysyłamy jakąś informację zwrotną
    sendHttpResponseHeader(client, 200, "text/html");
    client.println("<html><body>");
    client.println("<h2>Test wykonany pomyślnie!</h2>");
    client.println("<p>Możesz tu dodać inne dane diagnostyczne...</p>");
    client.println("</body></html>");
  } else {
    // Nieznana komenda => 404
    sendHttpError(client, "404 Not Found", "Nieznana komenda: " + cmd);
  }
}


String getUrlParameter(const String &requestLine, const char *paramName) {
  // Szukamy np. "cmd="
  String paramSearch = String(paramName) + "=";
  int startIndex = requestLine.indexOf(paramSearch);
  if (startIndex == -1) return "";

  startIndex += paramSearch.length();  // początek wartości
  // parametry oddzielone są '&' lub spacją
  int endIndex = requestLine.indexOf('&', startIndex);
  if (endIndex == -1) {
    // Może spacja w linii requestu
    endIndex = requestLine.indexOf(' ', startIndex);
    if (endIndex == -1) {
      endIndex = requestLine.length();
    }
  }
  // Zwrot parametru, ewentualnie urlDecode
  return urlDecode(requestLine.substring(startIndex, endIndex));
}


//zmiana ustawień serwera ntp,portu i offsetu//
void handleUstawNtp_GET(EthernetClient &client, const String &requestLine) {
  // Limit czasu między aktualizacjami
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 60000) {
    client.println("HTTP/1.1 429 Too Many Requests");
    client.println("Retry-After: 60");
    client.println();
    return;
  }
  lastUpdate = millis();

  // Odczyt parametrów
  String newServer = getUrlParameter(requestLine, "serwer").substring(0, 64);  // Limit długości
  String newPortStr = getUrlParameter(requestLine, "port");
  String newOffsetStr = getUrlParameter(requestLine, "offset");

  // Walidacja
  bool validServer = newServer.length() >= 3 && (newServer.indexOf('.') != -1 || newServer == "pool.ntp.org");
  int newPort = newPortStr.toInt();
  bool validPort = newPort > 0 && newPort <= 65535;
  int newOffset = newOffsetStr.toInt();
  bool validOffset = newOffset >= -43200 && newOffset <= 43200;

  if (validServer || validPort || validOffset) {
    preferences.begin("ntp", false);

    if (validServer) {
      ntpConfig.setServer(newServer);
      preferences.putString("ntpAddress", newServer);
      Serial.printf("Zapisano nowy serwer NTP: %s\n", newServer.c_str());
    }

    if (validPort) {
      ntpConfig.setPort(newPort);
      preferences.putInt("ntpPort", newPort);
      Serial.printf("Zapisano nowy port NTP: %d\n", newPort);
    }

    if (validOffset) {
      ntpConfig.setTimezoneOffset(newOffset);
      preferences.putInt("ntpOffset", newOffset);
      Serial.printf("Zapisano nowy offset: %d\n", newOffset);
    }

    preferences.end();

    syncTimeOnce();

    // Zaloguj zmianę
    logEvent("Zmiana NTP", String(newServer + ":" + newPortStr).c_str());
  }

  // Przekierowanie z cache-busterem
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /diagnostyka?t=" + String(millis()));
  client.println("Cache-Control: no-cache");
  client.println();
}

// Funkcja pomocnicza do pobierania inkrementalnego numeru backupu
int getNextBackupNumber() {
  Preferences preferences;
  preferences.begin("backupnum", false);
  int current = preferences.getInt("lastNum", 0);
  int next = current + 1;
  preferences.putInt("lastNum", next);
  preferences.end();
  return next;
}


// Konfiguracja Watchdoga 
unsigned long WDT_FEED_INTERVAL = 5000;  // 10 sekund
unsigned long lastWatchdogFeed = 0;
bool hwWatchdogActive = false;
uint32_t wdtResets = 0;  // Licznik resetów z watchdoga

//==========================SETUP==================================
//=================================================================

void setup() {
  esp_task_wdt_deinit();
  Serial.begin(115200);
  delay(200);
  Serial.println("\n======== START SYSTEMU ========");

  loadSettings();

  Serial.println("Start...");
  Serial.printf("Kompilacja: %s %s\n", __DATE__, __TIME__);

  if (!SPIFFS.begin(true)) {
    Serial.println("Błąd inicjalizacji SPIFFS!");
    return;
  }
  readStoredErrors();



  if (lastEpochTime == 0) {
    lastEpochTime = getCompileTimeEpoch();
    Serial.println("Brak zapisanego czasu, ustawiam czas kompilacji:");
  } else {
    Serial.println("Ostatni zapisany czas (z pamięci):");
  }

  char buf[32];
  time_t rawtime = lastEpochTime;
  struct tm *ti = localtime(&rawtime);
  // sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
  //         ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
  //         ti->tm_hour, ti->tm_min, ti->tm_sec);

  // Użyj snprintf zamiast sprintf:
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
           ti->tm_hour, ti->tm_min, ti->tm_sec);
  Serial.println(buf);

  preferences.begin("settings", true);
  enableLogFiles = preferences.getBool("logFilesEnabled", true);
  preferences.end();

  preferences.begin("timings", true);
  ACTIVE_RELAYS = preferences.getUInt("relayCount", 4);
  RESET_DURATION = preferences.getULong("resetDur", 30000);
  DEVICE_STARTTIME = preferences.getULong("devStart", 30000);
  WAITING_1MIN_TIME = preferences.getULong("waitTime", 60000);
  preferences.end();

  preferences.begin("autooff", true);
  autoOffTime = preferences.getUInt("autoOffSec", 60) * 1000UL;
  preferences.end();

  // Ogranicz minimalny czas autoOffTime do 30 sekund
  if (autoOffTime < 30000UL) autoOffTime = 30000UL;

  // Licznik restartów
  preferences.begin("stats", false);
  deviceRestarts = preferences.getUInt("restarts", 0) + 1;
  preferences.putUInt("restarts", deviceRestarts);
  preferences.end();
  Serial.printf("Liczba restartów urządzenia: %lu\n", deviceRestarts);

  // Przekaźniki i wejścia – inicjalizacja
  for (int i = 0; i < ACTIVE_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOW);
    relayStates[i] = "OFF";
    pinMode(inputPins[i], INPUT_PULLUP);
  }

  // Autoryzacja
  preferences.begin("auth", false);
  String savedUser = preferences.getString("login", "admin");
  String savedPass = preferences.getString("password", "1234");
  preferences.end();
  httpUser = savedUser;
  httpPass = savedPass;
  httpAuthBase64 = makeAuthBase64(httpUser, httpPass);

  Serial.println("Saved User: " + savedUser);
  Serial.println("Saved Pass: " + savedPass);
  Serial.println("Base64 Auth: " + httpAuthBase64);

  // Ustawienia sieciowe
  preferences.begin("ntp", true);
  timeZone = preferences.getString("timezone", timeZone);
  ntpServer1 = preferences.getString("ntp1", ntpServer1);
  ntpServer4 = preferences.getString("ntp2", ntpServer4);
  ntpServer3 = preferences.getString("ntp3", ntpServer3);
  ntpServerAddress = preferences.getString("ntpAddress", "192.168.0.10");
  ntpServerPort = preferences.getInt("ntpPort", 123);
  int savedOffset = preferences.getInt("ntpOffset", 3600);
  lastEpochTime = preferences.getULong("lastEpochTime", 0);
  preferences.end();

  // Ustawienia NTP
  preferences.begin("net", true);  // <-- tu!
  ipStr = preferences.getString("ip_str", "192.168.0.178");
  gatewayStr = preferences.getString("gw_str", "192.168.0.1");
  subnetStr = preferences.getString("subnet_str", "255.255.255.0");
  dnsStr = preferences.getString("dns_str", "8.8.8.8");
  preferences.end();

  // Parsowanie adresów IP
  if (!parseIpString(ipStr, ip)) ip = IPAddress(192, 168, 0, 178);
  if (!parseIpString(gatewayStr, gateway)) gateway = IPAddress(192, 168, 0, 1);
  if (!parseIpString(subnetStr, subnet)) subnet = IPAddress(255, 255, 255, 0);
  if (!parseIpString(dnsStr, dns)) dns = IPAddress(8, 8, 8, 8);

  // Opisy przekaźników, wejść, monitorowanie wejść
  for (int i = 0; i < ACTIVE_RELAYS; i++) {
    preferences.begin("linkdesc", true);
    linkDescriptions[i] = preferences.getString(("link" + String(i)).c_str(), "Brak opisu");
    preferences.end();

    preferences.begin("inputdesc", true);
    inputLabels[i] = preferences.getString(("input" + String(i)).c_str(), "Wejście " + String(i + 1));
    preferences.end();

    preferences.begin("inputMonitor", true);
    inputMonitoringEnabled[i] = preferences.getBool(("inMon" + String(i)).c_str(), false);
    preferences.end();
  }

  // Nazwa miejsca (usuwanie polskich znaków, jeśli masz funkcję removePolish)
  preferences.begin("info", false);
  placeStr = preferences.getString("place", "NieUstawione");
  preferences.end();
  placeStr = removePolish(placeStr);

  // Inicjalizacja Ethernet/W5500 – bez restartów
  Ethernet.init(W5500_CS);
  delay(3000);

  unsigned long startWait = millis();
  while (Ethernet.linkStatus() != LinkON && millis() - startWait < 20000) {
    Serial.println("Czekam na pojawienie się fizycznego linku Ethernet...");
    delay(500);
  }

  bool ethOK = false;
  int maxAttempts = 8;
  int delayMs = 1200;
  for (int i = 0; i < maxAttempts; i++) {
    Ethernet.begin(mac, ip, dns, gateway, subnet);
    delay(delayMs);

    Serial.print("Próba inicjalizacji Ethernet, próba: ");
    Serial.println(i + 1);

    Serial.print("Link status: ");
    Serial.println(Ethernet.linkStatus() == LinkON ? "OK" : "OFF");

    Serial.print("IP: ");
    Serial.println(Ethernet.localIP());

    if (Ethernet.linkStatus() == LinkON && Ethernet.localIP() != INADDR_NONE) {
      Serial.println("Ethernet OK! Przechodzę dalej.");
      ethOK = true;
      break;
    }
    delayMs += 1000;
  }
  if (!ethOK) {
    Serial.println("Nie udało się zainicjalizować Ethernet. Restart!");
    delay(5000);
    ESP.restart();
  }

  udp.setTimeout(200);
  udp.begin(9000);
  ntpConfig.begin(udp, ntpServerAddress, ntpServerPort, savedOffset);
  ntpConfig.setDebugEnabled(true);
  delay(1000);

  server.begin();
  Serial.print("Adres IP: ");
  Serial.println(Ethernet.localIP());
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  displayWelcomeScreen();

  syncTimeOnce();  // Synchronizacja czasu raz

  // Ostatni restart
  lastRestartEpoch = time(nullptr);
  preferences.begin("stats", false);
  preferences.putULong("lastRestartEpoch", lastRestartEpoch);
  preferences.end();
  Serial.printf("Ostatni restart (epoch): %lu\n", lastRestartEpoch);

  Serial.println(F("\n================= DIAGNOSTYKA STARTU ================="));

  Serial.println(F("--- SIEC ---"));
  Serial.print(F("IP: "));
  Serial.println(ipStr);
  Serial.print(F("Gateway: "));
  Serial.println(gatewayStr);
  Serial.print(F("Subnet: "));
  Serial.println(subnetStr);
  Serial.print(F("DNS: "));
  Serial.println(dnsStr);
  Serial.print(F("MAC: "));
  for (int i = 0; i < 6; i++) {
    if (i > 0) Serial.print(":");
    Serial.print(mac[i], HEX);
  }
  Serial.println();

  Serial.println(F("--- AUTORYZACJA ---"));
  Serial.print(F("Login: "));
  Serial.println(httpUser);
  Serial.print(F("Hasło: "));
  Serial.println(httpPass);
  Serial.println();
  Serial.println(F("--- KONFIGURACJA CZASÓW I RELAY ---"));
  Serial.print(F("Liczba przekaźników: "));
  Serial.println(ACTIVE_RELAYS);
  Serial.print(F("Czas RESET_DURATION [ms]: "));
  Serial.println(RESET_DURATION);
  Serial.print(F("Czas DEVICE_STARTTIME [ms]: "));
  Serial.println(DEVICE_STARTTIME);
  Serial.print(F("Czas WAITING_1MIN_TIME [ms]: "));
  Serial.println(WAITING_1MIN_TIME);
  Serial.print(F("Czas autoOffTime [ms]: "));
  Serial.println(autoOffTime);
  Serial.println();
  Serial.println(F("--- OPISY PRZEKAŹNIKÓW ---"));
  for (int i = 0; i < ACTIVE_RELAYS; i++) {
    Serial.printf("Relay %d opis: %s\n", i + 1, linkDescriptions[i].c_str());
    if (linkDescriptions[i].isEmpty()) linkDescriptions[i] = "Przekaźnik " + String(i + 1);
  }
  Serial.println();
  Serial.println(F("--- OPISY I MONITORING WEJŚĆ ---"));
  for (int i = 0; i < 4; i++) {
    Serial.printf("Input %d opis: %s | monitoring: %s\n",
                  i + 1, inputLabels[i].c_str(), inputMonitoringEnabled[i] ? "TAK" : "NIE");
    if (inputLabels[i] == "") inputLabels[i] = String("Wejście ") + String(i + 1);
  }
  Serial.println();
  Serial.println(F("--- USTAWIENIA NTP ---"));
  Serial.print(F("Strefa czasowa: "));
  Serial.println(timeZone);
  Serial.print(F("Offset (sek): "));
  Serial.println(timezoneOffset);
  Serial.print(F("Serwer NTP 1: "));
  Serial.println(ntpServer1);
  Serial.print(F("Serwer NTP 2: "));
  Serial.println(ntpServer4);
  Serial.print(F("Serwer NTP 3: "));
  Serial.println(ntpServer3);
  Serial.print(F("NTP (adres): "));
  Serial.println(ntpServerAddress);
  Serial.print(F("NTP (port): "));
  Serial.println(ntpServerPort);
  Serial.println();
  Serial.println(F("--- INNE ---"));
  Serial.print(F("placeStr: "));
  Serial.println(placeStr);
  Serial.printf("Ostatni restart (epoch): %lu\n", lastRestartEpoch);
  Serial.printf("Liczba restartów: %lu\n", deviceRestarts);
  
  Serial.println("======== KONIEC DIAGNOSTYKI ========");
  Serial.println();

  // Konfiguracja Watchdoga
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,
    .idle_core_mask = 0,  
    .trigger_panic = true
  };

  if (esp_task_wdt_init(&wdt_config) == ESP_OK) {
    if (esp_task_wdt_add(NULL) == ESP_OK) {
      hwWatchdogActive = true;
      Serial.println("HW Watchdog aktywny (30s)");
    }
  }
  lastWatchdogFeed = millis();

  // Serial.printf("Watchdog: %s, ostatnie karmienie: %lums temu\n",
  //               hwWatchdogActive ? "AKTYWNY" : "NIEAKTYWNY",
  //               millis() - lastWatchdogFeed);
}

//==========KONIEC SETUP=================================
//=======================================================

unsigned long lastAutoOffCheck = 0;
unsigned long lastDisplay = 0;
unsigned long lastEthernetCheck = 0;
unsigned long lastSyncPrint = 0;  // Dla wypisywania czasu


//==========LOOP=========================================
//=======================================================

void loop() {
  loopCounter++;

  // Karmienie Watchdoga co 10s
  if (millis() - lastWatchdogFeed >= WDT_FEED_INTERVAL) {
    feedWatchdog();
    lastWatchdogFeed = millis();
  }

  // --- stałe i zmienne statyczne do sterowania wymuszoną synchronizacją ---
  static bool firstSyncAttempt = true;                           // Flaga pierwszej próby
  static unsigned long lastForcedSync = 0;                       // Kiedy ostatnio wykonano forceUpdate()
  static unsigned long forcedSyncInterval = 0;                   // Odstęp między kolejnymi wymuszonymi synchronizacjami
  const unsigned long ONE_DAY_MS = 24UL * 60UL * 60UL * 1000UL;  // 24 h w milisekundach
  const unsigned long FIVE_MIN_MS = 5UL * 60UL * 1000UL;         // 5 min w milisekundach

  static unsigned long lastSave = 0;
  if (millis() - lastSave > 60000) {  // co minutę
    lastSave = millis();
    time_t now = time(nullptr);
    if (now > 100000) {  // sensowny czas
      preferences.begin("ntp", false);
      preferences.putULong("lastEpochTime", now);
      preferences.end();
    }
  }


  monitorInputsAndRelays();

  // 1) Jeśli to pierwsze przejście pętli, wymuś synchronizację natychmiast
  if (firstSyncAttempt) {
    firstSyncAttempt = false;

    bool success = ntpConfig.forceUpdate();  // Spróbuj od razu się zsynchronizować
    if (success) {
      Serial.println("Początkowa synchronizacja NTP udana. Następna za 24h.");
      forcedSyncInterval = ONE_DAY_MS;
    } else {
      Serial.println("Początkowa synchronizacja NTP nieudana. Ponowienie za 5 minut.");
      forcedSyncInterval = FIVE_MIN_MS;
    }
    lastForcedSync = millis();
  }

  // 2) Sprawdzaj, czy minął ustalony interwał, aby wymusić synchronizację ponownie
  if (millis() - lastForcedSync >= forcedSyncInterval) {
    bool success = ntpConfig.forceUpdate();
    if (success) {
      Serial.println("Synchronizacja NTP udana. Następna za 24h.");
      forcedSyncInterval = ONE_DAY_MS;
    } else {
      Serial.println("Synchronizacja NTP nieudana. Ponowienie za 5 minut.");
      forcedSyncInterval = FIVE_MIN_MS;
    }
    lastForcedSync = millis();
  }

  // 3) Normalna aktualizacja klienta NTP (mniejszy narzut, może poprawiać czas co pewien czas)
  ntpConfig.update();

  // --------------------------------------------------------------------------
  // Poniżej reszta Twojego kodu loop:
  // --------------------------------------------------------------------------

  // Ustal aktualny czas w milisekundach
  static unsigned long lastSyncPrint = 0;
  unsigned long currentMillissync = millis();


  // Sprawdzenie połączenia Ethernet
  reconnectEthernet();

  // Obsługa nowych klientów HTTP
  EthernetClient client = server.available();
  if (client) {
    handleHttpRequest(client);
    client.stop();
  }

  // Co 1 sekunda – auto-off przekaźników
  if (millis() - lastAutoOffCheck >= 1000) {
    lastAutoOffCheck = millis();
    for (int i = 0; i < 4; i++) {
      if (relayStates[i] == "ON") {
        if (millis() - relayOnTime[i] >= autoOffTime) {
          digitalWrite(relayPins[i], LOW);
          relayStates[i] = "OFF";
          Serial.printf("Relay %d -> auto-OFF po %lu s\n", i, autoOffTime / 1000);
        }
      }
    }
  }

  // Logika automatycznego resetu wejść
  if (autoResetEnabled) {
    handleAutoResetLogic();
  }

  // Co 1 sekunda – odświeżanie wyświetlacza TFT
  if (millis() - lastDisplay >= 1000) {
    lastDisplay = millis();
    displayOnTFT();
  }

  // Co 5 sekund – sprawdzenie ponownej inicjalizacji Ethernet
  if (millis() - lastEthernetCheck >= 5000) {
    lastEthernetCheck = millis();
    reconnectEthernet();
  }

  // Co 5 sekund – wypisywanie czasu na Serial (debug)
  if (millis() - lastSyncPrint >= 60000) {
    lastSyncPrint = millis();
    String timeStr = ntpConfig.getFormattedTime();
    if (timeStr != "00:00:00") {
      Serial.print("Czas: ");
      Serial.println(timeStr);
      Serial.print("Data: ");
      Serial.println(ntpConfig.getFormattedDate());
    } else {
      Serial.println("Oczekiwanie na synchronizację czasu...");
    }
  }
  deviceUptime = millis() / 1000;


  //===========TEST WATCHDOGA SOFTWEROWEGO=============
  // testWatchdog();
  adjustFeedInterval();
  //printWatchdogStatus();

 // Serial.printf("[WDT] Aktualny interwał: %lums\n", WDT_FEED_INTERVAL);
  
}

//=========KONIEC LOOP===============================

void feedWatchdog() {
  if (hwWatchdogActive) {
    esp_err_t ret = esp_task_wdt_reset();
    wdtResets++;
   // Serial.println("[WDT] Wykryto reset z watchdoga!");
    if (ret != ESP_OK) {
   //   Serial.printf("Błąd karmienia HW Watchdoga: %d\n", ret);
      hwWatchdogActive = false;
    }
  }
 // Serial.println("Watchdog nakarmiony");
}


void testWatchdog() {
  if (millis() > 120000) { // Po 2 minutach
    Serial.println("TEST: Celowe zawieszenie...");
    while(1) {} // Zawieś program
  }
}

// 2. Poprawna wersja funkcji:
void adjustFeedInterval() {
  static uint32_t lastAdjustment = 0;
  
  if (millis() - lastAdjustment > 60000) {  // Co 60 sekund
    if (wdtResets > 0) {  // Jeśli były resetu z watchdoga
      // Użyj funkcji max() z prawidłowymi typami
      WDT_FEED_INTERVAL = max((uint32_t)3000, WDT_FEED_INTERVAL - 1000);
      Serial.printf("[WDT] Skracam interwał do: %lums\n", WDT_FEED_INTERVAL);
    }
    wdtResets = 0;  // Wyzeruj licznik
    lastAdjustment = millis();
  }
}

void printWatchdogStatus() {
  Serial.printf("[WDT] Status: %s | Ostatnie karmienie: %lums temu | ",
    hwWatchdogActive ? "AKTYWNY" : "NIEAKTYWNY",
   millis() - lastWatchdogFeed);
 Serial.printf("Następne za: %lums\n", 
    WDT_FEED_INTERVAL - (millis() - lastWatchdogFeed));
}

void sendCommonHtmlHeader(EthernetClient &client, const String &title) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE html>");
  client.println("<html><head>");
  client.println("<meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  client.println("<title>" + title + "</title>");
  client.println("<style>");

  // ====== MOTYW JASNY ======
  client.println("html, body { margin: 0; padding: 0; background: #f3f6fa; color: #222; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }");
  client.println("body { font-size: 0.98em; transition: background 0.2s, color 0.2s; }");
  client.println(".sidebar { position: fixed; top: 0; left: 0; width: 250px; height: 100vh; background: #24344d; padding: 24px 18px; box-sizing: border-box; }");
  client.println(".sidebar h2 { color: #fff; text-align: center; margin-bottom: 20px; font-size: 1.25em; letter-spacing: 1px; }");
  client.println(".sidebar a { display: block; color: #a9cdf9; text-decoration: none; margin-bottom: 13px; font-size: 1.07em; padding: 6px 0 6px 16px; border-radius: 6px; transition: background 0.17s, color 0.16s; }");
  client.println(".sidebar a.active, .sidebar a:hover { background: #3b4a61; color: #fff; }");
  client.println(".sidebar #themeToggle { margin-bottom: 20px; width: 95%; padding: 8px 0; border-radius: 7px; border: none; background: #121d2b; color: #ffef91; font-weight: bold; font-size:1em; letter-spacing: 1px; cursor:pointer; }");
  client.println(".main-content { margin-left: 250px; padding: 32px 24px 24px 24px; box-sizing: border-box; min-height: 100vh; background: #f3f6fa; color: #222; transition: background 0.2s, color 0.2s; }");
  client.println(".container { max-width: 680px; margin: 0 auto; background: #fff; padding: 18px 24px; border-radius: 10px; box-shadow: 0 3px 16px #b6c9f133; }");
  client.println("h2 { margin-top: 0; font-size: 1.25em; color: #1976d2; }");
  client.println(".status-message { padding: 10px; margin: 12px 0; text-align: center; border-left: 3px solid; font-size: 0.98em; background: #f3f8ff; border-radius: 6px; }");
  client.println(".status-normal { color: #00e676; border-left-color: #00e676; }");
  client.println(".status-warning { color: #ffb74d; border-left-color: #ffb74d; }");
  client.println(".status-error   { color: #ff5252; border-left-color: #ff5252; }");
  client.println("table { width: 100%; border-collapse: collapse; margin: 12px 0; font-size: 0.98em; }");
  client.println("td { padding: 11px 7px; vertical-align: middle; border-bottom: 1px solid #e4e8f0; }");
  client.println("tr:last-child td { border-bottom: none; }");
  client.println(".relay-on  { color: #43e03b; font-weight: bold; }");
  client.println(".relay-off { color: #ff5252; font-weight: bold; }");
  client.println(".timeCell  { text-align: right; width: 70px; color: #555; font-size: 0.9em; }");
  client.println("button { padding: 7px 18px; background: linear-gradient(120deg, #61b4ff 0%, #b4d9fa 100%); border: none; border-radius: 8px; color: #143370; font-weight: bold; font-size: 1em; cursor: pointer; margin: 3px; box-shadow: 0 2px 8px #a5d8fa22; transition: background 0.16s, box-shadow 0.16s, color 0.16s; }");
  client.println("button:hover { background: #61b4ff; color: #fff; box-shadow: 0 4px 14px #81d4fa44; }");
  client.println("button:active { background: #1976d2; color: #fff; }");
  client.println(".footer { text-align: center; color: #757575; margin-top: 24px; font-size: 0.91em; border-top: 1px solid #ccc; padding-top: 15px; }");
  client.println(".checkbox-label { display: flex; align-items: center; gap: 8px; background: #2196f3; padding: 6px 12px; border-radius: 5px; color: white; cursor: pointer; font-size: 0.95em; }");
  client.println(".checkbox-label input[type='checkbox'] { transform: scale(1.2); cursor: pointer; }");
  client.println(".checkbox-label.enabled { background: #388E3C; }");
  client.println(".checkbox-label.disabled { background: #F44336; }");
  client.println(".checkbox-label.disabled input[type='checkbox'] { cursor: not-allowed; }");

  // ====== MOTYW CIEMNY (NIGHT MODE) ======
  client.println("body.dark-mode, .dark-mode html { background: #191a23 !important; color: #eee !important; }");
  client.println(".dark-mode .sidebar { background: #10131a !important; }");
  client.println(".dark-mode .sidebar a { color: #93c1f9 !important; }");
  client.println(".dark-mode .sidebar h2 { color: #ffd43b; }");
  client.println(".dark-mode .main-content { background: #191a23 !important; color: #eee !important; }");
  client.println(".dark-mode .container { background: #23242c !important; color: #eee !important; box-shadow: 0 3px 16px #000a; }");
  client.println(".dark-mode button { background: linear-gradient(120deg,#263557 0%,#191a23 100%) !important; color: #ffd43b !important; }");
  client.println(".dark-mode button:hover, .dark-mode button:active { background: #1976d2 !important; color: #fff !important; }");
  client.println(".dark-mode .status-message { background: #222a !important; color: #e3ffb3 !important; }");
  client.println(".dark-mode .footer { color: #aaa; border-top: 1px solid #3d3d3d; }");
  client.println(".dark-mode input, .dark-mode select, .dark-mode textarea { background: #23242c; color: #fff; border: 1px solid #888; }");
  client.println(".dark-mode table { color: #eee; border-color: #444; }");
  client.println(".dark-mode td { border-bottom: 1px solid #363e44; }");

  client.println("</style>");
  client.println("</head><body>");

  // Sidebar z menu i przyciskiem zmiany motywu
  client.println("<div class='sidebar'>");
  // client.println("<button id='themeToggle'>Tryb ciemny</button>");
  client.println("<h2>Menu</h2>");
  client.println("<a href='/' class='active'>Strona główna</a>");
  client.println("<a href='/diagnostics'>Monitor systemu</a>");
  client.println("<a href='/diagnostyka'>Diagnostyka</a>");
  client.println("<a href='/watchdog'>Watchdog</a>");
  client.println("<a href='/inputs'>Wejścia</a>");
  client.println("<a href='/logs'>Logi</a>");
  client.println("<a href='/changeNet'>Sieć</a>");
  //client.println("<a href='/changePlace'>Zmiana miejsca</a>");
  client.println("<a href='/changeAllLinks'>Etykiety</a>");
  client.println("<a href='/settings'>Ustawienia</a>");
  client.println("<a href='/changeAuth'>Logowanie</a>");
  client.println("<a href='/updateFirmware'>Firmware</a>");
  client.println("<a href='/logBackups'>Backup</a>");
  client.println("<a href='/stats'>Statystyki</a>");
  client.println("<a href='/about'><b>O programie</b></a>");
  client.println("</div>");

  // Początek głównej zawartości
  client.println("<div class='main-content'>");
  client.println("<h3>" + title + "</h3>");

  // Skrypt do przełączania motywu
  client.println("<script>");
  client.println("const themeToggle = document.getElementById('themeToggle');");
  client.println("function setDarkMode(on) {");
  client.println("  if(on){ document.body.classList.add('dark-mode'); localStorage.setItem('theme', 'dark'); themeToggle.innerText = 'Tryb jasny'; }");
  client.println("  else { document.body.classList.remove('dark-mode'); localStorage.setItem('theme', 'light'); themeToggle.innerText = 'Tryb ciemny'; }");
  client.println("}");
  client.println("themeToggle.onclick = function() { setDarkMode(!document.body.classList.contains('dark-mode')); };");
  client.println("if(localStorage.getItem('theme') === 'dark') setDarkMode(true);");
  client.println("</script>");
}




void sendCommonHtmlFooter(EthernetClient &client) {
  //client.println("<p><a href='/' style='color:#0af;'><- powrót></a></p>");
  client.println("</div>");  // koniec .container
  client.println("</body></html>");
}


void sendTimeServerSettingsPage_GET(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Ustawienia serwera czasu");

  client.println("<form method='POST' action='/saveTimeServerSettings' style='max-width:440px; margin:auto;'>");

  client.println("<div style='margin-bottom:22px;'>");
  client.println("<label style='display:block; margin-bottom:6px;'>Adres IP serwera czasu:</label>");
  client.print("<input type='text' name='ntpAddress' value='");
  client.print(ntpServerAddress);
  client.println("' required style='width:100%; padding:10px; background:#222; color:#e0e0e0; border:none; border-radius:5px;'>");
  client.println("</div>");

  client.println("<div style='margin-bottom:28px;'>");
  client.println("<label style='display:block; margin-bottom:6px;'>Port serwera czasu:</label>");
  client.print("<input type='number' name='ntpPort' value='");
  client.print(ntpServerPort);
  client.println("' min='1' max='65535' required style='width:100%; padding:10px; background:#222; color:#e0e0e0; border:none; border-radius:5px;'>");
  client.println("</div>");

  client.println("<button type='submit' style='padding:10px 24px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Zapisz ustawienia</button>");
  client.println("</form>");

  client.println("<div style='margin-top:24px; text-align:center;'>");
  client.println("<a href='/' style='color:#4fc3f7;'>Powrót do strony głównej</a>");
  client.println("</div>");

  client.println("</div>");  // zamyka kontener

  sendCommonHtmlFooter(client);
}


void handleSaveTimeServerSettings_POST(EthernetClient &client, const String &body) {
  String newAddress = getParamValue(body, "ntpAddress");
  String portStr = getParamValue(body, "ntpPort");

  if (newAddress.length() == 0 || portStr.length() == 0) {
    sendSimplePanelMessage(client, "Błąd", "Adres lub port serwera NTP nie został podany.");
    return;
  }

  int newPort = portStr.toInt();
  if (newPort < 1 || newPort > 65535) {
    sendSimplePanelMessage(client, "Błąd", "Port spoza zakresu 1–65535.");
    return;
  }

  // ZAPIS do pamięci trwałej
  preferences.begin("ntp", false);
  preferences.putString("ntpAddress", newAddress);
  preferences.putInt("ntpPort", newPort);
  preferences.end();

  // Zmiana globalnych zmiennych (aktualna sesja)
  ntpServerAddress = newAddress;
  ntpServerPort = newPort;

  addLog("[INFO] Zmieniono ustawienia serwera czasu: " + newAddress + ":" + String(newPort));

  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /timeServerSettings");
  client.println("Connection: close");
  client.println();

  delay(200);
  ESP.restart();  // Po zmianie adresu NTP najlepiej wymusić restart
}


void diagnostykaEthernet() {
  Serial.println("\n--- DIAGNOSTYKA ETHERNET ---");
  Serial.println("Status linku: " + String(Ethernet.linkStatus() == LinkON ? "OK" : "BRAK"));
  Serial.print("Adres IP: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Brama: ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("DNS: ");
  Serial.println(Ethernet.dnsServerIP());
  Serial.print("Maska: ");
  Serial.println(Ethernet.subnetMask());
  Serial.println("----------------------------\n");
}


void handleClientConnection(EthernetClient &client) {
  Serial.println(F("Nowe połączenie HTTP"));

  String request = client.readStringUntil('\r');
  client.flush();

  // Routing żądań
  if (request.indexOf("GET / ") != -1) {
    sendDiagnosticPage(client);
  } else if (request.indexOf("GET /system.json") != -1) {
    sendSystemJSON(client);
  } else if (request.indexOf("GET /serial.txt") != -1) {
    sendSerialText(client);
  } else if (request.indexOf("GET /network.json") != -1) {
    sendNetworkJSON(client);
  } else if (request.indexOf("GET /command?") != -1) {
    handleCommandRequest(client, request);
  }

  delay(10);  // Krótkie opóźnienie dla stabilności
  client.stop();
  Serial.println(F("Połączenie zamknięte"));
}


void sendDiagnosticPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  sendMainContainerBegin(client, "Strona diagnostyczna");

  // --- AKTUALNY CZAS na górze pod nagłówkiem ---
  client.println("<div style='margin:0 0 18px 0; text-align:left;'>");
  client.print("<span style='font-size:1.08em; color:#4fc3f7; font-weight:500;'>Aktualny czas:</span> ");
  client.print("<span style='font-size:1.1em; color:#fff;'>");
  client.print(ntpConfig.getFormattedDate());
  client.print(" ");
  client.print(ntpConfig.getFormattedTime());
  client.println("</span>");
  client.println("</div>");

  // --- Panel systemowy ---
  client.println("<div class='panel' style='margin-bottom:0px; padding:-4px 8px -10px 8px;'>");
  client.println("<table style='width:100%; background:#23262a; color:#e0e0e0; border-radius:6px;'>");

  unsigned long uptimeSec = millis() / 1000UL;
  client.print("<tr><td>Czas pracy</td><td>");
  client.print(uptimeSec / 3600);
  client.print(" h ");
  client.print((uptimeSec % 3600) / 60);
  client.print(" m ");
  client.print(uptimeSec % 60);
  client.println(" s</td></tr>");

  client.print("<tr><td>Wersja FW</td><td>");
  client.print(firmwareVersion);
  client.println("</td></tr>");

  client.print("<tr><td>RAM dostępna</td><td>");
  client.print(ESP.getFreeHeap());
  client.println(" B</td></tr>");
  client.print("<tr><td>RAM min.</td><td>");
  client.print(ESP.getMinFreeHeap());
  client.println(" B</td></tr>");

  client.print("<tr><td>Wielkość Flash</td><td>");
  client.print(ESP.getFlashChipSize() / 1024);
  client.println(" KB</td></tr>");
  client.print("<tr><td>Tryb Flash</td><td>");
  client.print(ESP.getFlashChipMode() == FM_QIO ? "QIO" : ESP.getFlashChipMode() == FM_DIO ? "DIO"
                                                                                           : "Inny");
  client.println("</td></tr>");

  size_t total = SPIFFS.totalBytes(), used = SPIFFS.usedBytes();
  client.print("<tr><td>SPIFFS</td><td>");
  client.print(used / 1024);
  client.print(" KB / ");
  client.print(total / 1024);
  client.println(" KB</td></tr>");

  client.print("<tr><td>MAC Ethernet</td><td>");
  byte mac[6];
  Ethernet.MACAddress(mac);
  for (int i = 0; i < 6; i++) {
    if (i) client.print(":");
    client.print(mac[i], HEX);
  }
  client.println("</td></tr>");

  client.print("<tr><td>Adres IP</td><td>");
  client.print(Ethernet.localIP());
  client.println("</td></tr>");

  client.println("</table>");
  client.println("</div>");

  // --- Panel NTP – jedna linia ---
  client.println("<div class='panel' style='margin-bottom:4px; padding:16px 10px;'>");
  client.println("<div class='naglowek-panelu'>Synchronizacja czasu (NTP)</div>");
  client.println("<form method='GET' action='/ustawntp'>");

  // FLEX: Serwer, port, strefa, przycisk w 1 linii
  client.println("<div style='display:flex; gap:10px; flex-wrap:wrap; align-items:center;'>");

  // Serwer NTP
  client.println("<div style='flex:2; min-width:140px;'>");
  client.println("<label for='serwer' style='display:block; color:#bdbdbd; font-size:0.96em; margin-bottom:2px;'>Serwer NTP</label>");
  client.print("<input type='text' id='serwer' name='serwer' value='");
  client.print(ntpConfig.getCurrentServer());
  client.println("' required style='width:100%; padding:7px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  // Port
  client.println("<div style='flex:1; min-width:70px;'>");
  client.println("<label for='port' style='display:block; color:#bdbdbd; font-size:0.96em; margin-bottom:2px;'>Port</label>");
  client.print("<input type='number' id='port' name='port' value='");
  client.print(ntpConfig.getCurrentPort());
  client.println("' min='1' max='65535' required style='width:100%; padding:7px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("</div>");

  // Strefa czasowa
  client.println("<div style='flex:1.2; min-width:115px;'>");
  client.println("<label for='offset' style='display:block; color:#bdbdbd; font-size:0.96em; margin-bottom:2px;'>Strefa</label>");
  client.println("<select id='offset' name='offset' style='width:100%; padding:7px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  int offsets[] = { 0, 3600, 7200, 10800, -3600, -7200 };
  const char *labels[] = { "UTC+0", "UTC+1", "UTC+2", "UTC+3", "UTC-1", "UTC-2" };
  for (int i = 0; i < 6; i++) {
    client.print("<option value='");
    client.print(offsets[i]);
    client.print(ntpConfig.getCurrentTimezoneOffset() == offsets[i] ? "' selected>" : "'>");
    client.print(labels[i]);
    client.println("</option>");
  }
  client.println("</select>");
  client.println("</div>");

  // ZAPISZ
  client.println("<div style='flex:0.6; min-width:88px; margin-top:18px;'>");
  client.println("<button type='submit' style='padding:9px 14px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Zapisz</button>");
  client.println("</div>");

  client.println("</div>");  // flex
  client.println("</form>");
  client.println("</div>");

  // --- Ustawienia odświeżania ---
  client.println("<div style='margin: -10px 0 16px 0;'>");
  client.println("<div class='panel' style='margin-bottom:4px; padding:16px 10px;'>");  //bottom 14
  client.println("<div class='naglowek-panelu'>Ustawienia odświeżania stron</div>");
  client.println("<form method='POST' action='/setRefresh'>");
  client.println("<div style='display:flex; gap:10px; align-items:center; flex-wrap:wrap;'>");
  client.println("<label for='refreshTime' style='color:#bdbdbd; min-width:120px;'>Czas (sekundy):</label>");
  client.println("<input type='number' id='refreshTime' name='refreshTime' value='" + String(refreshInterval) + "' min='1' max='3600' style='width:85px; padding:7px; border-radius:6px; background:#23262a; color:#e0e0e0; border:1px solid #444;'>");
  client.println("<button type='submit' style='padding:9px 18px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer;'>Zapisz</button>");
  client.println("</div>");
  client.println("</form>");
  client.println("</div>");


  // --- Panel narzędzi ---
  // client.println("<div class='panel' style='margin-bottom:10px; padding:12px;'>");
  // client.println("<div class='naglowek-panelu'>Narzędzia</div>");
  // client.println("<script>");
  // client.println("function wykonajAkcje(akcja) {");
  // client.println("  if(akcja === 'restart' && !confirm('Na pewno zrestartować?')) return;");
  // client.println("  window.location.href = '/komenda?cmd=' + encodeURIComponent(akcja);");
  // client.println("}");
  // client.println("</script>");
  // client.println("<button style='padding:10px 20px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer; margin-right:10px;' onclick='location.reload()'>Odśwież</button>");
  // client.println("<button style='padding:10px 20px; background:#2196f3; color:#fff; border:none; border-radius:4px; cursor:pointer; margin-right:10px;' onclick='wykonajAkcje(\"restart\")'>Restart</button>");
  // client.println("</div>");

  client.println("</div>");  // zamknięcie .container z sendMainContainerBegin
  sendCommonHtmlFooter(client);
}

//==================================================================================//
//                                                                                  //
//==================================================================================//


// Funkcje pomocnicze
void sendHTTPHeaders(EthernetClient &client, const __FlashStringHelper *contentType) {
  client.println(F("HTTP/1.1 200 OK"));
  client.print(F("Content-Type: "));
  client.println(contentType);
  client.println(F("Connection: close"));
  client.println(F("Cache-Control: no-cache, no-store, must-revalidate"));
  client.println(F("Pragma: no-cache"));
  client.println(F("Expires: 0"));
  client.println();
}

void sendSystemJSON(EthernetClient &client) {
  sendHTTPHeaders(client, F("application/json"));

  client.print(F("{"));
  client.print(F("\"uptime\":"));
  client.print(millis() / 1000);
  client.print(F(","));
  client.print(F("\"freeMemory\":"));
  client.print(getFreeMemory());
  client.print(F(","));
  client.print(F("\"memoryUsage\":"));
  client.print(getMemoryUsage());
  client.println(F("}"));
}

void sendNetworkJSON(EthernetClient &client) {
  sendHTTPHeaders(client, F("application/json"));

  client.print(F("{"));
  client.print(F("\"ip\":\""));
  client.print(Ethernet.localIP());
  client.print(F("\","));
  client.print(F("\"mac\":\""));
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  client.print(macStr);
  client.print(F("\","));
  client.print(F("\"linkStatus\":"));
  client.print(Ethernet.linkStatus() == LinkON ? "true" : "false");
  client.println(F("}"));
}

void sendSerialText(EthernetClient &client) {
  sendHTTPHeaders(client, F("text/plain"));
  client.print(serialBuffer);
}

void handleCommandRequest(EthernetClient &client, String &request) {
  int cmdStart = request.indexOf("cmd=") + 4;
  int cmdEnd = request.indexOf(" ", cmdStart);
  String command = request.substring(cmdStart, cmdEnd);
  command.replace("+", " ");  // Dekodowanie spacji

  sendHTTPHeaders(client, F("text/plain"));

  if (command == "reboot") {
    client.println(F("System będzie restartowany..."));
    delay(1000);
    resetFunc();
  } else if (command == "clear") {
    serialBuffer = "";
    client.println(F("Bufor portu szeregowego wyczyszczony"));
  } else if (command.startsWith("ping")) {
    client.print(F("Ping do: "));
    client.println(command.substring(5));
    client.println(F("Odpowiedź: 64 bytes from "));
    client.println(command.substring(5));
    client.println(F("time=25ms TTL=57"));
  } else if (command == "help") {
    client.println(F("Dostępne komendy:"));
    client.println(F("- reboot - restart systemu"));
    client.println(F("- clear - wyczyść bufor portu"));
    client.println(F("- ping [host] - test połączenia"));
  } else {
    client.print(F("Nieznana komenda: "));
    client.println(command);
  }
}

int getMemoryUsage() {
  unsigned long freeMem = getFreeMemory();
  if (MEMORY_SIZE == 0) return 0;
  return 100 - ((freeMem * 100) / MEMORY_SIZE);
}


void addToSerialBuffer(const String &message) {
  serialBuffer += message + "\n";
  // Ogranicz bufor do SERIAL_BUFFER_SIZE znaków
  if (serialBuffer.length() > SERIAL_BUFFER_SIZE) {
    serialBuffer = serialBuffer.substring(serialBuffer.length() - SERIAL_BUFFER_SIZE);
  }
}

void clearSerialBuffer() {
  serialBuffer = "";
}

String pingHost(const String &host) {
  // Uproszczona implementacja ping (możesz użyć biblioteki ICMP)
  EthernetClient pingClient;
  if (pingClient.connect(host.c_str(), 80)) {
    pingClient.stop();
    return "Host " + host + " osiągalny (TCP)";
  } else {
    return "Brak odpowiedzi od " + host;
  }
}

void handleDiagnosticRequest(EthernetClient &client, const String &request) {
  if (request.indexOf("GET /diagnostyka ") == 0) {
    sendDiagnosticPage(client);
  } else if (request.indexOf("GET /serialData ") == 0) {
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println(getSerialBuffer());
  } else if (request.indexOf("GET /ping?host=") == 0) {
    int start = request.indexOf("host=") + 5;
    int end = request.indexOf(" ", start);
    String host = request.substring(start, end);
    host.replace("%20", " ");  // Dekodowanie URL

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println("Connection: close");
    client.println();
    client.println(pingHost(host));
  }
}

void checkSerialBufferSize() {
  if (serialBuffer.length() > SERIAL_BUFFER_SIZE * 0.9) {
    serialBuffer = serialBuffer.substring(serialBuffer.length() - SERIAL_BUFFER_SIZE / 2);
  }
}

void addFilteredToSerialBuffer(const String &message) {
  if (message.indexOf("[DEBUG]") != -1) return;  // Ignoruj wiadomości debug
  addToSerialBuffer(message);
}

void handleDownloadLogs(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Content-Disposition: attachment; filename=serial_log.txt");
  client.println("Connection: close");
  client.println();
  client.println(getSerialBuffer());
}


// Funkcje pomocnicze
unsigned long getFreeMemory() {
#if defined(__AVR__)
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
#elif defined(ESP8266)
  return ESP.getFreeHeap();
#elif defined(ESP32)
  return esp_get_free_heap_size();
#else
  return 0;
#endif
}


String getSerialBuffer() {
  // Implementacja zależna od platformy
  String buffer;
  while (Serial.available()) {
    buffer += (char)Serial.read();
  }
  return buffer.substring(0, 1000);  // Ogranicz do 1000 znaków
}

void handleSystemData(EthernetClient &client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Connection: close"));
  client.println();

  client.print(F("{\"uptime\":"));
  client.print(millis() / 1000);
  client.print(F(",\"freeMemory\":"));
  client.print(getFreeMemory());
  client.print(F(",\"memoryUsage\":"));
  client.print(getMemoryUsage());
  client.println(F("}"));
}

void handleSerialData(EthernetClient &client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/plain"));
  client.println(F("Connection: close"));
  client.println();

  client.print(getSerialBuffer());
}

void handleCommand(EthernetClient &client, String cmd) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/plain"));
  client.println(F("Connection: close"));
  client.println();

  if (cmd == "reboot") {
    client.println(F("System będzie restartowany..."));
    delay(1000);
    resetFunc();
  } else if (cmd == "clear") {
    // Implementacja czyszczenia bufora
    client.println(F("Bufor wyczyszczony"));
  } else if (cmd.startsWith("ping")) {
    // Implementacja ping
    client.println(F("Ping: "));
    client.println(cmd.substring(5));
    client.println(F("Odpowiedź: OK (symulacja)"));
  } else {
    client.println(F("Nieznana komenda: "));
    client.println(cmd);
  }
}

void logUpdateError(const String &reason) {
  File logFile = SPIFFS.open("/update.log", "a");
  if (logFile) {
    logFile.printf("Nieudana aktualizacja: %s\n", reason.c_str());
    logFile.close();
  }
}

void handleDownloadLogBackup(EthernetClient &client, const String &fileParam) {
  String filePath = fileParam;
  if (!filePath.startsWith("/")) filePath = "/" + filePath;
  File file = SPIFFS.open(filePath, FILE_READ);
  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("Nie znaleziono pliku!");
    return;
  }
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.print("Content-Disposition: attachment; filename=\"");
  client.print(filePath.substring(1));
  client.println("\"");
  client.println("Connection: close");
  client.println();
  while (file.available()) {
    client.write(file.read());
  }
  file.close();
}


void sendLogBackupViewPage(EthernetClient &client, const String &fileName) {
  String cleanFileName = fileName;
  int spaceIdx = cleanFileName.indexOf(' ');
  if (spaceIdx > 0) cleanFileName = cleanFileName.substring(0, spaceIdx);
  if (cleanFileName.startsWith("/")) cleanFileName = cleanFileName.substring(1);

  std::vector<String> backups = getSortedLogBackups();
  for (auto &f : backups)
    if (f.startsWith("/")) f = f.substring(1);

  int currentIndex = -1;
  for (int i = 0; i < backups.size(); ++i) {
    if (backups[i] == cleanFileName) {
      currentIndex = i;
      break;
    }
  }
  String path = "/" + cleanFileName;

  sendCommonHtmlHeader(client, "");

  // --- Pasek nawigacji ---
  client.println("<div style='margin-bottom:16px; display:flex; gap:10px; flex-wrap:wrap;'>");
  client.println("<a href='/logBackups' style='padding:7px 15px; background:#1976d2; color:#fff; border-radius:5px; text-decoration:none;'>← Wszystkie kopie logów</a>");
  client.println("<a href='/logs' style='padding:7px 15px; background:#4fc3f7; color:#222; border-radius:5px; text-decoration:none;'>Logi główne</a>");
  if (currentIndex > 0) {
    client.println("<a href='/viewLogBackup?file=" + backups[currentIndex - 1] + "' style='padding:7px 15px; background:#b4d9fa; color:#143370; border-radius:5px; text-decoration:none;'>⟵ Poprzedni</a>");
  } else {
    client.println("<span style='padding:7px 15px; background:#ddd; color:#aaa; border-radius:5px;'>⟵ Poprzedni</span>");
  }
  if (currentIndex >= 0 && currentIndex < (int)backups.size() - 1) {
    client.println("<a href='/viewLogBackup?file=" + backups[currentIndex + 1] + "' style='padding:7px 15px; background:#b4d9fa; color:#143370; border-radius:5px; text-decoration:none;'>Następny ⟶</a>");
  } else {
    client.println("<span style='padding:7px 15px; background:#ddd; color:#aaa; border-radius:5px;'>Następny ⟶</span>");
  }
  if (!backups.empty() && currentIndex != backups.size() - 1) {
    client.println("<a href='/viewLogBackup?file=" + backups.back() + "' style='padding:7px 15px; background:#ffd43b; color:#222; border-radius:5px; text-decoration:none;'>Ostatni</a>");
  } else {
    client.println("<span style='padding:7px 15px; background:#ffd43b55; color:#aaa; border-radius:5px;'>Ostatni</span>");
  }
  client.println("</div>");

  // Nazwa i Download
  client.println("<div class='container' style='max-width:900px; margin:auto;'>");
  client.println("<h2 style='margin-top:0; font-size:1.4em; word-break:break-all;'>Zawartość kopii: <span style='color:#1976d2;'>" + cleanFileName + "</span></h2>");
  client.println("<a href='/downloadLogBackup?file=" + cleanFileName + "' style='display:inline-block;margin-bottom:16px;padding:7px 15px;background:#25cc69;color:#fff;border-radius:5px;text-decoration:none;font-size:1em;'>Pobierz plik</a>");

  // --- Zawartość loga ---
  File file = SPIFFS.open(path, FILE_READ);
  if (!file || file.size() == 0) {
    client.println("<p style='color:red; font-size:1.2em;'>Błąd otwarcia pliku lub plik pusty!</p>");
    Serial.println("[DEBUG][sendLogBackupViewPage] Błąd otwarcia pliku lub plik pusty! path=" + path);
  } else {
    // Zwracamy zawartość pliku w stylowym pre
    client.println("<pre style='max-height:450px; min-height:140px; overflow-y:auto; background:#181a1b; color:#e0e0e0; padding:20px 18px; border-radius:8px; font-size:1em; font-family:Consolas,monospace; border:1px solid #263b4f; box-shadow:0 2px 16px #1976d220;'>");
    while (file.available()) {
      String line = file.readStringUntil('\n');
      client.print(line);
    }
    client.println("</pre>");
    file.close();
  }

  client.println("</div>");
  sendCommonHtmlFooter(client);
}



void handleDownloadFile(EthernetClient &client, const String &fileName) {
  File file = SPIFFS.open(fileName, FILE_READ);
  if (!file) {
    sendNotFound(client);
    return;
  }
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Content-Disposition: attachment; filename=\"" + fileName.substring(1) + "\"");
  client.println("Connection: close");
  client.println();
  while (file.available()) {
    client.write(file.read());
  }
  file.close();
}

void handleDownloadLogBackup_GET(EthernetClient &client, const String &fileParamRaw) {
  String filename = fileParamRaw;
  filename.trim();
  int sp = filename.indexOf(' ');
  if (sp >= 0) filename = filename.substring(0, sp);
  if (!filename.startsWith("/")) filename = "/" + filename;

  File file = SPIFFS.open(filename, FILE_READ);
  if (!file) {
    client.println("HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n");
    client.println("<h2>Błąd: plik nie istnieje!</h2>");
    return;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.println("Content-Disposition: attachment; filename=\"" + filename.substring(1) + "\"");
  client.println();

  while (file.available()) client.write(file.read());
  file.close();
}



// Obsługa POST kasowania pliku (działa dla backup, log.txt, login_history.txt itd.)
void handleDeleteLogBackup_POST(EthernetClient &client, const String &body) {
  String fname = getParamValue(body, "file");
  fname = urlDecode(fname);  // Najważniejsze!
  if (!fname.startsWith("/")) fname = "/" + fname;
  if (SPIFFS.exists(fname)) {
    if (SPIFFS.remove(fname)) {
      addLog("[INFO] Usunięto plik logów: " + fname);
    } else {
      addLog("[WARN] Błąd usuwania pliku: " + fname);
    }
  } else {
    addLog("[WARN] Próba usunięcia nieistniejącego pliku: " + fname);
  }
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /logBackups");
  client.println();
}


void sendLogBackupsPage_GET(EthernetClient &client) {

  trimFileIfTooBig("/log.txt");
  trimFileIfTooBig("/login_history.txt");
  trimFileIfTooBig("/errors.log");

  Serial.println("[DEBUG] Wejście do sendLogBackupsPage_GET");
  sendCommonHtmlHeader(client, "");
  client.println("<div class='container' style='max-width:850px; margin:auto; background:#20232a; padding:34px 28px 28px 28px; border-radius:18px; box-shadow:0 2px 14px #111c; color:#e0e0e0;'>");
  client.println("<h2 style='margin-top:0;'>Kopie logów (backupy)</h2>");

  client.println("<div style='margin-bottom:18px;display:flex;gap:10px;flex-wrap:wrap;'>");
  client.println("<a href='/logs' style='padding:7px 18px;background:#4fc3f7;color:#222;border-radius:5px;text-decoration:none;'>Logi główne</a>");
  client.println("</div>");

  client.println("<table style='width:100%; border-collapse:collapse; background:#23262a;'>");
  client.println("<tr style='background:#2c323c;'><th style='padding:8px;'>Nazwa pliku</th><th style='padding:8px;'>Podgląd</th><th style='padding:8px;'>Pobierz</th><th style='padding:8px;'>Usuń</th><th style='padding:8px;'>Zmień nazwę</th></tr>");




  // --- Wylistuj pliki backupów logów ---
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  bool found = false;
  while (file) {
    String name = file.name();
    Serial.print("[DEBUG] Nazwa pliku: '");
    Serial.print(name);
    Serial.println("'");
    if (name.indexOf("log_backup_") >= 0 && name.endsWith(".txt")) {
      found = true;
      client.println("<tr>");

      // Nazwa pliku
      client.print("<td style='padding:8px;word-break:break-all;'>");
      client.print(name);
      client.println("</td>");
      // Podgląd
      client.print("<td style='padding:8px; text-align:center;'><a href='/viewLogBackup?file=");
      client.print(name);
      client.println("' style='color:#2196f3;text-decoration:underline;'>Podgląd</a></td>");
      // Pobierz
      client.print("<td style='padding:8px; text-align:center;'><a href='/downloadLogBackup?file=");
      client.print(name);
      client.println("' style='color:#25cc69;text-decoration:underline;'>Pobierz</a></td>");
      // Usuń
      client.print("<td style='padding:8px; text-align:center;'>");
      client.print("<form method='POST' action='/deleteLogBackup' style='display:inline;' onsubmit='return confirm(\"Na pewno usunąć kopię?\");'>");
      client.print("<input type='hidden' name='file' value=\"" + name + "\">");
      client.print("<button type='submit' style='background:#d32f2f;color:#fff;border:none;border-radius:4px;padding:6px 14px;cursor:pointer;'>Usuń</button>");
      client.print("</form>");
      client.print("</td>");
      // Zmień nazwę
      client.print("<td style='padding:8px; text-align:center;'>");
      client.print("<form method='POST' action='/renameLogBackup' style='display:inline;'>");
      client.print("<input type='hidden' name='oldfile' value=\"" + name + "\">");
      client.print("<input type='text' name='newfile' placeholder='Nowa nazwa' style='width:110px;padding:4px 6px;border-radius:4px;border:none;background:#2a3440;color:#eee;'>");
      client.print("<button type='submit' style='background:#1976d2;color:#fff;border:none;border-radius:4px;padding:5px 10px;cursor:pointer;margin-left:4px;'>Zmień</button>");
      client.print("</form>");
      client.println("</td>");
      client.println("</tr>");
    }
    file = root.openNextFile();
  }
  root.close();

  const char *extraFiles[] = { "/log.txt", "/login_history.txt", "/errors.log" };
  for (int i = 0; i < 3; i++) {
    String name = extraFiles[i];
    if (SPIFFS.exists(name)) {
      client.println("<tr>");
      // Nazwa pliku
      client.print("<td style='padding:8px;word-break:break-all;'>");
      client.print(name.substring(1));  // bez ukośnika
      client.println("</td>");
      // Podgląd
      client.print("<td style='padding:8px; text-align:center;'><a href='/viewLogBackup?file=");
      client.print(name.substring(1));
      client.println("' style='color:#2196f3;text-decoration:underline;'>Podgląd</a></td>");
      // Pobierz
      client.print("<td style='padding:8px; text-align:center;'><a href='/downloadLogBackup?file=");
      client.print(name.substring(1));
      client.println("' style='color:#25cc69;text-decoration:underline;'>Pobierz</a></td>");
      // Usuń
      client.print("<td style='padding:8px; text-align:center;'>");
      client.print("<form method='POST' action='/deleteTxtFile' style='display:inline;' onsubmit='return confirm(\"Na pewno usunąć plik?");
      client.print(name.substring(1));
      client.print("\");'>");
      client.print("<input type='hidden' name='file' value=\"" + name + "\">");
      client.print("<button type='submit' style='background:#d32f2f;color:#fff;border:none;border-radius:4px;padding:6px 14px;cursor:pointer;'>Usuń</button>");
      client.print("</form>");
      client.print("</td>");
      // Zmień nazwę – nie pozwalamy na zmianę (wyłączone)
      client.print("<td style='padding:8px; text-align:center;'>-</td>");
      client.println("</tr>");
      found = true;
    }
  }


  if (!found) {
    client.println("<tr><td colspan='5' style='padding:18px;text-align:center;color:#aaa;'>Brak kopii logów.</td></tr>");
  }
  client.println("</table>");

  client.println("</div>");
  sendCommonHtmlFooter(client);
}




void handleRenameLogBackup_POST(EthernetClient &client, const String &body) {
  String oldFile = getParamValue(body, "oldfile");
  String newFile = getParamValue(body, "newfile");
  if (SPIFFS.exists(oldFile) && newFile.length() > 4 && newFile.endsWith(".txt")) {
    SPIFFS.rename(oldFile, "/" + newFile);  // nowa nazwa zawsze zaczyna się od '/'
  }
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /logBackups");
  client.println("Connection: close");
  client.println();
}


void handleDownloadLogFile(EthernetClient &client, const String &filePath) {
  File file = SPIFFS.open(filePath, FILE_READ);
  if (!file) {
    client.println("HTTP/1.1 404 Not Found");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("Nie znaleziono pliku!");
    return;
  }
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/plain");
  client.print("Content-Disposition: attachment; filename=\"");
  client.print(filePath.substring(1));  // "log.txt"
  client.println("\"");
  client.println("Connection: close");
  client.println();
  while (file.available()) {
    client.write(file.read());
  }
  file.close();
}

void setRelay(int idx, bool on) {
  digitalWrite(relayPins[idx], on ? HIGH : LOW);
  relayStates[idx] = on ? "ON" : "OFF";
  if (on) relayOnTime[idx] = millis();
}

void sendPanelDiagnostyczny(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");
  // TUTAJ NIE DAJEMY META REFRESH!

  client.println("<div style='background:#20232a; color:#eee; padding:18px; border-radius:14px; max-width:820px; margin:40px auto; font-family:Segoe UI,Arial,sans-serif;'>");
  client.println("<h2 style='color:#4fc3f7;'>Monitor systemu (AJAX)</h2>");
  // Miejsce na dynamicznie odświeżane dane:
  client.println("<div id='diagnostic-content'>Ładowanie danych...</div>");

  // Skrypt AJAX do automatycznego pobierania danych:
  client.println(R"rawliteral(
<script>
function refreshDiagnosticContent() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("diagnostic-content").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/diagnostics_data", true);
  xhttp.send();
}
setInterval(refreshDiagnosticContent, 3000);
refreshDiagnosticContent();
</script>
)rawliteral");

  client.println("</div>");
  sendCommonHtmlFooter(client);
}


void sendInputsPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");

  // Kontener środkowy z maksymalną szerokością jak reszta paneli
  client.println("<div class='container' style='max-width:820px; margin:auto; background:#20232a; padding:34px 28px 28px 28px; border-radius:18px; box-shadow:0 2px 14px #111c; color:#e0e0e0;'>");

  client.println("<h2 style='margin-top:0;'>Panel wejść cyfrowych</h2>");
  client.println("<div id='inputs-content' style='min-height:120px; margin-bottom:18px;'>Ładowanie...</div>");
  client.println("<button onclick='reloadInputs()' style='padding:10px 26px; background:#2196f3; color:#fff; border:none; border-radius:7px; font-weight:bold; cursor:pointer;'>Odśwież</button>");
  client.println("</div>");

  client.println("<script>");
  client.println("function reloadInputs() {");
  client.println("  fetch('/inputs_data').then(r=>r.text()).then(t=>{ document.getElementById('inputs-content').innerHTML = t; });");
  client.println("}");
  client.println("window.onload = reloadInputs;");
  client.println("</script>");

  sendCommonHtmlFooter(client);
}



void sendAuthPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "Autoryzacja");

  // Nowoczesny styl kontenera
  client.println("<div class='container' style='max-width:480px; margin:auto; background:#20232a; padding:38px 28px 34px 28px; border-radius:18px; box-shadow:0 2px 14px #111c; color:#e0e0e0;'>");
  client.println("<h2 style='margin-top:0;'>Zmiana danych autoryzacji</h2>");
  client.println("<div id='ajax-content'><p>Ładowanie danych...</p></div>");
  client.println("</div>");

  client.println("<script>");
  client.println("function loadSection(url) {");
  client.println("  const box = document.getElementById('ajax-content');");
  client.println("  box.innerHTML = '<p style=\"text-align:center;color:#4fc3f7;font-size:1.1em;\">Ładowanie danych...</p>';");  // loader
  client.println("  fetch(url).then(r => r.text()).then(data => { box.innerHTML = data; });");
  client.println("}");
  client.println("document.addEventListener('DOMContentLoaded', function(){ loadSection(window.location.pathname + '_data'); });");
  client.println("</script>");

  sendCommonHtmlFooter(client);
}



void sendLogBackupsPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "");

  client.println("<div class='container' style='max-width:720px; margin:auto; background:#20232a; padding:38px 28px 34px 28px; border-radius:18px; box-shadow:0 2px 14px #111c; color:#e0e0e0;'>");
  client.println("<h2 style='margin-top:0;'>Kopie logów (backupy)</h2>");
  client.println("<div id='ajax-content'><p>Ładowanie danych...</p></div>");
  client.println("</div>");

  client.println("<script>");
  client.println("function loadSection(url) {");
  client.println("  const box = document.getElementById('ajax-content');");
  client.println("  box.innerHTML = '<p style=\"text-align:center;color:#4fc3f7;font-size:1.1em;\">Ładowanie danych...</p>';");  // loader
  client.println("  fetch(url).then(r => r.text()).then(data => { box.innerHTML = data; });");
  client.println("}");
  client.println("document.addEventListener('DOMContentLoaded', function(){ loadSection(window.location.pathname + '_data'); });");
  client.println("</script>");

  sendCommonHtmlFooter(client);
}


void sendSettingsPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "Ustawienia czasów");

  client.println("<div class='container' style='max-width:660px; margin:auto; background:#20232a; padding:40px 32px 34px 32px; border-radius:18px; box-shadow:0 2px 14px #111c; color:#e0e0e0;'>");
  client.println("<h2 style='margin-top:0;'>Ustawienia czasów</h2>");
  client.println("<div id='ajax-content'><p style=\"text-align:center;color:#4fc3f7;font-size:1.1em;\">Ładowanie danych...</p></div>");
  client.println("</div>");

  client.println("<script>");
  client.println("function loadSection(url) {");
  client.println("  const box = document.getElementById('ajax-content');");
  client.println("  box.innerHTML = '<p style=\"text-align:center;color:#4fc3f7;font-size:1.1em;\">Ładowanie danych...</p>';");  // ładniejszy loader
  client.println("  fetch(url).then(r => r.text()).then(data => { box.innerHTML = data; });");
  client.println("}");
  client.println("document.addEventListener('DOMContentLoaded', function(){ loadSection('/settings_data'); });");
  client.println("</script>");

  sendCommonHtmlFooter(client);
}



void sendChangeNetData(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  client.println("<h2>Zmiana ustawień sieci</h2>");
  client.println("<form method='POST' action='/saveChangeNet'>");
  client.println("<label>Adres IP: <input type='text' name='ip' value='192.168.1.100'></label><br>");
  client.println("<label>Maska: <input type='text' name='mask' value='255.255.255.0'></label><br>");
  client.println("<label>Bramka: <input type='text' name='gw' value='192.168.1.1'></label><br>");
  client.println("<button type='submit'>Zapisz</button>");
  client.println("</form>");
}
void sendChangePlacePage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "Zmiana miejsca");

  client.println("<div class='container' style='max-width:540px; margin:auto; background:#20232a; padding:40px 32px 32px 32px; border-radius:16px; box-shadow:0 2px 14px #111c; color:#e0e0e0;'>");
  client.println("<h2 style='margin-top:0;'>Zmiana miejsca</h2>");
  client.println("<div id='ajax-content'><p style=\"text-align:center;color:#4fc3f7;font-size:1.1em;\">Ładowanie danych...</p></div>");
  client.println("</div>");

  client.println("<script>");
  client.println("function loadSection(url) {");
  client.println("  const box = document.getElementById('ajax-content');");
  client.println("  box.innerHTML = '<p style=\"text-align:center;color:#4fc3f7;font-size:1.1em;\">Ładowanie danych...</p>';");  // loader
  client.println("  fetch(url).then(r => r.text()).then(data => { box.innerHTML = data; });");
  client.println("}");
  client.println("document.addEventListener('DOMContentLoaded', function(){ loadSection(window.location.pathname + '_data'); });");
  client.println("</script>");

  sendCommonHtmlFooter(client);
}
void sendChangeAllLinksPage(EthernetClient &client) {
  sendCommonHtmlHeader(client, "Zmiana etykiet");
  client.println("<div class='container' style='max-width:600px; margin:auto; background:#20232a; padding:40px 32px 32px 32px; border-radius:16px; box-shadow:0 2px 14px #111c; color:#e0e0e0;'>");
  client.println("<h2 style='margin-top:0;'>Zmiana etykiet przekaźników</h2>");
  client.println("<div id='ajax-content'><p style=\"text-align:center;color:#4fc3f7;font-size:1.1em;\">Ładowanie danych...</p></div>");
  client.println("</div>");
  client.println("<script>");
  client.println("function loadSection(url) {");
  client.println("  const box = document.getElementById('ajax-content');");
  client.println("  box.innerHTML = '<p style=\"text-align:center;color:#4fc3f7;font-size:1.1em;\">Ładowanie danych...</p>';");  // loader
  client.println("  fetch(url).then(r => r.text()).then(data => { box.innerHTML = data; });");
  client.println("}");
  client.println("document.addEventListener('DOMContentLoaded', function(){ loadSection(window.location.pathname + '_data'); });");
  client.println("</script>");
  sendCommonHtmlFooter(client);
}
void monitorInputsAndRelays() {
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  // Aktualizuj co sekundę (lub dostosuj interwał do swoich potrzeb)
  if (now - lastUpdate >= 1000) {
    lastUpdate = now;

    // ----------- WEJŚCIA -----------
    for (int i = 0; i < NUM_INPUTS; i++) {
      int currentState = digitalRead(inputPins[i]);

      // Licz zmiany stanu
      if (currentState != prevInputStates[i]) {
        inputCycles[i]++;
        inputLastChange[i] = now;

        if (prevInputStates[i] == LOW && currentState == HIGH) {
          inputRisingEdges[i]++;
        }
        if (prevInputStates[i] == HIGH && currentState == LOW) {
          inputFallingEdges[i]++;
        }

        prevInputStates[i] = currentState;
      }

      // Licz czas aktywności i nieaktywności (w sekundach)
      if (currentState == HIGH) {
        inputActiveTime[i]++;
      } else {
        inputInactiveTime[i]++;
      }
    }

    // ----------- PRZEKAŹNIKI -----------
    for (int i = 0; i < NUM_RELAYS; i++) {
      int currentState = digitalRead(relayPins[i]);  // lub z tablicy stanów, jeśli nie używasz fizycznego odczytu

      // Licz zmiany stanu (cykle)
      if (currentState != prevRelayStates[i]) {
        relayCycles[i]++;
        relayLastChange[i] = now;

        prevRelayStates[i] = currentState;
      }

      // Licz czas aktywności i nieaktywności (w sekundach)
      if (currentState == HIGH) {
        relayActiveTime[i]++;
      } else {
        relayInactiveTime[i]++;
      }
    }
  }
}
String getCurrentTimeString() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(buf);
}
const char *daysOfTheWeek[7] = { "Niedz", "Pon", "Wt", "Śr", "Czw", "Pt", "Sob" };

void logLoginAttemptToFile(const String &ip, bool success) {
  if (!enableLogFiles) return;  // Nie zapisuj jeśli flaga wyłączona!
  File file = SPIFFS.open("/login_history.txt", FILE_APPEND);
  if (file) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    char buf[40];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    String line = String(buf) + "|";
    line += daysOfTheWeek[timeinfo.tm_wday];
    line += "|admin|";  // zmień, jeśli masz różnych użytkowników!
    line += ip + "|";
    line += (success ? "OK" : "FAIL");
    line += "\n";
    file.print(line);
    logLoginAttemptToFile(ip, success);
    backupAndClearLoginHistoryIfTooBig();
    file.close();
  }
}
void resetAllStats() {
  // Przekaźniki
  for (int i = 0; i < NUM_RELAYS; i++) {
    relayCycles[i] = 0;
    relayActiveTime[i] = 0;
    relayInactiveTime[i] = 0;
    relayLastChange[i] = 0;
  }
  // Wejścia
  for (int i = 0; i < NUM_INPUTS; i++) {
    inputCycles[i] = 0;
    inputRisingEdges[i] = 0;
    inputFallingEdges[i] = 0;
    inputActiveTime[i] = 0;
    inputInactiveTime[i] = 0;
    inputLastChange[i] = 0;
  }
  // Statystyki logowań
  loginAttempts = 0;
  failedLoginAttempts = 0;
  lastLoginTime = "-";
  lastLoginIP = "-";
  // Dodatkowe statystyki? Dodaj tutaj!
}
///////////////////////--------------------------------------------------------------AJAX-----------
void handleResetStats_POST(EthernetClient &client) {
  // Wyzeruj statystyki, liczniki, czasy, logowania itp.
  // Przykład:
  for (int i = 0; i < NUM_RELAYS; i++) {
    relayCycles[i] = 0;
    relayActiveTime[i] = 0;
  }
  for (int i = 0; i < NUM_INPUTS; i++) {
    inputCycles[i] = 0;
    inputActiveTime[i] = 0;
  }
  loginAttempts = 0;
  failedLoginAttempts = 0;
  // Dodatkowo zapisz do Preferences/SPIFFS jeśli trzeba
  client.println("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK");
}
void handleResetRestarts_POST(EthernetClient &client) {
  deviceRestarts = 0;
  // preferences.putUInt("restarts", 0); // jeżeli masz zapis w Preferences
  client.println("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nOK");
}
void sendStatsApiJson(EthernetClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json; charset=UTF-8");
  client.println("Connection: close");
  client.println();

  client.print("{");

  client.print("\"deviceUptime\":");
  client.print(deviceUptime);
  client.print(",");
  client.print("\"totalUptime\":");
  client.print(totalUptime);
  client.print(",");
  client.print("\"deviceRestarts\":");
  client.print(deviceRestarts);
  client.print(",");
  client.print("\"lastRestartEpoch\":");
  client.print(lastRestartEpoch);
  client.print(",");

  // Przekaźniki
  client.print("\"relays\":[");
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (i != 0) client.print(",");
    client.print("{\"name\":\"");
    client.print(relayNames[i]);
    client.print("\",\"cycles\":");
    client.print(relayCycles[i]);
    client.print(",\"active\":");
    client.print(relayActiveTime[i]);
    client.print("}");
  }
  client.print("],");

  // Wejścia
  client.print("\"inputs\":[");
  for (int i = 0; i < NUM_INPUTS; i++) {
    if (i != 0) client.print(",");
    client.print("{\"name\":\"");
    client.print(inputLabels[i]);
    client.print("\",\"cycles\":");
    client.print(inputCycles[i]);
    client.print(",\"active\":");
    client.print(inputActiveTime[i]);
    client.print("}");
  }
  client.print("],");

  // Statystyki logowań
  client.print("\"loginAttempts\":");
  client.print(loginAttempts);
  client.print(",");
  client.print("\"failedLoginAttempts\":");
  client.print(failedLoginAttempts);
  client.print(",");
  client.print("\"lastLoginTime\":\"");
  client.print(lastLoginTime);
  client.print("\",");
  client.print("\"lastLoginIP\":\"");
  client.print(lastLoginIP);
  client.print("\",");

  // Info system/sieć
  client.print("\"firmwareVersion\":\"");
  client.print(firmwareVersion);
  client.print("\",");
  client.print("\"firmwareDescription\":\"");
  client.print(firmwareDescription);
  client.print("\",");
  client.print("\"mac\":\"");
  client.print(formatMac(mac));
  client.print("\",");
  client.print("\"ip\":\"");
  client.print(ipToString(Ethernet.localIP()));
  client.print("\",");
  client.print("\"subnet\":\"");
  client.print(ipToString(Ethernet.subnetMask()));
  client.print("\",");
  client.print("\"gateway\":\"");
  client.print(ipToString(Ethernet.gatewayIP()));
  client.print("\",");
  client.print("\"dns\":\"");
  client.print(ipToString(Ethernet.dnsServerIP()));
  client.print("\"");

  client.print("}");
  client.println();
}

// Zwraca posortowaną listę backupów logów (najstarsze pierwsze)
std::vector<String> getSortedLogBackups() {
  std::vector<String> backups;
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    if (name.indexOf("log_backup_") >= 0 && name.endsWith(".txt")) {
      backups.push_back(name);
    }
    file = root.openNextFile();
  }
  // Sortujemy po nazwie rosnąco
  std::sort(backups.begin(), backups.end());
  return backups;
}


bool extractBackupNumber(const String &filename, long &number) {
  int lastUnderscore = filename.lastIndexOf('_');
  int dot = filename.lastIndexOf('.');
  if (lastUnderscore == -1 || dot == -1 || dot <= lastUnderscore) {
    return false;
  }
  String numStr = filename.substring(lastUnderscore + 1, dot);
  char *endptr;
  number = strtol(numStr.c_str(), &endptr, 10);
  return (endptr != numStr.c_str() && *endptr == '\0');
}

void backupAndClearLogIfTooBig(size_t maxSize) {
  File f = SPIFFS.open("/log.txt", FILE_READ);
  if (!f) return;
  size_t sz = f.size();
  f.close();
  if (sz > maxSize) {
    String backupName = createLogsBackup();
    cleanupOldLogBackups(MAX_LOG_BACKUPS);  // Ogranicza ilość do 10
    if (backupName != "") {
      cleanupOldLogBackups(MAX_LOG_BACKUPS);
      File log = SPIFFS.open("/log.txt", FILE_WRITE);
      if (log) log.close();  // Czyści plik
      Serial.println("[INFO] Automatyczne czyszczenie logów po przekroczeniu " + String(maxSize / 1024) + " KB (kopia: " + backupName + ")");
    }
  }
}

long extractTimestampFromFilename(const String &filename) {
  if (filename.indexOf("NOSYNC_") > 0) {
    int start = filename.lastIndexOf('_') + 1;
    int end = filename.lastIndexOf('.');
    if (start > 0 && end > start) {
      String numStr = filename.substring(start, end);
      return numStr.toInt();  // millis jako timestamp
    }
  } else if (filename.startsWith("/log_backup_") && filename.endsWith(".txt")) {
    // np. /log_backup_20240527_124540.txt
    int start = 12;
    int end = filename.lastIndexOf('.');
    if (end > start) {
      String numStr = filename.substring(start, end);
      numStr.replace("_", "");
      return numStr.toInt();  // np. 20240527124540
    }
  }
  return 0;
}

void cleanupOldLogBackups(int maxFiles) {
  std::vector<String> backups;
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    String fname = file.name();
    if (fname.startsWith("/log_backup_") && fname.endsWith(".txt")) {
      backups.push_back(fname);
    }
    file = root.openNextFile();
  }
  root.close();

  std::sort(backups.begin(), backups.end());
  while ((int)backups.size() > maxFiles) {
    String toDel = backups.front();
    Serial.println("[cleanupOldLogBackups] Usuwam: " + toDel);
    SPIFFS.remove(toDel);
    backups.erase(backups.begin());
    delay(50);
  }
}


// =============== LISTOWANIE PLIKÓW (POMOCNICZA) ===============
void listFilesInSPIFFS() {
  File root = SPIFFS.open("/");
  File file = root.openNextFile();
  while (file) {
    Serial.printf("  %s\t%d\n", file.name(), file.size());
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

// 3. Funkcja obsługi:
void handleDeleteTxtFile_POST(EthernetClient &client, const String &body) {
  String fname = getParamValue(body, "file");
  fname = urlDecode(fname);  // <<<<<<<<<<<<<< TU DODAJ
  if (!fname.startsWith("/")) fname = "/" + fname;
  if (SPIFFS.exists(fname)) {
    if (SPIFFS.remove(fname)) {
      addLog("[INFO] Usunięto plik logów: " + fname);
    } else {
      addLog("[WARN] Błąd usuwania pliku: " + fname);
    }
  } else {
    addLog("[WARN] Próba usunięcia nieistniejącego pliku: " + fname);
  }
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /logBackups");
  client.println();
}

void backupAndClearLoginHistoryIfTooBig() {
  if (!enableLogFiles) return;  // Nie zapisuj jeśli flaga wyłączona!
  const char *filename = "/login_history.txt";
  File file = SPIFFS.open(filename, FILE_READ);

  if (!file) return;
  size_t size = file.size();
  file.close();

  if (size < LOGIN_HISTORY_MAX_SIZE) return;  // Plik nie za duży

  // Tworzymy nazwę backupu z datą/godziną lub kolejnym numerem
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buf[40];
  snprintf(buf, sizeof(buf), "/login_history_backup_%04d%02d%02d_%02d%02d%02d.txt",
           timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
           timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  // Kopiujemy dane do backupu
  File src = SPIFFS.open(filename, FILE_READ);
  File dst = SPIFFS.open(buf, FILE_WRITE);
  if (src && dst) {
    uint8_t buffer[128];
    while (src.available()) {
      size_t n = src.read(buffer, sizeof(buffer));
      dst.write(buffer, n);
    }
    dst.close();
  }
  src.close();

  // Czyścimy plik główny
  SPIFFS.remove(filename);                           // usuń oryginał
  File cleared = SPIFFS.open(filename, FILE_WRITE);  // utwórz pusty plik
  cleared.close();

  Serial.printf("[LOGIN_HISTORY] Backup %s i wyczyszczenie login_history.txt (rozmiar był %d bajtów)\n", buf, size);
}

void trimFileIfTooBig(const char *filename, size_t maxSize) {
  if (!SPIFFS.exists(filename)) return;
  File file = SPIFFS.open(filename, FILE_READ);
  size_t size = file.size();
  if (size <= maxSize) {
    file.close();
    return;
  }

  // Przesuń się na koniec, odczytaj tylko ostatnie maxSize bajtów
  file.seek(size - maxSize, SeekSet);
  std::unique_ptr<uint8_t[]> buf(new uint8_t[maxSize]);
  file.read(buf.get(), maxSize);
  file.close();

  // Nadpisz plik tylko ostatnimi danymi
  File out = SPIFFS.open(filename, FILE_WRITE);
  out.write(buf.get(), maxSize);
  out.close();
  Serial.printf("[TRIM] Plik %s obcięty do %d bajtów\n", filename, maxSize);
}

// void printResetReason() {
//   esp_reset_reason_t reason = esp_reset_reason();

//   Serial.print("Przyczyna restartu: ");
//   switch (reason) {
//     case ESP_RST_POWERON: Serial.println("Zasilanie"); break;
//     case ESP_RST_SW: Serial.println("Programowy reset"); break;
//     case ESP_RST_PANIC: Serial.println("Wyjątek/krytyczny błąd"); break;
//     case ESP_RST_INT_WDT: Serial.println("Watchdog (rdzeń 0)"); break;
//     case ESP_RST_TASK_WDT: Serial.println("Watchdog (zadanie)"); break;
//     case ESP_RST_WDT: Serial.println("Inny watchdog"); break;
//     case ESP_RST_BROWNOUT: Serial.println("Brownout (niskie napięcie)"); break;
//     default: Serial.println("Nieznana przyczyna"); break;
//   }
// }

String getLastKnownTime() {
  if (lastEpochTime == 0) return "brak danych";
  time_t approxNow = lastEpochTime + millis() / 1000;
  struct tm t;
  localtime_r(&approxNow, &t);
  char buf[20];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
  return String(buf);
}

void logEvent(const char *event, const char *details) {
  File file = SPIFFS.open("/events.log", FILE_APPEND);
  if (file) {
    file.printf("[%s] %s: %s\n", getDateTimeString().c_str(), event, details);
    file.close();
  }
}

String getDateTimeString() {
  time_t now = time(nullptr);
  struct tm *tm = localtime(&now);
  char buf[64];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
  return String(buf);
}