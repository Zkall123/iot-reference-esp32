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
#include "esp_log.h"
#include "driver/ledc.h"

#include "Piepser.h"
#include "TasksCommon.h"


//TODO Hängt kode auf darf nicht sein
static const char* TAG = "Buzzer";

TaskHandle_t soundTaskHandle = NULL;

int ScanningFreq = 500;
int AccessFreq = 1500;
int NoAccessFreq = 1000;

bool bUseBuzzer = true;

void makeSoundTask(void *param)
{
    int input = *(int *)param; // Dereferenzieren des Zeigers, um die Frequenz zu erhalten
    free(param);  // Den Speicher freigeben, nachdem der Wert gelesen wurde

    // Konfiguration des PWM-Controllers
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .freq_hz          = input,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    if (ledc_timer_config(&ledc_timer) == ESP_OK) {
        //ESP_LOGI(TAG, "PWM-Timer konfiguriert");
    } else {
        ESP_LOGE(TAG, "PWM-Timer Konfiguration fehlgeschlagen");
    }

    // Konfiguration des PWM-Kanals
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIEZO_GPIO_PIN,
        .duty           = 4096,
        .hpoint         = 0
    };
    if (ledc_channel_config(&ledc_channel) == ESP_OK) {
        //ESP_LOGI(TAG, "PWM-Kanal konfiguriert");
    } else {
        ESP_LOGE(TAG, "PWM-Kanal Konfiguration fehlgeschlagen");
    }

    // Starten des Tons
    ledc_set_duty(ledc_channel.speed_mode, ledc_channel.channel, ledc_channel.duty);
    ledc_update_duty(ledc_channel.speed_mode, ledc_channel.channel);

    // Piepser für eine bestimmte Zeit aktivieren
    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 Sekunde lang piepsen

    // PWM ausschalten (Duty Cycle auf 0 setzen)
    ledc_stop(ledc_channel.speed_mode, ledc_channel.channel, 0);

    // Task-Handle auf NULL setzen, nachdem der Task abgeschlossen ist
    soundTaskHandle = NULL;
    vTaskDelete(NULL); // Task beenden
}

void createSoundTask(int input)
{
    if(bUseBuzzer)
    {
        if (soundTaskHandle == NULL) {  // Prüfen, ob der Task bereits existiert
            int *freq = malloc(sizeof(int));  // Dynamischer Speicher für Frequenz
            if (freq == NULL) {
                ESP_LOGE(TAG, "Speicherallokation fehlgeschlagen");
                return;
            }
            *freq = input;  // Frequenzwert setzen

            if (xTaskCreate(makeSoundTask, "make_sound_task", MakeSoundTaskStackSize, freq, MakeSoundTaskPriority, &soundTaskHandle) != pdPASS) {
                ESP_LOGE(TAG, "Task-Erstellung fehlgeschlagen");
                free(freq);  // Speicher freigeben bei Fehler
            } else {
                //ESP_LOGI(TAG, "Task für makeSound erstellt");
            }
        } else {
            ESP_LOGW(TAG, "Sound-Task läuft bereits.");
        }
    }
}

void ScanningSound()
{
    //ESP_LOGI(TAG, "High Sound Task!");
    createSoundTask(ScanningFreq);
}

void AccessSound()
{
    ESP_LOGI(TAG, "Low Sound Task!");
    createSoundTask(AccessFreq);
}
void NoAccessSound()
{
    ESP_LOGI(TAG, "Low Sound Task!");
    createSoundTask(NoAccessFreq);
}

// Funktion, um den RGB-String zu parsen und die Farben zu setzen
void Sound_ChangeSettings(int Access, int NoAccess, int Scanning, bool UseBuzzer)
{
    bUseBuzzer = UseBuzzer; 
    ESP_LOGI(TAG, "bUseBusser = %s", bUseBuzzer ? "true" : "false");
    AccessFreq = Access;
    NoAccessFreq = NoAccess;
    ScanningFreq = Scanning;
    ESP_LOGI(TAG, "Succesfully changed settings");
}




