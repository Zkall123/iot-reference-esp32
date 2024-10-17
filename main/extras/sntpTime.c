/*
 * sntpTime.h
 *
 *  Created on: 7 Jun 2024
 *      Author: macra
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/apps/sntp.h"

#include "TasksCommon.h"
#include "sntpTime.h"
#include "TimeZones.h"

//TODO Make Timezonechanges working


static const char TAG[] = "sntp_time_sync";

//SNTP operating mode set status
static bool sntpOpModeSet = false;

char* TimeZone = Berlin_Germany;
char* OldTImeZone;

bool bValidTime = false;

bool isTimeValid(void)
{
    return bValidTime;
}

void sntpTimeChangeTimeZone(char* timezone)
{
    TimeZone = timezone;
}

//initialize SNTP service using SNTP_OPMODE_POLL mode
static void sntpTimeSyncInitSntp(void)
{
    ESP_LOGI(TAG, "Initializing the SNTP service");

    if(!sntpOpModeSet)
    {
        // Set the operating mode
		sntp_setoperatingmode(SNTP_OPMODE_POLL);
		sntpOpModeSet = true;
    }
    sntp_setservername(0, "pool.ntp.org");

    //initialize the service
    sntp_init();
}
//Variable um Die zeit zu aktuallisieren falls sie um eine minute versetzt ist
#define TIME_SYNC_THRESHOLD 180//60//1Minute 
static time_t last_sync_time = 0; // Variable to store the last sync time

//gets the current Time and if the current time is not up to date 
//the sntp_time_sync_init_sntp function is caled

static void sntpTimeSyncOptainTime(void)
{
    time_t now = 0;
    struct tm timeInfo = {0};

    time(&now);
    localtime_r(&now, &timeInfo);

    // Check if the time needs to be updated
    if(sntpOpModeSet == false || timeInfo.tm_year < (2016 - 1900) || (now - last_sync_time > TIME_SYNC_THRESHOLD) || TimeZone != OldTImeZone)
    {
        if(sntpOpModeSet == false)ESP_LOGI(TAG, "SNTP mode not set. Initializing SNTP...");
        if(timeInfo.tm_year < (2016 - 1900))ESP_LOGI(TAG, "Time is not set. Initializing SNTP...");
        if((now - last_sync_time > TIME_SYNC_THRESHOLD))ESP_LOGI(TAG, "Time is outdated. Initializing SNTP...");
        if(TimeZone != OldTImeZone)ESP_LOGI(TAG, "Timezone changed. Initializing SNTP...");
        sntpTimeSyncInitSntp();
        // Set the local Timezone
        setenv("TZ", TimeZone, 1);
        tzset();

        // Re-check the time after syncing with SNTP
        time(&now);
        localtime_r(&now, &timeInfo);
        if(timeInfo.tm_year >= (2016 - 1900))
        {
            ESP_LOGI(TAG, "Time successfully updated: %d-%02d-%02d %02d:%02d:%02d", 
                     timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
                     timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
            last_sync_time = now; // Update the last sync time
            sntpOpModeSet = true; // Mark that SNTP is set
        }
        else
        {
            ESP_LOGE(TAG, "Failed to update time.");
            bValidTime = false;
        }
    }
    else
    {
        //ESP_LOGI(TAG, "Current time is valid: %d-%02d-%02d %02d:%02d:%02d", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
        bValidTime = true;
    }
}

void waitForTimeAndExecute(int targetHour, int targetMinute, void (*callback)(void))
{
    while (1)
    {
        time_t now;
        struct tm timeInfo;

        // Hole die aktuelle Zeit
        time(&now);
        localtime_r(&now, &timeInfo);

        // Überprüfen, ob es die gewünschte Uhrzeit ist (z.B. 04:00)
        if (timeInfo.tm_hour == targetHour && timeInfo.tm_min == targetMinute)
        {
            ESP_LOGI(TAG, "Es ist jetzt %02d:%02d Uhr, führe den Code aus.", targetHour, targetMinute);

            // Führe die übergebene Funktion aus
            if (callback != NULL)
            {
                callback();  // Die Funktion wird aufgerufen, wenn die Zeit erreicht wurde
            }

            break;  // Nach Ausführung aus der Schleife ausbrechen
        }

        // Warte 1 Minute, bevor die Zeit erneut überprüft wird
        vTaskDelay(pdMS_TO_TICKS(60000));  // 60000 ms = 1 Minute
    }
}

// Beispiel Callback-Funktion
void executeAtFourAM(void)
{
    ESP_LOGI("Restart", "ESP wird neu gestartet...");
    vTaskDelay(pdMS_TO_TICKS(2000));  // 2 Sekunden Verzögerung, um die Log-Meldung zu sehen
    esp_restart();  // Neustart des ESP32
}


//SNTP Time syncronization task
//@param arg pvParam
static void sntpTimeSync(void *pvParam)
{
    OldTImeZone = TimeZone;

    while(1)
    {
        sntpTimeSyncOptainTime();
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        // Warte bis 04:00 Uhr und führe den Code aus
        waitForTimeAndExecute(10, 00, executeAtFourAM);
    }
    vTaskDelete(NULL);
}

char* sntpGetTIme(void)
{
    static char time_buff[100] = {0};

    time_t now = 0;
    struct tm timeInfo = {0};

    time(&now);
    localtime_r(&now, &timeInfo);

    if(timeInfo.tm_year < (2016 - 1900))
    {
        ESP_LOGI(TAG, "Time is not set yet");  
    }
    else
    {
        strftime(time_buff, sizeof(time_buff), "%d.%m.%Y %H:%M:%S", &timeInfo);
        ESP_LOGI(TAG, "Current time Info: %s", time_buff);
    }
    
    return time_buff;
}

void sntpTimeTaskStart(void)
{
    xTaskCreatePinnedToCore(&sntpTimeSync, "sntpTimeSync", SNTPTaskStackSize, NULL, SNTPTaskPriority, NULL, SNTPTaskCoreID);
}