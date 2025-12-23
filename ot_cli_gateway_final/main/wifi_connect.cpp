#include "wifi_connect.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" 
#include "driver/gpio.h"
#define WIFI_SSID      "Isadora :)"
#define WIFI_PASS      "dufa8704"
// #define WIFI_SSID      "Jeff"
// #define WIFI_PASS      "Jefferson10"
// #define WIFI_SSID      "GD03"
// #define WIFI_PASS      "Gd#03!@8"
#define TAG "wifi_connect"

static const gpio_num_t PINO_HTTP = GPIO_NUM_19;

esp_netif_t *wifi_netif;

int restartControl = 0;

void  init_sntp(); // Atualiza horario

void interfaceWifi() {
    // Cria interface de rede WiFi Station (cliente)
    wifi_netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(wifi_netif, "Isadora");
    assert(wifi_netif);

    // Configuracao padrao do driver WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Configuracao WiFi finalizada - Aguardando conexao...");
}

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Quando o WiFi interface inicia, tenta conectar
        esp_wifi_connect();
        ESP_LOGI(TAG, "Conectando ao Wi-Fi...");
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {

        if (restartControl == 0) {
            ESP_LOGW(TAG, "Wi-Fi desconectado! (modo desligado)");
            return;
        }

        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Reiniciando para conectar ao Wi-Fi! Razão: %d", event->reason);

       // --- Recria a interface e reinicializa tudo ---
        wifi_netif = esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        interfaceWifi();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Quando obtem IP - CONEXAO BEM SUCEDIDA
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Conectado! IP obtido: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Habilita o Wi-Fi
void wifi_enable(void) {
    gpio_reset_pin(PINO_HTTP);
    gpio_set_direction(PINO_HTTP, GPIO_MODE_OUTPUT);
    gpio_set_level(PINO_HTTP, 1);

    if (wifi_netif) {
        ESP_LOGW(TAG, "Wi-Fi já está ativo.");
        return;
    }    

    restartControl = 1;

    // Cria interface de rede WiFi Station (cliente)
    wifi_netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(wifi_netif, "Isadora");
    assert(wifi_netif);

    // Configuracao padrao do driver WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Registra handlers de eventos - CRITICO PARA RECONEXÃO
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    // Handler para eventos gerais do WiFi (conexao, desconexao, etc.)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    
    // Handler específico para quando obtém IP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Configura credenciais WiFi
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // Aplica configurações e inicia WiFi
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_connect();
    ESP_LOGI(TAG, "Wi-Fi ativado e tentando conectar...");

    init_sntp(); // Atualiza horario
}

// Desabilita o Wi-Fi
void wifi_disable(void) {
    restartControl = 0;

    // Para o Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());

    // Cancela registro dos handlers (evita callbacks futuros)
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);

    esp_netif_destroy(wifi_netif);
    
    esp_netif_destroy_default_wifi(wifi_netif);   // precisa manter o handle global!
    gpio_set_level(PINO_HTTP, 0);
}