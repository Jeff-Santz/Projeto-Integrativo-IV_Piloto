#include "http_request.hpp"
#include "esp_ot_cli.hpp"
#include "node_table.hpp"
#include <string>
#include <sstream>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#define TAG_HTTP "http_request.c"
//--------------------------------------------------------------------------------------------------------------------------------
// HTTP POST - Variáveis de controle
TaskHandle_t http_task_handle = NULL;
bool s_http_active = false;
volatile bool http_shutdown_requested = false;

#define WEB_SERVER "cmindustries.loca.lt"
#define WEB_PORT "80"
#define POST_PATH "/data"

void http_send_all_now()
{
    ESP_LOGI(TAG_HTTP, "Iniciando envio HTTP síncrono...");

    // ==========================
    // 1) Envia SEU próprio nó
    // ==========================
    extern sensor_data_t sensor_data;

    char* json_me = create_sensor_json(&sensor_data);
    if (json_me) {
        ESP_LOGI(TAG_HTTP, "Enviando meu próprio nó...");
        enviar_uma_requisicao_http(sensor_data.endereco, json_me);
        free(json_me);
    }

    // ==========================
    // 2) Envia TODOS os nós do vector
    // ==========================
    const auto& tabela = getTabelaNodos();
    ESP_LOGI(TAG_HTTP, "Enviando %d nós do vector...", (int)tabela.size());

    for (const auto& entry : tabela) {

        char* json = create_sensor_json(&entry.dados);
        if (json) {
            ESP_LOGI(TAG_HTTP, "Enviando nó: %s", entry.endereco.c_str());
            enviar_uma_requisicao_http(entry.endereco.c_str(), json);
            free(json);
        }
    }

    ESP_LOGI(TAG_HTTP, "Envio HTTP síncrono concluído!");
}

void enviar_uma_requisicao_http(const char *origem, const char *payload) {
    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    struct in_addr *addr;
    int s, r;
    char recv_buf[128];

    // 1. INCREASE BUFFER SIZE (Just in case payload grows)
    char req_buffer[1024]; 

    // 2. CRITICAL: CLEAR MEMORY BEFORE USE
    // This ensures no "garbage" bytes (like 0x8b) exist in the buffer
    memset(req_buffer, 0, sizeof(req_buffer));

    // 3. Format the HTTP Request
    // Note: We check the result of snprintf to ensure we didn't truncate the message
    int len = snprintf(req_buffer, sizeof(req_buffer),
             "POST %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: ESP32-Gateway\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "X-Origem: %s\r\n"
             "\r\n"
             "%s",
             POST_PATH, WEB_SERVER, (int)strlen(payload), origem, payload);

    // Safety check: Did the message fit in the buffer?
    if (len < 0 || len >= sizeof(req_buffer)) {
        ESP_LOGE(TAG_HTTP, "Error: Request buffer too small for payload!");
        return;
    }

    // --- DNS Lookup ---
    int err = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG_HTTP, "DNS lookup failed err=%d res=%p", err, res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }

    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(TAG_HTTP, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

    s = socket(res->ai_family, res->ai_socktype, 0);
    if (s < 0) {
        ESP_LOGE(TAG_HTTP, "Falha ao criar socket.");
        freeaddrinfo(res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }

    if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG_HTTP, "Falha ao conectar ao servidor.");
        close(s);
        freeaddrinfo(res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }
    freeaddrinfo(res);

    // 4. SEND DATA
    // Use the length calculated by snprintf, not strlen(req_buffer)
    // This is slightly more efficient and safer.
    if (write(s, req_buffer, len) < 0) {
        ESP_LOGE(TAG_HTTP, "Erro ao enviar POST.");
        close(s);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        return;
    }

    // Set timeout
    struct timeval receiving_timeout = {5, 0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout));

    // Read Response
    ESP_LOGI(TAG_HTTP, "Lendo resposta...");
    do {
        bzero(recv_buf, sizeof(recv_buf));
        r = read(s, recv_buf, sizeof(recv_buf)-1);
        for (int i = 0; i < r; i++) {
            putchar(recv_buf[i]);
        }
    } while (r > 0);

    ESP_LOGI(TAG_HTTP, "\n... leitura concluída.");
    close(s);
}

