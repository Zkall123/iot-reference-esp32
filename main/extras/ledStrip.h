/*
 * RGB_Led.h
 *
 *  Created on: 7 Jun 2024
 *      Author: macra
 */
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

#include <led_strip.h>
#include "app_state.h"

#ifndef MAIN_LEDSTRIP_H_
#define MAIN_LEDSTRIP_H_

//Define Colors
static const rgb_t colors[64] = {
	{ .r = 0x00, .g = 0x00, .b = 0x00 }, // 0 Black
    { .r = 0x80, .g = 0x00, .b = 0x00 }, // 1 Dark Red
    { .r = 0xFF, .g = 0x00, .b = 0x00 }, // 2 Red
    { .r = 0x80, .g = 0x40, .b = 0x00 }, // 3 Dark Orange
    { .r = 0xFF, .g = 0x80, .b = 0x00 }, // 4 Orange
    { .r = 0xFF, .g = 0xFF, .b = 0x00 }, // 5 Yellow
    { .r = 0x80, .g = 0xFF, .b = 0x00 }, // 6 Light Green-Yellow
    { .r = 0x00, .g = 0xFF, .b = 0x00 }, // 7 Green
    { .r = 0x00, .g = 0x80, .b = 0x00 }, // 8 Dark Green
    { .r = 0x00, .g = 0x80, .b = 0x40 }, // 9 Dark Turquoise
    { .r = 0x00, .g = 0xFF, .b = 0x80 }, // 10 Turquoise
    { .r = 0x00, .g = 0xFF, .b = 0xFF }, // 11 Cyan
    { .r = 0x00, .g = 0x80, .b = 0xFF }, // 12 Light Blue
    { .r = 0x00, .g = 0x00, .b = 0xFF }, // 13 Blue
    { .r = 0x00, .g = 0x00, .b = 0x80 }, // 14 Dark Blue
    { .r = 0x40, .g = 0x00, .b = 0x80 }, // 15 Violet
    { .r = 0x80, .g = 0x00, .b = 0xFF }, // 16 Purple
    { .r = 0xFF, .g = 0x00, .b = 0xFF }, // 17 Magenta
    { .r = 0xFF, .g = 0x00, .b = 0x80 }, // 18 Hot Pink
    { .r = 0xFF, .g = 0x40, .b = 0x80 }, // 19 Light Pink
    { .r = 0x80, .g = 0x00, .b = 0x40 }, // 20 Dark Pink
    { .r = 0xFF, .g = 0x80, .b = 0x80 }, // 21 Peach
    { .r = 0x80, .g = 0x40, .b = 0x40 }, // 22 Dark Peach
    { .r = 0xFF, .g = 0xA5, .b = 0x00 }, // 23 Orange-Red
    { .r = 0xFF, .g = 0xC0, .b = 0x00 }, // 24 Gold
    { .r = 0xFF, .g = 0xE0, .b = 0x00 }, // 25 Light Yellow
    { .r = 0x80, .g = 0x80, .b = 0x00 }, // 26 Olive
    { .r = 0x80, .g = 0xFF, .b = 0x80 }, // 27 Light Green
    { .r = 0x40, .g = 0x80, .b = 0x40 }, // 28 Forest Green
    { .r = 0x80, .g = 0x80, .b = 0x80 }, // 29 Gray
    { .r = 0xC0, .g = 0xC0, .b = 0xC0 }, // 30 Silver
    { .r = 0xFF, .g = 0xFF, .b = 0xFF }, // 31 White
    { .r = 0x80, .g = 0x00, .b = 0x80 }, // 32 Dark Purple
    { .r = 0x00, .g = 0x40, .b = 0x40 }, // 33 Dark Cyan
    { .r = 0x00, .g = 0x80, .b = 0x80 }, // 34 Teal
    { .r = 0x40, .g = 0xFF, .b = 0xFF }, // 35 Light Cyan
    { .r = 0x40, .g = 0xFF, .b = 0x80 }, // 36 Spring Green
    { .r = 0x80, .g = 0xFF, .b = 0xC0 }, // 37 Pale Green
    { .r = 0xC0, .g = 0xFF, .b = 0x80 }, // 38 Light Lime
    { .r = 0xFF, .g = 0xFF, .b = 0x80 }, // 39 Light Yellow-Green
    { .r = 0xFF, .g = 0x80, .b = 0xFF }, // 40 Light Magenta
    { .r = 0x80, .g = 0x40, .b = 0xFF }, // 41 Indigo
    { .r = 0x40, .g = 0x40, .b = 0x80 }, // 42 Midnight Blue
    { .r = 0x80, .g = 0x80, .b = 0x40 }, // 43 Olive Drab
    { .r = 0xC0, .g = 0x80, .b = 0x00 }, // 44 Dark Orange
    { .r = 0x40, .g = 0x40, .b = 0x00 }, // 45 Dark Olive
    { .r = 0x80, .g = 0x40, .b = 0x00 }, // 46 Saddle Brown
    { .r = 0xFF, .g = 0x80, .b = 0x40 }, // 47 Coral
    { .r = 0xFF, .g = 0xC0, .b = 0xC0 }, // 48 Light Pink
    { .r = 0x40, .g = 0x00, .b = 0x40 }, // 49 Dark Violet
    { .r = 0xFF, .g = 0x40, .b = 0xC0 }, // 50 Fuchsia
    { .r = 0xFF, .g = 0x00, .b = 0xC0 }, // 51 Deep Pink
    { .r = 0xC0, .g = 0x00, .b = 0xC0 }, // 52 Dark Magenta
    { .r = 0x40, .g = 0x00, .b = 0xC0 }, // 53 Royal Purple
    { .r = 0x00, .g = 0x40, .b = 0xFF }, // 54 Dodger Blue
    { .r = 0x00, .g = 0x80, .b = 0xC0 }, // 55 Sky Blue
    { .r = 0x00, .g = 0x40, .b = 0x80 }, // 56 Steel Blue
    { .r = 0xC0, .g = 0xFF, .b = 0xFF }, // 57 Light Sky Blue
    { .r = 0xFF, .g = 0xFF, .b = 0xC0 }, // 58 Light Goldenrod
    { .r = 0xFF, .g = 0xE0, .b = 0xC0 }, // 59 Light Salmon
    { .r = 0xFF, .g = 0xA5, .b = 0xFF }, // 60 Light Lavender
    { .r = 0xC0, .g = 0x00, .b = 0x80 }, // 61 Mulberry
    { .r = 0xC0, .g = 0xFF, .b = 0xC0 }, // 62 Honeydew
    { .r = 0x80, .g = 0xFF, .b = 0xFF }
};



// Startet den LED Task
void StartLED(void);

void RgbLed_ChangeSettings(char *Access, char *NoAccess, char *Scanning, bool UseRGB_LED);

// Funktionen zur Steuerung der LED-Farben basierend auf dem Zustand
void RgbLedCertsLoaded(void);
void RgbLedETHAppStarted(void);
void RgbLedWifiProvisioningStarted(void);
void RgbLedAWSConnected(void);
void RgbLedETHConnected(void);
void RgbLedHasAccess(bool access);
void RGBLEDScanning(void);
void RgbLedOTAUpdateDone(void);
void RgbLedOTAUpdateIncomming(void);


#endif /* MAIN_LEDSTRIP_H_ */
