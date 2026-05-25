#ifndef BUZZER_H
#define BUZZER_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* 蜂鸣器配置结构体 */
typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} BuzzerCfg_t;

/* 蜂鸣器句柄 */
typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} BuzzerHandle_t;

void buzzer_init(BuzzerHandle_t *handle, const BuzzerCfg_t *cfg);
void buzzer_on(BuzzerHandle_t *handle);
void buzzer_off(BuzzerHandle_t *handle);
void buzzer_toggle(BuzzerHandle_t *handle);

#endif
