#include <Arduino.h>
#include "globals.h"
#include "NTPConfig.h"
//#include <Preferences.h>
//Preferences preferences;
volatile uint32_t loopCounter = 0;
unsigned long lastEpochTime = 0; // przechowuje czas UNIX

bool wasLoggedIn = false;

unsigned long deviceUptime = 0;
unsigned long totalUptime = 0;
uint32_t deviceRestarts = 0;
unsigned long lastRestartEpoch = 0;
bool enableLogFiles = true;

uint32_t inputCycles[NUM_INPUTS] = {0};
uint32_t inputRisingEdges[NUM_INPUTS] = {0};
uint32_t inputFallingEdges[NUM_INPUTS] = {0};
unsigned long inputActiveTime[NUM_INPUTS] = {0};
unsigned long inputInactiveTime[NUM_INPUTS] = {0};
unsigned long inputLastChange[NUM_INPUTS] = {0};
uint8_t prevInputStates[NUM_INPUTS] = {0};
bool inputMonitoringEnabled[4] = {false, false, false, false};
uint32_t relayCycles[NUM_RELAYS] = {0};
unsigned long relayActiveTime[NUM_RELAYS] = {0};
unsigned long relayInactiveTime[NUM_RELAYS] = {0};
unsigned long relayLastChange[NUM_RELAYS] = {0};
uint8_t prevRelayStates[NUM_RELAYS] = {0};
uint8_t ACTIVE_RELAYS = 4 ;
String relayNames[NUM_RELAYS] = {
    "Przekaźnik 1", "Przekaźnik 2", "Przekaźnik 3", "Przekaźnik 4"
};
uint32_t loginAttempts = 0;
uint32_t failedLoginAttempts = 0;
String lastLoginTime = "";
String lastLoginIP = "";
String firmwareDescription = "Dodano statystyki, poprawki błędów i nowe funkcje";

int inputPins[4] = { 16, 17, 25, 26 }; 
const int relayPins[4] = { 12, 13, 14, 27 }; // lub Twoje numery  
// Definicja zmiennej globalnej
NTPConfig ntpConfig(false);//domyślnie false true-włącza debugowanie

String getCurrentDateTime()
{
    // Sprawdź, czy mamy poprawny czas (czy NTP się zsynchronizował).
    if (ntpConfig.isTimeValid())
    {
        // Zwróć np. "2025-04-06 12:34:56"
        return ntpConfig.getFormattedDate() + " " + ntpConfig.getFormattedTime();
    }
    else
    {
        // Jeśli nie mamy jeszcze synchronizacji, zwracamy informację o jej braku
        return "brak synchronizacji";
    }
}
int logIndex = 0;
String serialBuffer;
#define SERIAL_BUFFER_SIZE 2000

String inputLabels[NUM_INPUTS] = {"Wejście 1", "Wejście 2", "Wejście 3", "Wejście 4"};

const String firmwareVersion = "1.57";
const char* WERSJA_OPROGRAMOWANIA = "PowerReset 2";
ResetState currentState = IDLE;
unsigned long autoOffTime = 60000UL;
//unsigned long stateStartTime = 0;
String placeStr = "Brak miejsca";
//bool maxResetReached = false;
ResetState inputState[NUM_INPUTS] = { IDLE, IDLE, IDLE, IDLE };
unsigned long stateStartTime[NUM_INPUTS] = { 0, 0, 0, 0 };
unsigned int resetAttempts[NUM_INPUTS] = { 0, 0, 0, 0 };
bool maxResetReached[NUM_INPUTS] = { false, false, false, false };

String relayStates[4] = {"OFF", "OFF", "OFF", "OFF"};
unsigned long relayOnTime[4] = {0, 0, 0, 0};
String linkDescriptions[4] = {"Brak opisu", "Brak opisu", "Brak opisu", "Brak opisu"};
bool autoResetEnabled = false;
unsigned long WAITING_1MIN_TIME = 60000UL;
unsigned long RESET_DURATION = 60000UL;
unsigned long DEVICE_STARTTIME = 900000UL;
String logHistory[MAX_LOGS];
// bool ntpTimeSynced = false;
String currentDate = "0000-00-00";
String currentTime = "00:00:00";

// Ustawienia domyślne serwera czasu – adres i port
String ntpServerAddress = "192.168.0.10";
int ntpServerPort = 123;
// Przykładowe zmienne globalne
String timeZone     = "CET-1CEST,M3.5.0,M10.5.0/3";  // strefa czasowa
String ntpServer1   = "pl.pool.ntp.org";            // serwer NTP 1
String ntpServer4   = "europe.pool.ntp.org";        // serwer NTP 2
String ntpServer3   = "pool.ntp.org";               // serwer NTP 3
int savedOffset     = 3600;
String ipStr = "";
String gatewayStr = "";
String subnetStr = "";
String dnsStr = "";

bool autoRefreshEnabled = true; // lub false – według potrzeb
unsigned int refreshInterval = 5; // np. 5 sekund


unsigned int wolnaPamiec() {
  // Przykładowa implementacja – zależy od Twojego systemu
  return ESP.getFreeHeap();
}

// Zwraca czas kompilacji jako epoch (Unix time)
// Wywołuj np. getCompileTimeEpoch()
time_t getCompileTimeEpoch() {
    struct tm t;
    memset(&t, 0, sizeof(t));
    // __DATE__ ma format "Mmm dd yyyy", np. "Jun  1 2025"
    // __TIME__ ma format "hh:mm:ss"
    strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &t);
    return mktime(&t);
}

