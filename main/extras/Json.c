/*
 * RGB_Led.h
 *
 *  Created on: 7 Jun 2024
 *      Author: macra
 */
#include <stdbool.h>
#include "Json.h"
#include <stdio.h>
#include "esp_log.h"
#include "string.h"

#include "core_json.h"

#include "extras/ledStrip.h"
#include "extras/Piepser.h"
#include "networking/wifi/lan.h"


static const char* TAG = "JSON";

char* JsonAccessString(const char* UID)
{
	// JSON-String-Puffer
    static char json_buffer[1500];

    // Erstelle den JSON-String Schritt für Schritt
    char Mac_field[128];
    snprintf(Mac_field, sizeof(Mac_field), "\"macAddrHex\":\"%s\"", LanPrintMac());

    // Verwende die übergebene uid_value Variable im arguments-Feld
    char UID_field[128];
    snprintf(UID_field, sizeof(UID_field), "\"uid\":\"%s\"", UID);

    // Füge die einzelnen Teile zusammen
    snprintf(json_buffer, sizeof(json_buffer), "{%s,%s}", Mac_field, UID_field);
    
    // Ausgabe des JSON-Strings
    //ESP_LOGI(TAG, "Erstellter JSON-String: %s", json_buffer);
    
	return json_buffer;
}

void JsonParse(char* income, char* channel)
{
    if(channel == "settings")
    {
        JSONStatus_t result;
        char *value;
        size_t value_length;
        bool useWifi = false, useNfcReader = false, useRgbLed = false, useBuzzer = false;
        int beepOnScan = 0, beepOnValidScan = 0, beepOnInvalidScan = 0;
        char colorOnScan[7] = "", colorOnValidScan[7] = "", colorOnInvalidScan[7] = "";

        // Überprüfe, ob das JSON-Format gültig ist
        result = JSON_Validate(income, strlen(income));
        if (result != JSONSuccess) {
            ESP_LOGE(TAG, "Ungültiges JSON-Format");
            return;
        }
        ESP_LOGI(TAG, "JSON ist gültig");

        // useWifi parsen
        result = JSON_Search(income, strlen(income), "useWifi", strlen("useWifi"), &value, &value_length);
        if (result == JSONSuccess) {
            useWifi = (strncmp(value, "true", value_length) == 0);
            ESP_LOGI(TAG, "useWifi: %s", useWifi ? "true" : "false");
            //TODO Send use Wifi
        }

        // useNfcReader parsen
        result = JSON_Search(income, strlen(income), "useNfcReader", strlen("useNfcReader"), &value, &value_length);
        if (result == JSONSuccess) {
            useNfcReader = (strncmp(value, "true", value_length) == 0);
            ESP_LOGI(TAG, "useNfcReader: %s", useNfcReader ? "true" : "false");
            //Todo UseNFC Reader
        }

        // useBuzzer parsen__________________________________________________________________________________________________________________
        result = JSON_Search(income, strlen(income), "useBuzzer", strlen("useBuzzer"), &value, &value_length);
        if (result == JSONSuccess) {
            useBuzzer = (strncmp(value, "true", value_length) == 0);
            ESP_LOGI(TAG, "useBuzzer: %s", useBuzzer ? "true" : "false");
        }

        // beepOnScan parsen
        result = JSON_Search(income, strlen(income), "beepOnScan", strlen("beepOnScan"), &value, &value_length);
        if (result == JSONSuccess && strncmp(value, "null", value_length) != 0) {
            beepOnScan = atoi(value);
            ESP_LOGI(TAG, "beepOnScan: %d", beepOnScan);
        }

        // beepOnValidScan parsen
        result = JSON_Search(income, strlen(income), "beepOnValidScan", strlen("beepOnValidScan"), &value, &value_length);
        if (result == JSONSuccess && strncmp(value, "null", value_length) != 0) {
            beepOnValidScan = atoi(value);
            ESP_LOGI(TAG, "beepOnValidScan: %d", beepOnValidScan);
        }

        // beepOnInvalidScan parsen
        result = JSON_Search(income, strlen(income), "beepOnInvalidScan", strlen("beepOnInvalidScan"), &value, &value_length);
        if (result == JSONSuccess && strncmp(value, "null", value_length) != 0) {
            beepOnInvalidScan = atoi(value);
            ESP_LOGI(TAG, "beepOnInvalidScan: %d", beepOnInvalidScan);
        }

        Sound_ChangeSettings(beepOnValidScan, beepOnInvalidScan, beepOnScan, useBuzzer);

        // useRgbLed parsen__________________________________________________________________________________________________________________
        result = JSON_Search(income, strlen(income), "useRgbLed", strlen("useRgbLed"), &value, &value_length);
        if (result == JSONSuccess) {
            useRgbLed = (strncmp(value, "true", value_length) == 0);
            ESP_LOGI(TAG, "useRgbLed: %s", useRgbLed ? "true" : "false");
        }

        // colorOnScan parsen
        result = JSON_Search(income, strlen(income), "colorOnScan", strlen("colorOnScan"), &value, &value_length);
        if (result == JSONSuccess && strncmp(value, "null", value_length) != 0) {
            snprintf(colorOnScan, sizeof(colorOnScan), "%.*s", (int)value_length, value);
            ESP_LOGI(TAG, "colorOnScan: %s", colorOnScan);
        }

        // colorOnValidScan parsen
        result = JSON_Search(income, strlen(income), "colorOnValidScan", strlen("colorOnValidScan"), &value, &value_length);
        if (result == JSONSuccess && strncmp(value, "null", value_length) != 0) {
            snprintf(colorOnValidScan, sizeof(colorOnValidScan), "%.*s", (int)value_length, value);
            ESP_LOGI(TAG, "colorOnValidScan: %s", colorOnValidScan);
        }

        // colorOnInvalidScan parsen
        result = JSON_Search(income, strlen(income), "colorOnInvalidScan", strlen("colorOnInvalidScan"), &value, &value_length);
        if (result == JSONSuccess && strncmp(value, "null", value_length) != 0) {
            snprintf(colorOnInvalidScan, sizeof(colorOnInvalidScan), "%.*s", (int)value_length, value);
            ESP_LOGI(TAG, "colorOnInvalidScan: %s", colorOnInvalidScan);
        }
        RgbLed_ChangeSettings(colorOnValidScan, colorOnInvalidScan, colorOnScan, useRgbLed);
    }
    else if(channel == "access")
    {
        JSONStatus_t result;
        char *value;
        size_t value_length;
        bool bAccess = false;

        // Überprüfe, ob das JSON-Format gültig ist
        result = JSON_Validate(income, strlen(income));
        if (result != JSONSuccess) {
            ESP_LOGE(TAG, "Ungültiges JSON-Format");
            return;
        }
        ESP_LOGI(TAG, "JSON ist gültig");

        // useWifi parsen
        result = JSON_Search(income, strlen(income), "access", strlen("access"), &value, &value_length);
        if (result == JSONSuccess) {
            bAccess = (strncmp(value, "true", value_length) == 0);
            ESP_LOGI(TAG, "access: %s", bAccess ? "true" : "false");
        }

        RgbLedHasAccess(bAccess);

    }
}
