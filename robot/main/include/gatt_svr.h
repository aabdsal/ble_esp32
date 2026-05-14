/**
 * @file    gatt_svr.h
 * @author  BLE-SEM
 * @version V0.0
 * @date    2026-05-14
 * @brief   Interfaz publica del servidor GATT
 */

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef GATT_SVR_H
#define GATT_SVR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/** Servidor GATT, GATT = Generic Attribute Profile */
#define GATT_SVR_SVC_ALERT_UUID               0x1811
#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID   0x2A47
#define GATT_SVR_CHR_NEW_ALERT                0x2A46
#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID   0x2A48
#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID      0x2A45
#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT        0x2A44

/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief Callback usado por NimBLE al registrar servicios y atributos GATT
 * @param ctxt Contexto de registro proporcionado por NimBLE
 * @param arg Argumento de usuario, no usado
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

/**
 * @brief Inicializa el servidor GATT personalizado y registra sus servicios
 * @param None
 * @return None
 */
void gatt_svr_init(void);

#ifdef __cplusplus
}
#endif

#endif /* GATT_SVR_H */

/* End of file ****************************************************************/
