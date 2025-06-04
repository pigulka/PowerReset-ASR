/*************************************************************
 * Rozbudowany kod ESP32 + W5500 + TFT (z Sprite)
 * ----------------------------------------------
 * Wersja: v1.46
 * Data:   2025-05-25, godz. 17:00
 *
 * Opis:
 Sterowanie przekaźnikami i monitorowanie wejść z maszyną stanów: IDLE, WAITING_1MIN,

DO_RESET, WAIT_START, MAX_ATTEMPTS.

Dynamicznie ustawiana liczba przekaźników (1-4) przez panel WWW.

Możliwość włączania/wyłączania zapisywania logów i historii logowań do plików txt (flaga).

Limity rozmiarów log.txt, login_history.txt, errors.log — automatyczne przycinanie do 50kB.

Możliwość przywracania ustawień domyślnych (przycisk w panelu).

Zaawansowany panel statystyk (cykle, historia wejść i przekaźników, zerowanie liczników).

Szybka edycja etykiet wejść, konfiguracja monitorowania.

Panel aktualizacji firmware OTA przez WWW i curl (application/octet-stream).

Wyświetlacz TFT: dynamiczne dane, tryb auto ON/OFF, wizualizacja monitorowanych wejść.

Interfejs WWW: sidebar, nowoczesny ciemny motyw, zakładki, AJAX, dynamiczne odświeżanie.

Pełne logowanie zdarzeń (logi, backupy, możliwość wyłączenia).

Watchdog programowy (co 10 s).

Bezpieczne ustawienia: weryfikacja siły hasła, wymuszanie zmiany hasła, autoryzacja BasicAuth.

Automatyczna synchronizacja czasu (NTP lub TCP), RTC DS3231.

Obsługa kopii zapasowych logów, pobieranie, usuwanie i czyszczenie przez WWW.

Możliwość zerowania statystyk i liczników z poziomu interfejsu.
 *  - Sterowanie przekaźnikami oraz monitorowanie wejść z możliwością
 *    automatycznego resetu urządzenia (maszyna stanów: IDLE, WAITING_1MIN,
 *    DO_RESET, WAIT_START, MAX_ATTEMPTS).
 *
 * Główne funkcjonalności:
 *  - Uwierzytelnianie za pomocą BasicAuth. Dane logowania (login/hasło)
 *    są przechowywane w Preferences (w formie jawnej, kodowane Base64).
 *  - Zmiana ustawień sieciowych (IP, Gateway, Subnet, DNS) przez interfejs WWW.
 *  - Zmiana nazwy miejsca, która jest wyświetlana zarówno na TFT, jak i w interfejsie WWW.
 *  - Konfigurowalny czas auto-off – przekaźniki wyłączają się automatycznie
 *    po upływie określonego czasu.
 *  - Strony WWW utrzymane w ciemnym motywie.
 *  - Wyświetlanie stanu połączenia Ethernet (LAN) na stronie WWW oraz na TFT.
 *  - Automatyczna re-inicjalizacja Ethernetu, gdy kabel sieciowy zostanie
 *    odłączony i ponownie podłączony.
 *  - Odświeżanie wyświetlacza TFT co 1 sekundę przy użyciu obiektu Sprite.
 *  - Odliczanie czasu do automatycznego wyłączenia przekaźników.
 *  - Możliwość edycji etykiet opisujących relacje między urządzeniami dla
 *    poszczególnych przekaźników – opisy są zapisywane w Preferences.
 *  - Logowanie zdarzeń systemowych (LOGI).
 *  - Dodany watchdog co 10s
 *  - Dodano ustawienie etykiet dla wejść E1
 *  - Dodano weryfikację hasła
 *  - Dodano formularz ustawień czasów .
 *  - Dodano sprawdzanie siły haseł
 *  - Dodano formularz aktualizacji przez curl 
 *  - curl -X POST -H "Authorization: Basic YWRtaW46MTIzNA==" -H "Content-Type: application/octet-stream" --data-binary @C:\firmware.bin http://192.168.0.178/updateFirmware
 *  - lub przez formularz
 *  - dodano serwer czasu
 *  - dodano diagnostykę
 *  - dodano statystyki
 *
 * Bezpieczeństwo:
 *  - Komunikacja odbywa się przez HTTP (bez HTTPS). Aby zwiększyć bezpieczeństwo,
 *    należałoby wdrożyć HTTPS z własnymi certyfikatami – wymaga to wygenerowania
 *    certyfikatów i wgrania ich do pamięci flash.
 *  - Uwierzytelnianie BasicAuth wykorzystuje Base64, co nie zapewnia pełnej ochrony.
 *  - Dane konfiguracyjne są przechowywane w Preferences w formie niezaszyfrowanej.
 *  - Dodatkowo: Dla każdego wejścia E1 (transoptory) można edytować etykietę
 *    (zapis w Preferences) oraz wyświetlać ją na stronie WWW.
 *
 * Dodatkowe informacje:
 *  - Moduł Ethernet: W5500
 *  - Wyświetlacz TFT: TFT_eSPI (z Sprite)
 *  - RTC: DS3231
 *
 * Autor: Pigulk@ 2025
 *************************************************************/