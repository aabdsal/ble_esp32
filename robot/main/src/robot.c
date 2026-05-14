/**
 * @file robot.c
 * @author BLE-SEM
 * @version V0.0
 * @date 2026-05-14
 * @brief Robot control implementation
 */

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"

#include "driver/i2c_master.h"

#include "robot.h"
#include "gap_svc.h"

/* Private define ------------------------------------------------------------*/
#define ESP_INTR_FLAG_DEFAULT 0

static const char *tag = "[ROBOT]";

static const gpio_num_t INTERRUPTOR_PIN = GPIO_NUM_4;
static const gpio_num_t LED_PIN         = GPIO_NUM_3;
// static const gpio_num_t SDA_PIN         = GPIO_NUM_8; Per a que no done els errors ixos ho he comentat
// static const gpio_num_t SCL_PIN         = GPIO_NUM_10;

#define PCA9685_SERVO_BASE_REG 0x06
#define PCA9685_PWM_PERIOD_US  20000U
#define SERVO_MIN_PULSE_US     500U
#define SERVO_MAX_PULSE_US     2500U
#define SERVO_MIN_ANGLE        0U
#define SERVO_MAX_ANGLE        180U
#define SERVO_STEP             1U
#define SERVO_COUNT            6U

/* Private variables ---------------------------------------------------------*/

/*
 * La ESP32 manda comandos al PCA9685.
 * El PCA9685 ya está diseñado para ser slave I2C.
 * Por eso la ESP32 debe ir en modo master y escribir registros del PCA9685.
 */
i2c_master_dev_handle_t dev_handle;

