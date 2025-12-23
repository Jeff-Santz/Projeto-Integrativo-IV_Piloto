#pragma once

#include <vector>
#include <string>
#include "node_info.hpp"

using namespace std;

static vector<NodeInfo> nodes;

// No http_post_task, antes do loop:
void debug_tabela_nodos();

void registrarNodo(const std::string &ipv6, const sensor_data_t &dados);
const std::vector<NodeInfo>& getTabelaNodos();