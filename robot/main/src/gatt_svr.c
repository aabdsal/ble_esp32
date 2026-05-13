#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"
#include "gatt_svr.h"
#include "robot.h"

/* Client Characteristic Configuration Descriptor = CCCD */
/* Maximum number of characteristics with the notify flag */
#define MAX_NOTIFY 5


static const char *toto = "TOTO";
static const char *mando = "MANDO";
static const char *robot = "ROBOT";

static robot_move_t status_val;
static robot_servo_t servo_val;
static robot_move_t move_val;
static bool servo_selected;
static bool move_selected;

static const ble_uuid128_t robot_controller_uuid =
    BLE_UUID128_INIT(0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0xCA, 0x00, 0x00, 0xea, 0x00, 0xea, 0x00, 0xea);

static uint16_t command_chr_val_handle;
static const ble_uuid128_t command_chr_uuid =
    BLE_UUID128_INIT(0xbb, 0xbb, 0xbb, 0xcc, 0xcc, 0xcc, 0xcc, 0xaa,
                     0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xea, 0x00, 0xea);

static uint16_t status_chr_val_handle;
static const ble_uuid128_t status_chr_uuid =
    BLE_UUID128_INIT(0x13, 0x13, 0x13, 0x47, 0x47, 0x47, 0x47, 0xaa,
                     0x0, 0x0, 0x0, 0x0, 0x0, 0xea, 0x35, 0xea);

/* A custom descriptor */
static uint8_t gatt_svr_dsc_val;

int handle_message(const char *msg, uint16_t len)
{
    bool updated = false;

    if (strcmp(msg, "H") == 0 || strstr(msg, "H") != NULL)
    {
        move_val = HORARIO;
        move_selected = true;
        updated = true; // Saber si el mensaje coincide al menos con algun comando valido/conocido
    }
    else if (strcmp(msg, "A") == 0 || strstr(msg, "A") != NULL)
    {
        move_val = ANTIHORARIO;
        move_selected = true;
        updated = true;
    }

    if (servo_selected && move_selected)
    {
        move_servo(servo_val, move_val);
    }

    if (strstr(msg, "S1") != NULL)
    {
        servo_val = SERVO1;
        servo_selected = true;
        updated = true;
    }
    else if (strstr(msg, "S2") != NULL)
    {
        servo_val = SERVO2;
        servo_selected = true;
        updated = true;
    }
    else if (strstr(msg, "S3") != NULL)
    {
        servo_val = SERVO3;
        servo_selected = true;
        updated = true;
    }
    else if (strstr(msg, "S4") != NULL)
    {
        servo_val = SERVO4;
        servo_selected = true;
        updated = true;
    }
    else if (strstr(msg, "S5") != NULL)
    {
        servo_val = SERVO5;
        servo_selected = true;
        updated = true;
    }
    else if (strstr(msg, "S6") != NULL)
    {
        servo_val = SERVO6;
        servo_selected = true;
        updated = true;
    }

    if (!updated)
    {
        ESP_LOGW(toto, "Mensaje desconocido: %s", msg);
        servo_val = ERROR_SERVO;
        return -1; // Error: mensaje desconocido
    }

    return 0;
}

int handle_robot_message(const char *msg, uint16_t len)
{
    int rc = handle_message(msg, len);
    if (rc != 0)
    {
        ESP_LOGE(toto, "Error manejando el mensaje: %s", msg);
        return -1; // Error al manejar el mensaje
    }
    return 0; // Mensaje manejado exitosamente
}

static int gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                            struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = 
{
    {
        /*** Robot Control Service ***/
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &robot_controller_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) 
        { 
            {
                /* Command characteristic: writable from BLE client */
                .uuid = &command_chr_uuid.u,
                .access_cb = gatt_svc_access,
                .flags = BLE_GATT_CHR_F_WRITE, 
                .val_handle = &command_chr_val_handle,
            }, 
            {
                /* Status characteristic: readable from BLE client */
                .uuid = &status_chr_uuid.u,
                .access_cb = gatt_svc_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY, 
                .val_handle = &status_chr_val_handle,
            },
            {
                0,
            }
        },
    },
    {
        0, /* No more services. */
    },
};

// static int gatt_svr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
//                void *dst, uint16_t *len)
// {
//     uint16_t om_len;
//     int rc;

//     om_len = OS_MBUF_PKTLEN(om);
//     if (om_len < min_len || om_len > max_len) 
//     {
//         return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
//     }

//     rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
//     if (rc != 0) 
//     {
//         return BLE_ATT_ERR_UNLIKELY;
//     }

//     return 0;
// }

/**
 * Access callback whenever a characteristic/descriptor is read or written to.
 * Here reads and writes need to be handled.
 * ctxt->op tells weather the operation is read or write and
 * weather it is on a characteristic or descriptor,
 * ctxt->dsc->uuid tells which characteristic/descriptor is accessed.
 * attr_handle give the value handle of the attribute being accessed.
 * Accordingly do:
 *     Append the value to ctxt->om if the operation is READ
 *     Write ctxt->om to the value if the operation is WRITE
 **/
static int gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    uint8_t mensaje[20];
    uint16_t len;
    uint8_t err = 0;
    
    switch (ctxt->op) 
    {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:

            rc = ble_hs_mbuf_to_flat(ctxt->om, mensaje, sizeof(mensaje), &len);            
            
            if (rc != 0)            
            {                
                ESP_LOGE(toto, "Error decodificando el mensaje, rc=%d", rc);                
                return BLE_ATT_ERR_UNLIKELY;            
            }

            char traducido[21];
            for(int idx = 0; idx < len; idx++)
            {
                traducido[idx] = (char)mensaje[idx];
            }
            traducido[len] = '\0';
            
            err = handle_robot_message(traducido, len);

            if (strcmp(traducido, "H") == 0)
            {
                move_servo(servo_val, "H"/"A")
            }
            ESP_LOGI(robot, "Has escrit aso prim: %s", traducido);
            
            rc = ble_gatts_notify(conn_handle, status_val);
            if (rc != 0)
            {
                ESP_LOGE(toto, "Error notificando el mensaje, rc=%d", rc);                
                return BLE_ATT_ERR_UNLIKELY;            
            }

            return 0;

        case BLE_GATT_ACCESS_OP_READ_CHR:
            
            if (attr_handle != status_chr_val_handle)
            {
                return BLE_ATT_ERR_UNLIKELY;
            }
            
            os_mbuf_append(ctxt->om, &status_val, sizeof(uint8_t));
            ESP_LOGI(mando, "Operacion de lectura exitososa, se parlar?");
            
            return 0;
        
        default:
            return BLE_ATT_ERR_UNLIKELY;      
    }
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) 
    {
        case BLE_GATT_REGISTER_OP_SVC:
            MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                        ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                        ctxt->svc.handle);
            break;

        case BLE_GATT_REGISTER_OP_CHR:
            MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                        "def_handle=%d val_handle=%d\n",
                        ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                        ctxt->chr.def_handle,
                        ctxt->chr.val_handle);
            break;

        case BLE_GATT_REGISTER_OP_DSC:
            MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                        ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                        ctxt->dsc.handle);
            break;

        default:
            assert(0);
            break;
    }
}

int gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) 
    {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) 
    {
        return rc;
    }

    /* Setting a value for the read-only descriptor */
    gatt_svr_dsc_val = 0x99;
    status_val = READY;

    return 0;
}
