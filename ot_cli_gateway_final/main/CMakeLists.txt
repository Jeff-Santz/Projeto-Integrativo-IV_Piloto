set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_register(
     SRCS 
          "main.cpp" 
          "sensor_collect.cpp" 
          "cJSON.c" 
          "esp_ot_cli.cpp" 
          "wifi_connect.cpp"
          "http_request.cpp"
          "node_table.cpp"
     INCLUDE_DIRS 
          "."
     REQUIRES 
        # --- Seus drivers de sensores ---
        sensor_temp-umiA
        sensor_gases
        sensor_umiS

        # --- Infraestrutura ESP-IDF ---
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        esp_system
        freertos

        # --- OpenThread ---
        openthread
        esp_ot_cli_extension

        # --- lwIP (sockets, DNS, etc) ---
        lwip
)