idf_component_register(
     SRCS 
          "main.c" 
          "sensor_collect.c" 
          "cJSON.c" 
          "esp_ot_cli.c" 
         
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