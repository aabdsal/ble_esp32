/**
    Copyright (C) 2026 Bluetooth_ESP32
    
    @file    main.c
    @author  BLUETOOTH
    @version V0.0
    @date    2026-04-01
    @brief  Punto de entrada del programa (app_main).
*/

/* Includes ------------------------------------------------------------------*/

// Modulos estadares de C
#include <stdio.h>
#include <stdint.h>

// Modulos propios de esp-idf
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_bt_device.h"
#include "driver/gpio.h"

// Modulos propios para el proyecto
#include "bluetooth.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

void blueetooth_init();
void blueetooth_is_connect();
void recibirDigital(int boton, char *datos);
void recibirI2C(horario/antihorario);
void pwm_to_i2c();

void app_main(void)
{
    return;
}
/* End of file ****************************************************************/

