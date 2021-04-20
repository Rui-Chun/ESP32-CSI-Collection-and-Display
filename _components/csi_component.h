#ifndef ESP32_CSI_CSI_COMPONENT_H
#define ESP32_CSI_CSI_COMPONENT_H

#include "time_component.h"
#include "math.h"

char *project_type;

#define CSI_RAW 1
#define CSI_AMPLITUDE 0
#define CSI_PHASE 0

#define CSI_TYPE CSI_RAW

void _wifi_csi_cb(void *ctx, wifi_csi_info_t *data) {
    wifi_csi_info_t d = data[0];
    char mac[20] = {0};
    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", d.mac[0], d.mac[1], d.mac[2], d.mac[3], d.mac[4], d.mac[5]);

    printf("CSI_DATA,");
    printf("%s,", project_type);
    printf("%s,", mac);

    // https://github.com/espressif/esp-idf/blob/9d0ca60398481a44861542638cfdc1949bb6f312/components/esp_wifi/include/esp_wifi_types.h#L314
    printf("%d,", d.rx_ctrl.rssi);
    printf("%d,", d.rx_ctrl.rate);
    printf("%d,", d.rx_ctrl.sig_mode);
    printf("%d,", d.rx_ctrl.mcs);
    printf("%d,", d.rx_ctrl.cwb);
    printf("%d,", d.rx_ctrl.smoothing);
    printf("%d,", d.rx_ctrl.not_sounding);
    printf("%d,", d.rx_ctrl.aggregation);
    printf("%d,", d.rx_ctrl.stbc);
    printf("%d,", d.rx_ctrl.fec_coding);
    printf("%d,", d.rx_ctrl.sgi);
    printf("%d,", d.rx_ctrl.noise_floor);
    printf("%d,", d.rx_ctrl.ampdu_cnt);
    printf("%d,", d.rx_ctrl.channel);
    printf("%d,", d.rx_ctrl.secondary_channel);
    printf("%d,", d.rx_ctrl.timestamp);
    printf("%d,", d.rx_ctrl.ant);
    printf("%d,", d.rx_ctrl.sig_len);
    printf("%d,", d.rx_ctrl.rx_state);

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

void _print_csi_csv_header() {
    char *header_str = "type,role,mac,rssi,rate,sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,real_time_set,real_timestamp,len,CSI_DATA\n";
    printf(header_str);
}

void csi_init(char *type, wifi_csi_cb_t cb_func_ptr) {
    project_type = type;

    ESP_ERROR_CHECK(esp_wifi_set_csi(1));

    // @See: https://github.com/espressif/esp-idf/blob/master/components/esp_wifi/include/esp_wifi_types.h#L401
    wifi_csi_config_t configuration_csi;
    configuration_csi.lltf_en = 1;
    configuration_csi.htltf_en = 1;
    configuration_csi.stbc_htltf2_en = 1;
    configuration_csi.ltf_merge_en = 1;
    configuration_csi.channel_filter_en = 0;
    configuration_csi.manu_scale = 0;

    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&configuration_csi));
    if ( (void*)cb_func_ptr == NULL ) {
        // default callback, works but not optimal
        ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(&_wifi_csi_cb, NULL));
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(cb_func_ptr, NULL));
    }

    _print_csi_csv_header();
}

#endif //ESP32_CSI_CSI_COMPONENT_H
