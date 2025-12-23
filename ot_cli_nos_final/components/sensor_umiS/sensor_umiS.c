#include "sensor_umiS.h"
#include <esp_log.h>

static const char *TAG_TDS = "TDS";

#define VREF_MV 3300.0
#define ADC_MAX 4095.0

#define TDS_MIN_MV 19.0
#define TDS_MAX_MV 110.0

static float last_percent = 0.0f;
static float last_voltage_mv = 0.0f;

float UmiS_GetTDS(void)
{
    return last_percent;
}

float UmiS_GetVoltage(void)
{
    return last_voltage_mv;
}

static float convert_mv_to_percent(float mv)
{
    if (mv < TDS_MIN_MV) mv = TDS_MIN_MV;
    if (mv > TDS_MAX_MV) mv = TDS_MAX_MV;

    return (mv - TDS_MIN_MV) * (100.0f / (TDS_MAX_MV - TDS_MIN_MV));
}

void SensorUmiSTask(void *pvParams)
{
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(TDS_ADC_CHANNEL, TDS_ATTEN);

    while (1) {
        int raw = adc1_get_raw(TDS_ADC_CHANNEL);

        last_voltage_mv = ((float)raw / ADC_MAX) * VREF_MV;

        last_percent = convert_mv_to_percent(last_voltage_mv);

        ESP_LOGI(TAG_TDS,
                    "ADC Raw = %d | Voltage = %.2f mV | Soil Humidity = %.1f%%",
                    raw, last_voltage_mv, last_percent);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
