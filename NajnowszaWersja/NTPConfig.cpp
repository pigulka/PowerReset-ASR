#include "NTPConfig.h"
#include "globals.h"
#include <Arduino.h>

extern bool ntpSyncSuccess;

NTPConfig::NTPConfig(bool debug)
  : ntpUDP(nullptr), timeClient(nullptr), ntpServer("pool.ntp.org"),
    ntpPort(123), timezoneOffset(3600), debugEnabled(debug), lastUpdateTime(0)
{
    log("Konstruktor NTPConfig");
}

NTPConfig::~NTPConfig() {
    log("Destruktor NTPConfig");
    if (timeClient) {
        delete timeClient;
    }
}

void NTPConfig::log(const String& message) {
    if (debugEnabled) {
        Serial.print("[NTP] ");
        Serial.println(message);
    }
}

void NTPConfig::begin(EthernetUDP& udp, const String& server, int port, int offset) {
    log("Inicjalizacja klienta NTP");
    log("Serwer: " + server);
    log("Port: " + String(port));
    log("Przesunięcie: " + String(offset) + " s");

    ntpUDP = &udp;
    ntpServer = server;
    ntpPort = port;
    timezoneOffset = offset;
    
    if (timeClient) {
        delete timeClient;
    }
    
    // Uwaga: Konstruktor NTPClient nie wykorzystuje przekazywanego portu docelowego.
    // Jeśli Twój serwer wymaga niestandardowego portu, konieczne mogą być modyfikacje.
    timeClient = new NTPClient(*ntpUDP, ntpServer.c_str(), timezoneOffset);
    timeClient->begin();
    lastUpdateTime = 0;
}

// void NTPConfig::update() {
//     if (timeClient) {
//         if (timeClient->update()) {
//             lastUpdateTime = millis();
//             log("Czas zaktualizowany pomyślnie");
//         }
//     }
// }
void NTPConfig::update() {
    if (timeClient) {
        if (timeClient->update()) {
            lastUpdateTime = millis();
            log("Czas zaktualizowany pomyślnie");
            ntpSyncSuccess = true;  // Ustaw flagę synchronizacji
        }
    }
}

// bool NTPConfig::forceUpdate() {
//     if (timeClient) {
//         bool result = timeClient->forceUpdate();
//         if (result) {
//             lastUpdateTime = millis();
//             log("Wymuszenie aktualizacji czasu powiodło się");
//         }
//         return result;
//     }
//     return false;
// }
bool NTPConfig::forceUpdate() {
    if (timeClient) {
        bool result = timeClient->forceUpdate();
        if (result) {
            lastUpdateTime = millis();
            log("Wymuszenie aktualizacji czasu powiodło się");
            ntpSyncSuccess = true;  // Ustaw flagę synchronizacji
        } else {
            ntpSyncSuccess = false;
        }
        return result;
    }
    ntpSyncSuccess = false;
    return false;
}

unsigned long NTPConfig::getLastUpdate() const {
    return lastUpdateTime;
}

bool NTPConfig::isTimeValid() const {
    return timeClient && (lastUpdateTime > 0);
}

String NTPConfig::getFormattedDate() {
    if (!timeClient) {
        if (debugEnabled) {
            Serial.println("[NTP] Błąd: client NTP nie zainicjalizowany przy próbie pobrania daty!");
        }
        return "1970-01-01";
    }
    
    time_t rawtime = timeClient->getEpochTime();
    struct tm *ti = localtime(&rawtime);
    
    char buffer[20];
    sprintf(buffer, "%04d-%02d-%02d", ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday);
    return String(buffer);
}

String NTPConfig::getFormattedTime() {
    if (!timeClient) {
        if (debugEnabled) Serial.println("[NTP] Błąd: client NTP nie zainicjalizowany!");
        return "00:00:00";
    }
    return timeClient->getFormattedTime();
}

void NTPConfig::setServer(const String& server) {
    ntpServer = server;
    if (timeClient) {
        timeClient->end();
        delete timeClient;
    }
    timeClient = new NTPClient(*ntpUDP, ntpServer.c_str(), timezoneOffset);
    timeClient->begin();
}

void NTPConfig::setPort(int port) {
    ntpPort = port;
    // Funkcja setPort nie zmienia działania NTPClient, ponieważ biblioteka używa domyślnego portu.
    // W razie potrzeby modyfikacji biblioteki, można tu dodać odpowiednią logikę.
}

void NTPConfig::setTimezoneOffset(int offset) {
    timezoneOffset = offset;
    if (timeClient) {
        timeClient->setTimeOffset(timezoneOffset);
    }
}

void NTPConfig::setDebugEnabled(bool enabled) {
    debugEnabled = enabled;
}
