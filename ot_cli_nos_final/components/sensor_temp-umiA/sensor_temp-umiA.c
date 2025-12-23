#include "sensor_temp-umiA.h"

#include <stdio.h>
#include <esp_system.h>
#include <aht.h>
#include <string.h>
#include <esp_log.h>

#define I2C_MASTER_SDA 1
#define I2C_MASTER_SCL 0

#define ADDR AHT_I2C_ADDRESS_GND
#define AHT_TYPE AHT_TYPE_AHT20

TaskHandle_t AHTTaskHandle = NULL;

static const char *TAG_aht25 = "AHT25";

// Variáveis de armazenamento local
static float last_temperature = 0.0f;
static float last_humidity = 0.0f;

// Getters
float AHT_GetTemperature(void)
{
    return last_temperature;
}

float AHT_GetHumidity(void)
{
    return last_humidity;
}

void AHTtask(void *pvParams)
{
    aht_t dev = {0};
    dev.mode = AHT_MODE_NORMAL;
    dev.type = AHT_TYPE;

    ESP_ERROR_CHECK(aht_init_desc(&dev, ADDR, 0, I2C_MASTER_SDA, I2C_MASTER_SCL));
    ESP_ERROR_CHECK(aht_init(&dev));

    bool calibrated;
    ESP_ERROR_CHECK(aht_get_status(&dev, NULL, &calibrated));
    if (calibrated)
        ESP_LOGI(TAG_aht25, "Sensor calibrated");
    else
        ESP_LOGW(TAG_aht25, "Sensor not calibrated!");

    float temperature, humidity;

    while (1) {
        esp_err_t res = aht_get_data(&dev, &temperature, &humidity);
        if (res == ESP_OK)
        {
            last_temperature = temperature;
            last_humidity = humidity;

            ESP_LOGI(TAG_aht25, "Temperature: %.1f°C, Humidity: %.2f%%",
                        temperature, humidity);
        }
        else
        {
            ESP_LOGE(TAG_aht25, "Error reading data: %d (%s)",
                        res, esp_err_to_name(res));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
