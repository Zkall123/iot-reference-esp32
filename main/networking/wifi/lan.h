/*
 * lan.h
 *
 *  Created on: 17 Jun 2024
 *      Author: macra
 */
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_log.h"

#ifndef MAIN_LAN_H_
#define MAIN_LAN_H_

/**
 * @brief bool to check if we use Wifi or LAN.
 */
extern bool bUseWifi;

//Callback typedef
typedef void (*eth_connect_event_callback_t)(void);

//Starts ethernet connection
void start_ethernet(void);

bool bIsConnected();

//Sets the callback function
void EthSetCallback(eth_connect_event_callback_t cb);

//calls the callback function
void EthCallCallback();

void vWaitOnETHConnected( void );

//Function to switch between Wifi
void SwitchWifi(bool UseWifi);

char* LanPrintMac();


#endif /* MAIN_LAN_H_ */