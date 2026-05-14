#include "botones.h"

void botones_init(void)
{
    gpio_config_t io_conf = 
    {
        .pin_bit_mask = (1ULL << BTN_SELECT) | /* Configura TODOS estos pines con esta misma configuración */
                        (1ULL << BTN_OK) |
                        (1ULL << BTN_RIGHT) |
                        (1ULL << BTN_LEFT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&io_conf);
}

void botones_leer(uint8_t *b_sel, uint8_t *b_ok, uint8_t *b_r, uint8_t *b_l)
{
    *b_sel = !gpio_get_level(BTN_SELECT);
    *b_ok  = !gpio_get_level(BTN_OK);
    *b_r   = !gpio_get_level(BTN_RIGHT);
    *b_l   = !gpio_get_level(BTN_LEFT);
}