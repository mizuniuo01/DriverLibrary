#ifndef LED_H
#define LED_H

#include <stm32f4xx_hal.h>
#include <stdint.h>

/* LED 配置结构体 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t active_level; /* 有效电平：1=高电平点亮，0=低电平点亮 */
} LedCfg_t;

/* LED 句柄 */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t active_level;
} LedHandle_t;

void led_init(LedHandle_t *handle, const LedCfg_t *cfg);
void led_on(LedHandle_t *handle);
void led_off(LedHandle_t *handle);
void led_toggle(LedHandle_t *handle);
void led_set(LedHandle_t *handle, uint8_t state);

#endif
