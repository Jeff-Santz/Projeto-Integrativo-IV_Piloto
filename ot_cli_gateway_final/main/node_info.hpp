// node_info.hpp
#pragma once

#include <string>
#include "sensor_data.hpp"

class NodeInfo {
public:
    std::string endereco;
    sensor_data_t dados;
    uint32_t last_update_ms;

    NodeInfo(const std::string &addr, const sensor_data_t &d, uint32_t ts)
        : endereco(addr), dados(d), last_update_ms(ts) {}
};