#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <Ethernet.h>
//#include <Preferences.h>
//extern Preferences preferences;
extern volatile uint32_t loopCounter;
extern unsigned long lastEpochTime; // przechowuje czas UNIX
time_t getCompileTimeEpoch();

#define MAX_LOG_FILE_SIZE  (128 * 1024)
#define MAX_LOG_BACKUPS    10
extern uint8_t ACTIVE_RELAYS;
extern bool enableLogFiles;
extern bool timing;

extern bool wasLoggedIn;

extern bool inputMonitoringEnabled[4];
extern uint32_t loginAttempts;
extern uint32_t failedLoginAttempts;
extern String lastLoginTime;
extern String lastLoginIP;



void logUpdateError(const String& reason);
void sendPanelDiagnostyczny(EthernetClient &client);
void setRelay(int idx, bool state);

extern const int relayPins[4];

extern int inputPins[4];
extern unsigned long deviceUptime;
extern unsigned long totalUptime;
extern uint32_t deviceRestarts;
extern unsigned long lastRestartEpoch;

//#define SERIAL_BUFFER_SIZE 2000
extern String serialBuffer;
extern String getCurrentDateTime();
extern String ntpServerAddress;  // np. "192.168.0.100"
extern int ntpServerPort;        // np. 123
extern int savedOffset;
extern byte mac[];
extern String inputLabels[4];
#define NUM_INPUTS 4
#define NUM_RELAYS 4 
extern String relayNames[NUM_RELAYS];
extern uint32_t relayCycles[NUM_RELAYS];
extern unsigned long relayActiveTime[NUM_RELAYS];
//extern uint32_t loginAttempts;
//extern uint32_t failedLoginAttempts;
extern String lastLoginTime;
extern String lastLoginIP;
extern String firmwareDescription;
extern uint32_t inputCycles[NUM_INPUTS];
extern unsigned long inputActiveTime[NUM_INPUTS];
extern String inputLabels[NUM_INPUTS];
extern bool inputMonitoringEnabled[4];

extern uint32_t inputCycles[NUM_INPUTS];
extern uint32_t inputRisingEdges[NUM_INPUTS];
extern uint32_t inputFallingEdges[NUM_INPUTS];
extern unsigned long inputActiveTime[NUM_INPUTS];
extern unsigned long inputInactiveTime[NUM_INPUTS];
extern unsigned long inputLastChange[NUM_INPUTS];
extern uint8_t prevInputStates[NUM_INPUTS];



extern unsigned long relayInactiveTime[NUM_RELAYS];
extern unsigned long relayLastChange[NUM_RELAYS];
extern uint8_t prevRelayStates[NUM_RELAYS];

// --- Stany maszyny ---
enum ResetState {
  IDLE,
  WAITING_1MIN,
  DO_RESET,
  WAIT_START,
  MAX_ATTEMPTS
};

extern ResetState inputState[NUM_INPUTS];
extern unsigned long stateStartTime[NUM_INPUTS];
extern unsigned int resetAttempts[NUM_INPUTS];
extern bool maxResetReached[NUM_INPUTS];


// Zmienne globalne
extern const String firmwareVersion;
extern const char* WERSJA_OPROGRAMOWANIA;
extern ResetState currentState;
extern unsigned long autoOffTime;
//extern unsigned long stateStartTime;
extern String placeStr;
//extern bool maxResetReached;
extern String relayStates[4];
extern unsigned long relayOnTime[4];
extern String linkDescriptions[4];
extern bool autoResetEnabled;
extern bool ntpTimeSynced;
extern unsigned long WAITING_1MIN_TIME;
extern unsigned long RESET_DURATION;
extern unsigned long DEVICE_STARTTIME;
extern String currentDate;
extern String currentTime;

#include "NTPConfig.h"  // jeśli potrzebujesz typów

extern NTPConfig ntpConfig;

unsigned int wolnaPamiec();

// Przykładowe zmienne globalne
extern String timeZone;  // strefa czasowa
extern String ntpServer1;            // serwer NTP 1
extern String ntpServer4;            // serwer NTP 4
extern String ntpServer3;            // serwer NTP 3
extern String ipStr;
extern String gatewayStr;
extern String subnetStr;
extern String dnsStr;

extern bool autoRefreshEnabled; // lub false – według potrzeb
extern unsigned int refreshInterval; // np. 5 sekund


#define MAX_LOGS 100
extern String logHistory[MAX_LOGS];
extern int logIndex;

// Funkcje
String getCurrentDateTime();


#endif
