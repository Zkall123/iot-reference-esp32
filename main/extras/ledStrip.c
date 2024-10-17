/*
 * RGB_Led.h
 *
 *  Created on: 7 Jun 2024
 *      Author: macra
 */
#include <stdbool.h>

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"
#include "esp_log.h"

#include "TasksCommon.h"
#include <led_strip.h>
#include "ledStrip.h"
#include "app_state.h"

#include <math.h>

bool bUseRgbLed = true;

rgb_t scanning_color, access_color, no_access_color;

static const char* TAG = "LED_CONTROL";

#define LED_TYPE LED_STRIP_WS2812
#define LED_GPIO CONFIG_LED_STRIP_GPIO
#define RING_LEN CONFIG_LED_STRIP_LEN

static led_strip_t strip = {
    .type = LED_TYPE,
    .length = RING_LEN,
    .gpio = LED_GPIO,
    .buf = 0,
#ifdef LED_STRIP_BRIGHTNESS
    .brightness = 255,
#endif
};

static bool bRGBInit = false;
static int bRGBInstall = 0;
TaskHandle_t xLEDTaskHandle = NULL;
TaskHandle_t ScanningHandle;

static void Waterfall(rgb_t color, AppState state) {
    int delay = 30;

    // Initialisiere den LED-Streifen nur, wenn er noch nicht initialisiert wurde
    if (!bRGBInit) {
        ESP_ERROR_CHECK(led_strip_init(&strip));
        bRGBInit = true;
    }

    for (int i = 0; i < strip.length; i++) {
        led_strip_set_pixel(&strip, i, color);
        led_strip_flush(&strip);
        vTaskDelay(pdMS_TO_TICKS(delay));

        // Überprüfe regelmäßig den Zustand, um eine vorzeitige Rückgabe zu verhindern
        if (GetAppState() != state) {
            //break;
        }
    }

    for (int i = 0; i < strip.length; i++) {
        led_strip_set_pixel(&strip, i, colors[0]);  // Führe eine 'zurückgesetzte' Animation durch
        led_strip_flush(&strip);
        vTaskDelay(pdMS_TO_TICKS(delay));

        if (GetAppState() != state) {
            break;
        }
    }

    // Der Streifen wird nur freigegeben, wenn die Animation vollständig abgeschlossen ist
    led_strip_free(&strip);
    bRGBInit = false;  // Setze den Initialisierungsstatus zurück
}

static void LEDTask(void *pvParameters) {
    for (;;) {
        if (!bRGBInstall) {
            led_strip_install();
            bRGBInstall = 1;
        }

        switch (GetAppState()) {
            case APP_STATE_IDLE:
                Waterfall(colors[0], APP_STATE_IDLE);
                break;
            case APP_STATE_CERTS_LOADED:
                Waterfall(colors[6], APP_STATE_CERTS_LOADED);
                break;
            case APP_STATE_WIFI_STARTED:
                Waterfall(colors[9], APP_STATE_WIFI_STARTED);
                break;
            case APP_STATE_WIFI_PROVISIONING:
                Waterfall(colors[10], APP_STATE_WIFI_PROVISIONING);
                break;
            case APP_STATE_WIFI_CONNECTED:
                Waterfall(colors[5], APP_STATE_WIFI_CONNECTED);
                break;
            case APP_STATE_AWS_CONNECTED:
                Waterfall(colors[50], APP_STATE_AWS_CONNECTED);
				
				vTaskDelay(pdMS_TO_TICKS(200));
				if (xLEDTaskHandle != NULL) {
    				ESP_LOGI(TAG, "Deleting Task!");
					xLEDTaskHandle = NULL;
    				vTaskDelete(xLEDTaskHandle);
				}
                break;

			case APP_STATE_SCANNING:
				Waterfall(scanning_color, APP_STATE_SCANNING);
				break;
			case APP_STATE_ACCESS:
				Waterfall(access_color, APP_STATE_ACCESS);

				vTaskDelay(pdMS_TO_TICKS(200));
				if (xLEDTaskHandle != NULL) {
        			ESP_LOGI(TAG, "Deleting Task!");
					xLEDTaskHandle = NULL;
        			vTaskDelete(xLEDTaskHandle);
				}
				break;
			case APP_STATE_NOACCESS:
				Waterfall(no_access_color, APP_STATE_NOACCESS);

				vTaskDelay(pdMS_TO_TICKS(200));
				if (xLEDTaskHandle != NULL) {
        			ESP_LOGI(TAG, "Deleting Task!");
					xLEDTaskHandle = NULL;
        			vTaskDelete(xLEDTaskHandle);
				}
				break;

            default:
                break;
        }
    }
}

void StartLED(void) {
    if (xLEDTaskHandle == NULL && ScanningHandle == NULL) {
        xTaskCreate(&LEDTask, "LEDTask", LEDTaskStackSize, NULL, LEDTaskPriority, &xLEDTaskHandle);
    }
}

void RGBLEDScanning(void) {
    ESP_LOGI(TAG, "Starting Scanning LED Task");

    // Setze den Zustand auf "Scannen"
    UpdateAppState(APP_STATE_SCANNING, scanning_color, true, false);

    // Wenn der Task nicht mehr läuft (Handle ist NULL), erstelle den Task neu
    if (xLEDTaskHandle == NULL) {
        xTaskCreate(&LEDTask, "LEDTask", LEDTaskStackSize, NULL, LEDTaskPriority, &xLEDTaskHandle);
        ESP_LOGI(TAG, "LED Scanning Task created successfully");
    } else {
        ESP_LOGW(TAG, "LED Scanning Task is already running");
    }
}

