#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gap_svc.h"
#include "gatt_svr.h"
#include "robot.h"

static const char *tag = "NimBLE_BLE_PRPH";

void 
bleprph_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void
app_main(void)
{
    int rc; // returned code

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init(); // return
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to init nimble %d ", ret);
        return;
    }
    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_hs_cfg.sm_io_cap = CONFIG_EXAMPLE_IO_TYPE;
#ifdef CONFIG_EXAMPLE_BONDING
    ble_hs_cfg.sm_bonding = 1;
    /* Enable the appropriate bit masks to make sure the keys
     * that are needed are exchanged
     */
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC;
#endif
#ifdef CONFIG_EXAMPLE_MITM
    ble_hs_cfg.sm_mitm = 1;
#endif
#ifdef CONFIG_EXAMPLE_USE_SC
    ble_hs_cfg.sm_sc = 1;
#else
    ble_hs_cfg.sm_sc = 0;
#endif
#ifdef CONFIG_EXAMPLE_RESOLVE_PEER_ADDR
    /* Stores the IRK */
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ID;
#endif

#if MYNEWT_VAL(STATIC_PASSKEY) && NIMBLE_BLE_CONNECT
    /* WARNING: Hardcoded passkey for demonstration only.
     * In production, generate a random passkey per pairing. */
    ble_sm_configure_static_passkey(456789, true);
#endif

#if MYNEWT_VAL(BLE_GATTS)
    rc = gatt_svr_init();
    if (rc != 0) {
        ESP_LOGE(tag, "gatt_svr_init fallo, rc=%d", rc);
        return;
    }
#endif

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    /* Set the default device name. */
    rc = ble_svc_gap_device_name_set("ESP32S3_ROBOT");
    assert(rc == 0);
#endif

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(bleprph_host_task);

    /* Initialize command line interface to accept input from user */
    rc = scli_init();
    if (rc != ESP_OK) 
    {
        ESP_LOGE(tag, "scli_init() failed");
    }
    
    robot_init();
    
#if MYNEWT_VAL(BLE_EATT_CHAN_NUM) > 0
    bearers = 0;
    for (int i = 0; i < MYNEWT_VAL(BLE_EATT_CHAN_NUM); i++) 
    {
        cids[i] = 0;
    }
#endif
    
}
