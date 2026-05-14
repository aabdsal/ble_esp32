#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "botones.h"
#include "ble_client.h"

void app_main(void)
{
    botones_init();
    ble_client_init();

    uint8_t b_sel, b_ok, b_r, b_l;
    int servo = 1;

    for(;;)
    {
        botones_leer(&b_sel, &b_ok, &b_r, &b_l);

        /* seleccionar servo */
        if (b_sel)
        {
            servo++;
            if (servo > 6) servo = 1;
            printf("Servo seleccionado: %d\n", servo);
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        /* giro horario */
        if (b_r)
        {
            char msg[10];
            sprintf(msg, "s%d:r", servo);
            ble_send(msg);
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        /* giro antihorario */
        if (b_l)
        {
            char msg[10];
            sprintf(msg, "s%d:l", servo);
            ble_send(msg);
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}