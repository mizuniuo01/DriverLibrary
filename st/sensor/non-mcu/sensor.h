#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>
#include <stm32f4xx_hal.h>
#include "drv_err.h"

/* 最大支持通道数（实际使用通道数通过 sensor_cfg_t.channel_count 配置） */
typedef enum {
    SENSOR_MAX_CHANNELS = 8,
} sensor_limit_t;

/* 单通道 GPIO 配置 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} sensor_channel_cfg_t;

/* 传感器配置 */
typedef struct {
    sensor_channel_cfg_t channels[SENSOR_MAX_CHANNELS];
    uint8_t channel_count; /* 实际使用通道数（如 7） */
    uint8_t active_level;  /* 0=低电平为黑线，1=高电平为黑线 */
} sensor_cfg_t;

/* sensor_task 调度标志位（ISR 置1，task 清0） */
extern volatile uint8_t sensor_tick_flag;

drv_err_t sensor_init(const sensor_cfg_t *cfg);
void sensor_task(void);
uint8_t sensor_read_data(void);

#endif
