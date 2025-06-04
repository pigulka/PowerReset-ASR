#include <SPIFFS.h>
#include <FS.h>
#include "globals.h"
#include <Arduino.h>
#include <time.h>

void backupAndClearLogIfTooBig(size_t maxSize = 128 * 1024);
//void cleanupOldLogBackups(int maxFiles = 10);
int getNextBackupNumber();  

// Zwraca sformatowany czas jako string: "YYYY-MM-DD HH:MM:SS"
String formatTime(unsigned long epoch) {
    struct tm timeinfo;
    if (!gmtime_r((time_t*)&epoch, &timeinfo)) {
        return String("Brak czasu");
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return String(buf);
}

// DODAJ DEKLARACJĘ FUNKCJI PRZED createLogsBackup
void cleanupOldLogBackups(int maxFiles = 10);


String createLogsBackup() {
    String date = ntpConfig.getFormattedDate(); // "2025-05-20"
    String time = ntpConfig.getFormattedTime(); // "21:05:44"
    String backupFile;

    if (date == "" || time == "" || date.startsWith("1970")) {
        backupFile = "/log_backup_NOSYNC_" + String(millis() / 1000) + ".txt";
    } else {
        date.replace("-", "");
        time.replace(":", "");
        backupFile = "/log_backup_" + date + "_" + time + ".txt";
    }

    File src = SPIFFS.open("/log.txt", FILE_READ);
    if (!src) {
        Serial.println("[createLogsBackup] Brak log.txt do backupu!");
        return "";
    }

    File dst = SPIFFS.open(backupFile, FILE_WRITE);
    if (!dst) {
        Serial.println("[createLogsBackup] Nie mogę utworzyć backupu: " + backupFile);
        src.close();
        return "";
    }

    while (src.available()) dst.write(src.read());
    src.close();
    dst.close();

    Serial.println("[createLogsBackup] Utworzono backup: " + backupFile);

    cleanupOldLogBackups(10);

    return backupFile;
}



String formatMac(const byte* mac, size_t len) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

String ipToString(IPAddress ip) {
    return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}
