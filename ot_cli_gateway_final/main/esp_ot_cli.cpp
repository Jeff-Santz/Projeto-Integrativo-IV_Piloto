#include "sensor_data.hpp"
#include "sensor_collect.hpp"
#include "esp_ot_cli.hpp"
#include "node_table.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" 
#include "driver/gpio.h"
#include <string.h>

using namespace std;

static const gpio_num_t PINO_COAP = GPIO_NUM_15;

extern otInstance *global_ot_instance;
otInstance *instance = NULL;
extern sensor_data_t sensor_data;

TaskHandle_t ot_task_handle = NULL;
TaskHandle_t coap_send_task_handle = NULL;
esp_netif_t *openthread_netif;

bool s_ot_active = false;
volatile bool ot_shutdown_requested = false;
volatile bool coap_send_shutdown_requested = false;

// Envia mensagem via CoAP
void coap_send_task(void *pvParameters) {
    otInstance *instance = (otInstance *)pvParameters;

    // Salva handle
    coap_send_task_handle = xTaskGetCurrentTaskHandle();

    while(!coap_send_shutdown_requested) { 
        // Coleta dados atualizadps       
        collect_sensor_data(instance, &sensor_data); // Cria JSON        
        
        //Cria JSON
        char* jsonPayload = create_sensor_json(&sensor_data);
        if (jsonPayload == NULL) {
            ESP_LOGE(TAG_CLI, "Falha ao criar JSON");
            break;
        } 
        ESP_LOGI(TAG_CLI, "Enviando JSON: %s", jsonPayload);
        ESP_LOGI(TAG_CLI, "");

        otMessage *msg = otCoapNewMessage(instance, NULL);
        if(msg) {
            otCoapMessageInit(msg, OT_COAP_TYPE_NON_CONFIRMABLE, OT_COAP_CODE_POST);
            otCoapMessageSetToken(msg, (const uint8_t *)"tk", 2);

            // 1 - Define o URI do recurso do servidor envio
            otCoapMessageAppendUriPathOptions(msg, "sensor");
            
            // 2 - Payload Marker
            otCoapMessageSetPayloadMarker(msg); // <--- ESSENCIAL

            // 3 - Anexa o Payload: 1 byte com o contador
            //const char *payload = "aquiEstouMaisUmDia";            
            //otMessageAppend(msg, payload, strlen(payload));                     
            otMessageAppend(msg, jsonPayload, strlen(jsonPayload));
            
            // 4 - Define o destino - ENDERECO            
            otMessageInfo msgInfo;
            memset(&msgInfo, 0, sizeof(msgInfo));
            // "fdde:ad00:beef:0:0:ff:fe00:3c00"            
            otIp6AddressFromString("ff03::1", &msgInfo.mPeerAddr); 
            // IPv6 do outro nó            
            msgInfo.mPeerPort = OT_DEFAULT_COAP_PORT;
            
            otCoapSendRequest(instance, msg, &msgInfo, NULL, NULL);
        }

        free(jsonPayload);

        // delay, mas saí rapidamente se solicitado
        for (int i = 0; i < 100 && !coap_send_shutdown_requested; ++i) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Cleanup antes de sair
    sensors_disable();

    coap_send_task_handle = NULL;
    vTaskDelete(NULL); // encerra a si mesma
}

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
#include "esp_ot_cli_extension.h"
#endif // CONFIG_OPENTHREAD_CLI_ESP_EXTENSION

esp_netif_t *init_openthread_netif(const esp_openthread_platform_config_t *config)
{
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *netif = esp_netif_new(&cfg);
    assert(netif != NULL);
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_openthread_netif_glue_init(config)));

    return netif;
}

