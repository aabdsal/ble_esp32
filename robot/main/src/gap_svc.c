/**
 * @file    gap_svc.c
 * @author  BLE-SEM
 * @version V0.0
 * @date    2026-05-14
 * @brief   Implementacion del servicio GAP para anuncio y emparejamiento BLE
 */

/* Includes ------------------------------------------------------------------*/

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
// #include "esp_peripheral.h"

#include "gap_svc.h"
#include "gatt_svr.h"

/* Private define ------------------------------------------------------------*/

#define GAP_SVC_IO_CAP BLE_SM_IO_CAP_NO_IO

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static const char *tag = "GAP-BLE";
static int gap_svc_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t own_addr_type;

static bool s_ble_enabled = false;
static portMUX_TYPE ble_mux = portMUX_INITIALIZER_UNLOCKED;

void ble_store_config_init(void);

/**
 * @brief Imprime una descripcion legible de una conexion BLE
 * @param desc Descriptor de conexion proporcionado por NimBLE
 */
static void gap_svc_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle = %d our_ota_addr_type = %d our_ota_addr=",
                desc->conn_handle, desc->our_ota_addr.type);
    MODLOG_DFLT(INFO, " our_id_addr_type = %d our_id_addr=",
                desc->our_id_addr.type);
    MODLOG_DFLT(INFO, " peer_ota_addr_type = %d peer_ota_addr=",
                desc->peer_ota_addr.type);
    MODLOG_DFLT(INFO, " peer_id_addr_type = %d peer_id_addr=",
                desc->peer_id_addr.type);
    MODLOG_DFLT(INFO, " conn_itvl = %d conn_latency = %d supervision_timeout = %d "
                "encrypted = %d authenticated = %d bonded = %d\n",
                desc->conn_itvl, desc->conn_latency,
                desc->supervision_timeout,
                desc->sec_state.encrypted,
                desc->sec_state.authenticated,
                desc->sec_state.bonded);
}

/**
 * @brief Inicia el anuncio BLE con los parametros por defecto de conexion
 * @param None
 * @return None
 */
static void gap_svc_advertise(void)
{
    bool ble_active = gap_svc_get_enabled();
    if (!ble_active)
    {
        return;
    }

    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
    *  Configura los datos incluidos en el anuncio:
    *     o Flags (indican el tipo de anuncio y otra informacion general).
    *     o Potencia de transmision del anuncio.
    *     o Nombre del dispositivo.
    *     o UUIDs de servicio de 16 bits (notificaciones de alerta).
     */

    memset(&fields, 0, sizeof fields);

    /* Anuncia dos banderas:
     *     o Descubribilidad en el anuncio (general)
     *     o Solo BLE (sin soporte BR/EDR).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;
     /* Indica que el campo de potencia de transmision debe incluirse; deja que
      * la pila rellene este valor automaticamente. Esto se hace asignando el
      * valor especial BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]) 
    {
        BLE_UUID16_INIT(GATT_SVR_SVC_ALERT_UUID)
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) 
    {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Inicia el anuncio. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_svc_gap_event, NULL);
    if (rc != 0) 
    {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

/**
 * @brief Gestiona los eventos GAP reportados por NimBLE
 * @param event Carga util del evento
 * @param arg Argumento de la aplicacion, no usado
 * @return 0 cuando el evento se gestiona correctamente, distinto de cero en caso contrario
 */
static int gap_svc_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;
    bool ble_active;

    switch (event->type) 
    {
        case BLE_GAP_EVENT_CONNECT:
            /* Se establecio una nueva conexion o fallo un intento de conexion. */
            MODLOG_DFLT(INFO, "connection %s; status=%d ",
                        event->connect.status == 0 ? "established" : "failed",
                        event->connect.status);
            if (event->connect.status == 0) 
            {
                rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
                assert(rc == 0);
                gap_svc_print_conn_desc(&desc);
            }
            MODLOG_DFLT(INFO, "\n");

            if (event->connect.status != 0) 
            {
                /* La conexion fallo; reanuda el anuncio. */
                portENTER_CRITICAL(&ble_mux);
                ble_active = s_ble_enabled;
                portEXIT_CRITICAL(&ble_mux);
                if (ble_active)
                {
                    gap_svc_advertise();
                }
            }

            return 0;

        case BLE_GAP_EVENT_DISCONNECT:
            MODLOG_DFLT(INFO, "disconnect; reason=%d ", event->disconnect.reason);
            gap_svc_print_conn_desc(&event->disconnect.conn);
            MODLOG_DFLT(INFO, "\n");

            /* La conexion termino; reanuda el anuncio. */
            portENTER_CRITICAL(&ble_mux);
            ble_active = s_ble_enabled;
            portEXIT_CRITICAL(&ble_mux);
            if (ble_active)
            {
                gap_svc_advertise();
            }
            return 0;

        case BLE_GAP_EVENT_CONN_UPDATE:
            /* El central ha actualizado los parametros de conexion. */
            MODLOG_DFLT(INFO, "connection updated; status=%d ",
                        event->conn_update.status);
            rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
            assert(rc == 0);
            gap_svc_print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");
            return 0;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            MODLOG_DFLT(INFO, "advertise complete; reason=%d",
                        event->adv_complete.reason);
            portENTER_CRITICAL(&ble_mux);
            ble_active = s_ble_enabled;
            portEXIT_CRITICAL(&ble_mux);
            if (ble_active)
            {
                gap_svc_advertise();
            }
            return 0;

        case BLE_GAP_EVENT_ENC_CHANGE:
            /* El cifrado se ha activado o desactivado para esta conexion. */
            MODLOG_DFLT(INFO, "encryption change event; status=%d ",
                        event->enc_change.status);
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
            assert(rc == 0);
            gap_svc_print_conn_desc(&desc);
            MODLOG_DFLT(INFO, "\n");
            return 0;

        case BLE_GAP_EVENT_NOTIFY_TX:
            MODLOG_DFLT(INFO, "notify_tx event; conn_handle=%d attr_handle=%d "
                        "status=%d is_indication=%d",
                        event->notify_tx.conn_handle,
                        event->notify_tx.attr_handle,
                        event->notify_tx.status,
                        event->notify_tx.indication);
            return 0;

        case BLE_GAP_EVENT_SUBSCRIBE:
            MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d "
                        "reason=%d prevn=%d curn=%d previ=%d curi=%d\n",
                        event->subscribe.conn_handle,
                        event->subscribe.attr_handle,
                        event->subscribe.reason,
                        event->subscribe.prev_notify,
                        event->subscribe.cur_notify,
                        event->subscribe.prev_indicate,
                        event->subscribe.cur_indicate);
            return 0;

        case BLE_GAP_EVENT_MTU:
            MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d cid=%d mtu=%d\n",
                        event->mtu.conn_handle,
                        event->mtu.channel_id,
                        event->mtu.value);
            return 0;

        case BLE_GAP_EVENT_REPEAT_PAIRING:
            /* Ya existe un enlace con el par, pero intenta establecer una
            * nueva conexion segura. Esta aplicacion sacrifica seguridad por
            * comodidad: elimina el enlace anterior y acepta el nuevo.
            */

            /* Elimina el enlace anterior. */
            rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
            assert(rc == 0);
            ble_store_util_delete_peer(&desc.peer_id_addr);

            /* Devuelve BLE_GAP_REPEAT_PAIRING_RETRY para indicar que el host debe
            * continuar con la operacion de emparejamiento.
            */
            return BLE_GAP_REPEAT_PAIRING_RETRY;

        case BLE_GAP_EVENT_AUTHORIZE:
            MODLOG_DFLT(INFO, "authorize event: conn_handle=%d attr_handle=%d is_read=%d",
                        event->authorize.conn_handle,
                        event->authorize.attr_handle,
                        event->authorize.is_read);

            /* El comportamiento por defecto del evento es rechazar la solicitud de autorizacion */
            event->authorize.out_response = BLE_GAP_AUTHORIZE_REJECT;
            return 0;

    }
    return 0;
}

