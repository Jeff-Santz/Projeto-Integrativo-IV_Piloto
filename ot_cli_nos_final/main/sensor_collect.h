#pragma once
#include "sensor_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Handles das tasks dos sensores
extern TaskHandle_t AHTTaskHandle;
extern TaskHandle_t SensorGasesTaskHandle;
extern TaskHandle_t SensorUmiSTaskHandle;

// Estado dos sensores
extern bool sensors_initialized;

// Funcoes
char* create_sensor_json(const sensor_data_t* data);
void collect_sensor_data(otInstance *instance, sensor_data_t* data);
void sensors_enable(otInstance *instance, sensor_data_t *sensor_data);
void sensors_disable();
bool sensors_are_ready(const sensor_data_t *data);