#include "bluetooth.h"


void bluetooth_init(void)
{
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ESP_LOGI("ble", "iniciado bluetooth correctamente");
}