#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"


// #include "../../_components/nvs_component.h"
// #include "../../_components/sd_component.h"
#include "../../_components/csi_component.h"
// #include "../../_components/time_component.h"
// #include "../../_components/input_component.h"
// #include "../../_components/sockets_component.h"

/*
 * The examples use WiFi configuration that you can set via 'make menuconfig'.
 *
 * If you'd rather not, just change the below entries to strings with
 * the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
 */
#define LEN_MAC_ADDR 20
// #define EXAMPLE_ESP_WIFI_SSID      "StudiosL2F"
// #define EXAMPLE_ESP_WIFI_PASS      "sjb13359133"
#define EXAMPLE_ESP_WIFI_SSID      "ESP32-AP"
#define EXAMPLE_ESP_WIFI_PASS      "esp32-ap"
#define EXAMPLE_ESP_MAXIMUM_RETRY   10

#define CSI_QUEUE_SIZE             32
// #define HOST_IP_ADDR               "192.168.4.2" // the ip addr of the host computer.
#define HOST_UDP_PORT              8848



static const char *TAG = "CSI collection (Client)";

static char *target_host_ipv4 = NULL;

static xQueueHandle csi_info_queue;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static void csi_handler_task(void *pvParameter);
void wifi_csi_cb(void *ctx, wifi_csi_info_t *data);
void parse_csi (wifi_csi_info_t *data, char* payload);

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}


static char* generate_hostname(void)
{
    uint8_t mac[6];
    char   *hostname;
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", "ESP32_AP", mac[3], mac[4], mac[5])) {
        abort();
    }
    return hostname;
}

static void initialise_mdns(void)
{
    char* hostname = generate_hostname();
    //initialize mDNS
    ESP_ERROR_CHECK( mdns_init() );
    //set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK( mdns_hostname_set(hostname) );
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", hostname);
    //set default mDNS instance name
    // ESP_ERROR_CHECK( mdns_instance_name_set(EXAMPLE_MDNS_INSTANCE) );

    //initialize service
    // ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, 3) );

    free(hostname);
}

esp_err_t ping_start()
{
    static esp_ping_handle_t ping_handle = NULL;
    esp_ping_config_t ping_config        = {
        .count           = 0,
        .interval_ms     = 100,
        .timeout_ms      = 1000,
        .data_size       = 1,
        .tos             = 0,
        .task_stack_size = 4096,
        .task_prio       = 0,
    };

    esp_netif_ip_info_t local_ip;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &local_ip);
    ESP_LOGI(TAG, "got ip:" IPSTR ", gw: " IPSTR, IP2STR(&local_ip.ip), IP2STR(&local_ip.gw));
    inet_addr_to_ip4addr(ip_2_ip4(&ping_config.target_addr), (struct in_addr *)&local_ip.gw);

    esp_ping_callbacks_t cbs = { 0 };
    esp_ping_new_session(&ping_config, &cbs, &ping_handle);
    esp_ping_start(ping_handle);

    return ESP_OK;
}

void app_main() {
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    // init wifi as a station
    wifi_init_sta();

    // init queue
    csi_info_queue = xQueueCreate(CSI_QUEUE_SIZE, sizeof(wifi_csi_info_t));
    if (csi_info_queue == NULL) {
        ESP_LOGE(TAG, "Create queue fail");
        return;
    }
    // register callback that push csi info to the queue
    csi_init("AP", &wifi_csi_cb);

    // init mDNS
    initialise_mdns();

    // start ping the gateway
    ping_start();

    // start another task to handle CSI data
    xTaskCreate(csi_handler_task, "csi_handler_task", 4096, NULL, 4, NULL);
}


/* Callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task.
 * According to ESPNOW example. Makes sense. */
void wifi_csi_cb(void *ctx, wifi_csi_info_t *data) {
    // drop non-HT packets to prevent queue from overflowing
    if (data->rx_ctrl.sig_mode == 0) {
        // ESP_LOGI(TAG, "Non-HT packet csi filtered.");
        return; 
    }

    // if the host is not ready.
    if (target_host_ipv4 == NULL) {
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
    ESP_LOGI(TAG, "CSI info pushed to queue");

}

static int query_mdns_host(const char * host_name)
{
    ESP_LOGI(TAG, "Query A: %s.local", host_name);

    struct esp_ip4_addr addr;
    addr.addr = 0;

    esp_err_t err = mdns_query_a(host_name, 4000,  &addr);
    if(err){
        if(err == ESP_ERR_NOT_FOUND){
            ESP_LOGW(TAG, "%s: Host was not found!", esp_err_to_name(err));
            return -1;
        }
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return -1;
    }

    ESP_LOGI(TAG, "Query A: %s.local resolved to: " IPSTR, host_name, IP2STR(&addr));
    if (-1 == asprintf(&target_host_ipv4, IPSTR, IP2STR(&addr))) {
        abort();
    } 
    return 0;
}

int setup_udp_socket (struct sockaddr_in *dest_addr) {
    int addr_family = 0;
    int ip_protocol = 0;

    if (target_host_ipv4 == NULL) {
        while ( query_mdns_host("RuichunMacBook-Pro") < 0) {
            ESP_LOGW(TAG, "No target host found, try again ...");
        }
    }

    dest_addr->sin_addr.s_addr = inet_addr(target_host_ipv4);
    dest_addr->sin_family = AF_INET;
    dest_addr->sin_port = htons(HOST_UDP_PORT);
    addr_family = AF_INET;
    ip_protocol = IPPROTO_IP;

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return sock;
    }
    ESP_LOGI(TAG, "Socket created, sending to %s:%d", target_host_ipv4, HOST_UDP_PORT);
    return sock;
}

