/*
 * NFC.c
 *
 *  Created on: 12 Jul 2024
 *      Author: macra
 */


#include <inttypes.h>
#include "rc522.h"
#include <stdio.h>
#include <stdint.h>
#include "string.h"
#include "esp_log.h"

#include "NFC.h"
#include "Piepser.h"
#include "sntpTime.h"
#include "ledStrip.h"

#include "demo_tasks/sub_pub_unsub_demo/sub_pub_unsub_demo.h"

static const char* TAG = "NFC READ";
static rc522_handle_t scanner;

char uid_string[11];
uint64_t uid_decimal = 000000000000;

bool TagScanned = false;

bool bNFCStarted = false;

bool NFCStarted()
{
    return bNFCStarted;
}

char* NFC_TagScanned()
{
    if(TagScanned)
    {
        return uid_string;
    }
    else
    {
        return NULL;
    }
}

uint64_t PrintDecUID()
{
    return uid_decimal;
}

void DestroyNFC(void)
{
    bNFCStarted = false;
    if (scanner != NULL) {
        esp_err_t ret = rc522_destroy(scanner);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "NFC reader destroyed successfully.");
            scanner = NULL;  // Setze den Handle auf NULL, da er jetzt ungültig ist
        } else {
            ESP_LOGE(TAG, "Failed to destroy NFC reader.");
        }
    }
}

//Converts The SN to Hex
//@param UID in Decimal
//@param UID in HEX as String
void convert_uid_to_string(uint64_t uid_decimal, char* uid_string) 
{
	uint8_t uid[5];

    // Umwandlung der Dezimalzahl in ein Byte-Array
    uid[0] = (uid_decimal >> 32) & 0xFF;
    uid[1] = (uid_decimal >> 24) & 0xFF;
    uid[2] = (uid_decimal >> 16) & 0xFF;
    uid[3] = (uid_decimal >> 8) & 0xFF;
    uid[4] = uid_decimal & 0xFF;

    // Konvertierung der UID-Bytes in einen Hex-String
    snprintf(uid_string, 11, "%02X%02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3], uid[4]);
}

//Handler for NFC waits until a tag is scanned and Prints the the UID
static void rc522_handler(void* arg, esp_event_base_t base, int32_t event_id, void* event_data)
{
    rc522_event_data_t* data = (rc522_event_data_t*) event_data;

    switch(event_id) {
        case RC522_EVENT_TAG_SCANNED: 
            {
                rc522_tag_t* tag = (rc522_tag_t*) data->ptr;
                //ESP_LOGI(TAG, "Tag scanned (sn: %" PRIu64 ")", tag->serial_number);
                uid_decimal = tag->serial_number;

                convert_uid_to_string(uid_decimal, uid_string);

                //ESP_LOGI(TAG, "Time: %s  Serial Number in Hex: %s",sntpGetTIme(), uid_string);
                ScanningSound();
                RGBLEDScanning();

				//Send Data To AWS
                prvSendUIDToAWS(uid_string);
                TagScanned = true;

            }
        case RC522_EVENT_NONE:
            {
                ESP_LOGI(TAG, "Waiting for Tag");
                TagScanned = false;
            }
            break;
    }
}

void StartNFC(void)
{
    rc522_config_t config = {0};
        config.spi.host = SPI3_HOST;
        config.spi.miso_gpio = 15;
        config.spi.mosi_gpio = 16;
        config.spi.sck_gpio = 17;
        config.spi.sda_gpio = 18;

    

    esp_err_t ret = rc522_create(&config, &scanner);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "rc522_create FAILED!");
    }
    ESP_LOGI(TAG, "rc522_create Success!");
    ret = rc522_register_events(scanner, RC522_EVENT_ANY, rc522_handler, NULL);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "rc522_register_events FAILED!");
    }
    ESP_LOGI(TAG, "rc522_register_events Success!");
    ret = rc522_start(scanner);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "rc522_start FAILED!");
    }
    ESP_LOGI(TAG, "rc522_start Success!");
    bNFCStarted = true;
}


/*
#include <esp_log.h>
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"
#include <hal/spi_types.h>

static const char *TAG = "rc522-basic-example";

char uid_string[11];
uint64_t uid_decimal = 000000000000;

bool TagScanned = false;

bool bNFCStarted = false;

#define RC522_SPI_BUS_GPIO_MISO    (15)
#define RC522_SPI_BUS_GPIO_MOSI    (16)
#define RC522_SPI_BUS_GPIO_SCLK    (17)
#define RC522_SPI_SCANNER_GPIO_SDA (18)
#define RC522_SCANNER_GPIO_RST     (-1) // soft-reset

static rc522_spi_config_t driver_config = {
    .host_id = SPI3_HOST,
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = RC522_SPI_BUS_GPIO_MISO,
        .mosi_io_num = RC522_SPI_BUS_GPIO_MOSI,
        .sclk_io_num = RC522_SPI_BUS_GPIO_SCLK,
    },
    .dev_config = {
        .spics_io_num = RC522_SPI_SCANNER_GPIO_SDA,
    },
    .rst_io_num = RC522_SCANNER_GPIO_RST,
};

static rc522_driver_handle_t driver;
static rc522_handle_t scanner;

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        rc522_picc_print(picc);
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Card has been removed");
    }
}

void StartNFC()
{
    ESP_LOGI(TAG, "rc522_spi_create");
    rc522_spi_create(&driver_config, &driver);
    ESP_LOGI(TAG, "rc522_driver_install");
    rc522_driver_install(driver);

    rc522_config_t scanner_config = {
        .driver = driver,
    };
    ESP_LOGI(TAG, "rc522_create");
    rc522_create(&scanner_config, &scanner);
    ESP_LOGI(TAG, "rc522_register_events");
    rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_picc_state_changed, NULL);
    ESP_LOGI(TAG, "rc522_start");
    rc522_start(scanner);
}

bool NFCStarted()
{
    return bNFCStarted;
}

char* NFC_TagScanned()
{
    if(TagScanned)
    {
        return uid_string;
    }
    else
    {
        return NULL;
    }
}

uint64_t PrintDecUID()
{
    return uid_decimal;
}

void DestroyNFC(void)
{
    bNFCStarted = false;
    if (scanner != NULL) {
        esp_err_t ret = rc522_destroy(scanner);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "NFC reader destroyed successfully.");
            scanner = NULL;  // Setze den Handle auf NULL, da er jetzt ungültig ist
        } else {
            ESP_LOGE(TAG, "Failed to destroy NFC reader.");
        }
    }
}*/