#define TAG "main.c"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_vfs_eventfd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_sleep.h"

// --- OpenThread core ---
#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_ot_config.h"
#include "esp_mac.h"

#include "sdkconfig.h"
#include "sensor_data.hpp"
#include "sensor_collect.hpp"
#include "http_request.hpp"

// Declarações de funções
void ot_task_worker(void *aContext);
void wifi_enable(void);
void wifi_disable(void);
void ot_enable();
void ot_disable();
void sensors_enable(otInstance *instance, sensor_data_t *data);
void sensors_disable(void);
void http_send_all_now();
void debug_tabela_nodos();

otInstance *global_ot_instance;
sensor_data_t sensor_data;

// Timer para alternância periódica
TimerHandle_t alternancia_timer;

static const gpio_num_t PINO_INICIALIZACAO = GPIO_NUM_18;

// Task de alternância
void alternancia_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Alternância: Desativando Thread para envio WiFi");
    
    // 1. Desativa Thread
    ot_disable();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Espera um pouco para liberar o rádio
    
    // 2. Ativa WiFi
    wifi_enable();
    vTaskDelay(pdMS_TO_TICKS(5000)); // Espera conectar
    
    // 3. Coleta dados do gateway (se tiver sensores)
    sensors_enable(global_ot_instance, &sensor_data); // Se quiser coletar do gateway
    vTaskDelay(pdMS_TO_TICKS(2000));
    sensors_disable();
    
    // 4. Envia dados (gateway + nós)
    http_send_all_now();
    
    // 5. Desativa WiFi
    wifi_disable();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Espera desligar o rádio
    
    // 6. Reativa Thread
    ot_enable();
    
    ESP_LOGI(TAG, "Alternância: Concluída, Thread reativada");
    
    vTaskDelete(NULL);
}

// Callback do timer
static void timer_callback(TimerHandle_t xTimer)
{
    xTaskCreate(alternancia_task, "alternancia", 8192, NULL, 5, NULL);
}

extern "C" void app_main(void)
{
    // Configura pino LED
    gpio_reset_pin(PINO_INICIALIZACAO);
    gpio_set_direction(PINO_INICIALIZACAO, GPIO_MODE_OUTPUT);
    gpio_set_level(PINO_INICIALIZACAO, 1);

    // Configura e inicializa netif e eventos
    ESP_ERROR_CHECK(nvs_flash_init());
    esp_log_level_set("wifi", ESP_LOG_VERBOSE);  // Logs detalhados do WiFi
    esp_log_level_set("*", ESP_LOG_INFO);        // Logs normais para outros componentes
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 3,  // Número máximo de file descriptors para eventos
    };
    ESP_ERROR_CHECK(esp_vfs_eventfd_register(&eventfd_config));

    // Inicia Open Thread
    ot_enable();

    // Cria um timer periodico a cada 5 minutos (300000 ms)
    alternancia_timer = xTimerCreate(
        "AlternanciaTimer",
        pdMS_TO_TICKS(120000),  // 30 segundos
        pdTRUE,                 // Auto-reload
        (void *)0,              // ID do timer
        timer_callback
    );

    if (alternancia_timer != NULL) {
        xTimerStart(alternancia_timer, 0);
    }
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // A cada 10 segundos, por exemplo
    }
}