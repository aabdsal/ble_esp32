/**
 * @file    gap_svc.h
 * @author  BLE-SEM
 * @version V0.0
 * @date    2026-05-14
 * @brief   Interfaz publica del servicio GAP
 */

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef GAP_SVC_H
#define GAP_SVC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#include <stdbool.h>
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/**
 * @brief Configura el backend de almacenamiento de enlaces y pares de NimBLE
 * @param None
 * @return None
 */
void ble_store_config_init(void);
/**
 * @brief Callback de reinicio invocado por NimBLE cuando el host debe reiniciarse
 * @param reason Motivo del reinicio reportado por NimBLE
 */
void gap_svc_on_reset(int reason);
/**
 * @brief Callback de sincronizacion invocado cuando NimBLE esta listo para anunciar
 * @param None
 * @return None
 */
void gap_svc_on_sync(void);
/**
 * @brief Inicializa NVS, el host de NimBLE y el estado del servicio GAP
 * @param None
 * @return None
 */
void gap_svc_init(void);
/**
 * @brief Establece el nombre del dispositivo BLE anunciado por el servicio GAP
 * @param name Nuevo nombre del dispositivo
 */
void gap_svc_set_device_name(const char *name);
/**
 * @brief Inicia la tarea del host de NimBLE
 * @param None
 * @return None
 */
void gap_svc_start(void);
/**
 * @brief Habilita o deshabilita la visibilidad BLE del dispositivo
 * @param enabled true para anunciar BLE, false para detener anuncio
 */
void gap_svc_set_enabled(bool enabled);
/**
 * @brief Consulta si BLE esta habilitado para anunciar
 * @param None
 * @return true si BLE esta habilitado, false en caso contrario
 */
bool gap_svc_get_enabled(void);


void gap_svc_advertise(void);


void gap_svc_start_advertising(void);


void gap_svc_stop_advertising(void);


#ifdef __cplusplus
}
#endif

#endif /* GAP_SVC_H */

/* End of file ****************************************************************/