static void csi_handler_task(void *pvParameter) {
    wifi_csi_info_t local_csi;
    struct sockaddr_in dest_addr;
    char* payload;
    int sock;
    sock = setup_udp_socket(&dest_addr);
    
    // we use a max delay. so we keep waiting for new entry.
    while (xQueueReceive(csi_info_queue, &local_csi, portMAX_DELAY) == pdTRUE) {
        // NOTE: Even not connect to a computer, esp32 is still sending serial data of ESP_LOG. 
        //       so turn them off to speed up.
        // ESP_LOGI(TAG, "New CSI Info Recv!");
        // send a udp packet to host computer.
        payload = malloc(2048); // TODO: is this enough?
        memset(payload, 0, 2048);

        // test data
        // sprintf(payload, "test data msg ...");
        parse_csi(&local_csi, payload);

        // send out udp packet
        // Note: when the client sends csi info packets to host computer, it will also trigger packets from router.
        //       This will form a amplifying loop to create many packets. So drop some CSI info packets here.
        int err = 0;
        if (strlen(payload) % 4 == 0) {
            err = sendto(sock, payload, strlen(payload), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        }
        if (err < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            vTaskDelay(100  / portTICK_PERIOD_MS);
        } else {
            ESP_LOGI(TAG, "CSI message sent, payload len = %d", strlen(payload));
        }

        // data must be freed !!!
        free(local_csi.buf);
        free(payload);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "CSI Queue Time out!");
    vTaskDelete(NULL);
}


void parse_csi (wifi_csi_info_t *data, char* payload) {
    wifi_csi_info_t d = *data;
    char mac[20] = {0};

    // data description
    sprintf(payload + strlen(payload), "CSI_DATA from Soft-AP\n");
    sprintf(mac, "%02x:%02x:%02x:%02x:%02x:%02x", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);
    // src mac addr
    sprintf(payload + strlen(payload), "src mac = %s\n", mac);

    // https://github.com/espressif/esp-idf/blob/9d0ca60398481a44861542638cfdc1949bb6f312/components/esp_wifi/include/esp_wifi_types.h#L314
    // rx_ctrl info
    sprintf(payload + strlen(payload), "rx_ctrl info, len = %d\n", 19);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.rssi);      /**< Received Signal Strength Indicator(RSSI) of packet. unit: dBm */
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.rate);      /**< PHY rate encoding of the packet. Only valid for non HT(11bg) packet */
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.sig_mode);  /**< 0: non HT(11bg) packet; 1: HT(11n) packet; 3: VHT(11ac) packet */
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.mcs);       /**< Modulation Coding Scheme. If is HT(11n) packet, shows the modulation, range from 0 to 76(MSC0 ~ MCS76) */
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.cwb);       /**< Channel Bandwidth of the packet. 0: 20MHz; 1: 40MHz */
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.smoothing);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.not_sounding);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.aggregation);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.stbc);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.fec_coding);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.sgi);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.noise_floor); /**< noise floor of Radio Frequency Module(RF). unit: 0.25dBm*/
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.ampdu_cnt);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.channel);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.secondary_channel);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.timestamp);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.ant);
    sprintf(payload + strlen(payload), "%d,", d.rx_ctrl.sig_len);
    sprintf(payload + strlen(payload), "%d,\n", d.rx_ctrl.rx_state);
    // new line

    // show some info on monitor
    ESP_LOGI(TAG, "CSI from %s, buf_len = %d, rssi = %d, rate = %d, sig_mode = %d, mcs = %d, cwb = %d", \
                    mac, d.len, d.rx_ctrl.rssi, d.rx_ctrl.rate, d.rx_ctrl.sig_mode, d.rx_ctrl.mcs, d.rx_ctrl.cwb);

    int8_t *my_ptr;

#if CSI_RAW
    sprintf(payload + strlen(payload), "RAW, len = %d\n", data->len);
    my_ptr = data->buf;

    for (int i = 0; i < data->len; i++) {
        sprintf(payload + strlen(payload), "%d,", my_ptr[i]);
    }
    sprintf(payload + strlen(payload), "\n"); // new line
#endif
#if CSI_AMPLITUDE
    sprintf(payload + strlen(payload), "AMP len = %d \n", data->len/2);
    my_ptr = data->buf;

    for (int i = 0; i < data->len/2; i++) {
        sprintf(payload + strlen(payload), "%.4f, ", sqrt(pow(my_ptr[i * 2], 2) + pow(my_ptr[(i * 2) + 1], 2)));
    }
    sprintf(payload + strlen(payload), "\n");
#endif
#if CSI_PHASE
    sprintf(payload + strlen(payload), "PHASE len = %d \n", data->len/2);
    my_ptr = data->buf;

    for (int i = 0; i < data->len/2; i++) {
        sprintf(payload + strlen(payload), "%.4f, ", atan2(my_ptr[i*2], my_ptr[(i*2)+1]));
    }
    sprintf(payload + strlen(payload), "\n");
#endif
    vTaskDelay(0);
}