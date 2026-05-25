#ifndef BUZZER_H
#define BUZZER_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/* 蜂鸣器配置结构体 */
typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} buzzer_cfg_t;

/* 蜂鸣器句柄 */
typedef struct {
    GPIO_Regs *port;
    uint32_t pin;
} buzzer_handle_t;

void buzzer_init(buzzer_handle_t *handle, const buzzer_cfg_t *cfg);
void buzzer_on(buzzer_handle_t *handle);
void buzzer_off(buzzer_handle_t *handle);
void buzzer_toggle(buzzer_handle_t *handle);

#endif
