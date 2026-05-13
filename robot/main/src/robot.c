#include "robot.h"

static const char *TAG = "[ROBOT]";
static const gpio_num_t BUTTON_PIN = GPIO_NUM_2;
static const gpio_num_t LED = GPIO_NUM_5;
static const gpio_num_t SDA_PIN = GPIO_NUM_5;
static const gpio_num_t SCL_PIN = GPIO_NUM_5;

#define PCA9685_SERVO_BASE_REG 0x06
#define PCA9685_PWM_PERIOD_US 20000U
#define SERVO_MIN_PULSE_US 500U
#define SERVO_MAX_PULSE_US 2500U
#define SERVO_MIN_ANGLE 0U
#define SERVO_MAX_ANGLE 180U
#define SERVO_STEP 1U
#define SERVO_COUNT 6U

//    NOTE:
//    La ESP32 manda comandos al PCA9685.
//    El PCA9685 ya está diseñado para ser slave I2C.
//    Por eso la ESP32 debe ir en modo master y escribir registros del PCA9685.
i2c_master_dev_handle_t dev_handle;

// Canal de control del servo, se puede modificar para controlar diferentes servos
// Primero byte: Canal del servo (0-15), Segundo byte: valor de PWM (0-255)
static uint16_t servo_angle[SERVO_COUNT] = {90, 90, 90, 90, 90, 90};
static uint8_t canal_servo[] = {PCA9685_SERVO_BASE_REG, 0x00, 0x00, 0x00, 0x00};

static uint16_t
clamp_angle(int angle)
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

static uint8_t
servo_to_channel(robot_servo_t servo)
{
    if (servo < SERVO1 || servo > SERVO6)
    {
        return 0xFF;
    }

    return (uint8_t)servo;
}

// NOTE: Mirar esta funcion para saber como funciona, ENTENDERLA
static uint16_t
angle_to_ticks(uint16_t angle)
{
    uint32_t pulse_us = SERVO_MIN_PULSE_US;
    pulse_us += ((uint32_t)angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) /
                SERVO_MAX_ANGLE;

    return (uint16_t)((pulse_us * 4096U) / PCA9685_PWM_PERIOD_US);
}

void 
robot_init()
{
    ESP_LOGI(TAG, "Iniciando configuracion el boton, led y modulo para el robot");
    gpio_config_t button_conf = 
    {
        .pin_bit_mask = 1ULL << BUTTON_PIN, 
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config_t led_conf = 
    {
        .pin_bit_mask = 1ULL << LED, 
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&button_conf));
    ESP_ERROR_CHECK(gpio_config(&led_conf));

    // Configura la comunicacion (el bus) Inter-Integrated Circuit 
    i2c_master_bus_config_t bus_cfg = 
    {
        .i2c_port = I2C_NUM_0,                  // Set i2c port, -1 is for autodetect
        .sda_io_num = SDA_PIN,                  
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,                 // Glitch period, ajustar ruido de señales
        .flags.allow_pd = false,
        .flags.enable_internal_pullup = true, 
    };

    i2c_master_bus_handle_t bus_handle;

    // Configura un dispositivo concreto con el que va a hablar la ESP32
    i2c_device_config_t dev_cfg = 
    {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x40, /* Direccion del módulo PCA9685 */
        .scl_speed_hz = 100000,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus_handle));
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    const uint8_t mode_one_on[] = {0x00, 0x10}; /* Mode one con Sleep = 1, registro y valor ? */
    size_t size_data_wr = sizeof(mode_one_on);

    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, mode_one_on, size_data_wr, -1));   

    const uint8_t prescale[] = {0xFE, 121}; /* PRE_SCALE = 121 (50 Hz) */
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, prescale, sizeof(prescale), -1));

    const uint8_t mode_one_off[] = {0x00, 0x00}; /* Volver a MODE1 con SLEEP=0. */
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, mode_one_off, sizeof(mode_one_off), -1));
    
    const uint8_t restart[] = {0x00, 0x80}; /* TODO: Restart */
    ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, restart, sizeof(restart), -1));

    ESP_LOGI(TAG, "Configuracion completada");
}

void
move_servo(robot_servo_t servo, robot_move_t move)
{ 
    uint8_t channel = servo_to_channel(servo);

    if (channel == 0xFF)
    {
        ESP_LOGW(TAG, "Servo invalido: %d", servo);
        return;
    }
    int next_angle = (int)servo_angle[channel];
    switch (move)
    {
        case HORARIO:
            /* code */
            next_angle += SERVO_STEP;
        case ANTIHORARIO:
            /* code */
            next_angle -= SERVO_STEP;
        
        default:
            break;
            ESP_LOGW(TAG, "Direccion invalida: %d", move);
    }
    servo_angle[channel] = clamp_angle(next_angle);

    uint16_t ticks = angle_to_ticks(servo_angle[channel]);
    canal_servo[0] = PCA9685_SERVO_BASE_REG + (uint8_t)(4U * channel);
    canal_servo[1] = 0x00;
    canal_servo[2] = 0x00;
    canal_servo[3] = (uint8_t)(ticks & 0xFFU);
    canal_servo[4] = (uint8_t)((ticks >> 8) & 0xFFU);
    i2c_master_transmit(dev_handle, canal_servo, sizeof(canal_servo), -1);
}