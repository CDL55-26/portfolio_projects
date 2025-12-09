#include "wifi_transmitter.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_client.h"
#include "esp_websocket_client.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>
#include <stdint.h>
#include "esp_mac.h" // Required for MAC address printing

#define AP_SSID          "WBS_Server"
#define AP_PASS          "12345678"
#define MAX_STA_CONN     4

static char response_buffer[1024];
static int buffer_len = 0;

static esp_websocket_client_handle_t ws_client = NULL;
static wifi_remote_command_cb_t s_cmd_callback = NULL;

static const char *TAG = "wifi_transmitter";

void wifi_register_cmd_callback(wifi_remote_command_cb_t cb); 

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d", MAC2STR(event->mac), event->aid);
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_AP_STAIPASSIGNED) {
        ip_event_ap_staipassigned_t *event = (ip_event_ap_staipassigned_t *)event_data;
        ESP_LOGW(TAG, "Client connected! IP assigned: " IPSTR, IP2STR(&event->ip));
    }
}

static esp_err_t wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_AP_STAIPASSIGNED,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = 1,
            .password = AP_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    if (strlen(AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started. SSID: %s Password: %s", AP_SSID, AP_PASS);
    
    return ESP_OK;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            buffer_len = 0;
            break;
        case HTTP_EVENT_ON_DATA:
            if ((buffer_len + evt->data_len) < sizeof(response_buffer)) {
                memcpy(response_buffer + buffer_len, evt->data, evt->data_len);
                buffer_len += evt->data_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            response_buffer[buffer_len] = '\0';
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t wifi_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
      return ret;
    }

    if (wifi_init_softap() != ESP_OK) {
      ESP_LOGE(TAG, "Connection failed");
      return ESP_FAIL;
    }

  return ESP_OK;
}

void wifi_post_resonant_frequencies(const resonant_frequency_tuple *buffer, uint16_t count)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *points = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "points", points);

    for (uint16_t i = 0; i < count; i++) {
        const resonant_frequency_tuple *t = &buffer[i];

        cJSON *point = cJSON_CreateObject();
        cJSON_AddNumberToObject(point, "index",     t->frequency_index);
        cJSON_AddNumberToObject(point, "frequency", t->frequency); 

        cJSON_AddItemToArray(points, point);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    ESP_LOGI(TAG, "Posting JSON: %s", json_str);

    esp_http_client_config_t config = {
        .url = "http://192.168.4.2:8000/resonance", 
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d",
                 esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }
    
    cJSON_free(json_str);
    cJSON_Delete(root);
    esp_http_client_cleanup(client);
}

static void ws_event_handler(void *arg, esp_event_base_t base,
                             int32_t event_id, void *event_data)
{
    switch (event_id) {

    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WebSocket Connected");
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WebSocket Disconnected");
        break;

    case WEBSOCKET_EVENT_DATA: {
        esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
        
        if (data->op_code == 0x1 && data->data_len > 0) {
            
            char command_buffer[32];
            int copy_len = data->data_len < 31 ? data->data_len : 31;
            memcpy(command_buffer, data->data_ptr, copy_len);
            command_buffer[copy_len] = '\0';

            ESP_LOGI(TAG, "Received Command: %s", command_buffer);

            if (s_cmd_callback != NULL) {
                s_cmd_callback(command_buffer);
            }
        }
        break;
    }

    default:
        break;
    }
}

esp_err_t wifi_ws_start(void)
{
    esp_websocket_client_config_t cfg = {
        .uri = "ws://192.168.4.2:8000/esp",
        .buffer_size = 1024,
    };

    ws_client = esp_websocket_client_init(&cfg);
    if (!ws_client) {
        ESP_LOGE(TAG, "Failed to init ws_client");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK( esp_websocket_register_events(
        ws_client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL ) );

    if (esp_websocket_client_start(ws_client) != ESP_OK) {
      return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WebSocket client started");

    return ESP_OK;
}

void wifi_ws_send_datapoint(uint32_t freq, int16_t sample)
{
    if (!ws_client || !esp_websocket_client_is_connected(ws_client))
        return; 

    char msg[64];
    int len = snprintf(msg, sizeof(msg),
                       "DATA,%lu,%d",
                       (unsigned long)freq,
                       (int)sample);

    esp_websocket_client_send_text(ws_client, msg, len, portMAX_DELAY);
}

void wifi_ws_send_resonance(uint32_t freq, int16_t sample)
{
    char msg[64];
    int len = snprintf(msg, sizeof(msg),
                       "RES,%lu,%d", freq, sample);

    esp_websocket_client_send_text(ws_client, msg, len, portMAX_DELAY);
}

void wifi_register_cmd_callback(wifi_remote_command_cb_t cb) {
    s_cmd_callback = cb;
}
