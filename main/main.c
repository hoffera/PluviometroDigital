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
#include <freertos/semphr.h>
#include "freertos/queue.h"

#define THINGSPEAK_API_KEY "T6Q5FMMSL6AZG5U2"
#define THINGSPEAK_SERVER "api.thingspeak.com"

// Defina suas credenciais Wi-Fi
#define SSID "Redmi 9" // Substitua pelo seu SSID
#define PASS "12345678" // Substitua pela sua senha
#define RED_SWITCH_PIN  23

#define WATER_VOLUME_PER_OSCILATION 5.0

static QueueHandle_t gpio_queue = NULL;

static EventGroupHandle_t WifiEventGroup;
const int CONNECTED_BIT = BIT0;
int number_of_oscilations = 0;
SemaphoreHandle_t xMutex;

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    int gpio_num = (int) arg;
    // Envia o número do pino para a fila
    xQueueSendFromISR(gpio_queue, &gpio_num, NULL);
}

static void wifi_event_handler(
    void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id){
        case WIFI_EVENT_STA_START:
            printf("WiFi connecting WIFI_EVENT_STA_START ... \n");
            break;
        case WIFI_EVENT_STA_CONNECTED:
            printf("WiFi connected WIFI_EVENT_STA_CONNECTED ... \n");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            printf("WiFi lost connection WIFI_EVENT_STA_DISCONNECTED ... \n");
            esp_wifi_connect();
            break;
        case IP_EVENT_STA_GOT_IP:
            printf("WiFi got IP ... \n\n");
            xEventGroupSetBits(WifiEventGroup, CONNECTED_BIT);
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

esp_err_t send_data_to_thingspeak(float value) {
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
    return err;
}

void check_number_of_oscilations(void *pvParameters)
{
    while (true) {
        vTaskDelay(16000/portTICK_PERIOD_MS);
        if(number_of_oscilations > 0){
            float value = number_of_oscilations * WATER_VOLUME_PER_OSCILATION;
            esp_err_t result = send_data_to_thingspeak(value);
            if(result == ESP_OK){
                if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
                    number_of_oscilations = 0;
                    xSemaphoreGive(xMutex);
                }
            }
        }
    }
}

void sensor(void *pvParameters) {
    int gpio_num;
    while (true) {
        // Verifica se o sensor reed switch KY-025 foi ativado
        if (xQueueReceive(gpio_queue, &gpio_num, portMAX_DELAY)) {
            printf("Basculou\n");
            if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
                number_of_oscilations++;
                xSemaphoreGive(xMutex);
            }
        }
        vTaskDelay(10/portTICK_PERIOD_MS);
        // number_of_oscilations++;
        // printf("Numero de oscilacoes: %d\n", number_of_oscilations);   
        // vTaskDelay(10000/portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    // Conexão ao wi-fi
    WifiEventGroup = xEventGroupCreate();
    wifi_connection();
    xEventGroupWaitBits(WifiEventGroup, CONNECTED_BIT, false, true, portMAX_DELAY);

    // Configurar o pino GPIO
    esp_rom_gpio_pad_select_gpio(RED_SWITCH_PIN);
    gpio_set_direction(RED_SWITCH_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(RED_SWITCH_PIN, GPIO_PULLUP_ONLY); 
    gpio_set_intr_type(RED_SWITCH_PIN, GPIO_INTR_NEGEDGE);

    gpio_queue = xQueueCreate(10, sizeof(int));
    gpio_install_isr_service(0);
    gpio_isr_handler_add(RED_SWITCH_PIN, gpio_isr_handler, (void*) RED_SWITCH_PIN);

    xMutex = xSemaphoreCreateMutex();
    xTaskCreate(sensor, "Sensor Task", 2048, NULL, 1, NULL);
    xTaskCreate(check_number_of_oscilations, "Number of oscilations task", 4096, NULL, 1, NULL);
}