idf_component_register(SRCS "main.c"
                    REQUIRES freertos esp_wifi esp_netif nvs_flash esp_http_client driver
                    INCLUDE_DIRS ".")
