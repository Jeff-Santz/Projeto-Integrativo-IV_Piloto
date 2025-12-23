#define TAG "main.c"        // configura TAG

#include "esp_log.h"            // ESP_LOGI, ESP_LOGE
#include "esp_err.h"            // ESP_ERROR_CHECK
#include "nvs_flash.h"          // nvs_flash_init()

#include "esp_netif.h"          // Interface de rede
#include "esp_event.h"          // Loop de eventos
#include "esp_vfs_eventfd.h"    // Comunicação via eventfd (necessario ao OpenThread)

#include "freertos/FreeRTOS.h"  // Base do FreeRTOS
#include "freertos/task.h"      // xTaskCreate, vTaskDelay
#include "esp_sleep.h"

// --- OpenThread core ---
#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_ot_config.h"
#include "esp_mac.h"

#include "sdkconfig.h" // se usar algo do tipo CONFIG precisa ter
#include "sensor_data.h"
#include "sensor_collect.h"

#define PINO_INICIALIZACAO 18

#define intervaloSono 30   // tempo dormindo

void ot_task_worker(void *aContext);
void ot_enable();
void ot_disable();

otInstance *global_ot_instance;
sensor_data_t sensor_data;



void app_main(void)
{
    // 1. Reseta o pino para garantir que não tem lixo de configuração
    gpio_reset_pin(PINO_INICIALIZACAO);
    
    // 2. Define o pino como SAÍDA (Output)
    gpio_set_direction(PINO_INICIALIZACAO, GPIO_MODE_OUTPUT);
    gpio_set_level(PINO_INICIALIZACAO, 1);
    // 1) Inicializacao
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 3,
    };
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    // 2) Sensores
    // sensors_enable(global_ot_instance, &sensor_data);
    // vTaskDelay(pdMS_TO_TICKS(3000));
    // sensors_disable();

    // 3) Inicializa OpenThread + CoAP
    ot_enable();
    vTaskDelay(pdMS_TO_TICKS(70000)); // TEM QUE TER NO MININO 70s
    // ot_disable();

    ESP_LOGI(TAG, " ===== Ciclo finalizado - Indo dormir... ===== ");
    
    vTaskDelay(pdMS_TO_TICKS(15000));

    // 4) Deep Sleep
    esp_sleep_enable_timer_wakeup((uint64_t)intervaloSono * 1000000ULL);
    esp_deep_sleep_start();
}