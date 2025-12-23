#ifndef SENSOR_UMIS_H
#define SENSOR_UMIS_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDS_ADC_CHANNEL ADC1_CHANNEL_6   // GPIO 6 → ADC6 CH1
#define TDS_ATTEN ADC_ATTEN_DB_11

void SensorUmiSTask(void *pvParams);

// GETTERS
float UmiS_GetTDS(void);
float UmiS_GetVoltage(void);

#ifdef __cplusplus
}
#endif

#endif
