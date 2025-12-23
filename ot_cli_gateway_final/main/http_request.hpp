#pragma once

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"  
#include "openthread/instance.h"
#include "openthread/thread.h" 
#include "openthread/tasklet.h" 

// --- Inclui sensores e JSON ---
#include "cJSON.h"
#include "wifi_connect.hpp"
#include "sensor_data.hpp"
#include "sensor_collect.hpp"
#include "esp_ot_cli.hpp"

extern sensor_data_t sensor_data;

void http_send_all_now();
void http_post_task(void *pvParameters);
void http_enable(void);
void http_disable(void);
void enviar_uma_requisicao_http(const char *origem, const char *payload);