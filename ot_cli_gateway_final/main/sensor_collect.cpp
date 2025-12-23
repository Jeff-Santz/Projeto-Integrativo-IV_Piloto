#include "sensor_data.hpp"
#include "cJSON.h"
#include "esp_sntp.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <vector>

#include "openthread/thread.h"
#include "esp_log.h"

// Includes dos sensores
#include "sensor_umiS.h"   
#include "sensor_temp-umiA.h" 
#include "sensor_gases.h" 
#include <i2cdev.h>

#define TAG_SENSOR "sensor_collect"

// Definicao das variaveis globais
TaskHandle_t SensorUmiSTaskHandle = NULL;

bool sensors_initialized = false;

void init_sntp() {
    ESP_LOGI("SNTP", "Iniciando sincronização de horário...");
    
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");  // servidor NTP público
    sntp_init();

    // Aguarda o SNTP sincronizar (demora alguns segundos)
    time_t now = 0;
    struct tm timeinfo = { 0 };

    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI("SNTP", "Aguardando atualização... (%d/10)", retry);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    // Ajuste de fuso horario ------------------------------
    setenv("TZ", "<03>", 1);  // Brasil UTC-3
    tzset();

    // Recalcula o horario c/fuso
    time(&now);
    localtime_r(&now, &timeinfo);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI("SNTP", "Horário local ajustado: %s", buf);
}

char* create_sensor_json(const sensor_data_t* data) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "e", data->endereco);
    cJSON_AddStringToObject(root, "d", data->dataHora);
    cJSON_AddNumberToObject(root, "t", data->temperatura);
    cJSON_AddNumberToObject(root, "uA", data->umidadeAr);
    cJSON_AddNumberToObject(root, "uS", data->umidadeSolo);
    cJSON_AddNumberToObject(root, "p", data->particulas);

    char *json_string = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_string;
}

// 
void collect_sensor_data(otInstance *instance, sensor_data_t* data) {
    // Endereço Thread (se disponível)
    const otIp6Address *addr = NULL;
    if (instance) {
        addr = otThreadGetMeshLocalEid(instance);
    }
    if (addr) {
        // Converte para string (se sua funcao otIp6AddressToString existir)
        otIp6AddressToString(addr, data->endereco, sizeof(data->endereco));
    } else {
        // Fallback mais amigavel para o JSON
        strncpy(data->endereco, "unknown", sizeof(data->endereco));
        data->endereco[sizeof(data->endereco) - 1] = '\0';
    }

    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    if (timeinfo) {
        strftime(data->dataHora, sizeof(data->dataHora), "%Y-%m-%dT%H:%M:%S", timeinfo);
    } else {
        strncpy(data->dataHora, "1970-01-01T00:00:00", sizeof(data->dataHora));
        data->dataHora[sizeof(data->dataHora) - 1] = '\0';
    }

    // Usa os GETTERS (eles retornam os valores "bonitos")
    data->temperatura = AHT_GetTemperature();
    data->umidadeAr  = AHT_GetHumidity();
    data->umidadeSolo = UmiS_GetTDS();
    data->particulas  = gas_get_ppm_estimate();
}

bool sensors_are_ready(const sensor_data_t *data) {
    // Verifica se os sensores ja tem dados validos (nao zeros)
    return (data->temperatura != 0.0f && 
            data->umidadeAr != 0.0f && 
            data->umidadeSolo != 0.0f && 
            data->particulas != 0.0f);
}

void sensors_enable(otInstance *instance_local, sensor_data_t *sensor_data_local) {
    // Inicializa I2C (AHT20) 
    ESP_ERROR_CHECK(i2cdev_init());

    // TASK AHT20 (Temperatura e Umidade do Ar) 
    if (xTaskCreatePinnedToCore(
        AHTtask,
        "AHT20 Task",
        configMINIMAL_STACK_SIZE * 4,
        NULL,
        3,
        &AHTTaskHandle,
        0
    ) != pdPASS) {
        ESP_LOGE(TAG_SENSOR, "Falha ao criar task AHT20");
        return;
    }

    // TASK MQ135 (Gases/Partículas) 
    if (xTaskCreate(
        SensorGasesTask,
        "SensorGases",
        4096,
        NULL,
        5,
        &SensorGasesTaskHandle
    ) != pdPASS) {
        ESP_LOGE(TAG_SENSOR, "Falha ao criar task MQ135");
        // Limpa task anterior em caso de falha
        if (AHTTaskHandle) {
            vTaskDelete(AHTTaskHandle);
            AHTTaskHandle = NULL;
        }
        return;
    }

    // TASK TDS (Umidade do Solo)
    if (xTaskCreate(
        SensorUmiSTask,
        "SensorUmiS",
        4096,
        NULL,
        5,
        &SensorUmiSTaskHandle
    ) != pdPASS) {
        ESP_LOGE(TAG_SENSOR, "Falha ao criar task TDS");
        // Limpa tasks anteriores em caso de falha
        if (AHTTaskHandle) {
            vTaskDelete(AHTTaskHandle);
            AHTTaskHandle = NULL;
        }
        if (SensorGasesTaskHandle) {
            vTaskDelete(SensorGasesTaskHandle);
            SensorGasesTaskHandle = NULL;
        }
        return;
    }

    // Aguarda os sensores estarem prontos
    ESP_LOGI(TAG_SENSOR, "Aguardando sensores ficarem prontos...");
    
    int attempts = 0;
    const int max_attempts = 3; // 3 segundos no máximo
    
    while (attempts < max_attempts) {
        collect_sensor_data(instance_local, sensor_data_local);
        
        if (sensors_are_ready(sensor_data_local)) {
            ESP_LOGI(TAG_SENSOR, "Sensores prontos após %d tentativas", attempts + 1);
            sensors_initialized = true;
            break;
        }
        
        ESP_LOGD(TAG_SENSOR, "Aguardando sensores... (tentativa %d/%d)", attempts + 1, max_attempts);
        vTaskDelay(pdMS_TO_TICKS(1000));
        attempts++;
    }
}

void sensors_disable(void) {
    ESP_LOGI(TAG_SENSOR, "Desligando sensores...");

    // Para e deleta as tasks
    if (AHTTaskHandle) {
        vTaskDelete(AHTTaskHandle);
        AHTTaskHandle = NULL;
    }
    
    if (SensorGasesTaskHandle) {
        vTaskDelete(SensorGasesTaskHandle);
        SensorGasesTaskHandle = NULL;
    }
    
    if (SensorUmiSTaskHandle) {
        vTaskDelete(SensorUmiSTaskHandle);
        SensorUmiSTaskHandle = NULL;
    }

    // Aqui voce pode adicionar cleanup especifico de cada sensor
    // Exemplo: desligar pinos, colocar em modo sleep, etc.
    
    sensors_initialized = false;
    ESP_LOGI(TAG_SENSOR, "Sensores desligados com sucesso");
}