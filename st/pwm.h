#ifndef PWM_H
#define PWM_H

#include <stdint.h>
#include <stm32f4xx_hal.h>

/*
 * PWM_MAX_COMPARE 对应 CubeMX 中定时器的 ARR 值（自动重装载）。
 * 若修改 CubeMX 中的定时器时钟或 ARR，需同步更新此宏。
 */
#include "drv_err.h"

#define PWM_MAX_COMPARE 8400 /* 定时器 ARR 值，决定 PWM 分辨率 */

drv_err_t pwm_init(TIM_HandleTypeDef *htim);
void pwm_set_compare_ch3(TIM_HandleTypeDef *htim, uint16_t compare);
void pwm_set_compare_ch4(TIM_HandleTypeDef *htim, uint16_t compare);

#endif
