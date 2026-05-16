/**
 * @file    main.c
 * @author  BLE-SEM
 * @version V0.0
 * @date    2026-05-16
 * @brief   Punto de entrada de la aplicacion.
 */

/* Includes ------------------------------------------------------------------*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "mando.h"
#include "ble_client.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

static const char *tag = "[MANDO]";

/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/******************************************************************************/
/**
 * @brief  Bucle principal de la aplicacion.
 * @param  None
 * @retval None
 */
void app_main(void)
{
    mando_init();
    ble_client_init();
    
    uint8_t servo = 1;

    for(;;)
    {        
        if (mando_btn_select_read())
        {
            servo++;
            if (servo > 6) servo = 1;
            //ESP_LOGI(tag, "Servo seleccionado: %d", servo);
            ESP_LOGI(tag, "Boton select seleccionado");
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        if (mando_btn_right_read())
        {
            //char msg[10];
            ESP_LOGI(tag, "Boton right seleccionado");
            //ble_send(msg);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        
        if (mando_btn_left_read())
        {
            //char msg[10];
            ESP_LOGI(tag, "Boton left seleccionado");
            //ble_send(msg);
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        
        if (mando_btn_ok_read())
        {
            //char msg[10];
            ESP_LOGI(tag, "Boton ok seleccionado");
            //ble_send(msg);
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* Private functions ---------------------------------------------------------*/

/* End of file ****************************************************************/