void gap_svc_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

/**
 * @brief Sincroniza el host y arranca el anuncio BLE
 * @param None
 * @return None
 */
void gap_svc_on_sync(void)
{
    int rc;
    /* Asegura que tengamos configurada una direccion de identidad correcta (publica preferida) */
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Determina la direccion a usar durante el anuncio (sin privacidad por ahora) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) 
    {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    /* Imprime la direccion */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    MODLOG_DFLT(INFO, "\n");

    portENTER_CRITICAL(&ble_mux);
    bool ble_active = s_ble_enabled;
    portEXIT_CRITICAL(&ble_mux);
    if (ble_active)
    {
        /* Inicia el anuncio. */
        gap_svc_advertise();
    }
}

/**
 * @brief Punto de entrada de la tarea del host de NimBLE
 * @param param Parametro de tarea no usado
 * @return None
 */
void gap_svc_host_task(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");
    /* Esta funcion solo retornara cuando se ejecute nimble_port_stop() */
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void gap_svc_start(void)
{
    nimble_port_freertos_init(gap_svc_host_task);
}

void gap_svc_set_enabled(bool enabled)
{
    portENTER_CRITICAL(&ble_mux);
    s_ble_enabled = enabled;
    portEXIT_CRITICAL(&ble_mux);

    if (!ble_hs_synced())
    {
        return;
    }

    if (enabled)
    {
        if (!ble_gap_adv_active())
        {
            gap_svc_advertise();
        }
        return;
    }

    if (ble_gap_adv_active())
    {
        int rc = ble_gap_adv_stop();
        if (rc != 0)
        {
            ESP_LOGW(tag, "ble_gap_adv_stop fallo, rc=%d", rc);
        }
    }
}

bool gap_svc_get_enabled(void)
{
    bool ble_active;
    portENTER_CRITICAL(&ble_mux);
    ble_active = s_ble_enabled;
    portEXIT_CRITICAL(&ble_mux);
    return ble_active;
}

void gap_svc_set_device_name(const char * name)
{
    int rc = ble_svc_gap_device_name_set(name);
    
    if (rc != 0) 
    {
        ESP_LOGE(tag, "ble_svc_gap_device_name_set fallo, rc=%d", rc);
    }
}

void gap_svc_init(void)
{

    esp_err_t ret = nvs_flash_init(); /* Inicializa NVS: se usa para guardar datos de calibracion PHY */
    
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to init nvs %d ", ret);
        return;
    }

    ret = nimble_port_init();
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "Failed to init nimble %d ", ret);
        return;
    }

    /* Inicializa la configuracion del host de NimBLE. */
    ble_hs_cfg.reset_cb = gap_svc_on_reset;
    ble_hs_cfg.sync_cb = gap_svc_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = GAP_SVC_IO_CAP;
    ble_hs_cfg.sm_sc = 0;

    ble_store_config_init(); 
}
