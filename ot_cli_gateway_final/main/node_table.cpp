#include "node_table.hpp"
#include "node_table.hpp"
#include "esp_log.h"

static const char *TAG_NODES = "NODE_TABLE";

// Vetor dinamico — CRESCE SOZINHO
static std::vector<NodeInfo> tabela_nodos;

// Adicione em node_table.cpp
void debug_tabela_nodos() {
    ESP_LOGI(TAG_NODES, "=== DEBUG TABELA NODOS (%d nós) ===", tabela_nodos.size());
    for (size_t i = 0; i < tabela_nodos.size(); i++) {
        const auto& n = tabela_nodos[i];
        ESP_LOGI(TAG_NODES, "Nó %d: IP=%s", i, n.endereco.c_str());
        ESP_LOGI(TAG_NODES, "  Temp: %.2f, UAr: %.2f, USolo: %.2f, Part: %.2f",
                n.dados.temperatura, n.dados.umidadeAr, 
                n.dados.umidadeSolo, n.dados.particulas);
    }
}

// Registrar ou atualizar
void registrarNodo(const std::string &ipv6, const sensor_data_t &dados)
{
    uint32_t agora = esp_log_timestamp();

    // Se ja existe → atualiza
    for (auto &n : tabela_nodos) {
        if (n.endereco == ipv6) {
            n.dados = dados;
            n.last_update_ms = agora;
            ESP_LOGI(TAG_NODES, "Atualizado nó: %s", ipv6.c_str());
            return;
        }
    }

    // Se nao existe → cria novo
    tabela_nodos.emplace_back(ipv6, dados, agora);
    ESP_LOGI(TAG_NODES, "Adicionado novo nó: %s", ipv6.c_str());
}

const std::vector<NodeInfo>& getTabelaNodos() {
    return tabela_nodos;
}