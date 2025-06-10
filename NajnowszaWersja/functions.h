#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <Ethernet.h>
#include <Arduino.h>
#include <vector>

struct FileInfo {
    String name;
    size_t size;
    time_t mtime;
};

std::vector<String> getSortedLogBackups();

String createLogsBackup();
String formatTime(unsigned long epoch);
String formatMac(const byte* mac, size_t len = 6);
String ipToString(IPAddress ip);
InputViewState getInputViewState(int i);

// POST handlers
void setRelayState(int relayIndex, bool state);
void handleChangeAuthPage_POST(EthernetClient &client, const String &body);
void handleChangeNetPage_POST(EthernetClient &client, const String &body);
void handleChangePlacePage_POST(EthernetClient &client, const String &body);
void handleChangeAutoOffTimePage_POST(EthernetClient &client, const String &body);
void handleChangeLinkPage_POST(EthernetClient &client, int relayIndex, const String &body);
void handleChangeAllLinksPage_POST(EthernetClient &client, const String &body);
void handleSaveSettings_POST(EthernetClient &client, const String &body);
void handleChangeInputPage_POST(EthernetClient &client, int inputIndex, const String &body);
void handleUpdateFirmware_POST(EthernetClient &client, const String &headers, int contentLength);
void handleClearLogs_POST(EthernetClient &client);

// GET handlers
void sendChangeAuthPage_GET(EthernetClient &client);
void sendChangeNetPage_GET(EthernetClient &client);
void sendChangePlacePage_GET(EthernetClient &client);
void sendChangeAutoOffTimePage_GET(EthernetClient &client);
void sendChangeLinkPage_GET(EthernetClient &client, int relayIndex);
void sendChangeAllLinksPage_GET(EthernetClient &client);
void sendMainPage(EthernetClient &client);
void sendSettingsPage_GET(EthernetClient &client);
void sendUpdateFirmwarePage_GET(EthernetClient &client);
void sendLogPage_GET(EthernetClient &client);
void sendChangeInputPage_GET(EthernetClient &client, int inputIndex);
void sendInputsPage_GET(EthernetClient &client);
void sendUstawieniaPage_GET(EthernetClient &client);
void sendResetESPConfirmPage_GET(EthernetClient &client);
void sendToggleRelayConfirmPage(EthernetClient &client, int relayIndex);

// Dynamic GET/POST routing helpers
void handleToggleAutoReset_GET(EthernetClient &client);
void handleResetESPConfirm_GET(EthernetClient &client);
void handleResetAttempts_GET(EthernetClient &client);
void handleToggleInput_GET(EthernetClient &client, const String &requestLine);
void handleChangeInputPage_GET(EthernetClient &client, const String &requestLine);
void handleChangeInputPage_POST(EthernetClient &client, const String &requestLine, const String &body);
void handleChangeLink_POST(EthernetClient &client, const String &requestLine, const String &body);
void handleChangeLinkPage_GET(EthernetClient &client, const String &requestLine);
void handleToggleRelayConfirmPage_GET(EthernetClient &client, const String &requestLine);
void handleToggleRelayConfirm_GET(EthernetClient &client, const String &requestLine);
void sendLogBackupsPage_GET(EthernetClient &client);
void handleDownloadLogBackup_GET(EthernetClient & client, const String &fileParamRaw);


// HTML helpers
void sendHtmlHeader(EthernetClient &client, const String &title);
void sendHtmlFooter(EthernetClient &client);
void sendHttpError(EthernetClient &client, const String &status, const String &message);
void sendHttpResponseHeader(EthernetClient &client, int statusCode, const char *contentType);

// Utility functions
String getCurrentDateTime();
String getParamValue(const String &body, const String &param);
bool isPasswordStrong(const String &password);
String urlDecode(const String &src);
String getBoundary(const String &headers);
bool skipHttpHeaders(EthernetClient &client, unsigned long timeoutMs);
bool skipHeaders(EthernetClient &client);
bool readUntil(EthernetClient &client, const char *terminator);
String readLine(EthernetClient &client);
void skipToBinary(EthernetClient &client);
void skipToBinaryData(EthernetClient &client);
int extractRelayIndex(const String &requestLine);
int extractInputIndex(const String &requestLine);
int extractParamValue(const String &requestLine, const String &param);
void reconnectEthernet();
void logError(const String &errorMessage);
void readStoredErrors();
void criticalError(const String &errorMessage);

String getCurrentTimeStr();
void setAllRelays(bool on);
void toggleRelay(int index);
void displayOnTFT();
bool isAnyInputLow();
void handleAutoResetLogic();
void handleHttpRequest(EthernetClient &client);
void handleGetRequest(EthernetClient &client, const String &requestLine, const String &headers);
void handlePostRequest(EthernetClient &client, const String &requestLine, const String &body, const String &headers);
String readHttpHeaders(EthernetClient &client);
String readHttpBody(EthernetClient &client, int contentLen);
bool checkAuth(const String &headers);
void sendAuthRequired(EthernetClient &client);
void handleLogout(EthernetClient &client);
void sendNotFound(EthernetClient &client);
void sendLogBackupViewPage(EthernetClient & client, const String &fileName);
void handleDownloadLogFile(EthernetClient & client, const String &filePath);
// Przełącza przekaźnik o zadanym numerze (relayNum) na zadany stan (true-włączony, false-wyłączony)
void setRelay(int idx, bool on);
void sendPanelDiagnostyczny(EthernetClient & client);
void handleDiagnosticRequest(EthernetClient & client, const String &request);
void sendChangeNetData(EthernetClient &client);
void sendChangePlacePage(EthernetClient &client);
void sendChangeAllLinksPage(EthernetClient &client);
void sendUpdateFirmwarePage(EthernetClient &client);
void sendChangeNetPage(EthernetClient &client);
void handleRenameLogBackup_POST(EthernetClient &client, const String &body);
void handleDeleteLogBackup_POST(EthernetClient &client, const String &body);
void listFilesInSPIFFS();
void sendStatsPage(EthernetClient &client);
void sendDiagnosticPage(EthernetClient &client);
void backupAndClearLogIfTooBig(size_t maxSize = MAX_LOG_FILE_SIZE);
void cleanupOldLogBackups(int maxFiles = MAX_LOG_BACKUPS);
int getNextBackupNumber();
void handleDeleteTxtFile_POST(EthernetClient &client, const String &body);
void trimFileIfTooBig(const char* filename, size_t maxSize = 50 * 1024);
void handleResetSettings_POST(EthernetClient &client);
void sendWatchdogPage_GET(EthernetClient &client);
void saveRelayMode(int mode);
int getContentLength(const String &headers);
void setRelayState(int relayIndex, bool state);
void handleSetRelayMode_POST(EthernetClient &client, const String &body);
void triggerRelay(int inputIndex);
String getPairLabel(int pairIdx);
int getIntParam(const String &requestLine, const String &key);
String getBodyParam(const String &body, const String &key);





#endif // FUNCTIONS_H

