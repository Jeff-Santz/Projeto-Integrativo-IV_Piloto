#ifndef ESP_OT_CLI
#define ESP_OT_CLI

#include <string.h>             // strcpy, memcpy, strlen
#include <time.h>               // time(), localtime(), strftime
#include "cJSON.h"              // JSON encode/decode
#define TAG_CLI "ot_esp_cli"        // configura TAG

#include "esp_log.h"            // ESP_LOGI, ESP_LOGE
#include "esp_err.h"            // ESP_ERROR_CHECK

#include "esp_netif.h"          // Interface de rede
#include "esp_event.h"          // Loop de eventos
#include "esp_vfs_eventfd.h"    // Comunicação via eventfd (necessário ao OpenThread)

#include "freertos/FreeRTOS.h"  // Base do FreeRTOS
#include "freertos/task.h"      // xTaskCreate, vTaskDelay

// --- OpenThread core ---
#include "esp_openthread.h"
#include "esp_openthread_cli.h"
#include "esp_openthread_lock.h"
#include "esp_openthread_netif_glue.h"
#include "esp_ot_config.h"

// --- OpenThread API ---
#include "openthread/instance.h"
#include "openthread/ip6.h"
#include "openthread/coap.h"
#include "openthread/message.h"
#include "openthread/thread.h"

#include "sdkconfig.h" // se usar algo do tipo CONFIG precisa ter

#include "openthread/cli.h"
#include "openthread/logging.h"

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

extern otInstance *global_ot_instance;
extern sensor_data_t sensor_data;

void coap_send_task(void *pvParameters);
esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config);
void configure_thread_network(otInstance *instance);
void coap_handler(void *aContext, otMessage *message, const otMessageInfo *messageInfo);
void ot_task_worker(void *aContext);
void ot_enable(void);
void ot_disable(void);

#endif