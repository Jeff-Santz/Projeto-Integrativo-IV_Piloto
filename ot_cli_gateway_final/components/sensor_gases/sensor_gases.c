#include "sensor_gases.h"
#include <driver/adc.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <math.h>

#define R0_DEFAULT 30000.0f
TaskHandle_t SensorGasesTaskHandle = NULL;

static const char *TAG_gases = "MQ135";

// Pinos do MQ135
#define GAS_ANALOG_PIN   ADC1_CHANNEL_3
#define GAS_DIGITAL_PIN  4

// Conversão ADC
#define VREF 3.3f
#define ADC_MAX 4095.0f

// Resistor RL (confirma seu valor real no hardware!)
#define RL_VALUE 10000.0f

// Valores típicos de Rs do MQ135 em ar limpo e ar poluído
#define RS_CLEAN_AIR   30000.0f   // 30 kΩ típico (ajustável)
#define RS_VERY_BAD      2000.0f  // ~2 kΩ = ar muito poluído (ajustável)

// Getters — valores salvos
static int   last_raw = 0;
static float last_voltage = 0;
static float last_rs = 0;
static int   last_digital = 0;
static int   last_aqi = 0;


// Converte voltagem → Rs
static float calc_rs(float voltage)
{
    if (voltage < 0.01f) voltage = 0.01f; // evita divisão por zero
    float rs = (VREF / voltage - 1.0f) * RL_VALUE;
    return rs;
}

// Converte Rs → AQI (0–500)
static int rs_to_aqi(float rs)
{
    // Limitação do intervalo
    if (rs > RS_CLEAN_AIR) rs = RS_CLEAN_AIR;
    if (rs < RS_VERY_BAD) rs = RS_VERY_BAD;

    // Normaliza para 0–1
    float ratio = (RS_CLEAN_AIR - rs) / (RS_CLEAN_AIR - RS_VERY_BAD);

    // Convete para 0–500
    int aqi = (int)(ratio * 500.0f);

    if (aqi < 0) aqi = 0;
    if (aqi > 500) aqi = 500;

    return aqi;
}

// ---------------- GETTERS ------------------
int gas_get_raw(void)              { return last_raw; }
float gas_get_voltage(void)        { return last_voltage; }
float gas_get_rs(void)             { return last_rs; }
int gas_get_digital(void)          { return last_digital; }
int gas_get_air_quality_index(void){ return last_aqi; }

static float R0 = -1.0f;   // -1 significa "não calibrado ainda"
static bool r0_calibrated = false;

// Calibra R0 rapidamente (5 amostras só)
// Calibração R0 (Movida para ser usada ANTES do loop principal)
static float auto_calibrate_r0(void)
{
    float sum_rs = 0.0f;
    const int calibration_samples = 30; // Aumentamos as amostras para uma média melhor

    ESP_LOGI("MQ135", "Iniciando calibração R0 (%d amostras)...", calibration_samples);

    for (int i = 0; i < calibration_samples; i++) {
        int raw = adc1_get_raw(GAS_ANALOG_PIN);
        float voltage = raw * (VREF / ADC_MAX);
        float rs = calc_rs(voltage);

        sum_rs += rs;
        vTaskDelay(pdMS_TO_TICKS(100)); // 100 ms entre leituras
    }

    float r0_est = sum_rs / (float)calibration_samples;
    ESP_LOGI("MQ135", "R0 AUTO-CALIBRADO = %.1f ohm", r0_est);
    return r0_est;
}

float gas_get_ppm_estimate(void)
{
    // Calibra automaticamente só na primeira chamada
    if (!r0_calibrated)
    {
        float r0_est = auto_calibrate_r0();

        // Segurança: evita valores absurdos
        if (r0_est < 1000 || r0_est > 100000) {
            ESP_LOGE("MQ135", "R0 auto-calibrado está fora do esperado (%.1f). Usando 30000 ohm.", r0_est);
            R0 = r0_est;
        } else {
            R0 = r0_est;
        }

        r0_calibrated = true;
    }

    // Agora calcula ppm normalmente
    float rs = last_rs;
    if (rs < 1.0f) rs = 1.0f;

    float ratio = rs / R0;

    // Proteções
    if (ratio < 0.01f) ratio = 0.01f;
    if (ratio > 10.0f) ratio = 10.0f;

    float ppm = 116.6020682f * powf(ratio, -2.769034857f);

    if (ppm < 0) ppm = 0;
    if (ppm > 50000) ppm = 50000;

    return ppm;
}

// TASK PRINCIPAL ---------------------------------------------------
void SensorGasesTask(void *pvParams)
{
    // Configurações do ADC (correto para ESP32, assumindo ADC1)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(GAS_ANALOG_PIN, ADC_ATTEN_DB_11);

    // Configuração do GPIO Digital (correto)
    gpio_set_direction(GAS_DIGITAL_PIN, GPIO_MODE_INPUT);

    // --- CALIBRAÇÃO R0 ÚNICA VEZ ---
    if (!r0_calibrated)
    {
        float r0_est = auto_calibrate_r0();

        // Validação de segurança do R0
        if (r0_est < 1000 || r0_est > 100000) {
            ESP_LOGE("MQ135", "R0 fora do esperado (%.1f). Usando R0_DEFAULT (%.1f ohm).", r0_est, R0_DEFAULT);
            R0 = R0_DEFAULT;
        } else {
            R0 = r0_est;
        }
        r0_calibrated = true;
    }
    // ------------------------------------

    while (1) {
        // Leitura e média simples para estabilidade (Opção de melhoria)
        int raw_sum = 0;
        const int num_samples = 5;
        for (int i = 0; i < num_samples; i++) {
            raw_sum += adc1_get_raw(GAS_ANALOG_PIN);
            vTaskDelay(pdMS_TO_TICKS(5)); 
        }
        int raw = raw_sum / num_samples;

        // Processamento
        float voltage = raw * (VREF / ADC_MAX);
        int digital_state = gpio_get_level(GAS_DIGITAL_PIN);

        float rs = calc_rs(voltage);
        int aqi = rs_to_aqi(rs);

        // Salva no estado
        last_raw = raw;
        last_voltage = voltage;
        last_digital = digital_state;
        last_rs = rs;
        last_aqi = aqi;

        ESP_LOGI(TAG_gases,
                    "RAW=%d | V=%.3f V | Rs=%.1f Ω | AQI=%d | Digital=%d | R0=%.1f | ppm_est=%.2f",
                    raw, voltage, rs, aqi, digital_state, R0, gas_get_ppm_estimate());

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}