//------------------------Função para configurar dataset Thread com PANID, nome, canal e chave
void configure_thread_network(otInstance *instance)
{
    otOperationalDataset dataset;
    memset(&dataset, 0, sizeof(dataset));

    // Timestamp obrigatório
    dataset.mActiveTimestamp.mSeconds = 1;
    dataset.mComponents.mIsActiveTimestampPresent = true;

    // PAN ID
    dataset.mPanId = 0x7974; // <-- altere aqui se quiser
    dataset.mComponents.mIsPanIdPresent = true;

    // Nome da rede
    const char *networkName = "grupoCM"; // <-- altere aqui
    memcpy(dataset.mNetworkName.m8, networkName, strlen(networkName));
    dataset.mComponents.mIsNetworkNamePresent = true;

    // Canal (válido 11–26 em 2.4GHz)
    dataset.mChannel = 15; // <-- altere aqui
    dataset.mComponents.mIsChannelPresent = true;

    // Network Key (16 bytes hexadecimais)
    const uint8_t key[OT_NETWORK_KEY_SIZE] = {
        0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
        0x88,0x99,0xaa,0xaa,0xcc,0xdd,0xee,0xff
    };
    memcpy(dataset.mNetworkKey.m8, key, sizeof(key));
    dataset.mComponents.mIsNetworkKeyPresent = true;

    // Aplica dataset como ativo
    otDatasetSetActive(instance, &dataset);
}

// ==================== HANDLER  COAP ====================
void coap_handler(void *aContext, otMessage *message, const otMessageInfo *messageInfo)
{
    OT_UNUSED_VARIABLE(aContext);

    char addrString[OT_IP6_ADDRESS_STRING_SIZE];
    otIp6AddressToString(&messageInfo->mPeerAddr, addrString, sizeof(addrString));

    ESP_LOGI("CoAP", "Mensagem recebida de [%s]:%u", addrString, messageInfo->mPeerPort);

    // Lê todo o payload
    uint16_t offset = otMessageGetOffset(message);
    uint16_t msgLength = otMessageGetLength(message);
    uint16_t payloadLen = msgLength - offset;

    if (payloadLen == 0) {
        ESP_LOGE("CoAP", "Mensagem vazia");
        return;
    }

    // Lê o conteúdo
    char buffer[256]; // tamanho razoável pro seu JSON
    if (payloadLen >= sizeof(buffer)) payloadLen = sizeof(buffer) - 1;

    uint16_t bytesRead = otMessageRead(message, offset, buffer, payloadLen);
    buffer[bytesRead] = '\0';

    ESP_LOGI("CoAP", "Payload recebido (%d bytes): %s", bytesRead, buffer);

    // ==================== Interpretando JSON ====================
    cJSON *root = cJSON_Parse(buffer);
    if (!root) {
        ESP_LOGE("CoAP", "Erro ao fazer parse do JSON");
        return;
    }

    // Extrai campos
    const cJSON *endereco = cJSON_GetObjectItemCaseSensitive(root, "e");
    const cJSON *dataHora = cJSON_GetObjectItemCaseSensitive(root, "d");
    const cJSON *temperatura = cJSON_GetObjectItemCaseSensitive(root, "t");
    const cJSON *umidadeAr = cJSON_GetObjectItemCaseSensitive(root, "uA");
    const cJSON *umidadeSolo = cJSON_GetObjectItemCaseSensitive(root, "uS");
    const cJSON *particulas = cJSON_GetObjectItemCaseSensitive(root, "p");

    // Verifica e imprime se existir
    if (cJSON_IsString(endereco) && cJSON_IsString(dataHora)) {
        ESP_LOGI("JSON", "Endereço: %s", endereco->valuestring);
        ESP_LOGI("JSON", "Data/Hora: %s", dataHora->valuestring);
    }
    if (cJSON_IsNumber(temperatura))
        ESP_LOGI("JSON", "Temperatura: %.2f", temperatura->valuedouble);
    if (cJSON_IsNumber(umidadeAr))
        ESP_LOGI("JSON", "Umidade do Ar: %.2f", umidadeAr->valuedouble);
    if (cJSON_IsNumber(umidadeSolo))
        ESP_LOGI("JSON", "Umidade do Solo: %.2f", umidadeSolo->valuedouble);
    if (cJSON_IsNumber(particulas))
        ESP_LOGI("JSON", "Particulas: %.2f", particulas->valuedouble);

    sensor_data_t dados;
    memset(&dados, 0, sizeof(dados));  // limpa tudo

    // Copia strings recebidas
    strncpy(dados.endereco, endereco->valuestring, sizeof(dados.endereco)-1);
    strncpy(dados.dataHora, dataHora->valuestring, sizeof(dados.dataHora)-1);

    // Copia números recebidos
    dados.temperatura  = cJSON_IsNumber(temperatura)   ? temperatura->valuedouble : 0;
    dados.umidadeAr    = cJSON_IsNumber(umidadeAr)     ? umidadeAr->valuedouble : 0;
    dados.umidadeSolo  = cJSON_IsNumber(umidadeSolo)   ? umidadeSolo->valuedouble : 0;
    dados.particulas   = cJSON_IsNumber(particulas)    ? particulas->valuedouble : 0;

    // Registra (usar endereço como key)
    registrarNodo(string(dados.endereco), dados);

    cJSON_Delete(root);
}

