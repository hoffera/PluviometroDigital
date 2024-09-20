#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "driver/gpio.h"

#define THINGSPEAK_API_KEY "ZJ3GBSXHUDQW343W"
#define THINGSPEAK_SERVER "api.thingspeak.com"

// Defina suas credenciais Wi-Fi
#define SSID "ZTE_2.4G_UQNEPW" // Substitua pelo seu SSID
#define PASS "frederico123" // Substitua pela sua senha
#define RED_SWITCH_PIN  23

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
int cont = 0;

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        printf("WiFi connecting WIFI_EVENT_STA_START ... \n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        printf("WiFi connected WIFI_EVENT_STA_CONNECTED ... \n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        printf("WiFi lost connection WIFI_EVENT_STA_DISCONNECTED ... \n");
        break;
    case IP_EVENT_STA_GOT_IP:
        printf("WiFi got IP ... \n\n");
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
}

void wifi_connection()
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = SSID,
            .password = PASS
        }
    };
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    esp_wifi_connect();
}

bool check_internet_connection() {
    esp_http_client_config_t config = {
        .url = "http://api.thingspeak.com",
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    return (err == ESP_OK);
}

void send_data_to_thingspeak(float value) {
    char url[128];
    snprintf(url, sizeof(url), "http://%s/update?api_key=%s&field1=%.2f", THINGSPEAK_SERVER, THINGSPEAK_API_KEY, value);

    esp_http_client_config_t config = {
        .url = url,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        printf("Basculou: %.2f\n", value);
    } else {
        printf("Error sending data: %s\n", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}


void periodic_function(void *pvParameters) {
    while (1) {
       
        if(cont>0){
            float value = cont * 50;
        send_data_to_thingspeak(value);
        cont = 0;
        
        }
        
         vTaskDelay(16000 / portTICK_PERIOD_MS);

    }
}


void app_main(void)
{
    wifi_event_group = xEventGroupCreate();
    wifi_connection();
    // Aguarde até que a conexão Wi-Fi seja estabelecida
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xTaskCreate(periodic_function, "Periodic Task", 2048, NULL, 1, NULL);

    // Configurar o pino GPIO
    esp_rom_gpio_pad_select_gpio(RED_SWITCH_PIN);
    gpio_set_direction(RED_SWITCH_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(RED_SWITCH_PIN, GPIO_PULLUP_ONLY); 

   
    
  while (true) {
    // Verifica se o sensor reed switch KY-025 foi ativado
    if (gpio_get_level(RED_SWITCH_PIN) == 0) {
        cont = cont +1;    
         vTaskDelay(1000 / portTICK_PERIOD_MS);  
        }
          printf("Cont:%d \n", cont);
        }
  
}
