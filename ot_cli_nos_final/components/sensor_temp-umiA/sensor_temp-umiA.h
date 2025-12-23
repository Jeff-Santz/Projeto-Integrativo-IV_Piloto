#ifndef SENSOR_TEMP_UMIA_H
#define SENSOR_TEMP_UMIA_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>

extern TaskHandle_t AHTTaskHandle;

// Task principal
void AHTtask(void *pvParams);

// Getters
float AHT_GetTemperature(void);
float AHT_GetHumidity(void);

#endif