// ==================== FUNÇÃO PARA INICIAR THREAD ====================
void start_thread_network(otInstance *instance)
{
    gpio_reset_pin(PINO_COAP);
    gpio_set_direction(PINO_COAP, GPIO_MODE_OUTPUT);
    gpio_set_level(PINO_COAP, 1);

    // Método mais simples - apenas habilita a Thread
    otThreadSetEnabled(instance, true);
        
    // Aguarda um pouco para inicialização
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // Verifica o estado
    otDeviceRole role = otThreadGetDeviceRole(instance);
    
    // Se ainda estiver detached, tenta forçar
    if (role == OT_DEVICE_ROLE_DETACHED) {
        otThreadSetEnabled(instance, false);
        vTaskDelay(pdMS_TO_TICKS(1000));
        otThreadSetEnabled(instance, true);
        
        // Aguarda mais um pouco
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// ==================== TASK WORKER ====================
void ot_task_worker(void *aContext)
{
    esp_openthread_platform_config_t config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };

    // INICIA A THREAD - Initialize the OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&config));

    instance = esp_openthread_get_instance();
    global_ot_instance = instance; // Inicializa o global_ot_instance

    // Configura a rede Thread
    configure_thread_network(instance);  
    
    // INICIA A REDE THREAD
    start_thread_network(instance);

    // Criar e registrar o recurso CoAP
    static otCoapResource coap_resource;
    memset(&coap_resource, 0, sizeof(coap_resource));

    coap_resource.mUriPath = "sensor";
    coap_resource.mHandler = coap_handler;
    coap_resource.mContext = NULL;
    otCoapAddResource(instance, &coap_resource);
    ESP_LOGI(TAG_CLI, "Recurso /sensor adicionado");

    otError err = otCoapStart(instance, OT_DEFAULT_COAP_PORT);
    if(err == OT_ERROR_NONE) {    
        ESP_LOGI(TAG_CLI, "Servidor CoAP iniciado, recurso /sensor/dados");
    } else {
        ESP_LOGE(TAG_CLI, "Falha ao iniciar CoAP: %d", err);    
    }

    // cria task de envio
    xTaskCreate(coap_send_task, "coap_send_task", 4096, instance, 5, NULL);

    // Initialize the esp_netif bindings
    openthread_netif = init_openthread_netif(&config);
    esp_netif_set_default_netif(openthread_netif);

#if CONFIG_OPENTHREAD_STATE_INDICATOR_ENABLE
    ESP_ERROR_CHECK(esp_openthread_state_indicator_init(esp_openthread_get_instance()));
#endif

#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
#endif

#if CONFIG_OPENTHREAD_CLI
    esp_openthread_cli_init();
#endif

#if CONFIG_OPENTHREAD_CLI_ESP_EXTENSION
    esp_cli_custom_command_init();
#endif

#if CONFIG_OPENTHREAD_CLI
    esp_openthread_cli_create_task();
#endif

#if CONFIG_OPENTHREAD_AUTO_START
    otOperationalDatasetTlvs dataset;
    otError error = otDatasetGetActiveTlvs(esp_openthread_get_instance(), &dataset);
    ESP_ERROR_CHECK(esp_openthread_auto_start((error == OT_ERROR_NONE) ? &dataset : NULL));
#endif

    // ============ MAINLOOP COM CONTROLE MANUAL ============
    ot_task_handle = xTaskGetCurrentTaskHandle();
    s_ot_active = true;
    ESP_LOGI(TAG_CLI, "OpenThread iniciado - usando mainloop automático");

    // Esta função é BLOQUEANTE - só retorna quando OpenThread finalizar
    esp_openthread_launch_mainloop();

    // Quando chegar aqui, o OpenThread já foi finalizado
    ESP_LOGI(TAG_CLI, "Mainloop finalizado - fazendo cleanup");
    // ======================================================

    // Cleanup pós-mainloop
    coap_send_shutdown_requested = true;
    
    // Aguarda task de CoAP finalizar brevemente
    vTaskDelay(pdMS_TO_TICKS(500));
    
    if (coap_send_task_handle != NULL) {
        vTaskDelete(coap_send_task_handle);
        coap_send_task_handle = NULL;
    }

    // Limpa variáveis globais
    instance = NULL;
    openthread_netif = NULL;
    ot_task_handle = NULL;
    s_ot_active = false;
    coap_send_shutdown_requested = false;

    vTaskDelete(NULL);
}

void ot_enable(void) {
    if (s_ot_active) {
        ESP_LOGW(TAG_CLI, "OpenThread já está ativo.");
        return;
    }

    ot_shutdown_requested = false;
    coap_send_shutdown_requested = false;

    // ==================== INICIALIZAÇÃO OPENTHREAD ====================    
    // Cria tarefa do OpenThread (comunicação mesh)
    if(xTaskCreate(ot_task_worker,        // Função a ser executada
                "ot_cli_main",         // Nome da tarefa (para debugging)
                10240,                 // Tamanho da stack (bytes)
                NULL,                  // Parâmetros (NULL = sem parâmetros)
                5,                     // Prioridade (5 = média)
                &ot_task_handle)) { // Handle (NULL = não precisamos guardar) 
        ESP_LOGE(TAG_CLI, "Falha ao criar ot_task_worker");
        return;
    }                 
    
    // aguarda até a task setar s_ot_active (ou timeout)
    const TickType_t timeout = pdMS_TO_TICKS(3000);
    TickType_t t0 = xTaskGetTickCount();
    while (!s_ot_active && (xTaskGetTickCount() - t0) < timeout) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Executa mainloop (em task própria normalmente)
    ESP_LOGI(TAG_CLI, "OpenThread ativado.");
    s_ot_active = true;
}

void ot_disable(void)
{
    if (!s_ot_active) return;

    // 1) Para CoAP primeiro
    if (instance) {
        otCoapStop(instance);
    }

    // 2) Sinaliza para task de envio CoAP parar
    coap_send_shutdown_requested = true;

    // 3) **FORMA CORRETA NO ESP-IDF v5: Desabilita a Thread**
    if (instance) {
        otThreadSetEnabled(instance, false);
        otIp6SetEnabled(instance, false);
    }

    // 4) **Chama a função de finalização da plataforma**
    // No ESP-IDF v5, o mainloop termina quando a Thread é desabilitada
    // O cleanup é feito automaticamente quando o mainloop retorna

    // 5) Aguarda um pouco para o mainloop terminar
    vTaskDelay(pdMS_TO_TICKS(1000));

    // 6) Força finalização se necessário
    if (ot_task_handle != NULL) {
        vTaskDelete(ot_task_handle);
        ot_task_handle = NULL;
    }

    // 7) Limpa task de CoAP se ainda existir
    if (coap_send_task_handle != NULL) {
        vTaskDelete(coap_send_task_handle);
        coap_send_task_handle = NULL;
    }

    // 8) Destrói netif
    if (openthread_netif) {
        esp_netif_destroy(openthread_netif);
        openthread_netif = NULL;
    }

    // 9) Reseta estado
    instance = NULL;
    s_ot_active = false;
    coap_send_shutdown_requested = false;
    gpio_set_level(PINO_COAP, 0);
}