void RgbLedHasAccess(bool access) {
    ESP_LOGI(TAG, "Received Access: %s", access ? "granted" : "denied");

    if (access) {
        ESP_LOGI(TAG, "Changing to Access Color");
        UpdateAppState(APP_STATE_ACCESS, access_color, false, true);
    } else {
        ESP_LOGI(TAG, "Changing to No Access Color");
        UpdateAppState(APP_STATE_NOACCESS, no_access_color, false, false);
    }
}

void RgbLedCertsLoaded(void) {
    ESP_LOGI(TAG, "CertsLoaded!");
    if (xLEDTaskHandle == NULL) {
        xTaskCreate(&LEDTask, "LEDTask", LEDTaskStackSize, NULL, LEDTaskPriority, &xLEDTaskHandle);
    }
    UpdateAppState(APP_STATE_CERTS_LOADED, colors[6], false, false);
}

void RgbLedETHAppStarted(void) {
    ESP_LOGI(TAG, "Wifi/Eth Started");
    if (xLEDTaskHandle == NULL) {
        xTaskCreate(&LEDTask, "LEDTask", LEDTaskStackSize, NULL, LEDTaskPriority, &xLEDTaskHandle);
    }
    UpdateAppState(APP_STATE_WIFI_STARTED, colors[9], false, false);
}

void RgbLedETHConnected(void) {
    ESP_LOGI(TAG, "ETH Connected");
    if (xLEDTaskHandle == NULL) {
        xTaskCreate(&LEDTask, "LEDTask", LEDTaskStackSize, NULL, LEDTaskPriority, &xLEDTaskHandle);
    }
    UpdateAppState(APP_STATE_WIFI_CONNECTED, colors[50], false, false);
}

void RgbLedWifiProvisioningStarted(void) {
    ESP_LOGI(TAG, "Wifi/Eth Connecting");
    if (xLEDTaskHandle == NULL) {
        xTaskCreate(&LEDTask, "LEDTask", LEDTaskStackSize, NULL, LEDTaskPriority, &xLEDTaskHandle);
    }
    UpdateAppState(APP_STATE_WIFI_PROVISIONING, colors[10], false, false);
}

void RgbLedAWSConnected(void) {
    ESP_LOGI(TAG, "AWS Connected");
    if (xLEDTaskHandle == NULL) {
        xTaskCreate(&LEDTask, "LEDTask", LEDTaskStackSize, NULL, LEDTaskPriority, &xLEDTaskHandle);
    }
    UpdateAppState(APP_STATE_AWS_CONNECTED, colors[50], false, false);
}

// Funktion, um den RGB-String zu parsen und die Farben zu setzen
void RgbLed_ChangeSettings(char *Access, char *NoAccess, char *Scanning, bool UseRGB_LED)
{
    bUseRgbLed = UseRGB_LED;

    char access_r[8], access_g[8], access_b[8];
    char no_access_r[8], no_access_g[8], no_access_b[8];
    char scanning_r[8], scanning_g[8], scanning_b[8];
    char temp_access_r[3], temp_access_g[3], temp_access_b[3];
    char temp_no_access_r[3], temp_no_access_g[3], temp_no_access_b[3];
    char temp_scanning_r[3], temp_scanning_g[3], temp_scanning_b[3];

    // Access-String in RGB-Werte umwandeln und 0x hinzufügen
    sscanf(Access, "%2s%2s%2s", temp_access_r, temp_access_g, temp_access_b);
    sprintf(access_r, "0x%s", temp_access_r);
    sprintf(access_g, "0x%s", temp_access_g);
    sprintf(access_b, "0x%s", temp_access_b);
    access_color.r = strtol(access_r, NULL, 16);
    access_color.g = strtol(access_g, NULL, 16);
    access_color.b = strtol(access_b, NULL, 16);
    ESP_LOGI(TAG, "access_color: %s %s %s", access_r, access_g, access_b);

    // NoAccess-String in RGB-Werte umwandeln und 0x hinzufügen
    sscanf(NoAccess, "%2s%2s%2s", temp_no_access_r, temp_no_access_g, temp_no_access_b);
    sprintf(no_access_r, "0x%s", temp_no_access_r);
    sprintf(no_access_g, "0x%s", temp_no_access_g);
    sprintf(no_access_b, "0x%s", temp_no_access_b);
    no_access_color.r = strtol(no_access_r, NULL, 16);
    no_access_color.g = strtol(no_access_g, NULL, 16);
    no_access_color.b = strtol(no_access_b, NULL, 16);
    ESP_LOGI(TAG, "no_access_color: %s %s %s", no_access_r, no_access_g, no_access_b);

    // Scanning-String in RGB-Werte umwandeln und 0x hinzufügen
    sscanf(Scanning, "%2s%2s%2s", temp_scanning_r, temp_scanning_g, temp_scanning_b);
    sprintf(scanning_r, "0x%s", temp_scanning_r);
    sprintf(scanning_g, "0x%s", temp_scanning_g);
    sprintf(scanning_b, "0x%s", temp_scanning_b);
    scanning_color.r = strtol(scanning_r, NULL, 16);
    scanning_color.g = strtol(scanning_g, NULL, 16);
    scanning_color.b = strtol(scanning_b, NULL, 16);
    ESP_LOGI(TAG, "scanning_color: %s %s %s", scanning_r, scanning_g, scanning_b);
}

void RgbLedOTAUpdateIncomming(void)
{
	ESP_LOGI(TAG, "OTA Update Incoming!");
	Waterfall(colors[38], -1);
}
void RgbLedOTAUpdateDone(void)
{
	ESP_LOGI(TAG, "OTA Update Done!");
	Waterfall(colors[50], -1);
}




