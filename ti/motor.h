#ifndef MOTOR_H
#define MOTOR_H

#include "ti_msp_dl_config.h"
#include <stdint.h>

/*
 * 速度参数基于 20kHz PWM、PWM_MAX_COMPARE=2000：
 *   MOTOR_MAX_SPEED = PWM_MAX_COMPARE（100% 占空比）
 *   MOTOR_EFFECTIVE_MIN_SPEED = 60（3% 占空比，电机死区阈值）
 *   PWM_MAX_COMPARE 变更时需同步调整 MOTOR_MAX_SPEED。
 */
#define MOTOR_MAX_SPEED 2000 /* = PWM_MAX_COMPARE，100% 占空比 */
#define MOTOR_MIN_SPEED 0 /* 零速 */
#define MOTOR_EFFECTIVE_MIN_SPEED 60 /* 死区阈值，低于此值按 0 输出 */

/* 电机配置结构体 */
typedef struct {
    GPIO_Regs *port;
    uint32_t l_nsleep_pin;
    uint32_t r_nsleep_pin;
    uint32_t l_ph_in2_pin;
    uint32_t r_ph_in2_pin;
} MotorCfg_t;

/* 电机句柄 */
typedef struct {
    GPIO_Regs *port;
    uint32_t l_nsleep_pin;
    uint32_t r_nsleep_pin;
    uint32_t l_ph_in2_pin;
    uint32_t r_ph_in2_pin;
} MotorHandle_t;

void motor_init(MotorHandle_t *handle, const MotorCfg_t *cfg);
void motor_set_speed_left(MotorHandle_t *handle,
                          GPTIMER_Regs *htim,
                          int16_t speed);
void motor_set_speed_right(MotorHandle_t *handle,
                           GPTIMER_Regs *htim,
                           int16_t speed);
int8_t motor_get_right_direction_sign(void);

#endif
