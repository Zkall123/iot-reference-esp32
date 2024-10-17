#ifndef APP_STATE_H
#define APP_STATE_H

#include "led_strip.h"

// Enum zur Beschreibung der möglichen Anwendungszustände
typedef enum {
    APP_STATE_IDLE = -1,
    APP_STATE_CERTS_LOADED,
    APP_STATE_WIFI_STARTED,
    APP_STATE_WIFI_PROVISIONING,
    APP_STATE_WIFI_CONNECTED,
    APP_STATE_AWS_CONNECTED,
    APP_STATE_OTA_UPDATE,
    APP_STATE_SCANNING,
    APP_STATE_ACCESS,
    APP_STATE_NOACCESS
} AppState;

// Struktur zur Beschreibung des LED-Zustands der Anwendung
typedef struct {
    AppState appState;      // Der aktuelle Zustand der Anwendung
    rgb_t currentColor;     // Die aktuelle Farbe der LEDs
    bool isScanning;        // Zeigt an, ob der Scanning-Task aktiv ist
    bool hasAccess;         // Zeigt an, ob der Zugang gewährt wurde
} LedAppState;

// Initialisiert den App-Zustand
void InitAppState(void);

// Setzt den App-Zustand
void UpdateAppState(AppState newState, rgb_t newColor, bool scanning, bool access);

// Gibt den aktuellen Zustand zurück
AppState GetAppState(void);

#endif // APP_STATE_H