void http_post_task(void *pvParameters)
{
    ESP_LOGI(TAG_HTTP, "HTTP task iniciada");

    http_task_handle = xTaskGetCurrentTaskHandle();
    s_http_active = true;

    // No http_post_task, antes do loop:
    debug_tabela_nodos();

    while (!http_shutdown_requested) {

        // ==========================
        // 1) Envia SEU próprio nó
        // ==========================
        extern sensor_data_t sensor_data;   // já existe no seu projeto
        char* json_me = create_sensor_json(&sensor_data);

        if (json_me) {
            enviar_uma_requisicao_http(sensor_data.endereco, json_me);
            free(json_me);
        }

        // ==========================
        // 2) Envia TODOS os nós do vector
        // ==========================
        for (const auto& entry : getTabelaNodos()) {
            char* json = create_sensor_json(&entry.dados);
            if (json) {
                enviar_uma_requisicao_http(entry.endereco.c_str(), json);
                free(json);
            }
        }

        // Envia a cada X segundos
        for (int i = 0; i < 100 && !http_shutdown_requested; i++)
            vTaskDelay(pdMS_TO_TICKS(100));  // total: 10s
    }

    ESP_LOGI(TAG_HTTP, "HTTP task encerrando.");
    http_task_handle = NULL;
    s_http_active = false;
    http_shutdown_requested = false;

    vTaskDelete(NULL);
}

//--------------------------------------------------------------------------------------------------------------------------------
void http_enable(void) {
    if (s_http_active) {
        ESP_LOGW(TAG_HTTP, "HTTP já está ativo.");
        return;
    }

    http_shutdown_requested = false;

    // DNS de fallback
    ip_addr_t dns_primary;
    IP4_ADDR(&dns_primary.u_addr.ip4, 8, 8, 8, 8);
    dns_primary.type = IPADDR_TYPE_V4;
    dns_setserver(0, &dns_primary);

     // 4.2 - Inicia HTTP task (vai esperar OpenThread ficar pronto)
    if(xTaskCreate(http_post_task,        // Função HTTP
                "http_post",           // Nome da tarefa  
                8192,                  // Stack size
                NULL,                  // ⭐⭐ NÃO passa instance aqui!
                4,                     // Prioridade (menor que OpenThread)
                &http_task_handle) != pdPASS) { // Handle
        ESP_LOGE(TAG_HTTP, "Falha ao criar HTTP task");
        return;      
    }

    // Aguarda ate a task iniciar
    const TickType_t timeout = pdMS_TO_TICKS(3000);
    TickType_t t0 = xTaskGetTickCount();
    while (!s_http_active && (xTaskGetTickCount() - t0) < timeout) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (s_http_active) {
        ESP_LOGI(TAG_HTTP, "HTTP ativado com sucesso.");
    } else {
        ESP_LOGE(TAG_HTTP, "Falha ao ativar HTTP - timeout");
    }
}

void http_disable(void) {
    if (!s_http_active) return;

    // 2) Sinaliza para task de envio CoAP parar
    http_shutdown_requested = true;

    // 3) Aguarda a task encerrar normalmente
    const TickType_t wait_ms = pdMS_TO_TICKS(3000);
    TickType_t start = xTaskGetTickCount();

    while (http_task_handle != NULL && (xTaskGetTickCount() - start) < wait_ms) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 4) Se não encerrou a tempo, força
    if (http_task_handle != NULL) {
        ESP_LOGW(TAG_HTTP, "HTTP task não finalizou; deletando forçadamente.");
        vTaskDelete(http_task_handle);
        http_task_handle = NULL;
    }

    // 5) Reseta estado
    s_http_active = false;
    http_shutdown_requested = false;

    ESP_LOGI(TAG_HTTP, "HTTP desativado completamente.");
}