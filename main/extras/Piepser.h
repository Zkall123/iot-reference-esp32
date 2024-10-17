/*
 * Piepser.h
 *
 *  Created on: 7 Jun 2024
 *      Author: macra
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

// Definiere den GPIO-Pin, an den der Piezo-Summer angeschlossen ist
#define PIEZO_GPIO_PIN 2

#ifndef Piepser
#define Piepser

void ScanningSound();

void AccessSound();

void NoAccessSound();

void Sound_ChangeSettings(int Access, int NoAccess, int Scanning, bool UseBuzzer);

#endif /* Piepser */
