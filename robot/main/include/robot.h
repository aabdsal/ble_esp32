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
    SERVO6
} robot_servo_t;

typedef enum
{
    READY,
    HOME,
    HORARIO,
    ANTIHORARIO,
    ERROR,
    UNKOWN
} robot_state_t;

void robot_init();
void move_servo(robot_servo_t giro);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_ROBOT_H */