# PowerReset ASR

**PowerReset ASR** (Automatic System Restart) to projekt oparty na ESP32 z modułem Ethernet (W5500) oraz wyświetlaczem TFT. Urządzenie umożliwia automatyczne zarządzanie restartem urządzeń oraz diagnostyką systemu.

## Główne funkcje

- Sterowanie przekaźnikami i wejściami cyfrowymi
- Automatyczny restart według zdefiniowanych reguł
- Watchdog sprzętowy i programowy
- Interfejs webowy z konfiguracją sieci, NTP, logów i OTA
- Aktualizacja firmware przez przeglądarkę
- Logowanie zdarzeń do SPIFFS
- Synchronizacja czasu z serwerem TCP/NTP

## Wymagania sprzętowe

- ESP32 (np. ESP32-WROOM-32)
- W5500 Ethernet Module
- TFT 1.8” SPI
- Przekaźniki (np. moduły 5V)
- Zasilanie 3.7V–5V (np. akumulator z BMS)
- Inne komponenty: przyciski, diody LED, transoptory

## Zrzuty ekranu

_(Tu możesz dodać screenshoty z panelu konfiguracyjnego)_  
_(lub zdjęcia płytki/urządzenia)_

## Instalacja

1. Skopiuj repozytorium:
