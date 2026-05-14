/**
 * @file    robot.c
 * @author  BLE-SEM
 * @version V0.0
 * @date    2026-05-14
 * @brief   Robot control implementation
 */

/* Includes ------------------------------------------------------------------*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "robot.h"
#include "gap_svc.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

#define ESP_INTR_FLAG_DEFAULT 0

static const char *tag = "[ROBOT]";
static const gpio_num_t BUTTON_PIN = GPIO_NUM_3;
static const gpio_num_t LED_PIN = GPIO_NUM_5;
static const gpio_num_t SDA_PIN = GPIO_NUM_8;
static const gpio_num_t SCL_PIN = GPIO_NUM_10;

#define PCA9685_SERVO_BASE_REG 0x06
#define PCA9685_PWM_PERIOD_US 20000U
#define SERVO_MIN_PULSE_US 500U
#define SERVO_MAX_PULSE_US 2500U
#define SERVO_MIN_ANGLE 0U
#define SERVO_MAX_ANGLE 180U
#define SERVO_STEP 1U
#define SERVO_COUNT 6U

/* Private macro -------------------------------------------------------------*/

/*
 * NOTE:
 * La ESP32 manda comandos al PCA9685.
 * El PCA9685 ya está diseñado para ser slave I2C.
 * Por eso la ESP32 debe ir en modo master y escribir registros del PCA9685.
 */
i2c_master_dev_handle_t dev_handle;

/* Private variables ---------------------------------------------------------*/
// Canal de control del servo, se puede modificar para controlar diferentes servos
// Primer valor en hex: Canal del servo (0-15), Resto de valores en hex: valor de PWM (0-255)
static uint16_t servo_angle[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
static uint8_t canal_servo[] = {PCA9685_SERVO_BASE_REG, 0x00, 0x00, 0x00, 0x00};

static bool volatile interruptor_activado = false;
static portMUX_TYPE interruptor_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile TickType_t last_button_tick = 0;
static TaskHandle_t bluetooth_control_task_handle = NULL;

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

// NOTA: Mirar esta funcion para entender como funciona.
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

/* Exported functions --------------------------------------------------------*/


static void IRAM_ATTR gpio_handler_isr(void *arg)
{
    uint32_t pin = (uint32_t)arg;
    if (pin == BUTTON_PIN) 
    {
        TickType_t now_tick = xTaskGetTickCountFromISR();
        bool retardo;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        portENTER_CRITICAL_ISR(&interruptor_mux);
        retardo = (now_tick - last_button_tick) < pdMS_TO_TICKS(150);
        if (!retardo)
        {
            last_button_tick = now_tick;
            interruptor_activado = !interruptor_activado;
            if (bluetooth_control_task_handle != NULL)
            {
                vTaskNotifyGiveFromISR(bluetooth_control_task_handle, &xHigherPriorityTaskWoken);
            }
        }
        portEXIT_CRITICAL_ISR(&interruptor_mux);

        if (xHigherPriorityTaskWoken == pdTRUE)
        {
            portYIELD_FROM_ISR();
        }
    }
}

void bluetooth_control_task(void *arg)
{
    bool last_applied_state = !interruptor_activado;
    for(;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        bool activado;
        portENTER_CRITICAL(&interruptor_mux);
        activado = interruptor_activado;
        portEXIT_CRITICAL(&interruptor_mux);

        if (activado != last_applied_state)
        {
            gap_svc_set_enabled(activado);

            if (activado)
            {
                ESP_LOGI(tag, "Bluetooth activado");
            }
            else
            {
                ESP_LOGI(tag, "Bluetooth desactivado");
            }

            last_applied_state = activado;
        }
    }
}

void robot_init()
{
    ESP_LOGI(tag, "Iniciando configuracion del boton, led y PCA9685 para el robot");
    gpio_config_t interruptor_conf = 
    {
        .pin_bit_mask = 1ULL << BUTTON_PIN, 
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config_t led_conf = 
    {
        .pin_bit_mask = 1ULL << LED_PIN, 
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret;

    ret = gpio_config(&interruptor_conf);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "gpio_config(interruptor_conf) fallo: %s", esp_err_to_name(ret));
    }

    ret = gpio_config(&led_conf);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "gpio_config(led_conf) fallo: %s", esp_err_to_name(ret));
    }

    portENTER_CRITICAL(&interruptor_mux);
    interruptor_activado = (gpio_get_level(BUTTON_PIN) == 0);
    portEXIT_CRITICAL(&interruptor_mux);

    gap_svc_set_enabled(interruptor_activado);

    BaseType_t bluetooth_task = xTaskCreate(
        bluetooth_control_task,
        "bluetooth_control_task",
        2048,
        NULL,
        4,
        &bluetooth_control_task_handle);
    if (bluetooth_task != pdPASS)
    {
        ESP_LOGE(tag, "No se pudo crear bluetooth_control_task");
    }

    ret = gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(tag, "gpio_install_isr_service() fallo: %s", esp_err_to_name(ret));
    }

    ret = gpio_isr_handler_add(BUTTON_PIN, gpio_handler_isr, (void *)BUTTON_PIN);
    if (ret != ESP_OK)
    {
        ESP_LOGE(tag, "gpio_isr_handler_add() fallo: %s", esp_err_to_name(ret));
    }

    // Configura la comunicacion del bus Inter-Integrated Circuit 
    i2c_master_bus_config_t bus_cfg = 
    {
        .i2c_port = I2C_NUM_0,                  // Puerto I2C, -1 para autodeteccion
        .sda_io_num = SDA_PIN,                  
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,                 // Ajusta el filtrado de ruido en la senal
        .flags.allow_pd = false,
        .flags.enable_internal_pullup = true, 
    };

    i2c_master_bus_handle_t bus_handle;

    // Configura el dispositivo concreto con el que va a hablar la ESP32
    i2c_device_config_t dev_cfg = 
    {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x40, /* Direccion del modulo PCA9685 */
        .scl_speed_hz = 100000,
    };

    ret = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "i2c_new_master_bus() fallo: %s", esp_err_to_name(ret));
    }
    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "i2c_master_bus_add_device() fallo: %s", esp_err_to_name(ret));
    }
    const uint8_t mode_one_on[] = {0x00, 0x10}; /* MODE1 con SLEEP = 1 */
    size_t size_data_wr = sizeof(mode_one_on);

    ret = i2c_master_transmit(dev_handle, mode_one_on, size_data_wr, -1);   
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "i2c_master_transmit(mode_one_on) fallo: %s", esp_err_to_name(ret));
    }
    const uint8_t prescale[] = {0xFE, 121}; /* PRE_SCALE = 121 (50 Hz) */
    ret = i2c_master_transmit(dev_handle, prescale, sizeof(prescale), -1);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "i2c_master_transmit(prescale) fallo: %s", esp_err_to_name(ret));
    }
    const uint8_t mode_one_off[] = {0x00, 0x00}; /* Volver a MODE1 con SLEEP = 0 */
    ret = i2c_master_transmit(dev_handle, mode_one_off, sizeof(mode_one_off), -1);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "i2c_master_transmit(mode_one_off) fallo: %s", esp_err_to_name(ret));
    }
    const uint8_t restart[] = {0x00, 0x80}; /* Reinicio */
    ret = i2c_master_transmit(dev_handle, restart, sizeof(restart), -1);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "i2c_master_transmit(restart) fallo: %s", esp_err_to_name(ret));
    }
    ESP_LOGI(tag, "Configuracion completada");
}

void move_servo(robot_servo_t servo, robot_move_t move)
{ 
    uint8_t channel = servo_to_channel(servo);

    if (channel == 0xFF)
    {
        ESP_LOGW(tag, "Servo invalido: %d", servo);
        return;
    }
    int next_angle = (int)servo_angle[channel];
    switch (move)
    {
        case HORARIO:
            next_angle += SERVO_STEP;
            break;
        case ANTIHORARIO:
            next_angle -= SERVO_STEP;
            break;
        default:
            break;
            ESP_LOGW(tag, "Direccion invalida: %d", move);
    }
    servo_angle[channel] = clamp_angle(next_angle);

    uint16_t ticks = angle_to_ticks(servo_angle[channel]);
    
    canal_servo[0] = PCA9685_SERVO_BASE_REG + (uint8_t)(4U * channel);
    canal_servo[1] = 0x00;
    canal_servo[2] = 0x00;
    canal_servo[3] = (uint8_t)(ticks & 0xFFU);
    canal_servo[4] = (uint8_t)((ticks >> 8) & 0xFFU);

    esp_err_t ret;
    ret = i2c_master_transmit(dev_handle, canal_servo, sizeof(canal_servo), -1);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(tag, "i2c_master_transmit(canal_servo) fallo: %s", esp_err_to_name(ret));
    }
}

void check_interruptor_and_control_bluetooth(void)
{

    int interruptor_state = gpio_get_level(BUTTON_PIN);
    if (interruptor_state == 0) 
    {
        ESP_LOGI(tag, "Interruptor activado, encendiendo Bluetooth");
        gap_svc_set_enabled(true);
    } 
    else 
    {
        ESP_LOGI(tag, "Interruptor desactivado, apagando Bluetooth");
        gap_svc_set_enabled(false);
    }
}

/* End of file ****************************************************************/