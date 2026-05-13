#include <string.h>
#include "ble_client.h"
#include "esp_log.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

static const char *TAG = "BLE_CLIENT";

static uint16_t conn_handle = 0;
static uint16_t char_handle = 0;

/* UUIDs del robot */
/*static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0xaa,0xaa,0xaa,0x00,0x00,0x00,0x00,0x00,
                     0xCA,0x00,0x00,0xea,0x00,0xea,0x00,0xea);

static const ble_uuid128_t char_uuid =
    BLE_UUID128_INIT(0xbb,0xbb,0xbb,0xcc,0xcc,0xcc,0xcc,0xaa,
                     0xaa,0xaa,0xaa,0xaa,0xaa,0xea,0x00,0xea);
*/
/* ---------- TASK BLE (ARREGLADO) ---------- */
static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ---------- SCAN ---------- */

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
        case BLE_GAP_EVENT_DISC:
        {
            struct ble_hs_adv_fields fields;
            ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);

            if (fields.name != NULL)
            {
                if (strncmp((char*)fields.name, "ESP32S3_ROBOT", fields.name_len) == 0)
                {
                    ESP_LOGI(TAG, "Robot encontrado -> conectando");

                    ble_gap_disc_cancel();

                    struct ble_gap_conn_params conn_params = {
                        .scan_itvl = 0x0010,
                        .scan_window = 0x0010,
                        .itvl_min = 0x0018,
                        .itvl_max = 0x0028,
                        .latency = 0,
                        .supervision_timeout = 0x0100,
                    };

                    ble_gap_connect(BLE_OWN_ADDR_PUBLIC,
                                    &event->disc.addr,
                                    30000,
                                    &conn_params,
                                    gap_event,
                                    NULL);
                }
            }
            return 0;
        }

        case BLE_GAP_EVENT_CONNECT:
        {
            if (event->connect.status == 0)
            {
                conn_handle = event->connect.conn_handle;
                ESP_LOGI(TAG, "Conectado al robot!");

                // ⚠️ IMPORTANTE: ponemos handle fijo (simplificación práctica)
                char_handle = 12;  // <-- este valor puede cambiar
            }
            else
            {
                ESP_LOGI(TAG, "Fallo al conectar");
            }
            return 0;
        }

        default:
            return 0;
    }
}

/* ---------- START BLE ---------- */

static void ble_app_on_sync(void)
{
    ESP_LOGI(TAG, "Escaneando BLE...");

    struct ble_gap_disc_params disc_params = {
        .itvl = 0,
        .window = 0,
        .filter_policy = 0,
        .passive = 0
    };

    ble_gap_disc(BLE_OWN_ADDR_PUBLIC,
                 0,
                 &disc_params,
                 gap_event,
                 NULL);
}

void ble_client_init(void)
{
    nimble_port_init();

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    nimble_port_freertos_init(ble_host_task);
}

/* ---------- ENVÍO ---------- */

void ble_send(char *msg)
{
    if (conn_handle == 0 || char_handle == 0) {
        ESP_LOGW(TAG, "No conectado aún");
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(msg, strlen(msg));

    int rc = ble_gattc_write_no_rsp(conn_handle, char_handle, om);

    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error enviando: %d", rc);
    }
    else
    {
        ESP_LOGI(TAG, "Enviado: %s", msg);
    }
}