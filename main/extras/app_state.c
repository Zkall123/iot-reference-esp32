#include "app_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "ledStrip.h"

// Globale Variable für den App-Zustand
static LedAppState ledState;
static SemaphoreHandle_t appStateMutex = NULL;

void InitAppState(void) {
    if (appStateMutex == NULL) {
        appStateMutex = xSemaphoreCreateMutex(); // Erstelle den Mutex
    }

    ledState.appState = APP_STATE_IDLE;
    ledState.currentColor = colors[0];
    ledState.isScanning = false;
    ledState.hasAccess = false;
}

void UpdateAppState(AppState newState, rgb_t newColor, bool scanning, bool access) {
    xSemaphoreTake(appStateMutex, portMAX_DELAY);
    ledState.appState = newState;
    ledState.currentColor = newColor;
    ledState.isScanning = scanning;
    ledState.hasAccess = access;
    xSemaphoreGive(appStateMutex);
}

AppState GetAppState(void) {
    xSemaphoreTake(appStateMutex, portMAX_DELAY);
    AppState currentState = ledState.appState;
    xSemaphoreGive(appStateMutex);
    return currentState;
}