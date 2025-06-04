#ifndef NTP_CONFIG_H
#define NTP_CONFIG_H

#include <Ethernet.h>
#include <EthernetUdp.h>
#include <NTPClient.h>

class NTPConfig {
public:
    NTPConfig(bool debug = false);
    ~NTPConfig();

    // Inicjalizacja klienta NTP – udp musi być zainicjalizowany (np. udp.begin(localPort))
    // server – adres serwera NTP, port – UDP port serwera (zwykle 123), offset – przesunięcie czasowe w sekundach
    void begin(EthernetUDP& udp, const String& server = "pool.ntp.org", int port = 123, int timezoneOffset = 3600);

    // Aktualizacja czasu – wywoływana cyklicznie w pętli głównej
    void update();

    // Wymuszenie natychmiastowej synchronizacji
    bool forceUpdate();

    // Pobranie sformatowanego czasu i daty
    String getFormattedTime();
    String getFormattedDate();

    // Zwraca czas (millis) ostatniej udanej synchronizacji
    unsigned long getLastUpdate() const;

    // Settery
    void setServer(const String& server);
    void setPort(int port);
    void setTimezoneOffset(int offset);
    void setDebugEnabled(bool enabled);

    // Gettery
    String getCurrentServer() const { return ntpServer; }
    int getCurrentPort() const { return ntpPort; }
    int getCurrentTimezoneOffset() const { return timezoneOffset; }
    bool isTimeValid() const;

private:
    void log(const String& message);

    EthernetUDP* ntpUDP;
    NTPClient* timeClient;
    String ntpServer;
    int ntpPort;
    int timezoneOffset;
    bool debugEnabled;
    unsigned long lastUpdateTime;
};

#endif
