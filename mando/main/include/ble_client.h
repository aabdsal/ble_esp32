/**
 * @file    ble_client.h
 * @author  BLE-SEM
 * @version V0.0
 * @date    2026-05-16
 * @brief   Interfaz publica del cliente BLE.
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef BLE_CLIENT_H
#define BLE_CLIENT_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

/******************************************************************************/
/**
 * @brief  Inicializa el cliente BLE y comienza el escaneo.
 * @param  None
 * @retval None
 */
void ble_client_init(void);

/******************************************************************************/
/**
 * @brief  Envia un mensaje al dispositivo conectado.
 * @param  msg Puntero al buffer del mensaje terminado en null.
 * @retval None
 */
void ble_send(char *msg);

/******************************************************************************/
/**
 * @brief  Configura el nombre de dispositivo GAP local.
 * @param  name Cadena de nombre terminada en null.
 * @retval None
 */
void ble_client_set_device_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif
/*** End of file **************************************************************/