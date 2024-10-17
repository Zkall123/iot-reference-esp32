/*
 * NFC.h
 *
 *  Created on: 17 Jun 2024
 *      Author: macra
 */
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

#ifndef MAIN_NFC_H_
#define MAIN_NFC_H_

// Starts the NFC Loop to wait for a NFC Tag
void StartNFC(void);

//Destroy NFC
void DestroyNFC(void);

//Gives the UID in Decimal
uint64_t PrintDecUID();
char* NFC_TagScanned();

//Converts The SN to Hex
//@param UID in Decimal
//@param UID in HEX as String
void convert_uid_to_string(uint64_t uid_decimal, char* uid_string);

bool NFCStarted();

#endif /* MAIN_NFC_H_ */