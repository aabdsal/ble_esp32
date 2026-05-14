/**
 * @file    gatt_svr.c
 * @author  BLE-SEM
 * @version V0.0
 * @date    2026-05-14
 * @brief   Implementacion del servidor GATT
 */

/* Includes ------------------------------------------------------------------*/

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

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Client Characteristic Configuration Descriptor = CCCD */
/* Numero maximo de caracteristicas con la bandera notify */
#define MAX_NOTIFY 5

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static const char *gatt = "[GATT]";
static const char *mando = "[MANDO]";
static const char *robot = "[ROBOT]";

static robot_servo_t servo_val;
static robot_move_t move_val;
static robot_status_t status_val;

static bool servo_selected;
static bool move_selected;

static const ble_uuid128_t robot_controller_uuid = 
BLE_UUID128_INIT(0xaa, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00,0xCA, 0x00, 0x00, 0xea, 0x00, 0xea, 0x00, 0xea);

static uint16_t command_chr_val_handle;
static const ble_uuid128_t command_chr_uuid =
BLE_UUID128_INIT(0xbb, 0xbb, 0xbb, 0xcc, 0xcc, 0xcc, 0xcc, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xea, 0x00, 0xea);

static uint16_t status_chr_val_handle;
static const ble_uuid128_t status_chr_uuid =
BLE_UUID128_INIT(0x13, 0x13, 0x13, 0x47, 0x47, 0x47, 0x47, 0xaa, 0x0, 0x0, 0x0, 0x0, 0x0, 0xea, 0x35, 0xea);

static uint8_t gatt_svr_dsc_val; /* A custom descriptor */

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief Decodifica una orden y actualiza el servo o el movimiento seleccionado
 * @param msg Mensaje bruto recibido desde el cliente BLE
 * @param len Longitud del mensaje en bytes
 * @return 0 si tiene exito, -1 si el mensaje no se reconoce
 */
static int handle_message(const char *msg, uint16_t len)
{
    bool updated = false;

    if (strcmp(msg, "H") == 0 || strstr(msg, "H") != NULL)
    {
        move_val = HORARIO;
        move_selected = true;
        updated = true; // Saber si el mensaje coincide al menos con algun comando valido
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
        ESP_LOGW(gatt, "Mensaje desconocido: %s", msg);
        servo_val = ERROR_SERVO;
        return -1; // Error: mensaje desconocido
    }

    return 0;
}

/**
 * @brief Valida y procesa un comando de control del robot recibido por BLE
 * @param msg Cadena de comando bruta
 * @param len Longitud del comando
 * @return 0 si tiene exito, -1 si falla el analisis o la validacion
 */
static int handle_robot_message(const char *msg, uint16_t len)
{
    int rc = handle_message(msg, len);
    if (rc != 0)
    {
        ESP_LOGE(gatt, "Error manejando el mensaje: %s", msg);
        return -1; // Error al manejar el mensaje
    }
    return 0; // Mensaje manejado exitosamente
}

/**
 * @brief Callback de acceso GATT para las caracteristicas de comando y estado
 * @param conn_handle Identificador de conexion BLE
 * @param attr_handle Identificador del valor del atributo
 * @param ctxt Contexto de acceso GATT
 * @param arg Argumento de usuario, no usado
 * @return 0 si tiene exito o un codigo de error BLE ATT
 */
static int gatt_svc_access(
    uint16_t conn_handle, 
    uint16_t attr_handle, 
    struct ble_gatt_access_ctxt *ctxt, 
    void *arg
);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = 
{
    {
        /*** Robot Control Service ***/
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &robot_controller_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) 
        { 
            {
                /* Caracteristica de comando: escribible desde el cliente BLE */
                .uuid = &command_chr_uuid.u,
                .access_cb = gatt_svc_access,
                .flags = BLE_GATT_CHR_F_WRITE, 
                .val_handle = &command_chr_val_handle,
            }, 
            {
                /* Caracteristica de estado: legible desde el cliente BLE */
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
        0, /* No hay mas servicios. */
    },
};

/* Private functions ---------------------------------------------------------*/

/**
 * Callback de acceso cuando se lee o escribe una caracteristica o descriptor.
 * Aqui hay que gestionar lecturas y escrituras.
 * ctxt->op indica si la operacion es lectura o escritura y
 * si es sobre una caracteristica o un descriptor.
 * ctxt->dsc->uuid indica que caracteristica o descriptor se accede.
 * attr_handle da el identificador del valor del atributo accedido.
 * Segun corresponda:
 *     Anadir el valor a ctxt->om si la operacion es READ
 *     Escribir ctxt->om en el valor si la operacion es WRITE
 **/
static int gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    uint8_t mensaje[20];
    uint16_t len;
    
    switch (ctxt->op) 
    {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:

            rc = ble_hs_mbuf_to_flat(ctxt->om, mensaje, sizeof(mensaje), &len);            
            
            if (rc != 0)            
            {                
                ESP_LOGE(gatt, "Error decodificando el mensaje, rc=%d", rc);                
                return BLE_ATT_ERR_UNLIKELY;            
            }

            char traducido[21];
            for(int idx = 0; idx < len; idx++)
            {
                traducido[idx] = (char)mensaje[idx];
            }
            traducido[len] = '\0';
            
            rc = handle_robot_message(traducido, len);
            if (rc != 0)            
            {                
                ESP_LOGE(gatt, "Error decodificando el mensaje, rc=%d", rc);                
                return BLE_ATT_ERR_UNLIKELY;            
            }

            if (strcmp(traducido, "H") == 0)
            {
                move_servo(servo_val, HORARIO);
                ESP_LOGI(robot, "Giro horario");
            }
            else if (strcmp(traducido, "A") == 0)
            {
                move_servo(servo_val, ANTIHORARIO);
                ESP_LOGI(robot, "Giro antihorario");
            }
            
            ESP_LOGI(robot, "Mensaje: %s", traducido);
            /*rc = ble_gatts_notify(conn_handle, status_val);
            if (rc != 0)
            {
                ESP_LOGE(gatt, "Error notificando el mensaje, rc=%d", rc);                
                return BLE_ATT_ERR_UNLIKELY;            
            }*/

            return 0;

        case BLE_GATT_ACCESS_OP_READ_CHR:
            
            if (attr_handle != status_chr_val_handle)
            {
                return BLE_ATT_ERR_UNLIKELY;
            }
            
            os_mbuf_append(ctxt->om, &status_val, sizeof(uint8_t));
            ESP_LOGI(mando, "Lectura hecha");
            
            return 0;
        
        default:
            return BLE_ATT_ERR_UNLIKELY;      
    }
}

/* Exported functions --------------------------------------------------------*/

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

void gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != ESP_OK) 
    {
        ESP_LOGE(gatt, "ble_gatts_count_cfg(gatt_svr_svcs) fallo: %s", esp_err_to_name(rc));
        return;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != ESP_OK) 
    {
        ESP_LOGE(gatt, "ble_gatts_add_svcs(gatt_svr_svcs) fallo: %s", esp_err_to_name(rc));
        return;
    }

    /* Establece un valor para el descriptor de solo lectura */
    gatt_svr_dsc_val = 0x99;

}

/* End of file ****************************************************************/
