#ifndef BOTONES_H
#define BOTONES_H

#include "driver/gpio.h"

#define BTN_SELECT GPIO_NUM_4
#define BTN_OK     GPIO_NUM_5
#define BTN_RIGHT  GPIO_NUM_6
#define BTN_LEFT   GPIO_NUM_7

void botones_init(void);
void botones_leer(uint8_t *b_sel, uint8_t *b_ok, uint8_t *b_r, uint8_t *b_l);

#endif