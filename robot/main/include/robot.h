#ifndef GPIO_ROBOT_H
#define GPIO_ROBOT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

typedef enum
{
    SERVO1,
    SERVO2,
    SERVO3,
    SERVO4,
    SERVO5,
    SERVO6,
    ERROR_SERVO,
} robot_servo_t;

typedef enum
{
    HORARIO,
    ANTIHORARIO,
    ERROR,
} robot_move_t;

void robot_init();
void move_servo(robot_servo_t servo, robot_move_t move);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_ROBOT_H */