static uint16_t servo_angle[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
static uint8_t canal_servo[] = {PCA9685_SERVO_BASE_REG, 0x00, 0x00, 0x00, 0x00};

static TaskHandle_t bluetooth_control_task_handle = NULL;

/* Private function prototypes ----------------------------------------------*/
static void gpio_handler_isr(void *arg);
static void bluetooth_control_task(void *arg);

static uint16_t clamp_angle(int angle);
static uint8_t servo_to_channel(robot_servo_t servo);
static uint16_t angle_to_ticks(uint16_t angle);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Limita el angulo de un servo al rango soportado
 * @param angle Angulo solicitado en grados
 * @return Angulo limitado al intervalo valido del servo
 */
static uint16_t clamp_angle(int angle)
{
    if (angle < (int)SERVO_MIN_ANGLE)
    {
        return SERVO_MIN_ANGLE;
    }

    if (angle > (int)SERVO_MAX_ANGLE)
    {
        return SERVO_MAX_ANGLE;
    }

    return (uint16_t)angle;
}

/**
 * @brief Convierte un enum de servo del robot al canal asociado del PCA9685
 * @param servo Identificador del servo
 * @return Indice del canal, o 0xFF si el servo no es valido
 */
static uint8_t servo_to_channel(robot_servo_t servo)
{
    if (servo < SERVO1 || servo > SERVO6)
    {
        return 0xFF;
    }

    return (uint8_t)servo;
}

/**
 * @brief Convierte un angulo de servo en ticks PWM del PCA9685
 * @param angle Angulo del servo en grados
 * @return Valor de ticks PWM para el angulo objetivo
 */
static uint16_t angle_to_ticks(uint16_t angle)
{
    uint32_t pulse_us = SERVO_MIN_PULSE_US;

    pulse_us += ((uint32_t)angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) /
                SERVO_MAX_ANGLE;

    return (uint16_t)((pulse_us * 4096U) / PCA9685_PWM_PERIOD_US);
}

/**
 * @brief ISR del interruptor.
 *
 * La ISR NO arranca ni para BLE directamente.
 * Solo despierta/notifica a la tarea bluetooth_control_task.
 */
static void IRAM_ATTR gpio_handler_isr(void *arg)
{
    uint32_t pin = (uint32_t)(uintptr_t)arg;

    if (pin == INTERRUPTOR_PIN && bluetooth_control_task_handle != NULL)
    {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        vTaskNotifyGiveFromISR(
            bluetooth_control_task_handle,
            &xHigherPriorityTaskWoken
        );

        if (xHigherPriorityTaskWoken == pdTRUE)
        {
            portYIELD_FROM_ISR();
        }
    }
}

/**
 * @brief Tarea que controla el estado BLE segun el interruptor.
 *
 * ON  -> gap_svc_start_advertising()
 * OFF -> gap_svc_stop_advertising()
 */
static void bluetooth_control_task(void *arg)
{
    int last_applied_state = -1;

    for(;;)
    {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
        vTaskDelay(pdMS_TO_TICKS(50));

        int current_state = gpio_get_level(INTERRUPTOR_PIN);

        if (current_state != last_applied_state)
        {
            last_applied_state = current_state;

            if (current_state == 1)
            {
                ESP_LOGI(tag, "Interruptor ON -> activar BLE advertising");

                gpio_set_level(LED_PIN, 1);
                gap_svc_start_advertising();
            }
            else
            {
                ESP_LOGI(tag, "Interruptor OFF -> desactivar BLE advertising");

                gpio_set_level(LED_PIN, 0);
                gap_svc_stop_advertising();
            }
        }
    }
}

/* Exported functions --------------------------------------------------------*/

void robot_init(void)
{
    ESP_LOGI(tag, "Iniciando configuracion del interruptor, LED y robot");

    gpio_config_t interruptor_conf =
    {
        .pin_bit_mask = 1ULL << INTERRUPTOR_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_ANYEDGE
    };

    gpio_config_t led_conf =
    {
        .pin_bit_mask = 1ULL << LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&interruptor_conf));
    ESP_ERROR_CHECK(gpio_config(&led_conf));

    gpio_set_level(LED_PIN, 0);

    /*
     * Instala servicio ISR.
     * Si ya estaba instalado en otro modulo, ESP_ERR_INVALID_STATE no es critico.
     */
    esp_err_t err = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(
        gpio_isr_handler_add(
            INTERRUPTOR_PIN,
            gpio_handler_isr,
            (void *)(uintptr_t)INTERRUPTOR_PIN
        )
    );

    xTaskCreate(
        bluetooth_control_task,
        "ble_ctrl_task",
        4096,
        NULL,
        5,
        &bluetooth_control_task_handle
    );
}

void move_servo(robot_servo_t servo, robot_move_t move)
{
    uint8_t channel = servo_to_channel(servo);

    if (channel == 0xFF)
    {
        ESP_LOGE(tag, "Servo no valido");
        return;
    }

    int new_angle = servo_angle[channel];

    if (move == HORARIO)
    {
        new_angle += SERVO_STEP;
    }
    else if (move == ANTIHORARIO)
    {
        new_angle -= SERVO_STEP;
    }
    else
    {
        ESP_LOGE(tag, "Movimiento no valido");
        return;
    }

    servo_angle[channel] = clamp_angle(new_angle);

    uint16_t ticks = angle_to_ticks(servo_angle[channel]);

    uint16_t on = 0;
    uint16_t off = ticks;

    canal_servo[0] = PCA9685_SERVO_BASE_REG + (4 * channel);
    canal_servo[1] = on & 0xFF;
    canal_servo[2] = (on >> 8) & 0xFF;
    canal_servo[3] = off & 0xFF;
    canal_servo[4] = (off >> 8) & 0xFF;

    /*
     * Aqui iria tu escritura I2C al PCA9685 si ya la tenias implementada.
     * Ejemplo:
     *
     * ESP_ERROR_CHECK(
     *     i2c_master_transmit(dev_handle, canal_servo, sizeof(canal_servo), -1)
     * );
     */

    ESP_LOGI(
        tag,
        "Servo %u movido a %u grados, ticks=%u",
        channel + 1,
        servo_angle[channel],
        ticks
    );
}

/* End of file ***************************************************************/