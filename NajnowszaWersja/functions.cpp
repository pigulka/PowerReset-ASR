#include <SPIFFS.h>
#include <FS.h>
#include "globals.h"
#include <Arduino.h>
#include <time.h>
#include "functions.h"

void addLog(const String &);

#include <Preferences.h>
extern Preferences preferences;
extern ResetState inputState[];
//InputViewState getInputViewState(int i);

void backupAndClearLogIfTooBig(size_t maxSize);
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
void cleanupOldLogBackups(int maxFiles);


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

void triggerRelay(int inputIndex) {
  if (relayMode == 0) {
    // 4x1 – jedno wejście steruje jednym przekaźnikiem
    setRelayState(inputIndex, true);
  } else if (relayMode == 1) {
    // 2x2 – E1/E2 sterują R1/R2, E3/E4 sterują R3/R4
    if (inputIndex == 0 || inputIndex == 1) {
      setRelayState(0, true);
      setRelayState(1, true);
    } else if (inputIndex == 2 || inputIndex == 3) {
      setRelayState(2, true);
      setRelayState(3, true);
    }
  }
}

void setRelayState(int relayIndex, bool state) {
  if (relayIndex >= 0 && relayIndex < 4) {
    digitalWrite(relayPins[relayIndex], state ? HIGH : LOW);
    relayStates[relayIndex] = state ? "ON" : "OFF";
    if (state) {
      relayOnTime[relayIndex] = millis();
    }
  }
}

void handleSetRelayMode_POST(EthernetClient &client, const String &body) {
    String mode = getParamValue(body, "relayMode");
    if (mode == "4x1") {
        relayMode = 0;
        addLog("Zmieniono tryb pracy przekaźników na 4x1.");
    } else if (mode == "2x2") {
        relayMode = 1;
        addLog("Zmieniono tryb pracy przekaźników na 2x2.");
    } else {
        addLog("Próba ustawienia nieznanego trybu pracy przekaźników: " + mode);
    }
    saveRelayMode(relayMode);

    client.println("HTTP/1.1 303 See Other");
    client.println("Location: /");
    client.println("Connection: close");
    client.println();
}


// Zwraca stan dla wyświetlania na stronie
InputViewState getInputViewState(int i) {
    // Standardowo zwracamy prawdziwy stan
    if (relayMode == 0) {
        switch (inputState[i]) {
            case IDLE: return VIEW_IDLE;
            case WAITING_1MIN: return VIEW_WAITING_1MIN;
            case DO_RESET: return VIEW_DO_RESET;
            case WAIT_START: return VIEW_WAIT_START;
            case MAX_ATTEMPTS: return VIEW_MAX_ATTEMPTS;
        }
    } else if (relayMode == 1) {
        // W trybie 2x2 – jeśli para ma awarię, pokaż na obu!
        int pairStart = (i < 2) ? 0 : 2;
        int j = (i == pairStart) ? pairStart+1 : pairStart;
        if (inputState[i] != IDLE) {
            switch (inputState[i]) {
                case WAITING_1MIN: return VIEW_WAITING_1MIN;
                case DO_RESET: return VIEW_DO_RESET;
                case WAIT_START: return VIEW_WAIT_START;
                case MAX_ATTEMPTS: return VIEW_MAX_ATTEMPTS;
                default: return VIEW_IDLE;
            }
        } else if (inputState[j] != IDLE) {
            // Para jest aktywna, ale "to wejście" nie – pokaż status pary
            return VIEW_PAIR_ACTIVE;
        }
    }
    return VIEW_IDLE;
}


// Funkcja zwraca czytelny opis dla pary wejść (dla trybu 2x2)
String getPairLabel(int pairIdx) {
  if (pairIdx == 0)
    return inputLabels[0] + " / " + inputLabels[1]; // np. "E1 / E2"
  else
    return inputLabels[2] + " / " + inputLabels[3]; // np. "E3 / E4"
}


