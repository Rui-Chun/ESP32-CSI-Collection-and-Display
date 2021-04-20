#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"

// #include "../../_components/nvs_component.h"
// #include "../../_components/sd_component.h"
#include "../../_components/csi_component.h"
#include "../../_components/time_component.h"
// #include "../../_components/input_component.h"
// #include "../../_components/sockets_component.h"

/*
 * The examples use WiFi configuration that you can set via 'make menuconfig'.
 *
 * If you'd rather not, just change the below entries to strings with
 * the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
 */
#define LEN_MAC_ADDR 20
#define EXAMPLE_ESP_WIFI_SSID      "ESP32-AP"
#define EXAMPLE_ESP_WIFI_PASS      "esp32-ap"
#define EXAMPLE_ESP_WIFI_CHANNEL   1
#define EXAMPLE_MAX_STA_CONN       16

#define CSI_QUEUE_SIZE             16

// TODO: a wrapper for csi info?
// typedef struct {
//     uint8_t src_addr;
    
// } csi_entry_t;


static const char *TAG = "CSI collection (AP)";

static xQueueHandle csi_info_queue;

static const uint8_t PEER_NODE_NUM = 2; // self is also included.
static const char peer_mac_list[8][20] = {
    "3c:61:05:4c:36:cd", // esp32 official dev board 0, as soft ap
    "3c:61:05:4c:3c:28", // esp32 official dev board 1
    "",
};

static void csi_handler_task(void *pvParameter);

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);
}

void wifi_csi_cb(void *ctx, wifi_csi_info_t *data);

void app_main() {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    // init wifi as soft-ap
    wifi_init_softap();

    // init queue
    csi_info_queue = xQueueCreate(CSI_QUEUE_SIZE, sizeof(wifi_csi_info_t));
    if (csi_info_queue == NULL) {
        ESP_LOGE(TAG, "Create queue fail");
        return;
    }
    // register callback that push csi info to the queue
    csi_init("AP", &wifi_csi_cb);

    // start another task to handle CSI data
    xTaskCreate(csi_handler_task, "csi_handler_task", 4096, NULL, 4, NULL);
}


int is_peer_node (uint8_t mac[6]) {
    char mac_str[20] = {0};
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    // ESP_LOGI(TAG, "Potential peer mac = %s", mac_str);
    for (int p = 0; p < PEER_NODE_NUM; p++) {
        // ESP_LOGI(TAG, "Testing peer mac = %s, ret = %d", peer_mac_list[p], strcmp(mac_str, peer_mac_list[p]));
        if ( strcmp(mac_str, peer_mac_list[p])==0 )
            return 1;
    }
    return 0;
}

/* Callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task.
 * According to ESPNOW example. Makes sense. */
void wifi_csi_cb(void *ctx, wifi_csi_info_t *data) {
    // Done: filtering out packets accroding to mac addr.
    if (!is_peer_node(data->mac)) {
        ESP_LOGI(TAG, "Non-peer node csi filtered.");
        return;
    }

    // This callback pushs a csi entry to the queue.
    wifi_csi_info_t local_csi_info;
    if (data == NULL) {
        ESP_LOGE(TAG, "Receive csi cb arg error");
        return;
    }

    // copy content from wifi task to local
    memcpy(&local_csi_info, data, sizeof(wifi_csi_info_t));
    assert(local_csi_info.len == data->len);
    // malloc buf
    local_csi_info.buf = malloc(local_csi_info.len);
    if (local_csi_info.buf == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(local_csi_info.buf, data->buf, local_csi_info.len);
    // csi info will be copied to the queue, but the buf is still pointing to what we allocated above. 
    if (xQueueSend(csi_info_queue, &local_csi_info, portMAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send entry to queue fail");
        free(local_csi_info.buf);
    }

}


static void csi_handler_task(void *pvParameter) {
    wifi_csi_info_t local_csi;
    // we use a max delay. so we keep waiting for new entry.
    while (xQueueReceive(csi_info_queue, &local_csi, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG, "New CSI Info Recv!");
        // TODO: send a udp packet to host computer.
    }
    ESP_LOGI(TAG, "CSI Queue Time out!");
}

void print_csi (wifi_csi_info_t *data, char* payload) {
    wifi_csi_info_t d = *data;
    char mac[20] = {0};

    // data description
    sprintf(payload, "CSI_DATA from Soft-AP\n");
    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);
    // src mac addr
    sprintf(payload, "%s\n", mac);

    // https://github.com/espressif/esp-idf/blob/9d0ca60398481a44861542638cfdc1949bb6f312/components/esp_wifi/include/esp_wifi_types.h#L314
    // rx_ctrl info
    sprintf(payload, "%d,", d.rx_ctrl.rssi);
    sprintf(payload, "%d,", d.rx_ctrl.rate);
    sprintf(payload, "%d,", d.rx_ctrl.sig_mode);
    sprintf(payload, "%d,", d.rx_ctrl.mcs);
    sprintf(payload, "%d,", d.rx_ctrl.cwb);
    sprintf(payload, "%d,", d.rx_ctrl.smoothing);
    sprintf(payload, "%d,", d.rx_ctrl.not_sounding);
    sprintf(payload, "%d,", d.rx_ctrl.aggregation);
    sprintf(payload, "%d,", d.rx_ctrl.stbc);
    sprintf(payload, "%d,", d.rx_ctrl.fec_coding);
    sprintf(payload, "%d,", d.rx_ctrl.sgi);
    sprintf(payload, "%d,", d.rx_ctrl.noise_floor);
    sprintf(payload, "%d,", d.rx_ctrl.ampdu_cnt);
    sprintf(payload, "%d,", d.rx_ctrl.channel);
    sprintf(payload, "%d,", d.rx_ctrl.secondary_channel);
    sprintf(payload, "%d,", d.rx_ctrl.timestamp);
    sprintf(payload, "%d,", d.rx_ctrl.ant);
    sprintf(payload, "%d,", d.rx_ctrl.sig_len);
    sprintf(payload, "%d,\n", d.rx_ctrl.rx_state);
    // new line

    char *resp = time_string_get();
    printf("%d,", real_time_set);
    printf("%s,", resp);
    free(resp);

    int8_t *my_ptr;

#if CSI_RAW
    printf("%d,[", data->len);
    my_ptr = data->buf;

    for (int i = 0; i < 128; i++) {
        printf("%d ", my_ptr[i]);
    }
    printf("]");
#endif
#if CSI_AMPLITUDE
    printf("%d,[", data->len);
    my_ptr = data->buf;

    for (int i = 0; i < 64; i++) {
        printf("%.4f ", sqrt(pow(my_ptr[i * 2], 2) + pow(my_ptr[(i * 2) + 1], 2)));
    }
    printf("]");
#endif
#if CSI_PHASE
    printf("%d,[", data->len);
    my_ptr = data->buf;

    for (int i = 0; i < 64; i++) {
                printf("%.4f ", atan2(my_ptr[i*2], my_ptr[(i*2)+1]));
            }
    printf("]");
#endif
    printf("\n");
    // sd_flush();
    vTaskDelay(0);
}