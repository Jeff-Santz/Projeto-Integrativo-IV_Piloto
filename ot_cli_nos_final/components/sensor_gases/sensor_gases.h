#ifndef SENSOR_GASES_H
#define SENSOR_GASES_H

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

extern TaskHandle_t SensorGasesTaskHandle;

int gas_get_raw(void);
float gas_get_voltage(void);
float gas_get_rs(void);
int gas_get_digital(void);
int gas_get_air_quality_index(void);
float gas_get_ppm_estimate(void);
void SensorGasesTask(void *pvParams);

extern TaskHandle_t SensorGasesTaskHandle;

#endif
