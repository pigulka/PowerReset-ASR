#ifndef PAGES_H
#define PAGES_H

#include <Arduino.h>
#include <Ethernet.h>
#include "globals.h"

// Prototypy wszystkich stron (GET)
void sendMainPage(EthernetClient &client);
void sendChangeAuthPage(EthernetClient &client);
void sendChangeNetPage(EthernetClient &client);
void sendChangePlacePage(EthernetClient &client);
void sendChangeAllLinksPage(EthernetClient &client);
void sendInputsPage(EthernetClient &client);
void sendLogsPage(EthernetClient &client);
void sendSettingsPage(EthernetClient &client);
void sendUpdateFirmwarePage(EthernetClient &client);

// Prototypy funkcji GET do poszczególnych podstron (pełne widoki formularzy)
void sendChangeAuthPage_GET(EthernetClient &client);
void sendChangeNetPage_GET(EthernetClient &client);
void sendChangePlacePage_GET(EthernetClient &client);
void sendChangeAllLinksPage_GET(EthernetClient &client);
void sendSettingsPage_GET(EthernetClient &client);
void sendUpdateFirmwarePage_GET(EthernetClient &client);
void sendChangeInputPage_GET(EthernetClient &client, int inputIndex);
void sendAboutPage(EthernetClient &client);

void sendMainContainerBegin(EthernetClient &client, const String &title);


// Jeśli masz dodatkowe strony, dodaj tutaj kolejne prototypy

#endif



